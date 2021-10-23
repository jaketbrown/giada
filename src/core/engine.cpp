/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2021 Giovanni A. Zuliani | Monocasual
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */

#include "core/engine.h"
#include "core/model/model.h"
#include "core/model/storage.h"
#include "utils/fs.h"
#include "utils/log.h"

namespace giada::m
{
Engine::Engine()
: midiMapper(kernelMidi)
, channelManager(conf.data, model)
, midiDispatcher(model)
, actionRecorder(model)
, synchronizer(conf.data, kernelMidi)
, sequencer(model, synchronizer, jackTransport)
, mixer(model)
, mixerHandler(model, mixer)
, recorder(model, sequencer, mixerHandler)
#ifdef WITH_VST
, pluginHost(model)
, pluginManager(static_cast<PluginManager::SortMethod>(conf.data.pluginSortMethod))
#endif
{
	kernelAudio.onAudioCallback = [this](KernelAudio::CallbackInfo info) {
		return audioCallback(info);
	};

	kernelMidi.onMidiReceived = [this](uint32_t msg) { midiDispatcher.dispatch(msg); };

#ifdef WITH_AUDIO_JACK
	if (kernelAudio.getAPI() == G_SYS_API_JACK)
		jackTransport.setHandle(kernelAudio.getJackHandle());

	synchronizer.onJackRewind    = [this]() { sequencer.rawRewind(); };
	synchronizer.onJackChangeBpm = [this](float bpm) { sequencer.rawSetBpm(bpm, kernelAudio.getSampleRate()); };
	synchronizer.onJackStart     = [this]() { sequencer.rawStart(); };
	synchronizer.onJackStop      = [this]() { sequencer.rawStop(); };
#endif

	eventDispatcher.onMidiLearn       = [this](const MidiEvent& e) { midiDispatcher.learn(e); };
	eventDispatcher.onMidiProcess     = [this](const MidiEvent& e) { midiDispatcher.process(e); };
	eventDispatcher.onProcessChannels = [this](const EventDispatcher::EventBuffer& eb) {
		for (channel::Data& ch : model.get().channels)
			channel::react(ch, eb, mixer.isChannelAudible(ch));
		model.swap(model::SwapType::SOFT); // TODO - is this necessary???
	};
	eventDispatcher.onProcessSequencer = [this](const EventDispatcher::EventBuffer& eb) {
		sequencer.react(eb);
	};
	eventDispatcher.onMixerSignalCallback = [this]() {
		recorder.startInputRecOnCallback();
	};
	eventDispatcher.onMixerEndOfRecCallback = [this]() {
		if (recorder.isRecordingInput())
			recorder.stopInputRec(conf.data.inputRecMode, kernelAudio.getSampleRate());
	};

	midiDispatcher.onDispatch = [this](EventDispatcher::EventType event, Action action) {
		/* Notify Event Dispatcher when a MIDI signal is received. */
		eventDispatcher.pumpMidiEvent({event, 0, 0, action});
	};

	midiDispatcher.onEventReceived = [this]() {
		recorder.startActionRecOnCallback();
	};

	mixer.onSignalTresholdReached = [this]() {
		/* Invokes the signal callback. This is done by pumping a MIXER_SIGNAL_CALLBACK
        event to the Event Dispatcher, rather than invoking the callback directly.
        This is done on purpose: the callback might (and surely will) contain 
        blocking stuff from model:: that the realtime thread cannot perform directly. */
		eventDispatcher.pumpUIevent({EventDispatcher::EventType::MIXER_SIGNAL_CALLBACK});
	};

	mixer.onEndOfRecording = [this]() {
		/* Same rationale as above, for the end-of-recording callback. */
		eventDispatcher.pumpUIevent({EventDispatcher::EventType::MIXER_END_OF_REC_CALLBACK});
	};

	mixerHandler.onChannelsAltered = [this]() {
		if (!recorder.canEnableFreeInputRec())
			conf.data.inputRecMode = InputRecMode::RIGID;
	};
	mixerHandler.onChannelRecorded = [this](Frame recordedFrames) {
		std::string filename = "TAKE-" + std::to_string(patch.data.lastTakeId++) + ".wav";
		return waveManager.createEmpty(recordedFrames, G_MAX_IO_CHANS, kernelAudio.getSampleRate(), filename);
	};

	sequencer.onAboutStart = [this](SeqStatus status) {
		/* TODO move this logic to Recorder */
		if (status == SeqStatus::WAITING)
			recorder.stopActionRec(actionRecorder);
		conf.data.recTriggerMode = RecTriggerMode::NORMAL;
	};

	sequencer.onAboutStop = [this]() {
		/* If recordings (both input and action) are active deactivate them, but 
	store the takes. RecManager takes care of it. */
		/* TODO move this logic to Recorder */
		if (recorder.isRecordingAction())
			recorder.stopActionRec(actionRecorder);
		else if (recorder.isRecordingInput())
			recorder.stopInputRec(conf.data.inputRecMode, kernelAudio.getSampleRate());
	};

	sequencer.onBpmChange = [this](float oldVal, float newVal, int quantizerStep) {
		actionRecorder.updateBpm(oldVal / newVal, quantizerStep);
	};
}

/* -------------------------------------------------------------------------- */

void Engine::updateMixerModel()
{
	model.get().mixer.limitOutput     = conf.data.limitOutput;
	model.get().mixer.allowsOverdub   = conf.data.inputRecMode == InputRecMode::RIGID;
	model.get().mixer.maxFramesToRec  = conf.data.inputRecMode == InputRecMode::FREE ? sequencer.getMaxFramesInLoop(kernelAudio.getSampleRate()) : sequencer.getFramesInLoop();
	model.get().mixer.recTriggerLevel = conf.data.recTriggerLevel;
	model.swap(model::SwapType::NONE);
}

/* -------------------------------------------------------------------------- */

void Engine::init()
{
	if (!conf.read())
		u::log::print("[Engine::init] Can't read configuration file! Using default values\n");

	model::load(conf.data);

	if (!u::log::init(conf.data.logMode))
		u::log::print("[Engine::init] log init failed! Using default stdout\n");

	midiMapper.init();
	if (midiMapper.read(conf.data.midiMapPath) != MIDIMAP_READ_OK)
		u::log::print("[Engine::init] MIDI map read failed!\n");

	/* Initialize KernelAudio. If fails, interrupt the Engine initialization:
    Giada can't work without a functional KernelAudio. */

	kernelAudio.openDevice(conf.data);
	if (!kernelAudio.isReady())
		return;

	mixerHandler.reset(sequencer.getMaxFramesInLoop(kernelAudio.getSampleRate()),
	    kernelAudio.getBufferSize(), channelManager);
	sequencer.reset(kernelAudio.getSampleRate());
#ifdef WITH_VST
	pluginHost.reset(kernelAudio.getBufferSize());
#endif
	mixer.enable();
	kernelAudio.startStream();

	kernelMidi.setApi(conf.data.midiSystem);
	kernelMidi.openOutDevice(conf.data.midiPortOut);
	kernelMidi.openInDevice(conf.data.midiPortIn);
	midiMapper.sendInitMessages(midiMapper.getCurrentMap());

	eventDispatcher.start();

	updateMixerModel();
}

/* -------------------------------------------------------------------------- */

void Engine::reset()
{
	model.reset();
	channelManager.reset();
	waveManager.reset();
	synchronizer.reset();
	sequencer.reset(kernelAudio.getSampleRate());
	mixerHandler.reset(sequencer.getMaxFramesInLoop(kernelAudio.getSampleRate()),
	    kernelAudio.getBufferSize(), channelManager);
	actionRecorder.reset();
#ifdef WITH_VST
	pluginHost.reset(kernelAudio.getBufferSize());
	pluginManager.reset(static_cast<PluginManager::SortMethod>(conf.data.pluginSortMethod));
#endif
}

/* -------------------------------------------------------------------------- */

void Engine::shutdown()
{
	if (kernelAudio.isReady())
	{
		kernelAudio.closeDevice();
		u::log::print("[Engine::shutdown] KernelAudio closed\n");
		mixer.disable();
		u::log::print("[Engine::shutdown] Mixer closed\n");
	}

	model::store(conf.data);
	if (!conf.write())
		u::log::print("[Engine::shutdown] error while saving configuration file!\n");
	else
		u::log::print("[Engine::shutdown] configuration saved\n");

	u::log::close();

#ifdef WITH_VST
	/* Currently the Engine is global/static, and so are all of its sub-components, 
	Model included. Some plug-ins (JUCE-based ones) crash hard on destructor when 
	deleted as a result of returning from main, so it's better to free them all first.
	TODO - investigate this! */

	pluginHost.freeAllPlugins();
#endif
}

/* -------------------------------------------------------------------------- */

int Engine::audioCallback(KernelAudio::CallbackInfo kernelInfo)
{
	mcl::AudioBuffer out(static_cast<float*>(kernelInfo.outBuf), kernelInfo.bufferSize, kernelInfo.channelsOutCount);
	mcl::AudioBuffer in;
	if (kernelInfo.channelsInCount > 0)
		in = mcl::AudioBuffer(static_cast<float*>(kernelInfo.inBuf), kernelInfo.bufferSize, kernelInfo.channelsInCount);

	/* Clean up output buffer before any rendering. Do this even if mixer is
	disabled to avoid audio leftovers during a temporary suspension (e.g. when
	loading a new patch). */

	out.clear();

	if (!kernelInfo.ready)
		return 0;

	/* Prepare the LayoutLock. From this point on (until out of scope) the 
	Layout is locked for realtime rendering by the audio thread. Rendering 
	functions must access the realtime layout coming from layoutLock.get(). */

	const model::LayoutLock layoutLock = model.get_RT();
	const model::Layout&    layout_RT  = layoutLock.get();

	/* Mixer disabled, nothing to do here. */

	if (!layout_RT.mixer.a_isActive())
		return 0;

#ifdef WITH_AUDIO_JACK
	if (kernelInfo.withJack == G_SYS_API_JACK)
		synchronizer.recvJackSync(jackTransport.getState());
#endif

	/* Render Mixer first: render channels, process I/O. */

	mixer.render(out, in, layout_RT);

	/* Then, if the sequencer is running, advance it (i.e. parse it for events). 
	Also advance channels (i.e. let them react to sequencer events), only if the 
	layout is not locked: another thread might altering channel's data in the 
	meantime (e.g. Plugins or Waves). */

	if (layout_RT.sequencer.isRunning())
	{
		const Sequencer::EventBuffer& events = sequencer.advance(in.countFrames(), actionRecorder);
		sequencer.render(out);
		if (!layout_RT.locked)
			mixer.advanceChannels(events, layout_RT);
	}

	return 0;
}

/* -------------------------------------------------------------------------- */

bool Engine::store(const std::string& projectName, const std::string& projectPath,
    const std::string& patchPath)
{
	if (!u::fs::mkdir(projectPath))
	{
		u::log::print("[Engine::store] Unable to make project directory!\n");
		return false;
	}

	u::log::print("[Engine::store] Project dir created: %s\n", projectPath);

	/* Update all existing file paths in Waves, so that they point to the project
	folder they belong to. */

	for (std::unique_ptr<Wave>& w : model.getAll<model::WavePtrs>())
	{
		w->setPath(makeUniqueWavePath(projectPath, *w, model.getAll<model::WavePtrs>()));
		waveManager.save(*w, w->getPath()); // TODO - error checking
	}

	/* Write Model into Patch, then into file. */

	patch.data.name = projectName;
	model::store(patch.data);

	if (!patch.write(patchPath))
		return false;

	/* Store the parent folder the project belongs to, in order to reuse it the 
	next time. */

	conf.data.patchPath = u::fs::getUpDir(u::fs::getUpDir(patchPath));

	u::log::print("[Engine::store] Project patch saved as %s\n", patchPath);

	return true;
}

/* -------------------------------------------------------------------------- */

int Engine::load(const std::string& projectPath, const std::string& patchPath)
{
	u::log::print("[Engine::load] Load project from %s\n", projectPath);

	patch.reset();
	if (int res = patch.read(patchPath, projectPath); res != G_PATCH_OK)
		return res;

	/* Then suspend Mixer, reset and fill the model. */

	mixer.disable();
	reset();
	m::model::load(patch.data);

	/* Prepare the engine. Recorder has to recompute the actions positions if 
	the current samplerate != patch samplerate. Clock needs to update frames
	in sequencer. */

	mixerHandler.updateSoloCount();
	actionRecorder.updateSamplerate(kernelAudio.getSampleRate(), patch.data.samplerate);
	sequencer.recomputeFrames(kernelAudio.getSampleRate());
	mixer.allocRecBuffer(sequencer.getMaxFramesInLoop(kernelAudio.getSampleRate()));

	/* Store the parent folder the project belongs to, in order to reuse it the 
	next time. */

	conf.data.patchPath = u::fs::getUpDir(projectPath);

	/* Mixer is ready to go back online. */

	mixer.enable();

	return G_PATCH_OK;
}

} // namespace giada::m