#pragma once

extern "C" {
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
	#include "libavdevice/avdevice.h"
	#include "libavutil/timestamp.h"
	#include "libavfilter/avfilter.h"
}

#include <memory>
#include <unordered_map>
#include <string>

class output
{
public:
	bool openOutput(std::string filePath, std::unordered_map<const AVCodecParameters*, AVRational> streamParams);
	bool writePacket(std::shared_ptr<AVPacket> pkt);
	bool closeOutput();

private:
	AVFormatContext* _ctx = nullptr;
};

