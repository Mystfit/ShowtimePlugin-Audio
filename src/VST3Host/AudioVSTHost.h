#pragma 

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <memory>
#include <boost/thread.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/vector_of.hpp>
#include <boost/bimap/list_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/bimap/unconstrained_set_of.hpp>

#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>

#include "../AudioComponentBase.h"
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


//struct PlugPair {
//	showtime::ZstInputPlug* input;
//	showtime::ZstOutputPlug* output;
//};

//inline bool operator==(const PlugPair& lhs, const PlugPair& rhs){
//	return std::tie(lhs.input, lhs.output) == std::tie(rhs.input, rhs.output);
//}
//
//inline bool operator<(const PlugPair& lhs, const PlugPair& rhs) {
//	return std::tie(lhs.input, lhs.output) < std::tie(rhs.input, rhs.output);
//}
//
//
//std::size_t hash_value(PlugPair const& pair)
//{
//	boost::hash<int> hasher;
//	std::size_t h1 = pair.output ? showtime::ZstURIHash{}(pair.output->URI()) : 0;
//	std::size_t h2 = pair.input ? showtime::ZstURIHash{}(pair.input->URI()) : 0;
//	return hasher(h1 ^ (h2 << 1));
//}

//
//inline bool operator<(const PlugPair& lhs, const PlugPair& rhs){
//	return std::tie(lhs.input, lhs.output) < std::tie(rhs.input, rhs.output);
//}

class AudioVSTHost :
	public AudioComponentBase,
	public Steinberg::Vst::IComponentHandler,
	public Steinberg::Vst::IComponentHandler2
{
public:
	ZST_PLUGIN_EXPORT AudioVSTHost(const char* name, const char* vst_path, Steinberg::Vst::HostApplication* plugin_context);

	// IComponentHandler interface
	ZST_PLUGIN_EXPORT Steinberg::tresult beginEdit(Steinberg::Vst::ParamID id) override;
	ZST_PLUGIN_EXPORT Steinberg::tresult performEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) override;
	ZST_PLUGIN_EXPORT Steinberg::tresult endEdit(Steinberg::Vst::ParamID id) override;
	ZST_PLUGIN_EXPORT Steinberg::tresult restartComponent(Steinberg::int32 flags) override;

	// IComponentHandler2 interface
	ZST_PLUGIN_EXPORT Steinberg::tresult setDirty(Steinberg::TBool state) override;
	ZST_PLUGIN_EXPORT Steinberg::tresult requestOpenEditor(Steinberg::FIDString name = Steinberg::Vst::ViewType::kEditor) override;
	ZST_PLUGIN_EXPORT Steinberg::tresult startGroupEdit() override;
	ZST_PLUGIN_EXPORT Steinberg::tresult finishGroupEdit() override;

	// FUnknown interface
	ZST_PLUGIN_EXPORT Steinberg::tresult queryInterface(const Steinberg::TUID /*_iid*/, void** /*obj*/) override { return Steinberg::kNoInterface; }
	ZST_PLUGIN_EXPORT Steinberg::uint32 addRef() override { return 1000; }
	ZST_PLUGIN_EXPORT Steinberg::uint32 release() override { return 1000; }

private:
	void load_VST(const std::string& path, Steinberg::Vst::HostApplication* plugin_context);
	void createViewAndShow(Steinberg::Vst::IEditController* controller);
	void compute(showtime::ZstInputPlug* plug) override;

	void ProcessAudio();

	void on_registered() override;

	// VST setup
	bool prepareProcessing();

	// VST interface
	std::shared_ptr<VST3::Hosting::Module> m_module;
	Steinberg::IPtr<Steinberg::Vst::PlugProvider> m_plugProvider;

	// VST Processing
	Steinberg::Vst::IAudioProcessor* m_audioEffect;
	Steinberg::Vst::IComponent* m_vstPlug;
	Steinberg::Vst::IEditController* m_editController;
	Steinberg::Vst::HostProcessData m_processData;
	Steinberg::Vst::ProcessSetup m_processSetup;
	std::shared_ptr<Steinberg::Vst::ProcessContext> m_processContext;

	// VST GUI
	Steinberg::Vst::EditorHost::WindowControllerPtr m_windowController;
	Steinberg::Vst::EditorHost::WindowPtr m_window;

	// VST Parameters
	Steinberg::Vst::ParameterInfo getParameterInfoByID(Steinberg::Vst::ParamID paramID);

	// MIDI
	std::unique_ptr<showtime::ZstInputPlug> m_midi_in;

	// VST parameters
	std::vector<std::unique_ptr< showtime::ZstPlug> > m_vst_plugs;

	typedef boost::bimap< Steinberg::Vst::ParamID, showtime::ZstInputPlug* > ParamInputPlugBimap;
	typedef boost::bimap< Steinberg::Vst::ParamID, showtime::ZstOutputPlug* > ParamOutputPlugBimap;

	ParamInputPlugBimap m_param_input_plug_lookup;
	ParamOutputPlugBimap m_param_output_plug_lookup;

	long long m_elapsed_samples;
};

inline std::string ParameterTitleToString(Steinberg::Vst::String128 title) {
	std::wstring ws(title);
	return std::string(ws.begin(), ws.end());
}
