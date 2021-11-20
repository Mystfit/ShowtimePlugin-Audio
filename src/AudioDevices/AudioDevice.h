#pragma once

#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <boost/circular_buffer.hpp>
#include "RtAudio.h"
#include "../AudioComponentBase.h"

#define AUDIODEVICE_COMPONENT_TYPE "audiodevice"

// Forwards
class RtAudio;


//struct AudioData {
//	//DEVICE_BUFFER_T* buffer;
//	unsigned long read_offset;
//	unsigned long write_offset;
//	unsigned long bufferBytes;
//	unsigned long totalFrames;
//	unsigned long frameCounter;
//	unsigned int channels;
//};


class AudioDevice :
	public AudioComponentBase
{
public:
	ZST_PLUGIN_EXPORT AudioDevice(const char* name, size_t device_index, size_t num_device_inputs, size_t num_device_outputs, unsigned long native_formats_bmask);
	ZST_PLUGIN_EXPORT ~AudioDevice();

private:
	virtual void compute(showtime::ZstInputPlug* plug) override;
	
	// Audio callback for processing incoming/outgoing device audio
	// ------------------------------------------------------------
	int audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data);
	void SendAudioToDevice(const RtAudioStreamStatus& status, void * outputBuffer, unsigned int& nBufferFrames);
	void ReceiveAudioFromDevice(const RtAudioStreamStatus& status, void * inputBuffer, unsigned int nBufferFrames);


	std::shared_ptr<RtAudio> m_audio_device;

	bool bLogAmplitude;
	size_t m_total_writes;
	size_t m_total_reads;

	//std::shared_ptr<AudioData> m_audio_data;
	std::mutex m_incoming_audio_lock;
	std::vector < std::shared_ptr< boost::circular_buffer< AUDIO_BUFFER_T> > > m_incoming_graph_audio_channel_buffers;
};
