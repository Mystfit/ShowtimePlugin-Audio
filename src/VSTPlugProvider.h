#pragma once

#include "public.sdk/source/vst/hosting/plugprovider.h"
#include <public.sdk/source/vst/hosting/hostclasses.h>


class VSTPlugProvider : public Steinberg::Vst::PlugProvider {
public:
	VSTPlugProvider(const PluginFactory& factory, ClassInfo info, bool plugIsGlobal = false);

private:
	std::shared_ptr<Steinberg::Vst::HostApplication> m_pluginContext;
};