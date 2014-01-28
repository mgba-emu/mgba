#include <QApplication>
#include "Window.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QGBA::Window window;
    window.show();

    return application.exec();
}
