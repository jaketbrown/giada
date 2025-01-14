
#include "../src/core/channels/channelFactory.h"
#include "../src/core/types.h"
#include <catch2/catch.hpp>

using std::string;
using namespace giada;
using namespace giada::m;

TEST_CASE("channelFactory")
{
	SECTION("test creation")
	{
		channelFactory::Data data = channelFactory::create(
		    /*id=*/0,
		    ChannelType::SAMPLE,
		    /*columnId=*/0,
		    /*position=*/0,
		    /*bufferSize=*/1024,
		    Resampler::Quality::LINEAR,
		    /*overdubProtection=*/false);

		REQUIRE(data.channel.id != 0); // If ID == 0, must be auto-generated
		REQUIRE(data.channel.type == ChannelType::SAMPLE);

		SECTION("test clone")
		{
			channelFactory::Data clone = channelFactory::create(data.channel, /*bufferSize=*/1024, Resampler::Quality::LINEAR);

			REQUIRE(clone.channel.id != data.channel.id); // Clone must have new ID
			REQUIRE(clone.channel.type == data.channel.type);
			REQUIRE(clone.channel.columnId == data.channel.columnId);
			REQUIRE(clone.channel.position == data.channel.position);
			REQUIRE(clone.channel.volume == data.channel.volume);
			REQUIRE(clone.channel.volume_i == data.channel.volume_i);
			REQUIRE(clone.channel.pan == data.channel.pan);
			REQUIRE(clone.channel.armed == data.channel.armed);
			REQUIRE(clone.channel.key == data.channel.key);
			REQUIRE(clone.channel.hasActions == data.channel.hasActions);
			REQUIRE(clone.channel.name == data.channel.name);
			REQUIRE(clone.channel.height == data.channel.height);
		}
	}
}
