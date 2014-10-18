#include <QApplication>
#include "Window.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName(PROJECT_NAME);
    QApplication::setApplicationVersion(PROJECT_VERSION);
    QGBA::Window window;
    window.show();

    return application.exec();
}
