#pragma once

#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>
#include <boost/thread.hpp>
#include "RtAudio.h"
#include "../AudioComponentBase.h"
#include "../ringbuffer.h"

#define AUDIODEVICE_COMPONENT_TYPE "audiodevice"
#define AUDIO_BUFFER_FRAMES 512

// Forwards
class RtAudio;


class AudioDevice :
	public AudioComponentBase
{
public:
	ZST_PLUGIN_EXPORT AudioDevice(const char* name, size_t device_index, size_t num_device_inputs, size_t num_device_outputs, unsigned long native_formats_bmask);
	ZST_PLUGIN_EXPORT ~AudioDevice();

private:
	void on_registered() override;
	
	void process_graph();
	void compute(showtime::ZstInputPlug* plug) override;
	
	// Audio callback for processing incoming/outgoing device audio
	// ------------------------------------------------------------
	int audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data);
	void SendAudioToDevice(const RtAudioStreamStatus& status, void * outputBuffer, unsigned int& nBufferFrames);
	void ReceiveAudioFromDevice(const RtAudioStreamStatus& status, void * inputBuffer, unsigned int nBufferFrames);


	std::shared_ptr<RtAudio> m_audio_device;

	bool bLogAmplitude;
	size_t m_total_writes;
	size_t m_total_reads;
	unsigned int m_buffer_frames;

	//std::shared_ptr<AudioData> m_audio_data;
	boost::thread m_audio_graph_thread;
	std::mutex m_incoming_audio_lock;
	std::mutex m_outgoing_audio_lock;
	std::condition_variable m_outgoing_audio_cv;
	bool m_audio_buffer_received_samples;
	std::vector<std::unique_ptr<RingBuffer<AUDIO_BUFFER_T> > > m_incoming_graph_audio_channel_buffers;
	std::vector<std::unique_ptr<RingBuffer<AUDIO_BUFFER_T> > > m_outgoing_graph_audio_channel_buffers;
};
