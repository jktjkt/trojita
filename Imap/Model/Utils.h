#ifndef IMAP_MAILBOX_UTILS_H
#define IMAP_MAILBOX_UTILS_H

#include <QObject>
#include <QString>

namespace Imap {
namespace Mailbox {

class PrettySize: public QObject {
    Q_OBJECT
public:
    static QString prettySize( uint bytes );
};

}
}

#endif // IMAP_MAILBOX_UTILS_H
