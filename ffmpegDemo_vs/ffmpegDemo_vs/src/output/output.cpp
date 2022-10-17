#include "output.h"


bool output::openOutput(std::string filePath, std::unordered_map<const AVCodecParameters*, AVRational> streamParams)
{
    auto funcAddStream = [](AVFormatContext * ctx, const AVCodecParameters * param, AVRational timebase)->bool {

        AVStream* out_stream = avformat_new_stream(ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            return false;
        }

        int ret = avcodec_parameters_copy(out_stream->codecpar, param);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            return false;
        }

        out_stream->codecpar->codec_tag = 0;
        out_stream->time_base = timebase;
        printf("out_stream index %d type : %d\r\n", out_stream->index, param->codec_type);

        return true;
    };
    
    do {
        int ret = avformat_alloc_output_context2(&_ctx, NULL, NULL, filePath.c_str());
        if (ret < 0) {
            fprintf(stderr, "Could not create output context\n");
            break;
        }
    
        for (auto & param : streamParams) {
            funcAddStream(_ctx, param.first, param.second);
        }

        ret = avio_open(&_ctx->pb, filePath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", filePath.c_str());
            break;
        }

        ret = avformat_write_header(_ctx, NULL);
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file\n");
            break;
        }
    
        return true;
    } while (0);

    if (_ctx && !(_ctx->flags & AVFMT_NOFILE))
        avio_closep(&_ctx->pb);
    
    avformat_free_context(_ctx);

    return false;
}

bool output::writePacket(std::shared_ptr<AVPacket> pkt)
{
    if (!pkt) {
        return false;
    }
    pkt.get()->pos = -1;
    int ret = av_interleaved_write_frame(_ctx, pkt.get());
    if (ret != 0) {
        return false;
    }

    return true;
}

bool output::closeOutput()
{
    av_write_trailer(_ctx);

    if (_ctx && !(_ctx->flags & AVFMT_NOFILE))
        avio_closep(&_ctx->pb);

    avformat_free_context(_ctx);

    return false;
}
