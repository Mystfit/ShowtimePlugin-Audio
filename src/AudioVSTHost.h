#pragma 

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <memory>
#include <boost/thread.hpp>

#include "platform/iwindow.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>


#define AUDIOVSTHOST_COMPONENT_TYPE "vsthost"

// Forwards
namespace VST3 {
	namespace Hosting {
		class Module;
	}
}

namespace Steinberg {
	template <class I>
	class IPtr;

	namespace Vst {
		class PlugProvider;
		class HostApplication;
		class IAudioProcessor;
	}
}


class AudioVSTHost :
	public virtual showtime::ZstComponent
{
public:
	ZST_PLUGIN_EXPORT AudioVSTHost(const char* name, const char* vst_path, Steinberg::Vst::HostApplication* plugin_context);
	ZST_PLUGIN_EXPORT virtual void on_registered() override;

private:
	void load_VST(const std::string& path, Steinberg::Vst::HostApplication* plugin_context);
	void createViewAndShow(Steinberg::Vst::IEditController* controller);

	std::shared_ptr<VST3::Hosting::Module> m_module;
	Steinberg::IPtr<Steinberg::Vst::PlugProvider> m_plugProvider;

	Steinberg::Vst::IAudioProcessor* m_audioEffect = nullptr;

	Steinberg::Vst::EditorHost::WindowControllerPtr m_windowController;
	Steinberg::Vst::EditorHost::WindowPtr m_window;


	boost::thread m_events;
};
