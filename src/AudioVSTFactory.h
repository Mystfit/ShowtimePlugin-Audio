#pragma once

#include <showtime/ZstExports.h>
#include <showtime/entities/ZstEntityFactory.h>
#include <showtime/ZstURI.h>
#include <showtime/ZstFilesystemUtils.h>



// Forwards

class RtAudio;

class ZST_CLASS_EXPORTED AudioVSTFactory : public showtime::ZstEntityFactory
{
public:
	AudioVSTFactory(const char* name);
	void scan_vst_path(const std::string& path);
private:
};