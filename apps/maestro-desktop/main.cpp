#include <QApplication>

#include "EngineController.hpp"
#include "MainWindow.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Maestro");
    QApplication::setOrganizationName("Maestro");

    maestro::desktop::EngineController controller;
    maestro::desktop::MainWindow window(&controller);
    window.show();

    return QApplication::exec();
}
