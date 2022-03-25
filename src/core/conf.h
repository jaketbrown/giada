/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2022 Giovanni A. Zuliani | Monocasual Laboratories
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

#ifndef G_CONF_H
#define G_CONF_H

#include "core/const.h"
#include "core/types.h"
#include "utils/gui.h"
#include <string>
#include <unordered_map>

namespace giada::m
{
class Conf final
{
public:
	using KeyBindings = std::unordered_map<int, int>;

	static constexpr int KEY_BIND_PLAY           = 1;
	static constexpr int KEY_BIND_REWIND         = 2;
	static constexpr int KEY_BIND_RECORD_ACTIONS = 3;
	static constexpr int KEY_BIND_RECORD_INPUT   = 4;
	static constexpr int KEY_BIND_EXIT           = 5;

	struct Data
	{
		int  logMode          = LOG_MODE_MUTE;
		bool showTooltips     = true;
		int  soundSystem      = G_DEFAULT_SOUNDSYS;
		int  soundDeviceOut   = G_DEFAULT_SOUNDDEV_OUT;
		int  soundDeviceIn    = G_DEFAULT_SOUNDDEV_IN;
		int  channelsOutCount = G_MAX_IO_CHANS;
		int  channelsOutStart = 0;
		int  channelsInCount  = 1;
		int  channelsInStart  = 0;
		int  samplerate       = G_DEFAULT_SAMPLERATE;
		int  buffersize       = G_DEFAULT_BUFSIZE;
		bool limitOutput      = false;
		int  rsmpQuality      = 0;

		int         midiSystem  = 0;
		int         midiPortOut = G_DEFAULT_MIDI_PORT_OUT;
		int         midiPortIn  = G_DEFAULT_MIDI_PORT_IN;
		std::string midiMapPath = "";
		std::string lastFileMap = "";
		int         midiSync    = G_MIDI_SYNC_NONE;
		float       midiTCfps   = 25.0f;

		bool chansStopOnSeqHalt         = false;
		bool treatRecsAsLoops           = false;
		bool inputMonitorDefaultOn      = false;
		bool overdubProtectionDefaultOn = false;

		std::string pluginPath;
		std::string patchPath;
		std::string samplePath;

		int mainWindowX = u::gui::centerWindowX(G_MIN_GUI_WIDTH);
		int mainWindowY = u::gui::centerWindowY(G_MIN_GUI_HEIGHT);
		int mainWindowW = G_MIN_GUI_WIDTH;
		int mainWindowH = G_MIN_GUI_HEIGHT;

		int         browserX = u::gui::centerWindowX(G_DEFAULT_SUBWINDOW_W);
		int         browserY = u::gui::centerWindowY(G_DEFAULT_SUBWINDOW_H);
		int         browserW = G_DEFAULT_SUBWINDOW_W;
		int         browserH = G_DEFAULT_SUBWINDOW_H;
		int         browserPosition;
		int         browserLastValue;
		std::string browserLastPath;

		int actionEditorY          = u::gui::centerWindowY(G_DEFAULT_SUBWINDOW_H);
		int actionEditorX          = u::gui::centerWindowX(G_DEFAULT_SUBWINDOW_W);
		int actionEditorW          = G_DEFAULT_SUBWINDOW_W;
		int actionEditorH          = G_DEFAULT_SUBWINDOW_H;
		int actionEditorZoom       = G_DEFAULT_ZOOM_RATIO;
		int actionEditorSplitH     = -1;
		int actionEditorGridVal    = 0;
		int actionEditorGridOn     = false;
		int actionEditorPianoRollY = -1;

		int sampleEditorX;
		int sampleEditorY;
		int sampleEditorW       = G_DEFAULT_SUBWINDOW_W;
		int sampleEditorH       = G_DEFAULT_SUBWINDOW_H;
		int sampleEditorGridVal = 0;
		int sampleEditorGridOn  = false;

		int midiInputX;
		int midiInputY;
		int midiInputW = G_DEFAULT_SUBWINDOW_W;
		int midiInputH = G_DEFAULT_SUBWINDOW_H;

		int pluginListX;
		int pluginListY;

		RecTriggerMode recTriggerMode  = RecTriggerMode::NORMAL;
		float          recTriggerLevel = G_DEFAULT_REC_TRIGGER_LEVEL;
		InputRecMode   inputRecMode    = InputRecMode::FREE;

		bool     midiInEnabled    = false;
		int      midiInFilter     = -1;
		uint32_t midiInRewind     = 0x0;
		uint32_t midiInStartStop  = 0x0;
		uint32_t midiInActionRec  = 0x0;
		uint32_t midiInInputRec   = 0x0;
		uint32_t midiInMetronome  = 0x0;
		uint32_t midiInVolumeIn   = 0x0;
		uint32_t midiInVolumeOut  = 0x0;
		uint32_t midiInBeatDouble = 0x0;
		uint32_t midiInBeatHalf   = 0x0;

#ifdef WITH_VST

		int pluginChooserX;
		int pluginChooserY;
		int pluginChooserW   = G_DEFAULT_SUBWINDOW_W;
		int pluginChooserH   = G_DEFAULT_SUBWINDOW_H;
		int pluginSortMethod = 0;

#endif

		KeyBindings keyBindings = {
		    {KEY_BIND_PLAY, ' '},
		    {KEY_BIND_REWIND, FL_BackSpace},
		    {KEY_BIND_RECORD_ACTIONS, FL_Enter},
		    {KEY_BIND_RECORD_INPUT, FL_End},
		    {KEY_BIND_EXIT, FL_Escape}};
	};

	Conf();

	bool read();
	bool write() const;

	Data data;

private:
	/* createConfigFolder
	Creates local folder where to put the configuration file. Path differs from 
	OS to OS. */

	bool createConfigFolder() const;

	void sanitize();

	std::string m_confFilePath;
	std::string m_confDirPath;
};
} // namespace giada::m

#endif
