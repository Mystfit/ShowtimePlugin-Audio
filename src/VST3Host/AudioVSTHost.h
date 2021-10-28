#pragma 

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <memory>
#include <boost/thread.hpp>

#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>

#include <public.sdk/source/vst/hosting/processdata.h>

#include "WindowController.h"


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
	void compute(showtime::ZstInputPlug* plug) override;

	// VST setup
	bool prepareProcessing();

	// VST interface
	std::shared_ptr<VST3::Hosting::Module> m_module;
	Steinberg::IPtr<Steinberg::Vst::PlugProvider> m_plugProvider;

	// VST Processing
	Steinberg::Vst::IAudioProcessor* m_audioEffect;
	Steinberg::Vst::IComponent* m_vstPlug;
	Steinberg::Vst::HostProcessData m_processData;
	Steinberg::Vst::ProcessSetup m_processSetup;
	std::shared_ptr<Steinberg::Vst::ProcessContext> m_processContext;

	// VST GUI
	Steinberg::Vst::EditorHost::WindowControllerPtr m_windowController;
	Steinberg::Vst::EditorHost::WindowPtr m_window;

	// Plugs
	std::shared_ptr<showtime::ZstInputPlug> m_incoming_network_audio;
	std::shared_ptr<showtime::ZstOutputPlug> m_outgoing_network_audio;

	long long m_elapsed_samples;
};
