
#include "localdesk.h"

std::string man_text("\033[1mlocalDeskX\033[0m - Program for remote"\
					 "control of a computer in a local network. "	\
					 "(alpha)\nUsage: ldxc [Ip] [Port] [Password]"	\
					 " [Function] [Mode] [Range]\n\n"				\
					 "[Ip]\t   - Server ip address.\n"				\
					 "[Port]\t   - Server port number.\n"			\
					 "[Password] - Secret word without"				\
					 " spaces.\n[Function] - Number of one"			\
					 " of the listed functions:\n\t" 				\
					 "0  : Terminate server side process.\n\t"		\
					 "1  : Connect to the server and start "		\
					 "managing\n\t2  : Get a screenshot of "		\
					 "the desktop.\n[Mode]\t   - 0 when launched"	\
					 " in window mode, 1 when full window "			\
					 "mode.\n[Range]\t   - Compression range "		\
					 "(0 to 255).");
netw *net;

input args_read(int argc, char *argv[]) {
	struct arg {
		std::string inp; int min;
	};

	std::vector<arg> l = {
		{"--compression=",	15},
		{"--password=",		12},
		{"--fullscreen",	12},
		{"--port=",			 8},
		{"--cmd=",			 9},
		{"--ip=",			 8}
	};

	auto cmp = [&](arg &a, char *str, char **p) {
		int l1 = strlen(str);
		int l2 = a.inp.length();

		int cmp = strncmp(a.inp.c_str(), str, l2);
		*p = str + l2;

		return cmp == 0 && l1 >= a.min;
	};

	input retval;
	char *ptr;

	for (int i = 1; i < argc; i++) {
		if (cmp(l[0], argv[i], &ptr)) {
			retval.comp = (uint8_t)atoi(ptr);
		}

		else if (cmp(l[1], argv[i], &ptr)) {
			retval.pass = std::string(ptr);
		}

		else if (cmp(l[2], argv[i], &ptr)) {
			retval.full = true;
		}

		else if (cmp(l[3], argv[i], &ptr)) {
			retval.port = atoi(ptr);
		}

		else if (cmp(l[4], argv[i], &ptr)) {
			retval.cmd = std::string(ptr);
		}

		else if (cmp(l[5], argv[i], &ptr)) {
			retval.ip = std::string(ptr);
		}
	}

	return retval;
}

void recv_screenshot(input &args, byte *req) {
	
}

void start_streaming(input &args, byte *req) {
	req[1 + MD5_DIGEST_LENGTH] = args.comp;
	req[0] = 0x01;

	assert(net->send_data(req, RSIZE));

	headers hdrs;
	byte hbuff[hdrs.size];
	memset(hbuff, 0, hdrs.size);

	if (!net->recv_data(hbuff, hdrs.size)) {
		std::cout << "Error: Incorrect headers"
				  << " or password.\n";
		return;
	}

	hdrs.from(hbuff);
	x11_client x11(hdrs, args.full);

	size_t size = hdrs.width * hdrs.height
				* BSIZE;
	byte *buff = new byte[size];
	uint32_t blen;
	byte *bp = reinterpret_cast<byte *>(&blen);

	assert(buff);

	for (;;) {
		BREAK_IF(!net->recv_data(bp, U32S));
		BREAK_IF(blen == 0);

		size_t rs = blen * BSIZE;
		BREAK_IF(!net->recv_data(buff, rs));
		x11.set_pixels(buff, blen);

		// mouse & keys
		size_t evsize = x11.get_events(buff);
		evsize = evsize * 2 + U16S * 2 + 1;

		BREAK_IF(!net->send_data(buff, evsize));
	}

	delete[] buff;
}

int main(int argc, char *argv[]) {
	byte hash[MD5_DIGEST_LENGTH];
	byte req[RSIZE];

	input args = args_read(argc, argv);
	memset(hash, 0 , MD5_DIGEST_LENGTH);
	
	if (args.ip.empty() || args.port < 1) {
		std::cout << man_text << "\n";
		return 1;
	}

	byte *bpass = (byte *)args.pass.c_str();
	net = new netw(args.ip, args.port);

	MD5(bpass, args.pass.length(), hash);
	memset(req, 0, RSIZE);
	memcpy(req + 1, hash, MD5_DIGEST_LENGTH);

	assert(net);

	switch (*args.cmd.c_str()) {
	case 'e':
		net->send_data(req, RSIZE);
		break;

	case 'r':
		start_streaming(args, req);
		break;

	case 's':
		recv_screenshot(args, req);
	default: break;
	}

	delete net;
	return 0;
}

// just debug function.
void write_data(byte *buff, uint32_t blocks) {
	std::ofstream fs("./out.txt");

	assert(fs.is_open());
	fs << "Blocks: " << blocks << "\n";

	for (unsigned int i = 0; i < blocks; i++) {
		size_t shift = i * BSIZE;
		uint8_t len;
		uint8_t r, g, b;

		memcpy(&len, buff + shift, U8TS);
		r = buff[shift + U8TS];
		g = buff[shift + U8TS + 1];
		b = buff[shift + U8TS + 2];
		fs << "rgb(" << static_cast<int>(r) << ", "
		   << static_cast<int>(g) << ", "
		   << static_cast<int>(b) << "): "
		   << len << "\n";
	}

	fs.close();
}
