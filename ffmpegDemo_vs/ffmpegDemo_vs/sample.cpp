#include <cstdio> 

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

	std::shared_ptr<AVPacket> pkt;
	while (1) {
		pkt = in.getPacket();
		if (pkt) {
			printf("-----packet  pts : %lld \r\n", pkt.get()->pts);
			if (pkt.get()->pts < 0) {
				continue;
			}
			if (pkt.get()->stream_index == 0) {
				auto frm = decInput.decodePkt(pkt);
				if (frm) {
					printf("frame pts : %lld \r\n", frm.get()->pts);
				}
				else {
					printf("nullptr\r\n");
				}

				auto encPkt  = enc.encodeFrame(frm);
				if (encPkt) {
					printf("encPkt pts : %lld \r\n", encPkt.get()->pts);
				}
				else {
					printf("nullptr\r\n");
				}
			}

			if (outfile.writePacket(pkt) != true) {
				break;
			}
		}
		else {
			break;
		}
	}
	outfile.closeOutput();
}