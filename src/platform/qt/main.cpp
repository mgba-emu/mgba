#include "GBAApp.h"
#include "Window.h"

int main(int argc, char* argv[]) {
    QGBA::GBAApp application(argc, argv);
    return application.exec();
}
