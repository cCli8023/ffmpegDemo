#if 0
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/imgutils.h>
}



static void encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt,
    FILE* outfile)
{
    int ret;
    /* send the frame to the encoder */
    if (frame) {
        printf("Send frame %lld\n", frame->pts);
    }
        //printf("Send frame %3"PRIdPTR"\n", frame->pts);
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            printf("again\r\n");
            //Sleep(5000);
            return;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }
        printf("Write packet %lld (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}
int main(int argc, char** argv)
{
    const char* filename, * codec_name;
    const AVCodec* codec;
    AVCodecContext* c = NULL;
    int i, ret, x, y;
    FILE* f;
    AVFrame* frame;
    AVPacket* pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    filename = "E:\\lr\\ubuntu\\share\\lr\\demo.yuv";
    codec_name = "264";
    /* find the mpeg1video encoder */
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        exit(1);
    }
    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }
    pkt = av_packet_alloc();
    if (!pkt)
        exit(1);
    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = 352;
    c->height = 288;
    /* frames per second */
    AVRational base = { 1, 25 };
    AVRational framerate = { 25, 1 };
    c->time_base = base;
    c->framerate = framerate;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(c->priv_data, "preset", "slow"/*"superfast"*/, 0);
        av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    }
        
    /* open it */
    ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: \n" );
        exit(1);
    }
    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;
    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        exit(1);
    }
    /* encode 1 second of video */
    for (i = 0; i < 25; i++) {
        fflush(stdout);
        /* make sure the frame data is writable */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);
        /* prepare a dummy image */
        /* Y */
        for (y = 0; y < c->height; y++) {
            for (x = 0; x < c->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }
        /* Cb and Cr */
        for (y = 0; y < c->height / 2; y++) {
            for (x = 0; x < c->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }
        frame->pts = i;
        /* encode the image */
        encode(c, frame, pkt, f);
    }
    /* flush the encoder */
    encode(c, NULL, pkt, f);
    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return 0;
}











#else
#include <cstdio> 
#include <map>

#include "input.h"
#include "output.h"
#include "decode/decode.h"
#include "encode/encode.h"

