// This source file is placed into the public domain.
#include <mgba/core/core.h>
#include <mgba/feature/commandline.h>
#include <mgba-util/socket.h>

#define DEFAULT_PORT 13721

static bool _mExampleRun(const struct mArguments* args, Socket client);
static void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args);

static int _logLevel = 0;

int main(int argc, char** argv) {
	bool didFail = false;

	// Arguments from the command line are parsed by the parseArguments function.
	// The NULL here shows that we don't give it any arguments beyond the default ones.
	struct mArguments args = {};
	bool parsed = mArgumentsParse(&args, argc, argv, NULL, 0);
	// Parsing can succeed without finding a filename, but we need one.
	if (!args.fname) {
		parsed = false;
	}
	if (!parsed || args.showHelp) {
		// If parsing failed, or the user passed --help, show usage.
		usage(argv[0], NULL, NULL, NULL, 0);
		didFail = !parsed;
		goto cleanup;
	}

	if (args.showVersion) {
		// If the user passed --version, show version.
		version(argv[0]);
		goto cleanup;
	}

	// Set up a logger. The default logger prints everything to STDOUT, which is not usually desirable.
	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);

	// Initialize the socket layer and listen on the default port for this protocol.
	SocketSubsystemInit();
	Socket sock = SocketOpenTCP(DEFAULT_PORT, NULL);
	if (SOCKET_FAILED(sock) || SOCKET_FAILED(SocketListen(sock, 0))) {
		SocketSubsystemDeinit();
		didFail = true;
		goto cleanup;
	}

	// We only grab one client.
	Socket client = SocketAccept(sock, NULL);
	if (SOCKET_FAILED(client)) {
		SocketClose(sock);
		SocketSubsystemDeinit();
		didFail = true;
		goto cleanup;		
	}

	// Run the server
	didFail = _mExampleRun(&args, client);

	// Clean up the sockets.
	SocketClose(client);
	SocketClose(sock);
	SocketSubsystemDeinit();

	cleanup:
	mArgumentsDeinit(&args);

	return didFail;
}

bool _mExampleRun(const struct mArguments* args, Socket client) {
	// First, we need to find the mCore that's appropriate for this type of file.
	// If one doesn't exist, it returns NULL and we can't continue.
	struct mCore* core = mCoreFind(args->fname);
	if (!core) {
		return false;
	}

	// Initialize the received core.
	core->init(core);

	// Get the dimensions required for this core and send them to the client.
	unsigned width, height;
	core->baseVideoSize(core, &width, &height);
	ssize_t bufferSize = width * height * BYTES_PER_PIXEL;
	uint32_t sendNO;
	sendNO = htonl(width);
	SocketSend(client, &sendNO, sizeof(sendNO));
	sendNO = htonl(height);
	SocketSend(client, &sendNO, sizeof(sendNO));
	sendNO = htonl(BYTES_PER_PIXEL);
	SocketSend(client, &sendNO, sizeof(sendNO));

	// Create a video buffer and tell the core to use it.
	// If a core isn't told to use a video buffer, it won't render any graphics.
	// This may be useful in situations where everything except for displayed
	// output is desired.
	void* videoOutputBuffer = malloc(bufferSize);
	core->setVideoBuffer(core, videoOutputBuffer, width);

	// Tell the core to actually load the file.
	mCoreLoadFile(core, args->fname);

	// Initialize the configuration system and load any saved settings for
	// this frontend. The second argument to mCoreConfigInit should either be
	// the name of the frontend, or NULL if you're not loading any saved
	// settings from disk.
	mCoreConfigInit(&core->config, "client-server");
	mCoreConfigLoad(&core->config);

	// Take any settings overrides from the command line and make sure they get
	// loaded into the config system, as well as manually overriding the
	// "idleOptimization" setting to ensure cores that can detect idle loops
	// will attempt the detection.
	mArgumentsApply(args, NULL, 0, &core->config);
	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "detect");

	// Tell the core to apply the configuration in the associated config object.
	mCoreLoadConfig(core);

	// Set our logging level to be the logLevel in the configuration object.
	mCoreConfigGetIntValue(&core->config, "logLevel", &_logLevel);

	// Reset the core. This is needed before it can run.
	core->reset(core);

	uint16_t inputNO;
	while (SocketRecv(client, &inputNO, sizeof(inputNO)) == sizeof(inputNO)) {
		// After receiving the keys from the client, tell the core that these are
		// the keys for the current input.
		core->setKeys(core, ntohs(inputNO));

		// Emulate a single frame.
		core->runFrame(core);

		// Send back the video buffer.
		if (SocketSend(client, videoOutputBuffer, bufferSize) != bufferSize) {
			break;
		}
	}

	// Deinitialization associated with the core.
	mCoreConfigDeinit(&core->config);
	core->deinit(core);

	return true;
}

void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	// We don't need the logging object, so we call UNUSED to ensure there's no warning.
	UNUSED(log);
	// The level parameter is a bitmask that we can easily filter.
	if (level & _logLevel) {
		// Categories are registered at runtime, but the name can be found
		// through a simple lookup.
		printf("%s: ", mLogCategoryName(category));

		// We get a format string and a varargs context from the core, so we
		// need to use the v* version of printf.
		vprintf(format, args);

		// The format strings do NOT include a newline, so we need to
		// append it ourself.
		putchar('\n');
	}
}
