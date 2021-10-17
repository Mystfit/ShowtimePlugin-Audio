#include "AudioVSTFactory.h"
#include <showtime/ZstLogging.h>
#include <showtime/ZstFilesystemUtils.h>

#include <public.sdk/source/vst/hosting/module.h>
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include <pluginterfaces/vst/ivstaudioprocessor.h>

#include "platform/iplatform.h"
#ifdef WIN32
//#include <WinUser.h>
#include <windows.h>
#endif

#include "AudioVSTHost.h"

using namespace showtime;
using namespace Steinberg;


AudioVSTFactory::AudioVSTFactory(const char* name) :
	showtime::ZstEntityFactory(name),
	m_plugin_context(std::make_shared<Vst::HostApplication>())
{
}

void AudioVSTFactory::on_registered()
{
	register_tick();
}

void AudioVSTFactory::on_tick()
{
	MSG msg;
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void AudioVSTFactory::scan_vst_path(const std::string& path)
{
	Log::app(Log::Level::debug, "Scanning VST path {}", path.c_str());
	fs::path vst_dir(path);

	for (const auto& file : fs::directory_iterator(vst_dir)) {
		if (file.is_regular_file()) {
			if (file.path().extension() == ".vst3")
			{
				std::string error;
				auto module = VST3::Hosting::Module::create(file.path().string(), error);
				if (!module)
				{
					Log::entity(Log::Level::error, "Could not create Module for file: {}, {}", file.path().string().c_str(), error.c_str());
					continue;
				}

				VST3::Hosting::PluginFactory factory = module->getFactory();
				for (auto& classInfo : factory.classInfos()){
					add_creatable(classInfo.name().c_str(), [this, file, classInfo](const char* name) -> std::unique_ptr<ZstEntityBase> {
						return std::make_unique<AudioVSTHost>((classInfo.name() + "_" + std::string(name)).c_str(), file.path().string().c_str(), m_plugin_context.get());
					});
				}
			}
		}
	}
}
