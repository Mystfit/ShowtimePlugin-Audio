#include "AudioDevice.h"
#include "RtAudio.h"
#include <showtime/ZstLogging.h>

using namespace showtime;
using namespace std::placeholders;


AudioDevice::AudioDevice(const char* name, size_t device_index, size_t num_device_inputs, size_t num_device_outputs, unsigned long native_formats_bmask) : 
	AudioComponentBase(num_device_inputs, num_device_outputs, AUDIODEVICE_COMPONENT_TYPE, name),
	m_audio_device(std::make_shared<RtAudio>()),
	bLogAmplitude(false),
	m_total_writes(0),
	m_total_reads(0),
	m_buffer_frames(AUDIO_BUFFER_FRAMES)
{
	Log::entity(Log::Level::notification, "Creating audio device {} with device ID {} {}", URI().last().path(), device_index, sizeof(AUDIO_BUFFER_T));

	double time = 0.1;
	unsigned long samplerate = 44100;

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
		m_audio_device->openStream((num_device_outputs) ? &outparams : nullptr, (num_device_inputs) ? &inparams : nullptr, format, samplerate, &m_buffer_frames, cb, (void*)this, &opts);
	} 
	catch (RtAudioError& e) {
		Log::entity(Log::Level::error, e.getMessage().c_str());
	}

	size_t buffer_size = m_buffer_frames * 4;
	for (size_t idx = 0; idx < num_device_outputs; ++idx) {
		m_incoming_graph_audio_channel_buffers.emplace_back(std::make_unique< RingBuffer<AUDIO_BUFFER_T> >(buffer_size, fmt::format("deviceoutputchannel{}", idx)));
	}
	for (size_t idx = 0; idx < num_device_inputs; ++idx) {
		m_outgoing_graph_audio_channel_buffers.emplace_back(std::make_unique< RingBuffer<AUDIO_BUFFER_T> >(buffer_size, fmt::format("deviceinputchannel{}", idx)));
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
	m_audio_graph_thread.interrupt();
	m_audio_graph_thread.join();
}

void AudioDevice::on_registered()
{
	AudioComponentBase::on_registered();

	// Start a thread to handle executing the audio graph when audio data is available
	m_audio_graph_thread = boost::thread(boost::bind(&AudioDevice::process_graph, this));
}

void AudioDevice::process_graph()
{
	while(true){
		//bool has_audio = m_outgoing_graph_audio_channel_buffers.size() > 0 ? m_outgoing_graph_audio_channel_buffers[0]->numItemsAvailableForRead() > m_buffer_frames : false;
		try {
			//if (has_audio)
			std::unique_lock<std::mutex> lk(m_graph_mtx);
			m_trigger_process_graph.wait(lk, [this] { 
				return m_outgoing_graph_audio_channel_buffers.size() > 0 ? m_outgoing_graph_audio_channel_buffers[0]->numItemsAvailableForRead() > m_buffer_frames : false; 
			});

			//if (m_outgoing_audio_lock.try_lock()) {
				m_audio_buffer_received_samples = false;
				this->execute_upstream();
			//	m_outgoing_audio_lock.unlock();
			//}
		}
		catch (boost::thread_interrupted e) { break; }
	}
}

void AudioDevice::compute(ZstInputPlug* plug)
{
	float total = 0.0;
	std::string buffer_str;

	// Receive graph audio -> send audio for device
	for (size_t channel = 0; channel < m_input_channel_plugs.size(); ++channel) {
		if (auto channel_plug = incoming_audio(channel)) {
			if (m_incoming_graph_audio_channel_buffers.size() == m_input_channel_plugs.size()) {
				auto num_frames = channel_plug->size();

				m_incoming_graph_audio_channel_buffers[channel]->push(channel_plug->raw_value()->float_buffer(), channel_plug->size());

				if (bLogAmplitude) {
					for (size_t frame_idx = 0; frame_idx < num_frames; ++frame_idx) {
						auto val = channel_plug->float_at(frame_idx);
						if (bLogAmplitude) {
							total += abs(val);
						}
					}
				}

				if (bLogAmplitude && total > 0.0 && num_frames > 0) {
					total /= num_frames;
					int numblocks = std::max(total * 100, 0.0f);
					for (size_t block_idx = 0; block_idx < numblocks; ++block_idx) {
						buffer_str += "\xDB";
					}

					Log::app(Log::Level::debug, buffer_str.c_str());
				}
			}
		}
	}
	m_total_writes++;

	// Receive incoming audio from device -> send to graph
	bool has_audio = false;
	if (m_outgoing_graph_audio_channel_buffers.size() == m_output_channel_plugs.size()) {
		for (size_t channel = 0; channel < m_output_channel_plugs.size(); ++channel) {
			if (auto channel_plug = outgoing_audio(channel)) {
				// NOOOOOOOOOOOO
				AUDIO_BUFFER_T* samples = (AUDIO_BUFFER_T*)malloc(sizeof(float) * m_buffer_frames);
				size_t num_frames = m_outgoing_graph_audio_channel_buffers[channel]->pop(samples, m_buffer_frames);
				channel_plug->raw_value()->assign(samples, num_frames);
				channel_plug->fire();
				free(samples);
			}
		}
	}
	
	ZstComputeComponent::compute(plug);
}

int AudioDevice::audio_callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* data)
{
	AudioDevice* data_this = (AudioDevice*)data;
	if (!data_this)
		return 0;

	if (inputBuffer) {
		data_this->ReceiveAudioFromDevice(status, inputBuffer, nBufferFrames);
	}

	if (outputBuffer) {
		// Audio graph is processed upstream - processed data will only be available next buffer
		m_trigger_process_graph.notify_one();

		data_this->SendAudioToDevice(status, outputBuffer, nBufferFrames);
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
	size_t channel_offset = 0;

	for (size_t channel = 0; channel < m_incoming_graph_audio_channel_buffers.size(); ++channel) {
		channel_offset = channel * nBufferFrames;
		size_t num_available_frames = m_incoming_graph_audio_channel_buffers[channel]->pop(samples + channel_offset, nBufferFrames);
		
		if (num_available_frames < nBufferFrames) {
			Log::entity(Log::Level::warn, "Output underflow");
		}
	}
	m_total_reads++;
}

void AudioDevice::ReceiveAudioFromDevice(const RtAudioStreamStatus& status, void * inputBuffer, unsigned int nBufferFrames)
{
	float* samples = (float*)inputBuffer;
	for (size_t channel_idx = 0; channel_idx < this->num_output_channels(); ++channel_idx) {
		m_outgoing_graph_audio_channel_buffers[channel_idx]->push(samples, nBufferFrames);
	}
	m_audio_buffer_received_samples = true;
}
