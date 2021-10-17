#pragma once

#include "public.sdk/source/vst/hosting/plugprovider.h"
#include <public.sdk/source/vst/hosting/hostclasses.h>


class VSTPlugProvider : public Steinberg::Vst::PlugProvider {
public:
	VSTPlugProvider(const PluginFactory& factory, ClassInfo info, Steinberg::Vst::HostApplication* plugin_contexte);
};