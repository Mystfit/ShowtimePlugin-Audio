#pragma once

#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <boost/circular_buffer.hpp>
#include "RtAudio.h"

#define AUDIODEVICE_COMPONENT_TYPE "audiodevice"

// Forwards
class RtAudio;


typedef float DEVICE_BUFFER_T;
struct AudioData {
	//DEVICE_BUFFER_T* buffer;
	boost::circular_buffer< DEVICE_BUFFER_T> buffer;
	unsigned long read_offset;
	unsigned long write_offset;
	unsigned long bufferBytes;
	unsigned long totalFrames;
	unsigned long frameCounter;
	unsigned int channels;
};


class AudioDevice :
	public virtual showtime::ZstComponent
{
public:
	ZST_PLUGIN_EXPORT AudioDevice(const char* name, size_t device_index, size_t num_inputs, size_t num_outputs, unsigned long native_formats_bmask);
	ZST_PLUGIN_EXPORT virtual void on_registered() override;

private:
	virtual void compute(showtime::ZstInputPlug* plug) override;
	int audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data);

	std::shared_ptr<RtAudio> m_audio_device;
	
	size_t m_num_inputs;
	size_t m_num_outputs;

	bool bLogAmplitude;
	
	std::shared_ptr<showtime::ZstInputPlug> m_incoming_network_audio;
	std::shared_ptr<showtime::ZstOutputPlug> m_outgoing_network_audio;
	std::shared_ptr<AudioData> m_audio_data;

	std::shared_ptr< boost::circular_buffer< DEVICE_BUFFER_T> > m_buffer;


};
