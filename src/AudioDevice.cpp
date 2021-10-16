#include "AudioDevice.h"
#include "RtAudio.h"

AudioDevice::AudioDevice(const char* name, size_t device_index, size_t num_inputs, size_t num_outputs, unsigned long native_formats_bmask) : 
	ZstComponent(AUDIODEVICE_COMPONENT_TYPE, ("AudioDevice_" + std::string(name)).c_str()),
	m_audio_device(std::make_shared<RtAudio>()),
	m_num_inputs(num_inputs),
	m_num_outputs(num_outputs)
{
	RtAudio::StreamParameters outparams;
	outparams.deviceId = device_index;
	outparams.nChannels = num_inputs;
	
	RtAudio::StreamParameters inparams;
	inparams.deviceId = device_index;
	inparams.nChannels = num_inputs;

	bool supports_byte_format = (native_formats_bmask & RTAUDIO_SINT8) == RTAUDIO_SINT8;
	RtAudioFormat format = RTAUDIO_SINT8;
}

void AudioDevice::on_registered()
{
}
