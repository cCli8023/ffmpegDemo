#include "decode.h"

decode::~decode()
{
    avcodec_free_context(&_ctx);
}

bool decode::initDecode(const AVStream * st)
{
    if (!st) {
        printf("stream info err\r\n");
        return false;
    }
    do {
        printf("%d \r\n", __LINE__);
        const AVCodec* c = avcodec_find_decoder(st->codecpar->codec_id);
        if (c == NULL) {
            break;
        }
        printf("%d \r\n", __LINE__);
        _ctx = avcodec_alloc_context3(c);
        if (_ctx == NULL) {
            break;
        }

        if ( avcodec_parameters_to_context(_ctx, st->codecpar)  < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            break;
        }

        printf("%d \r\n", __LINE__);
        int ret = avcodec_open2(_ctx, c, NULL);
        if (ret != 0) {
            break;
        }
        printf("init codec ok\r\n");
        return true;

    } while (0);

    printf("init codec err\r\n");
    avcodec_free_context(&_ctx);
    return false;
}

std::shared_ptr<AVFrame> decode::decodePkt(std::shared_ptr<AVPacket> pkt)
{
    int ret = avcodec_send_packet(_ctx, pkt.get());
    if (ret != 0) {
        printf("send frame err %d %d %d %d %d\r\n", ret, AVERROR(EAGAIN), AVERROR_EOF,  AVERROR(EINVAL), AVERROR(ENOMEM));
        return nullptr;
    }

    std::shared_ptr<AVFrame> frm(av_frame_alloc(), [&](AVFrame* p) {
            av_frame_free(&p);
    });
    ret = avcodec_receive_frame(_ctx, frm.get());
    if (ret != 0) {
        printf("receive frame err %d %d %d %d\r\n", ret == AVERROR(EAGAIN), ret == AVERROR_EOF, ret == AVERROR(EINVAL), ret == AVERROR(ENOMEM));
        return nullptr;
    }

    return frm;
}
