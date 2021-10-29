#include "AudioDevice.h"
#include "RtAudio.h"
#include <showtime/ZstLogging.h>

using namespace showtime;
using namespace std::placeholders;


AudioDevice::AudioDevice(const char* name, size_t device_index, size_t num_inputs, size_t num_outputs, unsigned long native_formats_bmask) : 
	AudioComponentBase(AUDIODEVICE_COMPONENT_TYPE, name),
	m_audio_device(std::make_shared<RtAudio>()),
	m_num_inputs(num_inputs),
	m_num_outputs(num_outputs),
	m_audio_data(std::make_shared<AudioData>()),
	bLogAmplitude(true)
{
	Log::entity(Log::Level::notification, "Creating audio device {} with device ID {} {}", URI().last().path(), device_index, sizeof(AUDIO_BUFFER_T));

	double time = 0.1;
	unsigned long samplerate = 44100;
	unsigned int bufferFrames =  512;

	RtAudio::StreamParameters outparams;
	outparams.deviceId = device_index;
	outparams.nChannels = num_outputs;
	
	RtAudio::StreamParameters inparams;
	inparams.deviceId = device_index;
	inparams.nChannels = num_inputs;

	bool supports_byte_format = (native_formats_bmask & RTAUDIO_SINT8) == RTAUDIO_SINT8;
	RtAudioFormat format = RTAUDIO_FLOAT32;
	
	RtAudio::StreamOptions opts;
	opts.flags = RTAUDIO_NONINTERLEAVED;// | RTAUDIO_MINIMIZE_LATENCY;

	try {
		RtAudioCallback cb = [](void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data) -> int {
			return ((AudioDevice*)data)->audio_callback(outputBuffer, inputBuffer, nBufferFrames, streamTime, status, data);
		};
		m_audio_device->openStream((num_outputs) ? &outparams : nullptr, (num_inputs) ? &inparams : nullptr, format, samplerate, &bufferFrames, cb, (void*)this, &opts);
	} 
	catch (RtAudioError& e) {
		Log::entity(Log::Level::error, e.getMessage().c_str());
	}

	size_t total_channels = (num_inputs + num_outputs);
	m_audio_data->read_offset = 0;
	m_audio_data->write_offset = 0;
	m_audio_data->bufferBytes = bufferFrames * total_channels * sizeof(AUDIO_BUFFER_T);
	m_audio_data->totalFrames = (unsigned long)samplerate;
	m_audio_data->frameCounter = 0;
	m_audio_data->channels = total_channels;
	unsigned long totalBytes;
	totalBytes = m_audio_data->totalFrames * total_channels * sizeof(AUDIO_BUFFER_T);
	
	// Allocate the entire data buffer before starting stream.
	m_audio_data->buffer = boost::circular_buffer< AUDIO_BUFFER_T>(bufferFrames * total_channels);
	m_received_network_audio_buffer_left = std::make_shared<boost::circular_buffer< AUDIO_BUFFER_T>>(bufferFrames * 4);
	m_received_network_audio_buffer_right = std::make_shared<boost::circular_buffer< AUDIO_BUFFER_T>>(bufferFrames * 4);
	for (size_t idx = 0; idx < bufferFrames * 4; ++idx) {
		m_received_network_audio_buffer_left->push_back(0.0);
		m_received_network_audio_buffer_right->push_back(0.0);
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
	if (plug == m_incoming_network_audio.get()) {
		float total = 0.0;
		std::string buffer_str;
		
		size_t r_channel_offset = floor(abs(incoming_audio()->size()*0.5));
		Log::entity(Log::Level::error, "Write");

		for (size_t channel = 0; channel < 2; ++channel) {
			auto in_buf = (channel == 0) ? m_received_network_audio_buffer_left : m_received_network_audio_buffer_right;
			auto channel_offset = (channel == 0) ? 0 : r_channel_offset;
			for (size_t idx = 0; idx < r_channel_offset; ++idx) {
				//float draw_buffer_end = (idx < r_channel_offset - 1) ? m_incoming_network_audio->float_at(((channel == 0) ? 0 : r_channel_offset) + idx) : 1.0;
				std::scoped_lock<std::mutex> lock(m_incoming_audio_lock);
				in_buf->push_back(incoming_audio()->float_at(((channel == 0) ? 0 : r_channel_offset) + idx));
			}
		}
		
		//m_received_network_audio_buffer_left->assign(m_incoming_network_audio->raw_value()->float_buffer(), m_incoming_network_audio->raw_value()->float_buffer() + r_channel_offset-1);
		//m_received_network_audio_buffer_right->assign(m_incoming_network_audio->raw_value()->float_buffer() + r_channel_offset, m_incoming_network_audio->raw_value()->float_buffer() + m_incoming_network_audio->size());
		
		/*for (size_t idx = 0; idx < m_received_network_audio_buffer->size(); ++idx) {
			if (bLogAmplitude) {
				float val = m_received_network_audio_buffer->at(idx);
				total += abs(val);
			}
		}

		if (bLogAmplitude && total > 0.0) {
			total /= plug->size();
			int numblocks = total * 100;
			for (size_t block_idx = 0; block_idx < numblocks; ++block_idx) {
				buffer_str += "\xDB";
			}

			Log::app(Log::Level::debug, buffer_str.c_str());
		}*/
	}
}

int AudioDevice::audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data)
{
	AudioDevice* data_this = (AudioDevice*)data;
	if (outputBuffer) {
		if (status == RTAUDIO_OUTPUT_UNDERFLOW) {
			Log::entity(Log::Level::warn, "Output underflow");
		}
		else if (status == RTAUDIO_INPUT_OVERFLOW) {
			Log::entity(Log::Level::warn, "Input overflow");
		}

		float* samples = (float*)outputBuffer;
		for (size_t channel = 0; channel < m_audio_data->channels; ++channel) {
			auto channel_buffer = (channel == 0) ? m_received_network_audio_buffer_left : m_received_network_audio_buffer_right;

			if (channel_buffer->size() > nBufferFrames) {
				for (auto offset = 0; offset < nBufferFrames; ++offset) {
					std::scoped_lock<std::mutex> lock(m_incoming_audio_lock);
					*(samples++) = channel_buffer->front(); //read_offset
					channel_buffer->pop_front();
				}
				if (!channel_buffer->size()) {
					Log::entity(Log::Level::warn, "Audio in buffer empty!");
				}
			}
		}
	}

	if (inputBuffer) {
		float* samples = (float*)inputBuffer;
		outgoing_audio()->raw_value()->assign(samples, m_audio_data->channels * nBufferFrames);
		outgoing_audio()->fire();
	}

	return 0;
}
