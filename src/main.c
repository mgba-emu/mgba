#include "gba-thread.h"


#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char** argv) {
	int fd = open("test.rom", O_RDONLY);

	sigset_t signals;
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTRAP);
	pthread_sigmask(SIG_BLOCK, &signals, 0);

	struct GBAThread context;
	context.fd = fd;
	pthread_t thread;
	GBAThreadStart(&context, &thread);

	pthread_join(thread, 0);
	close(fd);

	return 0;
}
