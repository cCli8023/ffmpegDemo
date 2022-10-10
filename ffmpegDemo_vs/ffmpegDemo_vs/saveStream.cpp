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

    int videoIdx = av_find_best_stream(_inputFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (videoIdx >= 0) {
        AVStream* inputVideoStream = _inputFormatCtx->streams[videoIdx];
        std::cout << "video start time : " << inputVideoStream->start_time
            << " duration : " << inputVideoStream->duration
            << " time(s) : " << 1.0f * inputVideoStream->duration * av_q2d(inputVideoStream->time_base)
            << " " << inputVideoStream->time_base.num << " / " << inputVideoStream->time_base.den
            << std::endl;
    }

    int audioIdx = av_find_best_stream(_inputFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audioIdx >= 0) {
        AVStream* inputAudioStream = _inputFormatCtx->streams[audioIdx];
        std::cout << "audio start time : " << inputAudioStream->start_time 
                    << " duration : " << inputAudioStream->duration
                    << " time(s) : " << 1.0f * inputAudioStream->duration * av_q2d(inputAudioStream->time_base)
                    << " " << inputAudioStream->time_base.num << " / " << inputAudioStream->time_base.den
                    << std::endl;
    }

    int cnt = 100;
    int ret = -1;
    std::map<int, std::string> typeStr= { {AVMEDIA_TYPE_VIDEO, "video : "}, {AVMEDIA_TYPE_AUDIO, "                  audio : "}};

    int64_t vNextTs = 0, aNextTs = 0;
    int64_t videoPointa = _inputFormatCtx->streams[0]->start_time + _inputFormatCtx->streams[0]->duration / 10;
    int64_t videoPointb = _inputFormatCtx->streams[0]->duration - _inputFormatCtx->streams[0]->duration / 10;
    int64_t audioPointa = _inputFormatCtx->streams[1]->start_time + _inputFormatCtx->streams[1]->duration / 10;
    int64_t audioPointb = _inputFormatCtx->streams[1]->duration - _inputFormatCtx->streams[1]->duration / 10;
    std::cout << videoPointa << " " << videoPointb << " " << audioPointa << " " << audioPointb << std::endl;
    bool dropFrame = false, bReSeek = false;
    int64_t reSeekPts = 0;

#if 0
    av_seek_frame(_inputFormatCtx, videoIdx, 1029115, AVSEEK_FLAG_BACKWARD);
    int64_t curPts = _inputFormatCtx->streams[videoIdx]->duration;
    while (1) {
        ret = av_read_frame(_inputFormatCtx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error av_read_frame\n");
            std::cout << "Error av_read_frame" << std::endl;
            return;
        }

        if (pkt->stream_index != videoIdx) {
            av_packet_unref(pkt);
            continue;
        }
        else {
            std::cout << pkt->pts * av_q2d(_inputFormatCtx->streams[videoIdx]->time_base) << std::endl;
            curPts = pkt->pts - 10;
            av_packet_unref(pkt);
            av_seek_frame(_inputFormatCtx, videoIdx, curPts, AVSEEK_FLAG_BACKWARD);
        }
        if (curPts < _inputFormatCtx->streams[videoIdx]->start_time) {
            break;
        }
    }
    return;
#endif
    while (1) {
        ret = av_read_frame(_inputFormatCtx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error av_read_frame\n");
            std::cout << "Error av_read_frame" << std::endl;
            break;
        }
        
        timebaseIn = _inputFormatCtx->streams[pkt->stream_index]->time_base;
        timebaseOut = _outputFormatCtx->streams[_idxMap.at(pkt->stream_index)]->time_base;
        /*
        
            有没有可能截取的部分，第一个packet不是关键帧。 我这截取出来中间有一点花屏但是又没有全部花掉。        
        
        
        */

        if (AVMEDIA_TYPE_VIDEO == _inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) {
            if (pkt->pts > videoPointa && pkt->pts < videoPointb && bReSeek == false) {
                std::cout << "-+-+" << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << pkt->flags << "-+-+" << std::endl;
                av_packet_unref(pkt);
                dropFrame = true;
                continue;
            }
            if (dropFrame == true) {
                dropFrame = false;
                bReSeek = true;
                reSeekPts = pkt->pts;
                av_seek_frame(_inputFormatCtx, pkt->stream_index, pkt->pts, AVSEEK_FLAG_BACKWARD);
                av_packet_unref(pkt);
                continue;
            }

            vNextTs = vNextTs == 0 ? pkt->pts : vNextTs + pkt->duration;
            std::cout << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << vNextTs << " " << pkt->flags << std::endl;
            pkt->pts = pkt->dts = vNextTs;

            if (bReSeek) {
                //pkt->dts = -1;
                pkt->flags |= AV_PKT_FLAG_DISCARD;

                av_seek_frame(_inputFormatCtx, pkt->stream_index, reSeekPts, AVSEEK_FLAG_ANY);
                bReSeek = false;
            }
        }
        else if (AVMEDIA_TYPE_AUDIO == _inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) {
            if (pkt->pts > audioPointa && pkt->pts < audioPointb) {
                std::cout << "-+-+" << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << "-+-+" << std::endl;
                av_packet_unref(pkt);
                continue;
            }
            aNextTs = aNextTs == 0 ? pkt->pts : aNextTs + pkt->duration;
            std::cout << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << aNextTs << " " << pkt->flags << std::endl;
            pkt->pts = pkt->dts = aNextTs;
        }
        else {
            av_packet_unref(pkt);
            continue;
        }

        av_packet_rescale_ts(pkt, timebaseIn, timebaseOut);
        
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
