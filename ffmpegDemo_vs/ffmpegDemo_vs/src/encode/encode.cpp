#include "encode.h"

encode::~encode()
{
    avcodec_free_context(&_ctx);
}

bool encode::initEncode(const AVStream* st)
{
    if (!st) {
        printf("stream info err\r\n");
        return false;
    }
    do {
        printf("%d \r\n", __LINE__);
        const AVCodec* c = avcodec_find_encoder(st->codecpar->codec_id);
        if (c == NULL) {
            break;
        }
        printf("%d \r\n", __LINE__);
        _ctx = avcodec_alloc_context3(c);
        if (_ctx == NULL) {
            break;
        }
        AVCodecParameters* par = st->codecpar;
        _ctx->bit_rate = par->bit_rate;
        _ctx->width = par->width;
        _ctx->height = par->height;
        _ctx->time_base = st->time_base;
        _ctx->framerate = st->r_frame_rate;
        _ctx->gop_size = 100;
        _ctx->max_b_frames = 1;
        _ctx->pix_fmt = (AVPixelFormat)par->format;
        if (c->id == AV_CODEC_ID_H264)
            av_opt_set(_ctx->priv_data, "preset", "slow", 0);


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

std::shared_ptr<AVPacket> encode::encodeFrame(std::shared_ptr<AVFrame> frm)
{
    int ret = avcodec_send_frame(_ctx, frm.get());
    if (ret != 0) {
        return nullptr;
    }

    std::shared_ptr<AVPacket> pkt(av_packet_alloc(), [&](AVPacket* p) {
        av_packet_free(&p);
    });

    ret = avcodec_receive_packet(_ctx, pkt.get());
    if (ret != 0) {
        return nullptr;
    }

	return pkt;
}
