#include "input.h"

#include <fstream>

input::~input()
{
    this->close();
}

bool input::openInput(std::string url)
{

    auto funcFileVaild = [](std::string filePath)->bool {
        std::ifstream inputFile;

        inputFile.open(filePath, std::ios::in);
        if (inputFile.is_open() == false) {
            return false;
        }
        inputFile.close();
        return true;
    };

    do {
        if (!funcFileVaild(url)) {
            printf("file invailed\r\n");
            break;
        }
    
        int ret = avformat_open_input(&_ctx, url.c_str(), NULL, NULL);
        if (ret != 0) {
            printf("open input err\r\n");
            break;
        }
    
        ret = avformat_find_stream_info(_ctx, NULL);
        if (ret < 0) {
            printf("find stream err\r\n");
            break;
        }

        ret = av_find_best_stream(_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (ret > 0) {
            _videoStream = _ctx->streams[ret];
        }

        ret = av_find_best_stream(_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (ret > 0) {
            _audioStream = _ctx->streams[ret];
        }

        return true;
    } while (0);


    return false;
}

std::shared_ptr<AVPacket> input::getPacket()
{
    std::shared_ptr<AVPacket> pkt(av_packet_alloc(), [&](AVPacket* p) {
            av_packet_free(&p);
        });

    int ret = av_read_frame(_ctx, pkt.get());
    if (ret != 0) {
        return nullptr;
    }

    return pkt;
}

void input::close()
{
    if (_ctx) {
        avformat_close_input(&_ctx);
        _videoStream = _audioStream = nullptr;
    }
}

std::unordered_map<const AVCodecParameters*, AVRational> input::getStreamParam()
{
    std::unordered_map<const AVCodecParameters*, AVRational> params;

    for (int i = 0; i < _ctx->nb_streams; i++) {
        int streamType = _ctx->streams[i]->codecpar->codec_type;
        if (streamType == AVMEDIA_TYPE_VIDEO || streamType == AVMEDIA_TYPE_AUDIO) {
            params.insert(std::make_pair(_ctx->streams[i]->codecpar, _ctx->streams[i]->time_base));
        }
    }
    printf("nbstreams %d, params size : %d \r\n", _ctx->nb_streams, params.size());
    return params;
}
