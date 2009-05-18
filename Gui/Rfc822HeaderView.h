#ifndef GUI_RFC822HEADERVIEW_H
#define GUI_RFC822HEADERVIEW_H

#include <QLabel>

class QModelIndex;

namespace Imap {
namespace Mailbox {
class Model;
class TreeItemPart;
}
}

namespace Gui {

class Rfc822HeaderView : public QLabel
{
    Q_OBJECT
public:
    Rfc822HeaderView( QWidget* parent, Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemPart* _part );
private slots:
    void handleDataChanged(const QModelIndex&, const QModelIndex& );
    void setCorrectText();
private:
    Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemPart* part;
};

}

#endif // GUI_RFC822HEADERVIEW_H
