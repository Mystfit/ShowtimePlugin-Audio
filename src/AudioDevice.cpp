#include "AudioDevice.h"
#include "RtAudio.h"
#include <showtime/ZstLogging.h>

using namespace showtime;
using namespace std::placeholders;


AudioDevice::AudioDevice(const char* name, size_t device_index, size_t num_inputs, size_t num_outputs, unsigned long native_formats_bmask) : 
	ZstComponent(AUDIODEVICE_COMPONENT_TYPE, ("AudioDevice_" + std::string(name)).c_str()),
	m_audio_device(std::make_shared<RtAudio>()),
	m_num_inputs(num_inputs),
	m_num_outputs(num_outputs),
	m_audio_data(std::make_shared<AudioData>()),
	m_incoming_network_audio(std::make_shared<ZstInputPlug>("audio_to_device", ZstValueType::FloatList)),
	m_outgoing_network_audio(std::make_shared<ZstOutputPlug>("audio_from_device", ZstValueType::FloatList)),
	bLogAmplitude(true)
{
	Log::entity(Log::Level::notification, "Creating audio device {} with device ID {} {}", URI().last().path(), device_index, sizeof(DEVICE_BUFFER_T));

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

	//m_audio_data->buffer = 0;
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
	m_audio_data->bufferBytes = bufferFrames * total_channels * sizeof(DEVICE_BUFFER_T);
	m_audio_data->totalFrames = (unsigned long)samplerate;//(unsigned long)(samplerate * time);
	m_audio_data->frameCounter = 0;
	m_audio_data->channels = total_channels;
	unsigned long totalBytes;
	totalBytes = m_audio_data->totalFrames * total_channels * sizeof(DEVICE_BUFFER_T);
	
	// Allocate the entire data buffer before starting stream.
	m_audio_data->buffer = boost::circular_buffer< DEVICE_BUFFER_T>(m_audio_data->totalFrames);
	m_buffer = std::make_shared<boost::circular_buffer< DEVICE_BUFFER_T>>(bufferFrames * total_channels);

	//m_audio_data->buffer = (DEVICE_BUFFER_T*)malloc(totalBytes);
	/*if (m_audio_data->buffer == 0) {
		Log::entity(Log::Level::error, "Memory allocation error ... quitting!\n");
	}*/

	try {
		m_audio_device->startStream();
	}
	catch (RtAudioError& e) {
		Log::entity(Log::Level::error, e.getMessage().c_str());
	}
}

void AudioDevice::on_registered()
{
	add_child(m_outgoing_network_audio.get());
	add_child(m_incoming_network_audio.get());
}

void AudioDevice::compute(ZstInputPlug* plug)
{
	if (plug == m_incoming_network_audio.get()) {
		float total = 0.0;
		std::string buffer_str;
		
		m_buffer->assign(m_incoming_network_audio->raw_value()->float_buffer(), m_incoming_network_audio->raw_value()->float_buffer() + m_incoming_network_audio->size());
		//std::copy(m_incoming_network_audio->raw_value()->float_buffer(), m_incoming_network_audio->raw_value()->float_buffer() + m_incoming_network_audio->size(), m_buffer->begin());
		
		for (size_t idx = 0; idx < m_buffer->size(); ++idx) {
			//m_incoming_network_audio->float_at(idx);
			//m_buffer->push_back(m_incoming_network_audio->float_at(idx));
			if (bLogAmplitude) {
				float val = m_buffer->at(idx);
				total += abs(val);
			}

			//m_buffer->push_back(val);
			//m_audio_data->write_offset++;
		}

		if (bLogAmplitude && total > 0.0) {
			total /= plug->size();
			int numblocks = total * 100;
			for (size_t block_idx = 0; block_idx < numblocks; ++block_idx) {
				buffer_str += "\xDB";
			}

			Log::app(Log::Level::debug, buffer_str.c_str());
		}
	}
}

int AudioDevice::audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data)
{
	AudioDevice* data_this = (AudioDevice*)data;
	if (outputBuffer) {
		if (status == RTAUDIO_OUTPUT_UNDERFLOW) {
			Log::entity(Log::Level::warn, "Output underflow");
		}

		float* samples = (float*)outputBuffer;
		for (size_t channel = 0; channel < m_audio_data->channels; ++channel) {
			for (auto offset = 0; offset < m_buffer->size(); ++offset) {
				*(samples++) = m_buffer->at(offset);
			}
		}
	}

	if (inputBuffer) {
		float* samples = (float*)inputBuffer;
		m_outgoing_network_audio->raw_value()->assign(samples, m_audio_data->channels * nBufferFrames);

		/*size_t offset = 0;
		for (size_t channel = 0; channel < m_audio_data->channels; ++channel) {
			for (size_t i = 0; i < nBufferFrames; i++) {
				m_outgoing_network_audio->append_float(*(samples + (offset++)));
			}
		}*/

		m_outgoing_network_audio->fire();
	}

	return 0;
}
