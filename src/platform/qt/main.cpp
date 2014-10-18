#include <QApplication>
#include "Window.h"

extern "C" {
#include "platform/commandline.h"
}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(PROJECT_NAME);
    QApplication::setApplicationVersion(PROJECT_VERSION);

    QGBA::Window window;

	struct StartupOptions opts;
	if (parseCommandArgs(&opts, argc, argv, 0)) {
		window.optionsPassed(&opts);
	}
    window.show();

    int rcode = application.exec();
	freeOptions(&opts);
    return rcode;
}
