#include <QtGui/QApplication>
#include <QDeclarativeContext>
#include "qmlapplicationviewer.h"
#include "ImapAccess.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QmlApplicationViewer viewer;
    viewer.setOrientation(QmlApplicationViewer::ScreenOrientationAuto);
    viewer.setMainQmlFile(QLatin1String("qml/trojita/main.qml"));
    viewer.showExpanded();

    ImapAccess imapAccess;
    QDeclarativeContext *ctxt = viewer.rootContext();
    ctxt->setContextProperty(QLatin1String("imapModel"), imapAccess.model);
    ctxt->setContextProperty(QLatin1String("msgListModel"), imapAccess.msgListModel);
    ctxt->setContextProperty(QLatin1String("mailboxModel"), imapAccess.mailboxModel);

    return app.exec();
}
