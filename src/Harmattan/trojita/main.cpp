#include <QtGui/QApplication>
#include <QDeclarativeContext>
#include "qmlapplicationviewer.h"
#include "ImapAccess.h"
#include "Common/SetCoreApplication.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromAscii("trojita"));
    Common::setCoreApplicationData();

    QmlApplicationViewer viewer;

    ImapAccess imapAccess;
    QDeclarativeContext *ctxt = viewer.rootContext();
    ctxt->setContextProperty(QLatin1String("imapModel"), imapAccess.model);
    ctxt->setContextProperty(QLatin1String("msgListModel"), imapAccess.msgListModel);
    ctxt->setContextProperty(QLatin1String("mailboxModel"), imapAccess.mailboxModel);

    viewer.setOrientation(QmlApplicationViewer::ScreenOrientationAuto);
    viewer.setMainQmlFile(QLatin1String("qml/trojita/main.qml"));
    viewer.showExpanded();

    return app.exec();
}
