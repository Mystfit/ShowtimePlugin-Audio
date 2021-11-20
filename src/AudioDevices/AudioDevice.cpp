#include "AudioDevice.h"
#include "RtAudio.h"
#include <showtime/ZstLogging.h>

using namespace showtime;
using namespace std::placeholders;


AudioDevice::AudioDevice(const char* name, size_t device_index, size_t num_device_inputs, size_t num_device_outputs, unsigned long native_formats_bmask) : 
	AudioComponentBase(num_device_inputs, num_device_outputs, AUDIODEVICE_COMPONENT_TYPE, name),
	m_audio_device(std::make_shared<RtAudio>()),
	//m_audio_data(std::make_shared<AudioData>()),
	bLogAmplitude(true),
	m_total_writes(0),
	m_total_reads(0)
{
	Log::entity(Log::Level::notification, "Creating audio device {} with device ID {} {}", URI().last().path(), device_index, sizeof(AUDIO_BUFFER_T));

	double time = 0.1;
	unsigned long samplerate = 44100;
	unsigned int bufferFrames =  512;

	RtAudio::StreamParameters outparams;
	outparams.deviceId = device_index;
	outparams.nChannels = num_device_outputs;
	
	RtAudio::StreamParameters inparams;
	inparams.deviceId = device_index;
	inparams.nChannels = num_device_inputs;

	bool supports_byte_format = (native_formats_bmask & RTAUDIO_SINT8) == RTAUDIO_SINT8;
	RtAudioFormat format = RTAUDIO_FLOAT32;
	
	RtAudio::StreamOptions opts;
	opts.flags = RTAUDIO_NONINTERLEAVED;// | RTAUDIO_MINIMIZE_LATENCY;

	try {
		RtAudioCallback cb = [](void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data) -> int {
			return ((AudioDevice*)data)->audio_callback(outputBuffer, inputBuffer, nBufferFrames, streamTime, status, data);
		};
		m_audio_device->openStream((num_device_outputs) ? &outparams : nullptr, (num_device_inputs) ? &inparams : nullptr, format, samplerate, &bufferFrames, cb, (void*)this, &opts);
	} 
	catch (RtAudioError& e) {
		Log::entity(Log::Level::error, e.getMessage().c_str());
	}

	//size_t total_channels = (num_inputs + num_outputs);
	//m_audio_data->read_offset = 0;
	//m_audio_data->write_offset = 0;
	//m_audio_data->bufferBytes = bufferFrames * total_channels * sizeof(AUDIO_BUFFER_T);
	//m_audio_data->totalFrames = (unsigned long)samplerate;
	//m_audio_data->frameCounter = 0;
	//m_audio_data->channels = total_channels;
	//unsigned long totalBytes;
	//totalBytes = m_audio_data->totalFrames * total_channels * sizeof(AUDIO_BUFFER_T);
	//
	//// Allocate the entire data buffer before starting stream.
	//m_audio_data->buffer = boost::circular_buffer< AUDIO_BUFFER_T>(bufferFrames * total_channels);

	for (size_t idx = 0; idx < num_device_outputs; ++idx) {
		auto buffer = std::make_shared<boost::circular_buffer< AUDIO_BUFFER_T>>(bufferFrames * 4);
		for (size_t idx = 0; idx < bufferFrames * 4; ++idx) {
			buffer->push_back(0.0);
		}
		m_incoming_graph_audio_channel_buffers.push_back(buffer);
	}

	try {
		m_audio_device->startStream();
	}
	catch (RtAudioError& e) {
		Log::entity(Log::Level::error, e.getMessage().c_str());
	}
}

AudioDevice::~AudioDevice()
{
	if (m_audio_device) {
		m_audio_device->closeStream();
	}
}

void AudioDevice::compute(ZstInputPlug* plug)
{
	float total = 0.0;
	std::string buffer_str;
		
	for (size_t channel = 0; channel < m_input_channel_plugs.size(); ++channel) {
		if (auto channel_plug = incoming_audio(channel)) {
			if (m_incoming_graph_audio_channel_buffers.size() == m_input_channel_plugs.size()) {
				auto in_buf = m_incoming_graph_audio_channel_buffers[channel];
				auto num_frames = channel_plug->size();
				for (size_t frame_idx = 0; frame_idx < num_frames; ++frame_idx) {
					std::scoped_lock<std::mutex> lock(m_incoming_audio_lock);
					auto val = channel_plug->float_at(frame_idx);
					in_buf->push_back(val);

					if (bLogAmplitude) {
						total += abs(val);
					}
				}

				if (bLogAmplitude && total > 0.0) {
					total /= num_frames;
					int numblocks = total * 100;
					for (size_t block_idx = 0; block_idx < numblocks; ++block_idx) {
						buffer_str += "\xDB";
					}

					Log::app(Log::Level::debug, buffer_str.c_str());
				}
			}
		}
	}
	m_total_writes++;
	
	//outgoing_audio()->fire();
	ZstComponent::compute(plug);
}

int AudioDevice::audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data)
{
	AudioDevice* data_this = (AudioDevice*)data;
	if (!data_this)
		return 0;

	if (outputBuffer) {
		data_this->SendAudioToDevice(status, outputBuffer, nBufferFrames);
	}

	if (inputBuffer) {
		data_this->ReceiveAudioFromDevice(status, inputBuffer, nBufferFrames);
		data_this->execute();
	}

	return 0;
}

void AudioDevice::SendAudioToDevice(const RtAudioStreamStatus& status, void * outputBuffer, unsigned int& nBufferFrames)
{
	if (m_incoming_graph_audio_channel_buffers.size() != this->num_input_channels()) {
		Log::entity(Log::Level::warn, "Mismatch between total ougoing audio buffers and incoming channel plugs.");
		return;
	}

	if (status == RTAUDIO_OUTPUT_UNDERFLOW) {
		Log::entity(Log::Level::warn, "Output underflow");
	}
	else if (status == RTAUDIO_INPUT_OVERFLOW) {
		Log::entity(Log::Level::warn, "Input overflow");
	}


	float* samples = (float*)outputBuffer;

	for (size_t channel = 0; channel < m_incoming_graph_audio_channel_buffers.size(); ++channel) {
		auto channel_buffer = m_incoming_graph_audio_channel_buffers[channel];

		//if (channel_buffer->size() > nBufferFrames) {
		for (auto offset = 0; offset < nBufferFrames; ++offset) {
			std::scoped_lock<std::mutex> lock(m_incoming_audio_lock);
			*(samples++) = channel_buffer->front(); //read_offset
			channel_buffer->pop_front();
		}
		if (!channel_buffer->size()) {
			Log::entity(Log::Level::warn, "Audio in buffer empty!");
		}
		//}
	}
	m_total_reads++;
}

void AudioDevice::ReceiveAudioFromDevice(const RtAudioStreamStatus& status, void * inputBuffer, unsigned int nBufferFrames)
{
	float* samples = (float*)inputBuffer;
	int channel_offset = 0;

	for (size_t channel_idx = 0; channel_idx < this->num_output_channels(); ++channel_idx) {
		channel_offset = channel_idx * nBufferFrames;
		outgoing_audio(channel_idx)->raw_value()->assign(channel_offset + samples, nBufferFrames);
		outgoing_audio(channel_idx)->fire();
	}
	//outgoing_audio()->raw_value()->assign(samples, m_audio_data->channels * nBufferFrames);
}
