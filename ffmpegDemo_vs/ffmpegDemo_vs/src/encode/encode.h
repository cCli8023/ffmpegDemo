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

class encode
{
public:
	~encode();
	bool initEncode(const AVStream* st);
	std::shared_ptr<AVPacket> encodeFrame(std::shared_ptr<AVFrame>);

private:
	AVCodecContext* _ctx = nullptr;
};

