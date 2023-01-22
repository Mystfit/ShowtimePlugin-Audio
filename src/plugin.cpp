#include "plugin.h"
#include <boost/dll/alias.hpp>
#include "AudioDevices/AudioFactory.h"
#include "VST3Host/AudioVSTFactory.h"
#include <showtime/ZstFilesystemUtils.h>
#include <showtime/ZstLogging.h>

namespace showtime {
	BOOST_DLL_ALIAS(RtAudioPlugin::create, create_plugin);

	RtAudioPlugin::RtAudioPlugin() : ZstPlugin()
	{
	}

	RtAudioPlugin::~RtAudioPlugin()
	{
	}

	std::shared_ptr<RtAudioPlugin> showtime::RtAudioPlugin::create()
	{
		return std::make_shared<RtAudioPlugin>();
	}

	void showtime::RtAudioPlugin::init(const char* plugin_data_path)
	{
		fs::path data_path = plugin_data_path;
		auto audio_factory = std::make_unique<AudioFactory>("audio_ports");
		auto vst_factory = std::make_unique<AudioVSTFactory>("vsts");

		 if (!data_path.empty()) {
		 	auto vst_data_path = data_path;
			//vst_data_path.append(PLUGIN_NAME).append(MIDI_MAP_DIR);
		 	if(fs::exists(vst_data_path))
				vst_factory->scan_vst_path(vst_data_path.string());
		 }
		 else {
		 	Log::app(Log::Level::warn, "ShowtimeAudioPlugin: No plugin data path set. Can't load content");
		 }
		add_factory(std::unique_ptr<ZstEntityFactory>(std::move(audio_factory)));
		add_factory(std::unique_ptr<ZstEntityFactory>(std::move(vst_factory)));
	}

	const char* showtime::RtAudioPlugin::name()
	{
		return PLUGIN_NAME;
	}

	int showtime::RtAudioPlugin::version_major()
	{
		return PLUGIN_MAJOR_VER;
	}

	int showtime::RtAudioPlugin::version_minor()
	{
		return PLUGIN_MINOR_VER;
	}

	int showtime::RtAudioPlugin::version_patch()
	{
		return PLUGIN_PATCH_VER;
	}
}
