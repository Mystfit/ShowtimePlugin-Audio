#include "AudioFactory.h"
#include "AudioDevice.h"
#include <RtAudio.h>
#include <showtime/ZstLogging.h>
#include <fstream>
#include <memory>

using namespace showtime;

AudioFactory::AudioFactory(const char* name) : 
	showtime::ZstEntityFactory(name),
	m_query_audio(std::make_shared<RtAudio>())
{
	try {
		m_query_audio = std::make_shared<RtAudio>();
	}
	catch (RtAudioError& error) {
		Log::app(Log::Level::error, "Audio construction failed: {}", error.getMessage().c_str());
		return;
	}

	// Determine the number of devices available
	int devices = m_query_audio->getDeviceCount();

	// Scan through devices for various capabilities
	RtAudio::DeviceInfo info;

	for (size_t device_idx = 0; device_idx < devices; device_idx++) {
		try {
			info = m_query_audio->getDeviceInfo(device_idx);
		}
		catch (RtAudioError& error) {
			error.printMessage();
			break;
		}

		// Print, for example, the maximum number of output channels for each device
		Log::app(Log::Level::notification, "Device:{} Name:{}, I/O channels:{}|{}, SampleRate:{}", device_idx, info.name.c_str()
			, info.inputChannels, info.outputChannels, info.preferredSampleRate);

		this->add_creatable(info.name.c_str(), [info, device_idx](const char* e_name) -> std::unique_ptr<ZstEntityBase> {
			return std::make_unique<AudioDevice>(e_name, device_idx, info.inputChannels, info.outputChannels, info.nativeFormats);
		});
	}
}
