#include "saveStream.h"


#include <fstream>
#include <iostream>




bool saveStream::openInput(std::string filePath)
{
    auto funcFileVaild = [filePath]()->bool {
        std::ifstream inputFile;

        inputFile.open(filePath, std::ios::in);
        if (inputFile.is_open() == false) {
            std::cout << "open file fail" << std::endl;
            return false;
        }
        inputFile.close();
        return true;
    };
    //判断文件是否有效。 先这样吧，后期去开源库里面抠一个出来。
    if (funcFileVaild() == false) {
        return false;
    }   

    if (avformat_open_input(&_inputFormatCtx, filePath.c_str(), NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", filePath.c_str());
        return false;
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(_inputFormatCtx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        avformat_close_input(&_inputFormatCtx);
        return false;
    }

    for (int i = 0; i < _inputFormatCtx->nb_streams; i++) {
        int tp = _inputFormatCtx->streams[i]->codecpar->codec_type;

        if (tp != AVMEDIA_TYPE_VIDEO && tp != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        _idxMap.emplace(std::pair<int, int>(i, -1));
    }

    return true;
}

bool saveStream::openOutput(std::string filePath)
{
    avformat_alloc_output_context2(&_outputFormatCtx, NULL, NULL, filePath.c_str());
    if (!_outputFormatCtx) {
        fprintf(stderr, "Could not create output context\n");
        return false;
    }

    auto funcAddStream = [this](int idx) {
        if (idx < 0) {
            return false;
        }
        AVStream* out_stream = avformat_new_stream(_outputFormatCtx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            return false;
        }

        int ret = avcodec_parameters_copy(out_stream->codecpar, _inputFormatCtx->streams[idx]->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            return false;
        }
        std::cout << "add input stream " << idx << " to output" << std::endl;

        _idxMap.at(idx) = out_stream->index;

        return true;
    };

    for (auto & itor : _idxMap) {
        funcAddStream(itor.first);
    }
    for (auto& itor : _idxMap) {
        std::cout << itor.first << " : " << itor.second << std::endl;
    }
    int ret = avio_open(&_outputFormatCtx->pb, filePath.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        fprintf(stderr, "Could not open output file '%s'", filePath.c_str());
        return false;
    }


    ret = avformat_write_header(_outputFormatCtx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return false;
    }

    return false;
}

void saveStream::save()
{
    AVRational timebaseIn, timebaseOut;
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        return;
    }

    int cnt = 100;
    int ret = -1;
    while (cnt--) {
        ret = av_read_frame(_inputFormatCtx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error av_read_frame\n");
            std::cout << "Error av_read_frame" << std::endl;
            break;
        }
        
        timebaseIn = _inputFormatCtx->streams[pkt->stream_index]->time_base;
        timebaseOut = _outputFormatCtx->streams[_idxMap.at(pkt->stream_index)]->time_base;

        av_packet_rescale_ts(pkt, timebaseIn, timebaseOut);
        std::cout << cnt <<  " ;; " << pkt->pts << std::endl;
        pkt->pos = -1;

        ret = av_interleaved_write_frame(_outputFormatCtx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error muxing packet\n");
            std::cout << "Error muxing packet" << std::endl;
            break;
        }
    }
    av_write_trailer(_outputFormatCtx);
    std::cout << "save end " << std::endl;
}
