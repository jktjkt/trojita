#ifndef MSGPARTNETACCESSMANAGER_H
#define MSGPARTNETACCESSMANAGER_H

#include <QNetworkAccessManager>

namespace Imap {
namespace Network {

class MsgPartNetAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    MsgPartNetAccessManager();
};

}
}
#endif // MSGPARTNETACCESSMANAGER_H
