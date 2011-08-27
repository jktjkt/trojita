#include <QtGui/QApplication>
#include <QtDeclarative>
#include "Common/SetCoreApplication.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromAscii("trojita"));
    Common::setCoreApplicationData();
    QDeclarativeView view;
    view.setSource(QUrl("qrc:/qml/main.qml"));
    view.showFullScreen();
    return app.exec();
}
