#include "AudioComponentBase.h"

using namespace showtime;

AudioComponentBase::AudioComponentBase(const char* component_type, const char* name) : 
	ZstComponent(component_type, name),
	m_incoming_network_audio(std::make_shared<ZstInputPlug>("IN_audio", ZstValueType::FloatList, 1)),
	m_outgoing_network_audio(std::make_shared<ZstOutputPlug>("OUT_audio", ZstValueType::FloatList))
{
}

void AudioComponentBase::on_registered()
{
	add_child(m_outgoing_network_audio.get());
	add_child(m_incoming_network_audio.get());
}

showtime::ZstInputPlug* AudioComponentBase::incoming_audio()
{
	return m_incoming_network_audio.get();
}

showtime::ZstOutputPlug* AudioComponentBase::outgoing_audio()
{
	return m_outgoing_network_audio.get();
}
