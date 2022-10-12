#pragma once

#include <string>
#include <map>

extern "C" {
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
	#include "libavdevice/avdevice.h"
	#include <libavutil/timestamp.h>
}

class saveStream
{
public:
	bool openInput(std::string filePath = "E:\\lr\\ubuntu\\share\\lr\\in.mp4");
	bool openOutput(std::string filePath = "E:\\lr\\ubuntu\\share\\lr\\out.mp4");
	bool openCodec();
	void save();

private:
	std::string _inputFilePath;
	std::string _outputFilePath;

	AVFormatContext* _inputFormatCtx;
	AVFormatContext* _outputFormatCtx;
	std::map<int, int> _idxMap;

	AVCodecContext* _decodeCtx;
	AVCodecContext* _encodeCtx;
	AVCodecContext* _encodeJpegCtx;
};

