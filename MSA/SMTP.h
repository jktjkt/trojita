#ifndef SMTP_H
#define SMTP_H

#include "AbstractMSA.h"
#include "qwwsmtpclient/qwwsmtpclient.h"

namespace MSA {

class SMTP : public AbstractMSA
{
    Q_OBJECT
public:
    SMTP( QObject* parent, const QString& host, quint16 port,
          bool encryptedConnect, bool startTls, bool auth,
          const QString& user, const QString& pass );
    virtual void sendMail( const QString& from, const QStringList& to, const QByteArray& data );
public slots:
    virtual void cancel();
    void handleDone( bool ok );
    void handleError( QAbstractSocket::SocketError err, const QString& msg );
private:
    QwwSmtpClient* _qwwSmtp;
    QString _host;
    quint16 _port;
    bool _encryptedConnect;
    bool _startTls;
    bool _auth;
    QString _user;
    QString _pass;
    bool _failed;
};

}

#endif // SMTP_H
