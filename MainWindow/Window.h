#ifndef TROJITA_WINDOW_H
#define TROJITA_WINDOW_H

#include <QMainWindow>

#include "Imap/Authenticator.h"
#include "Imap/Cache.h"

class QTreeView;

namespace Imap {
namespace Mailbox {

class Model;
class MailboxModel;
class MsgListModel;

}
}

namespace Gui {

class MainWindow: public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private:
    void createDockWindows();
    void setupModels();

    Imap::Mailbox::AuthenticatorPtr authenticator;
    Imap::Mailbox::CachePtr cache;

    Imap::Mailbox::Model* model;
    Imap::Mailbox::MailboxModel* mboxModel;
    Imap::Mailbox::MsgListModel* msgListModel;

    QTreeView* mboxTree;
    QTreeView* msgListTree;
};

}

#endif
