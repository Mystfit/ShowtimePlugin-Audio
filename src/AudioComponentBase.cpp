#include "AudioComponentBase.h"
#include "fmt/format.h"
#include "showtime/ZstLogging.h"

using namespace showtime;

AudioComponentBase::AudioComponentBase(size_t num_device_input_channels, size_t num_device_output_channels, const char* component_type, const char* name) :
	ZstComputeComponent(component_type, name)
{
	init_plugs(num_device_output_channels, num_device_input_channels);
}

void AudioComponentBase::init_plugs(size_t num_incoming_plug_channels, size_t num_outgoing_plug_channels)
{
	for (size_t idx = 0; idx < num_incoming_plug_channels; ++idx) {
		m_input_channel_plugs.push_back(std::make_unique<ZstInputPlug>(fmt::format("IN_audio_{}", idx).c_str(), ZstValueType::FloatList, 1));
	}

	for (size_t idx = 0; idx < num_outgoing_plug_channels; ++idx) {
		m_output_channel_plugs.push_back(std::make_unique<ZstOutputPlug>(fmt::format("OUT_audio_{}", idx).c_str(), ZstValueType::FloatList));
	}
}

void AudioComponentBase::on_registered()
{
	ZstComponent::on_registered();

	for(auto& plug : m_input_channel_plugs){
		add_child(plug.get());
	}
	for (auto& plug : m_output_channel_plugs) {
		add_child(plug.get());
	}
}

showtime::ZstInputPlug* AudioComponentBase::incoming_audio(size_t channel)
{
	try {
		return m_input_channel_plugs.at(channel).get();
	}
	catch (std::out_of_range e) {
		Log::app(Log::Level::debug, "No input plug for audio channel {} found", channel);
	}
	return nullptr;
}

showtime::ZstOutputPlug* AudioComponentBase::outgoing_audio(size_t channel)
{
	try {
		return m_output_channel_plugs.at(channel).get();
	}
	catch (std::out_of_range e) {
		Log::app(Log::Level::debug, "No output plug for audio channel {} found", channel);
	}
	return nullptr;
}

size_t AudioComponentBase::num_input_channels()
{
	return m_input_channel_plugs.size();
}

size_t AudioComponentBase::num_output_channels()
{
	return m_output_channel_plugs.size();
}