int main() {
	input in;
	output outfile;
	decode decInput;
	encode enc;

	if (in.openInput("E:\\lr\\ubuntu\\share\\lr\\in.mp4") != true) {
		return -1;
	}
	printf("openInput suc\r\n");

	if (outfile.openOutput("E:\\lr\\ubuntu\\share\\lr\\out.mp4", in.getStreamParam()) != true) {
		return -1;
	}
	printf("openOutput suc\r\n");

	if (decInput.initDecode(in.videoStream()) != true) {
		return -1;
	}
	printf("initDecode suc\r\n");

	if (enc.initEncode(in.videoStream()) != true) {
		return -1;
	}
	printf("init encode suc\r\n");

	AVRational videoTimebase = in.videoStream()->time_base;
    AVRational audioTimebase = in.audioStream()->time_base;
    const int videoIdx = in.videoStream()->index;
    const int audioIdx = in.audioStream()->index;
    int64_t videoPts = 0, audioPts = 0;
    int64_t nextVideoPts = 0;
    bool reCoding = 0;
    std::map<float, float> vaildTimeSecondRange = {
        {0, 10},
        {45, 50}
    };

	std::shared_ptr<AVPacket> pkt;
    int curIdx = 0;
    auto funcInRange = [vaildTimeSecondRange](int64_t pts, AVRational timebase)->bool {
        float curTime = pts * av_q2d(timebase);

        for (const auto& pr : vaildTimeSecondRange) {
            if (curTime >= pr.first && curTime <= pr.second) {
                return true;
            }
        }

        return false;
    };
	while (1) {
		pkt = in.getPacket();
        if (pkt == nullptr) {
            //input return null, file end
            printf("[%d] input return null\r\n", __LINE__);
            break;
        }
        else {
            curIdx = pkt.get()->stream_index;

            if (curIdx == videoIdx) {
                //printf("vaild pts %lld %lld time %f %d\r\n", pkt->pts, pkt->dts, pkt->pts * av_q2d(videoTimebase), pkt->flags);
                //continue;
            }

            if (funcInRange(pkt.get()->pts, curIdx==videoIdx ? videoTimebase : audioTimebase) == false) {
                if (reCoding == true && curIdx == videoIdx) {
                
                }
                else {
                    continue;
                }
            }

            if (curIdx == videoIdx) {
                printf("vaild pts %lld %lld time %f %d\r\n", pkt->pts, pkt->dts, pkt->pts * av_q2d(videoTimebase), pkt->flags);
                //find jump point
                if (nextVideoPts !=0 && nextVideoPts < pkt->pts && reCoding != true) {
                    printf("video is jump %f\r\n", pkt->pts * av_q2d(videoTimebase));
                    reCoding = true;
                    //seek
                    in.seek(pkt->stream_index, pkt->pts, AVSEEK_FLAG_BACKWARD);
                    nextVideoPts = pkt->pts;
                    continue;
                }
                //exit recoding
                if (pkt->flags & AV_PKT_FLAG_KEY && pkt->pts >= nextVideoPts && reCoding==true) {
                    printf("exit recoding pts %lld %lld time %f %d\r\n", pkt->pts, pkt->dts, pkt->pts * av_q2d(videoTimebase), pkt->flags);
                    reCoding = false;
                }
                //recoding
                if (reCoding == true) {
                    //read pkt
                    printf("-----recoding pts %lld %lld time %f %d\r\n", pkt->pts, pkt->dts, pkt->pts * av_q2d(videoTimebase), pkt->flags);
                    // add flag
                    if (pkt->pts < nextVideoPts) {
                        pkt->flags |= AV_PKT_FLAG_DISCARD;//decode but not output frame
                        printf("-------------------------drop pts %lld time %f\r\n", pkt->pts, pkt->pts * av_q2d(videoTimebase));
                    }
                    //send to decode receive frame
                    auto frm = decInput.decodePkt(pkt);
                    if (frm == nullptr) {
                        continue;
                    }
                    //send to encode receive pkt
                    pkt = enc.encodeFrame(frm);
                    if (pkt == nullptr) {
                        continue;
                    }

                }else{
                    nextVideoPts = pkt->pts + pkt->duration;
                }
                printf("------------------write pts %lld time %f %llu\r\n", pkt->pts, pkt->pts*av_q2d(videoTimebase), pkt->duration);
                videoPts == 0 ? videoPts = pkt->pts : videoPts += pkt->duration;
                int64_t offset = pkt->pts - pkt->dts;
                pkt->pts = videoPts;
                pkt->dts = pkt->pts - offset;
                printf("write pts %llu dts %llu \r\n", pkt->pts, pkt->dts);
            }
            else if (curIdx == audioIdx) {
                audioPts == 0 ? audioPts = pkt->pts : audioPts += pkt->duration;
                pkt->dts = pkt->pts = audioPts;//audio no B frame
            }
            else {
                continue;
            }
        }
        if (outfile.writePacket(pkt) != true) {
            printf("[%d] writePacket err\r\n", __LINE__ );
            break;
        }
		if (0) {
			if (pkt.get()->stream_index == 0) {
				printf("-----packet  pts : %lld %lf  key %d\r\n", pkt.get()->pts, pkt.get()->pts * av_q2d(videoTimebase), pkt.get()->flags);
				
				auto frm = decInput.decodePkt(pkt);
				if (frm) {
					printf("frame pts : %lld %lf key %d\r\n", frm.get()->pts, frm.get()->pts * av_q2d(videoTimebase), frm.get()->key_frame);
				}
				else {
					printf("decInput nullptr\r\n");
				}

				auto encPkt  = enc.encodeFrame(frm);
				if (encPkt) {
					printf("encPkt pts : %lld %lf\r\n", encPkt.get()->pts, encPkt.get()->pts * av_q2d(videoTimebase));
				}
				else {
					printf("encodeFrame nullptr\r\n");
				}
			}
			else {
				continue;
			}

			if (outfile.writePacket(pkt) != true) {
				break;
			}
		}
	}

	outfile.closeOutput();
}
#endif