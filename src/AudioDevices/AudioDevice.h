#pragma once

#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <boost/circular_buffer.hpp>
#include "RtAudio.h"
#include "../AudioComponentBase.h"

#define AUDIODEVICE_COMPONENT_TYPE "audiodevice"

// Forwards
class RtAudio;


struct AudioData {
	//DEVICE_BUFFER_T* buffer;
	boost::circular_buffer< AUDIO_BUFFER_T> buffer;
	unsigned long read_offset;
	unsigned long write_offset;
	unsigned long bufferBytes;
	unsigned long totalFrames;
	unsigned long frameCounter;
	unsigned int channels;
};


class AudioDevice :
	public AudioComponentBase
{
public:
	ZST_PLUGIN_EXPORT AudioDevice(const char* name, size_t device_index, size_t num_inputs, size_t num_outputs, unsigned long native_formats_bmask);
	ZST_PLUGIN_EXPORT ~AudioDevice();

private:
	virtual void compute(showtime::ZstInputPlug* plug) override;
	int audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data);

	std::shared_ptr<RtAudio> m_audio_device;
	
	size_t m_num_inputs;
	size_t m_num_outputs;

	bool bLogAmplitude;

	std::shared_ptr<AudioData> m_audio_data;
	std::mutex m_incoming_audio_lock;

	std::shared_ptr< boost::circular_buffer< AUDIO_BUFFER_T> > m_received_network_audio_buffer_left;
	std::shared_ptr< boost::circular_buffer< AUDIO_BUFFER_T> > m_received_network_audio_buffer_right;
};
