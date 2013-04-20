#include "gba-thread.h"
#include "renderers/video-software.h"

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
	struct GBAVideoSoftwareRenderer renderer;
	context.fd = fd;
	context.renderer = 0;
	pthread_t thread;

	GBAThreadStart(&context, &thread);

	pthread_join(thread, 0);
	close(fd);

	return 0;
}
