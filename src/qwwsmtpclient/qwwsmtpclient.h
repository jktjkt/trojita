//
// C++ Interface: qwwsmtpclient
//
// Description:
//
//
// Author: Witold Wysota <wysota@wysota.eu.org>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef QWWSMTPCLIENT_H
#define QWWSMTPCLIENT_H

#include <QObject>
#include <QHostAddress>
#include <QString>
#include <QSslError>

class QwwSmtpClientPrivate;

/*!
        \class  QwwSmtpClient
        \author Witold Wysota <wysota@wysota.eu.org>
        \brief  Cross-platform asynchronous handling of client side SMTP connections

                Features:

                Connection mode - open, TLS, SSL
                Authentication  - PLAIN, LOGIN
                Handshake       - HELO, EHLO

                - low-level mail sending (everything you pass, goes through to the server)
                - raw command sending
                - multiple rcpt
                - option reporting

       \todo    CRAM-MD5 Authentication
                VRFY
                abort()
                SSL errors handling
                network errors
                error handling (status codes, etc.)

*/
class QwwSmtpClient : public QObject {
    Q_OBJECT
    Q_ENUMS(State);
    Q_FLAGS(Options);
    Q_ENUMS(AuthMode);
    Q_FLAGS(AuthModes);

public:
    explicit QwwSmtpClient(QObject *parent = 0);
    ~QwwSmtpClient();
    enum State { Disconnected, Connecting, Connected, TLSRequested, Authenticating, Sending, Disconnecting };
    enum Option { NoOptions = 0, StartTlsOption, SizeOption, PipeliningOption, EightBitMimeOption, AuthOption };
    Q_DECLARE_FLAGS ( Options, Option );
    enum AuthMode { AuthNone, AuthAny, AuthPlain, AuthLogin };
    Q_DECLARE_FLAGS ( AuthModes, AuthMode );
    void setLocalName(const QString &ln);
    void setLocalNameEncrypted(const QString &ln);

    int connectToHost ( const QString & hostName, quint16 port = 25);
    int connectToHostEncrypted(const QString &hostName, quint16 port = 465);
//     int connectToHost ( const QHostAddress & address, quint16 port = 25);
    int authenticate(const QString &user, const QString &password, AuthMode mode = AuthAny);
    int sendMail(const QString &from, const QString &to, const QString &content);
    int sendMail(const QByteArray &from, const QList<QByteArray> &to, const QString &content);
    int sendMailBurl(const QByteArray &from, const QList<QByteArray> &to, const QString &url);
    int rawCommand(const QString &cmd);
    AuthModes supportedAuthModes() const;
    Options options() const;
    QString errorString() const;
public slots:
    int disconnectFromHost();
    int startTls();
    void ignoreSslErrors();

signals:
    void done(bool);
    void connected();
    void disconnected();
    void stateChanged(State);
    void commandFinished(int, bool error);
    void commandStarted(int);
    void tlsStarted();
    void authenticated();
    void rawCommandReply(int code, const QString &details);
    void sslErrors(const QList<QSslError> &);
    void socketError(QAbstractSocket::SocketError err, const QString& message);

private:
    QwwSmtpClientPrivate *d;
    Q_PRIVATE_SLOT(d, void onConnected());
    Q_PRIVATE_SLOT(d, void onDisconnected());
    Q_PRIVATE_SLOT(d, void onError(QAbstractSocket::SocketError));
    Q_PRIVATE_SLOT(d, void _q_readFromSocket());
    Q_PRIVATE_SLOT(d, void _q_encrypted());
    friend class QwwSmtpClientPrivate;

    QwwSmtpClient(const QwwSmtpClient&); // don't implement
    QwwSmtpClient& operator=(const QwwSmtpClient&); // don't implement
};

#endif
