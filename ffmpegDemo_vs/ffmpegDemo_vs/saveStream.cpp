#include "saveStream.h"


#include <fstream>
#include <iostream>
#include <vector>
extern "C" {
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}
#include <windows.h>

static char* dup_wchar_to_utf8(const wchar_t* w)
{
    char* s = NULL;
    int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
    s = (char*)av_malloc(l);
    if (s)
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
    return s;
}


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

bool saveStream::openCodec()
{

    int ret, stream_index;
    AVStream* st;
    const AVCodec* dec = NULL;
    const AVCodec* enc = NULL;
    const AVCodec* encJpeg = NULL;
    AVDictionary* opts = NULL;

    ret = av_find_best_stream(_inputFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file ''\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }else {
        stream_index = ret;
        st = _inputFormatCtx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            return false;
        }

        /* Allocate a codec context for the decoder */
        _decodeCtx = avcodec_alloc_context3(dec);
        if (!_decodeCtx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            return false;
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(_decodeCtx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            return false;
        }
        _decodeCtx->time_base = av_stream_get_codec_timebase(st);
        
        /* Init the decoders */
        if ((ret = avcodec_open2(_decodeCtx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            return false;
        }
    }

    enc = avcodec_find_encoder(st->codecpar->codec_id);
    if (!enc) {
        fprintf(stderr, "Failed to find %s codec\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }

    /* Allocate a codec context for the decoder */
    _encodeCtx = avcodec_alloc_context3(enc);
    if (!_decodeCtx) {
        fprintf(stderr, "Failed to allocate the %s codec context\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }

    _encodeCtx->bit_rate = st->codecpar->bit_rate;
    _encodeCtx->width = st->codecpar->width;
    _encodeCtx->height = st->codecpar->height;
    _encodeCtx->framerate = st->r_frame_rate;
    _encodeCtx->gop_size = 50;
    _encodeCtx->max_b_frames = 1;
    _encodeCtx->pix_fmt = (AVPixelFormat)st->codecpar->format;

    _encodeCtx->time_base  = av_stream_get_codec_timebase(st);
    if (enc->id == AV_CODEC_ID_H264)
        av_opt_set(_encodeCtx->priv_data, "preset", "slow", 0);
    /* Init the decoders */
    if ((ret = avcodec_open2(_encodeCtx, enc, NULL)) < 0) {
        fprintf(stderr, "Failed to open %s codec\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }


    encJpeg = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!encJpeg) {
        fprintf(stderr, "Failed to find %s codec\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }

    /* Allocate a codec context for the decoder */
    _encodeJpegCtx = avcodec_alloc_context3(encJpeg);
    if (!_encodeJpegCtx) {
        fprintf(stderr, "Failed to allocate the %s codec context\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }


    _encodeJpegCtx->codec_id = AV_CODEC_ID_MJPEG;
    _encodeJpegCtx->pix_fmt = (AVPixelFormat)st->codecpar->format;
    _encodeJpegCtx->width = st->codecpar->width;
    _encodeJpegCtx->height = st->codecpar->height;

    _encodeJpegCtx->time_base = av_stream_get_codec_timebase(st);
    _encodeJpegCtx->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
    /* Init the decoders */
    if ((ret = avcodec_open2(_encodeJpegCtx, encJpeg, NULL)) < 0) {
        fprintf(stderr, "Failed to open %s codec\n",
            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return false;
    }


    return true;
}

bool saveStream::openFilter()
{
    char args[512];
    int ret = 0;

    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    std::string  filters_descr = "drawtext=fontfile='E:/lr/ubuntu/share/lr/ffmpegDemo/Expo.ttf':text='llllrrrr':fontsize=100:x=100:y=100";

    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P };

    _filterGrap = avfilter_graph_alloc();
    if (!outputs || !inputs || !_filterGrap) {
        ret = AVERROR(ENOMEM);
        std::cout << "avfilter_graph_alloc err  " << std::endl;
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    sprintf_s(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        _encodeCtx->width, _encodeCtx->height, _encodeCtx->pix_fmt,
        _encodeCtx->time_base.num, _encodeCtx->time_base.den,
        _encodeCtx->sample_aspect_ratio.num, _encodeCtx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&_filterCtx, buffersrc, "in",
        args, NULL, _filterGrap);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        std::cout << "avfilter_graph_create_filter err  " << std::endl;
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&_filterCtxSink, buffersink, "out",
        NULL, NULL, _filterGrap);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        std::cout << "avfilter_graph_create_filter err  " << std::endl;
        goto end;
    }

    ret = av_opt_set_int_list(_filterCtxSink, "pix_fmts", pix_fmts,
        AV_PIX_FMT_YUV420P, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        std::cout << "av_opt_set_int_list err  " << std::endl;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = _filterCtx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = _filterCtxSink;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    if ((ret = avfilter_graph_parse_ptr(_filterGrap, filters_descr.c_str(),
        &inputs, &outputs, NULL)) < 0) {
        char errBuf[128] = { 0 };
        av_strerror(ret, errBuf, 128);
        std::cout << "-----avfilter_graph_parse_ptr err  " << errBuf << "-- " <<  std::endl;
        goto end;
    }
        

    if ((ret = avfilter_graph_config(_filterGrap, NULL)) < 0) {
        std::cout << "avfilter_graph_config err  " <<  std::endl;
        goto end;
    }
        
    return true;
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    std::cout << "err end " << std::endl;
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
    int64_t videoPointa = _inputFormatCtx->streams[0]->start_time + _inputFormatCtx->streams[0]->duration / 5;
    int64_t videoPointb = _inputFormatCtx->streams[0]->duration - _inputFormatCtx->streams[0]->duration / 5;
    int64_t audioPointa = _inputFormatCtx->streams[1]->start_time + _inputFormatCtx->streams[1]->duration / 5;
    int64_t audioPointb = _inputFormatCtx->streams[1]->duration - _inputFormatCtx->streams[1]->duration / 5;
    std::cout << videoPointa << " " << videoPointb << " " << audioPointa << " " << audioPointb << std::endl;
    bool dropFrame = false, bReSeek = false;
    int64_t reSeekPts = 0;

#if 0
    av_seek_frame(_inputFormatCtx, videoIdx, 814587, AVSEEK_FLAG_BACKWARD);
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
            av_seek_frame(_inputFormatCtx, videoIdx, 814587, AVSEEK_FLAG_BACKWARD);
        }
        if (curPts < _inputFormatCtx->streams[videoIdx]->start_time) {
            break;
        }
    }
    return;
#endif
    bool reCoding = false;
    int64_t startPts = 0;
   

    std::vector<AVPacket* > audioTemp;
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
        
            尝试重编码解决这个问题：
            有丢packet，在新的packet序列开始时，进行重编码。记录startPts
            视频丢过去解码重编码，音频存起来
            当读取到KEY，且pts>=startPts,就退出重编码。
        */

        if (AVMEDIA_TYPE_VIDEO == _inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) {
            if (pkt->pts > videoPointa && pkt->pts < videoPointb && reCoding == false) {
                std::cout << "drop" << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << pkt->flags  << std::endl;
                av_packet_unref(pkt);
                dropFrame = true;
                continue;
            }

            if (dropFrame == true) {
                dropFrame = false;
                reCoding = true;
                startPts = pkt->pts;
                av_seek_frame(_inputFormatCtx, pkt->stream_index, pkt->pts, AVSEEK_FLAG_BACKWARD);
                std::cout << "stop drop : " << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << pkt->flags << std::endl;
                av_packet_unref(pkt);
                continue;
            }

            if (pkt->flags & AV_PKT_FLAG_KEY && pkt->pts >= startPts) {
                reCoding = false;
                std::cout << "new key frame , stop recording : " << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << pkt->flags << std::endl;
            }

            if (reCoding) {
                //send to decode 
                if (pkt->pts < startPts) {
                    pkt->flags |= AV_PKT_FLAG_DISCARD;
                    std::cout << "decode but not out frame : " << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << pkt->flags << std::endl;
                }
                ret = avcodec_send_packet(_decodeCtx, pkt);
                if (ret < 0) {
                    fprintf(stderr, "--------Error submitting a packet for decoding ()\n");
                    break;
                }
                av_packet_unref(pkt);

                AVFrame* frame = av_frame_alloc();
                if (!frame) {
                    break;
                }

                ret = avcodec_receive_frame(_decodeCtx, frame);
                if (ret == AVERROR(EAGAIN)) {
                    av_frame_free(&frame);
                    continue;
                }else if (ret != 0) {
                    fprintf(stderr, "--------Error avcodec_receive_frame ()\n");
                    break;
                }
                std::cout << "get recoding frame : " << "video " << frame->pts * av_q2d(timebaseIn) << " " << frame->pts << std::endl;
                
                ret = av_buffersrc_add_frame_flags(_filterCtx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
                if (ret < 0) {
                    std::cout << "add err   " << AVERROR(ret) << std::endl;
                    break;
                }

                AVFrame* filterFrame = av_frame_alloc();
                ret = av_buffersink_get_frame(_filterCtxSink, filterFrame);
                if (ret == AVERROR(EAGAIN)) {
                    av_frame_free(&filterFrame);
                    av_frame_free(&frame);
                    std::cout << "continue : " << __LINE__ << std::endl;
                    continue;
                }
                else if (ret != 0) {
                    fprintf(stderr, "--------Error avcodec_receive_frame ()\n");
                    break;
                }
                else {
                    //std::cout << "av_buffersink_get_frame suc" << std::endl;
                }
  
                ret = avcodec_send_frame(_encodeJpegCtx, filterFrame);
                if (ret != 0) {
                    fprintf(stderr, "--------Error avcodec_send_frame ()\n");
                    break;
                }

                if (filterFrame->pts == startPts) {
                    //filterFrame->key_frame = 1;
                    //filterFrame->pict_type = AV_PICTURE_TYPE_I;
                }
                AVFrame* frameEnc = av_frame_alloc();
                av_frame_ref(frameEnc, filterFrame);
                std::cout << frameEnc->width << " " << frameEnc->height << std::endl;
                ret = avcodec_send_frame(_encodeCtx, frameEnc);
                if (ret != 0) {
                    fprintf(stderr, "--------Error avcodec_send_frame ()\n");
                    break;
                }
                av_frame_free(&filterFrame);
                av_frame_free(&frame);
                av_frame_free(&frameEnc);

                ret = avcodec_receive_packet(_encodeJpegCtx, pkt);
                if (ret == AVERROR(EAGAIN)) {
                    //continue;
                }
                else if (ret != 0) {
                    fprintf(stderr, "--------Error avcodec_receive_packet ()\n");
                    break;
                }
                else {
                    //write file
                    static int fileNum = 0;
                    char filepath[128] = { 0 };
                    sprintf_s(filepath,"E:\\lr\\ubuntu\\share\\lr\\out%d.jpeg", fileNum++);

                    AVFormatContext* jpegOut;
                    ret = avformat_alloc_output_context2(&jpegOut, nullptr, NULL, filepath);
                    if (ret < 0)
                    {
                        av_log(NULL, AV_LOG_ERROR, "open output context failed\n");
                    }

                    ret = avio_open2(&jpegOut->pb, filepath, AVIO_FLAG_WRITE, nullptr, nullptr);
                    if (ret < 0)
                    {
                        av_log(NULL, AV_LOG_ERROR, "open avio failed");
                    }

                    AVStream* stream = avformat_new_stream(jpegOut, NULL);
                    avcodec_parameters_copy(stream->codecpar, _inputFormatCtx->streams[videoIdx]->codecpar);
                    stream->codecpar->codec_id = AV_CODEC_ID_MJPEG;

                    ret = avformat_write_header(jpegOut, nullptr);
                    if (ret < 0)
                    {
                        av_log(NULL, AV_LOG_ERROR, "format write header failed");
                    }
                    av_interleaved_write_frame(jpegOut, pkt);
                    int ret = av_write_trailer(jpegOut);
 
                    avformat_close_input(&jpegOut);
                    av_packet_unref(pkt);
                }

                ret = avcodec_receive_packet(_encodeCtx, pkt);
                if (ret == AVERROR(EAGAIN)) {  
                    std::cout << "continue : " << __LINE__ << std::endl;
                    continue;
                }
                else if (ret != 0) {
                    fprintf(stderr, "--------Error avcodec_receive_packet ()\n");
                    break;
                }
            }

            vNextTs = vNextTs == 0 ? pkt->pts : vNextTs + pkt->duration;
            std::cout << typeStr.at(_inputFormatCtx->streams[pkt->stream_index]->codecpar->codec_type) << pkt->pts * av_q2d(timebaseIn) << " " << pkt->pts << " " << vNextTs << " " << pkt->flags << std::endl;
            pkt->pts = pkt->dts = vNextTs;


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
