#include <cstdio> 

#include "input.h"
#include "output.h"

int main() {
	input in;
	output outfile;

	if (in.openInput("E:\\lr\\ubuntu\\share\\lr\\in.mp4") != true) {
		return -1;
	}

	if (outfile.openOutput("E:\\lr\\ubuntu\\share\\lr\\out.mp4", in.getStreamParam()) != true) {
		return -1;
	}

	std::shared_ptr<AVPacket> pkt;
	while (1) {
		pkt = in.getPacket();
		if (pkt) {
			printf(" pts : %lld \r\n", pkt.get()->pts);
			if (pkt.get()->pts < 0) {
				continue;
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