#include "VSTPlugProvider.h"

using namespace Steinberg;

VSTPlugProvider::VSTPlugProvider(const PluginFactory& factory, ClassInfo info, Vst::HostApplication* plugin_context) :
	Vst::PlugProvider(factory, info, false)
{
	setupPlugin(plugin_context);
}
