#pragma once

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstEntityFactory.h>
#include <showtime/ZstURI.h>
#include <showtime/ZstFilesystemUtils.h>


// Forwards

class RtAudio;

class ZST_CLASS_EXPORTED AudioFactory : public showtime::ZstEntityFactory
{
public:
	AudioFactory(const char* name);
private:
	std::shared_ptr<RtAudio> m_query_audio;
};
