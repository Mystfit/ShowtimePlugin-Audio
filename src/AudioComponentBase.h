#pragma once

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstComputeComponent.h>
#include <showtime/entities/ZstPlug.h>

#include <boost/circular_buffer.hpp>
#include <memory>

typedef float AUDIO_BUFFER_T;

class AudioComponentBase : public showtime::ZstComputeComponent
{
public:
	ZST_PLUGIN_EXPORT AudioComponentBase(size_t num_input_channels, size_t num_output_channels, const char* component_type, const char* name, size_t num_samples);
	void init_plugs(size_t num_incoming_plug_channels, size_t num_outgoing_plug_channels, size_t num_samples);
	
	ZST_PLUGIN_EXPORT virtual void on_registered() override;

	showtime::ZstInputPlug* incoming_audio(size_t channel);
	showtime::ZstOutputPlug* outgoing_audio(size_t channel);

	size_t num_input_channels();
	size_t num_output_channels();
protected:
	std::vector< std::unique_ptr<showtime::ZstInputPlug> > m_input_channel_plugs;
	std::vector< std::unique_ptr<showtime::ZstOutputPlug> > m_output_channel_plugs;
};
