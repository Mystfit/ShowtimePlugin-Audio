#pragma once

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstComponent.h>
#include <showtime/entities/ZstPlug.h>

#include <boost/circular_buffer.hpp>
#include <memory>

typedef float AUDIO_BUFFER_T;

class AudioComponentBase : public showtime::ZstComponent
{
public:
	ZST_PLUGIN_EXPORT AudioComponentBase(const char* component_type, const char* name);
	ZST_PLUGIN_EXPORT virtual void on_registered() override;

	showtime::ZstInputPlug* incoming_audio();
	showtime::ZstOutputPlug* outgoing_audio();
protected:
	std::shared_ptr<showtime::ZstInputPlug> m_incoming_network_audio;
	std::shared_ptr<showtime::ZstOutputPlug> m_outgoing_network_audio;
};
