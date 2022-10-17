#pragma once

#include <cstdio>

#include <string>
#include <memory>

extern "C" {
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
	#include "libavdevice/avdevice.h"
	#include <libavutil/timestamp.h>
	#include "libavfilter/avfilter.h"
}

class input
{
public:
	input() {};
	input(std::string url) {
		_url = url;
	};
	~input();

	bool openInput(std::string url = "");
	std::shared_ptr<AVPacket> getPacket();
	void close();

private:
	AVFormatContext* _ctx = NULL; 
	AVStream* _videoStream = NULL;
	AVStream* _audioStream = NULL;

	std::string _url;
};

