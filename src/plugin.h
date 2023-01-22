#pragma once

#include <showtime/ZstPlugin.h>
#include <showtime/ZstExports.h>

#define PLUGIN_MAJOR_VER 0
#define PLUGIN_MINOR_VER 0
#define PLUGIN_PATCH_VER 1
#define PLUGIN_NAME "ShowtimePluginAudio"

// Forwards
class ZstEntityFactory;

namespace showtime {

	class ZST_CLASS_EXPORTED RtAudioPlugin : public ZstPlugin {
	public:
		ZST_PLUGIN_EXPORT RtAudioPlugin();
		ZST_PLUGIN_EXPORT virtual ~RtAudioPlugin();

		ZST_PLUGIN_EXPORT static std::shared_ptr<RtAudioPlugin> create();
		ZST_PLUGIN_EXPORT virtual void init(const char* plugin_data_path) override;
		ZST_PLUGIN_EXPORT virtual const char* name() override;
		ZST_PLUGIN_EXPORT virtual int version_major() override;
		ZST_PLUGIN_EXPORT virtual int version_minor() override;
		ZST_PLUGIN_EXPORT virtual int version_patch() override;
	};
}
