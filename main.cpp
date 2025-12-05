#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // register custom data type
    qRegisterMetaType<SpectrumData>("SpectrumData");

    MainWindow window;
    window.showFullScreen();  // For BeagleBone display

    return app.exec();
}
