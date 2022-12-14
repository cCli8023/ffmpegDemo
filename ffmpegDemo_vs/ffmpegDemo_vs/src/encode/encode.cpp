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
        printf("%d  %d \r\n", __LINE__, st->codecpar->codec_id);
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
        _ctx->max_b_frames = 0;
        _ctx->pix_fmt = (AVPixelFormat)par->format;

        if (c->id == AV_CODEC_ID_H264) {
            av_opt_set(_ctx->priv_data, "preset", "slow", 0);
            av_opt_set(_ctx->priv_data, "tune", "zerolatency", 0);
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

std::shared_ptr<AVPacket> encode::encodeFrame(std::shared_ptr<AVFrame> frm)
{
    printf("avcodec_send_frame %lld  %lld \r\n", frm->pts, frm->pkt_duration);
    int ret = avcodec_send_frame(_ctx, frm.get());
    if (ret != 0) {
        printf("avcodec_send_frame err\r\n");
        return nullptr;
    }

    std::shared_ptr<AVPacket> pkt(av_packet_alloc(), [&](AVPacket* p) {
        av_packet_free(&p);
    });

    ret = avcodec_receive_packet(_ctx, pkt.get());
    if (ret != 0) {
        printf("avcodec_receive_packet err %d %d %d %d  \r\n", ret, AVERROR(EAGAIN), AVERROR_EOF, AVERROR(EINVAL));
        return nullptr;
    }
    pkt->duration = frm->pkt_duration;

    printf("avcodec_receive_packet : %llu %llu %llu, pts \r\n",  pkt->pts, pkt->dts, pkt->duration);

	return pkt;
}

void encode::receiveOtherPkt(AVRational timebase)
{
    std::shared_ptr<AVPacket> pkt(av_packet_alloc(), [&](AVPacket* p) {
        av_packet_free(&p);
        });

    while (1) {
        int ret = avcodec_receive_packet(_ctx, pkt.get());
        if (ret != 0) {
            printf("other avcodec_receive_packet err %d %d %d %d  \r\n", ret, AVERROR(EAGAIN), AVERROR_EOF, AVERROR(EINVAL));
            return;
        }
        else {
            printf("other encPkt pts : %lld %lf\r\n", pkt.get()->pts, pkt.get()->pts * av_q2d(timebase));
        }
        av_packet_unref(pkt.get());
    }
}
