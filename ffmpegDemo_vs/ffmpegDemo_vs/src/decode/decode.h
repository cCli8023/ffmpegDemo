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

class decode
{
public:
	~decode();
	bool initDecode(const AVStream* st);
	std::shared_ptr<AVFrame> decodePkt(std::shared_ptr<AVPacket> pkt);

private:
	AVCodecContext* _ctx = nullptr;
};

