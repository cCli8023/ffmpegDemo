#include <cstdio> 

#include "input.h"

int main() {
	input in;
	
	if (in.openInput("E:\\lr\\ubuntu\\share\\lr\\in.mp4") != true) {
		return -1;
	}

	std::shared_ptr<AVPacket> pkt;
	while (1) {
		pkt = in.getPacket();
		if (pkt) {
			printf(" pts : %lld \r\n", pkt.get()->pts);
		}
		else {
			break;
		}
	}

}