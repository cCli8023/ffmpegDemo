#pragma once

#include <cstdio>

#include <string>
#include <memory>
#include <unordered_map>

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
	std::unordered_map<const AVCodecParameters*, AVRational> getStreamParam();
	const AVStream* videoStream() {
		return _videoStream;
	}
	const AVStream* audioStream() {
		return _audioStream;
	}
private:
	AVFormatContext* _ctx = NULL; 
	AVStream* _videoStream = NULL;
	AVStream* _audioStream = NULL;

	std::string _url;
};

