#pragma once

#include <showtime/entities/ZstComponent.h>

#define AUDIODEVICE_COMPONENT_TYPE "audiodevice"

// Forwards
class RtAudio;

class AudioDevice :
	public virtual showtime::ZstComponent
{
public:
	ZST_PLUGIN_EXPORT AudioDevice(const char* name, size_t device_index, size_t num_inputs, size_t num_outputs, unsigned long native_formats_bmask);
	ZST_PLUGIN_EXPORT virtual void on_registered() override;

private:
	std::shared_ptr<RtAudio> m_audio_device;
	
	size_t m_num_inputs;
	size_t m_num_outputs;
	
	std::shared_ptr<showtime::ZstInputPlug> m_incoming_network_audio;
	std::shared_ptr< showtime::ZstOutputPlug> m_outgoing_network_audio;
};
