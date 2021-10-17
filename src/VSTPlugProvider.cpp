#include "VSTPlugProvider.h"

using namespace Steinberg;

VSTPlugProvider::VSTPlugProvider(const PluginFactory& factory, ClassInfo info, bool plugIsGlobal) :
	Vst::PlugProvider(factory, info, plugIsGlobal),
	m_pluginContext(std::make_shared<Vst::HostApplication>()) 
{
	if (!plugIsGlobal) {
		setupPlugin(m_pluginContext.get());
	}
}
