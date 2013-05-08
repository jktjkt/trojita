//
// C++ Implementation: qwwsmtpclient
//
// Description:
//
//
// Author: Witold Wysota <wysota@wysota.eu.org>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "qwwsmtpclient.h"
#include <QSslSocket>
#include <QtDebug>
#include <QQueue>
#include <QVariant>
#include <QStringList>

/*
CONNECTION ESTABLISHMENT
      S: 220
      E: 554
   EHLO or HELO
      S: 250
      E: 504, 550
   MAIL
      S: 250
      E: 552, 451, 452, 550, 553, 503
   RCPT
      S: 250, 251 (but see section 3.4 for discussion of 251 and 551)
      E: 550, 551, 552, 553, 450, 451, 452, 503, 550
   DATA
      I: 354 -> data -> S: 250
                        E: 552, 554, 451, 452
      E: 451, 554, 503
   RSET
      S: 250
   VRFY
      S: 250, 251, 252
      E: 550, 551, 553, 502, 504
   EXPN
      S: 250, 252
      E: 550, 500, 502, 504
   HELP
      S: 211, 214
      E: 502, 504
   NOOP
      S: 250
   QUIT
      S: 221
*/

struct SMTPCommand {
    enum Type { Connect, Disconnect, StartTLS, Authenticate, Mail, MailBurl, RawCommand };
    int id;
    Type type;
    QVariant data;
    QVariant extra;
};

class QwwSmtpClientPrivate {
public:
    QwwSmtpClientPrivate(QwwSmtpClient *qq) {
        q = qq;
    }
    QSslSocket *socket;

    QwwSmtpClient::State state;
    void setState(QwwSmtpClient::State s);
    void parseOption(const QString &buffer);

    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError);
    void _q_readFromSocket();
    void _q_encrypted();
    void processNextCommand(bool ok = true);
    void abortDialog();

    void sendAuthPlain(const QString &username, const QString &password);
    void sendAuthLogin(const QString &username, const QString &password, int stage);

    void sendEhlo();
    void sendHelo();
    void sendQuit();
    void sendRcpt();

    int lastId;
    bool inProgress;
    QString localName;
    QString localNameEncrypted;
    QString errorString;

    // server caps:
    QwwSmtpClient::Options options;
    QwwSmtpClient::AuthModes authModes;

    QQueue<SMTPCommand> commandqueue;
private:
    QwwSmtpClient *q;

    QwwSmtpClientPrivate(const QwwSmtpClientPrivate&); // don't implement
    QwwSmtpClientPrivate& operator=(const QwwSmtpClientPrivate&); // don't implement
};

// private slot triggered upon connection to the server
// - clears options
// - notifies the environment
void QwwSmtpClientPrivate::onConnected() {
    options = QwwSmtpClient::NoOptions;
    authModes = QwwSmtpClient::AuthNone;
    emit q->stateChanged(QwwSmtpClient::Connected);
    emit q->connected();
}

// private slot triggered upon disconnection from the server
// - checks the cause of disconnection
// - aborts or continues processing
void QwwSmtpClientPrivate::onDisconnected() {
    setState(QwwSmtpClient::Disconnected);
    if (commandqueue.isEmpty()) {
        inProgress = false;
        emit q->done(true);
        return;
    }

    if (commandqueue.head().type == SMTPCommand::Disconnect) {
        inProgress = false;
        emit q->done(true);
        return;
    }

    emit q->commandFinished(commandqueue.head().id, true);
    commandqueue.clear();
    inProgress = false;
    emit q->done(false);
}

void QwwSmtpClientPrivate::onError(QAbstractSocket::SocketError e)
{
    emit q->socketError(e, socket->errorString());
    onDisconnected();
}

// main logic of the component - a slot triggered upon data entering the socket
// comments inline...
void QwwSmtpClientPrivate::_q_readFromSocket() {
    while (socket->canReadLine()) {
        QString line = socket->readLine();
        qDebug() << "SMTP <<<" << line.toUtf8().constData();
        QRegExp rx("(\\d+)-(.*)\n");        // multiline response (aka 250-XYZ)
        QRegExp rxlast("(\\d+) (.*)\n");    // single or last line response (aka 250 XYZ)
        bool mid = rx.exactMatch(line);
        bool last = rxlast.exactMatch(line);
        // multiline
        if (mid){
            int status = rx.cap(1).toInt();
            SMTPCommand &cmd = commandqueue.head();
            switch (cmd.type) {
            // trying to connect
            case SMTPCommand::Connect: {
                    int stage = cmd.extra.toInt();
                    // stage 0 completed with success - socket is connected and EHLO was sent
                    if(stage==1 && status==250){
                        QString arg = rx.cap(2).trimmed();
                        parseOption(arg);   // we're probably receiving options
                    }
                }
                break;
            // trying to establish deferred SSL handshake
            case SMTPCommand::StartTLS: {
                    int stage = cmd.extra.toInt();
                    // stage 0 (negotiation) completed ok
                    if(stage==1 && status==250){
                        QString arg = rx.cap(2).trimmed();
                        parseOption(arg);   // we're probably receiving options
                    }
                }
                default: break;
            }
        } else
        // single line
        if (last) {
            int status = rxlast.cap(1).toInt();
            SMTPCommand &cmd = commandqueue.head();
            switch (cmd.type) {
            // trying to connect
            case SMTPCommand::Connect: {
                int stage = cmd.extra.toInt();
                // connection established, server sent its banner
                if (stage==0 && status==220) {
                    sendEhlo(); // connect ok, send ehlo
                }
                // server responded to EHLO
                if (stage==1 && status==250){
                    // success (EHLO)
                    parseOption(rxlast.cap(2).trimmed()); // we're probably receiving the last option
                    errorString.clear();
                    setState(QwwSmtpClient::Connected);
                    processNextCommand();
                }
                // server responded to HELO (EHLO failed)
                if (state==2 && status==250) {
                    // success (HELO)
                    errorString.clear();
                    setState(QwwSmtpClient::Connected);
                    processNextCommand();
                }
                // EHLO failed, reason given in errorString
                if (stage==1 && (status==554 || status==501 || status==502 || status==421)) {
                    errorString = rxlast.cap(2).trimmed();
                    sendHelo(); // ehlo failed, send helo
                    cmd.extra = 2;
                }
                //abortDialog();
            }
            break;
            // trying to establish a delayed SSL handshake
            case SMTPCommand::StartTLS: {
                int stage = cmd.extra.toInt();
                // received an invitation from the server to enter TLS mode
                if (stage==0 && status==220) {
                    qDebug() << "SMTP ** startClientEncruption";
                    socket->startClientEncryption();
                }
                // TLS established, connection is encrypted, EHLO was sent
                else if (stage==1 && status==250) {
                    setState(QwwSmtpClient::Connected);
                    parseOption(rxlast.cap(2).trimmed());   // we're probably receiving options
                    errorString.clear();
                    emit q->tlsStarted();
                    processNextCommand();
                }
                // starttls failed
                else {
                    qDebug() << "TLS failed at stage " << stage << ": " << line;
                    errorString = "TLS failed";
                    emit q->done(false);
                }
            }
            break;
            // trying to authenticate the client to the server
            case SMTPCommand::Authenticate: {
                int stage = cmd.extra.toInt();
                if (stage==0 && status==334) {
                    // AUTH mode was accepted by the server, 1st challenge sent
                    QwwSmtpClient::AuthMode authmode = (QwwSmtpClient::AuthMode)cmd.data.toList().at(0).toInt();
                    errorString.clear();
                    switch (authmode) {
                    case QwwSmtpClient::AuthPlain:
                        sendAuthPlain(cmd.data.toList().at(1).toString(), cmd.data.toList().at(2).toString());
                        break;
                    case QwwSmtpClient::AuthLogin:
                        sendAuthLogin(cmd.data.toList().at(1).toString(), cmd.data.toList().at(2).toString(), 1);
                        break;
                    default:
                        qWarning("I shouldn't be here");
                        setState(QwwSmtpClient::Connected);
                        processNextCommand();
                        break;
                    }
                    cmd.extra = stage+1;
                } else if (stage==1 && status==334) {
                    // AUTH mode and user names were acccepted by the server, 2nd challenge sent
                    QwwSmtpClient::AuthMode authmode = (QwwSmtpClient::AuthMode)cmd.data.toList().at(0).toInt();
                    errorString.clear();
                    switch (authmode) {
                    case QwwSmtpClient::AuthPlain:
                        // auth failed
                        setState(QwwSmtpClient::Connected);
                        processNextCommand();
                        break;
                    case QwwSmtpClient::AuthLogin:
                        sendAuthLogin(cmd.data.toList().at(1).toString(), cmd.data.toList().at(2).toString(), 2);
                        break;
                    default:
                        qWarning("I shouldn't be here");
                        setState(QwwSmtpClient::Connected);
                        processNextCommand();
                        break;
                    }
                } else if (stage==2 && status==334) {
                    // auth failed
                    errorString = rxlast.cap(2).trimmed();
                    setState(QwwSmtpClient::Connected);
                    processNextCommand();
                } else if (status==235) {
                    // auth ok
                    errorString.clear();
                    emit q->authenticated();
                    setState(QwwSmtpClient::Connected);
                    processNextCommand();
                } else {
                    errorString = rxlast.cap(2).trimmed();
                    setState(QwwSmtpClient::Connected);
                    emit q->done(false);
                }
            }
            break;
            // trying to send mail
            case SMTPCommand::Mail:
            case SMTPCommand::MailBurl:
            {
                int stage = cmd.extra.toInt();
                // temporary failure upon receiving the sender address (greylisting probably)
                if (status==421 && stage==0) {
                    errorString = rxlast.cap(2).trimmed();
                    // temporary envelope failure (greylisting)
                    setState(QwwSmtpClient::Connected);
                    processNextCommand(false);
                }
                if (status==250 && stage==0) {
                    // sender accepted
                    errorString.clear();
                    sendRcpt();
                } else if (status==250 && stage==1) {
                    // all receivers accepted
                    if (cmd.type == SMTPCommand::MailBurl) {
                        errorString.clear();
                        QByteArray url = cmd.data.toList().at(2).toByteArray();
                        qDebug() << "SMTP >>> BURL" << url << "LAST";
                        socket->write("BURL " + url + " LAST\r\n");
                        cmd.extra=2;
                    } else {
                        errorString.clear();
                        qDebug() << "SMTP >>> DATA";
                        socket->write("DATA\r\n");
                        cmd.extra=2;
                    }
                } else if ((cmd.type == SMTPCommand::Mail && status==354 && stage==2)) {
                    // DATA command accepted
                    errorString.clear();
                    QByteArray toBeWritten = cmd.data.toList().at(2).toString().toUtf8();
                    qDebug() << "SMTP >>>" << toBeWritten << "\r\n.\r\n";
                    socket->write(toBeWritten); // expecting data to be already escaped (CRLF.CRLF)
                    socket->write("\r\n.\r\n"); // termination token - CRLF.CRLF
                    cmd.extra=3;
                } else if ((cmd.type == SMTPCommand::MailBurl && status==250 && stage==2)) {
                    // BURL succeeded
                    setState(QwwSmtpClient::Connected);
                    errorString.clear();
                    processNextCommand();
                } else if ((cmd.type == SMTPCommand::Mail && status==250 && stage==3)) {
                    // mail queued
                    setState(QwwSmtpClient::Connected);
                    errorString.clear();
                    processNextCommand();
                } else {
                    // something went wrong
                    errorString = rxlast.cap(2).trimmed();
                    setState(QwwSmtpClient::Connected);
                    emit q->done(false);
                    processNextCommand();
                }
            }
                default: break;
            }
        } else {
            qDebug() << "None of two regular expressions matched the input" << line;
        }
    }
}

void QwwSmtpClientPrivate::setState(QwwSmtpClient::State s) {
    QwwSmtpClient::State old = state;
    state = s;
    emit q->stateChanged(s);
    if (old == QwwSmtpClient::Connecting && s==QwwSmtpClient::Connected) emit q->connected();
    if (s==QwwSmtpClient::Disconnected) emit q->disconnected();
}

void QwwSmtpClientPrivate::processNextCommand(bool ok) {
    if (inProgress && !commandqueue.isEmpty()) {
        emit q->commandFinished(commandqueue.head().id, !ok);
        commandqueue.dequeue();
    }
    if (commandqueue.isEmpty()) {
        inProgress = false;
        emit q->done(false);
        return;
    }
    const SMTPCommand &cmd = commandqueue.head();
    switch (cmd.type) {
    case SMTPCommand::Connect: {
        QString hostName = cmd.data.toList().at(0).toString();
        uint port = cmd.data.toList().at(1).toUInt();
        bool ssl = cmd.data.toList().at(2).toBool();
        if(ssl){
            qDebug() << "SMTP ** connectToHostEncrypted";
            socket->connectToHostEncrypted(hostName, port);
        } else {
            qDebug() << "SMTP ** connectToHost";
            socket->connectToHost(hostName, port);
        }
        setState(QwwSmtpClient::Connecting);
    }
    break;
    case SMTPCommand::Disconnect: {
        sendQuit();
    }
    break;
    case SMTPCommand::StartTLS: {
        qDebug() << "SMTP >>> STARTTLS";
        socket->write("STARTTLS\r\n");
        setState(QwwSmtpClient::TLSRequested);
    }
    break;
    case SMTPCommand::Authenticate: {
        QwwSmtpClient::AuthMode authmode = (QwwSmtpClient::AuthMode)cmd.data.toList().at(0).toInt();
        switch (authmode) {
        case QwwSmtpClient::AuthPlain:
            qDebug() << "SMTP >>> AUTH PLAIN";
            socket->write("AUTH PLAIN\r\n");
            setState(QwwSmtpClient::Authenticating);
            break;
        case QwwSmtpClient::AuthLogin:
            qDebug() << "SMTP >>> AUTH LOGIN";
            socket->write("AUTH LOGIN\r\n");
            setState(QwwSmtpClient::Authenticating);
            break;
        default:
            qWarning("Unsupported or unknown authentication scheme");
            //processNextCommand(false);
        }
    }
    break;
    case SMTPCommand::Mail:
    case SMTPCommand::MailBurl:
    {
        setState(QwwSmtpClient::Sending);
        QByteArray buf = QByteArray("MAIL FROM:<").append(cmd.data.toList().at(0).toByteArray()).append(">\r\n");
        qDebug() << "SMTP >>>" << buf;
        socket->write(buf);
        break;
    }
    case SMTPCommand::RawCommand: {
	QString cont = cmd.data.toString();
	if(!cont.endsWith("\r\n")) cont.append("\r\n");
    setState(QwwSmtpClient::Sending);
    qDebug() << "SMTP >>>" << cont;
	socket->write(cont.toUtf8());
	} break;
    }
    inProgress = true;
    emit q->commandStarted(cmd.id);
}

void QwwSmtpClientPrivate::_q_encrypted() {
        options = QwwSmtpClient::NoOptions;
    // forget everything, restart ehlo
//    SMTPCommand &cmd = commandqueue.head();
    sendEhlo();
}


void QwwSmtpClientPrivate::sendEhlo() {
    SMTPCommand &cmd = commandqueue.head();
    QString domain = localName;
    if (socket->isEncrypted() && !localNameEncrypted.isEmpty())
        domain = localNameEncrypted;
    QByteArray buf = QString("EHLO "+domain+"\r\n").toUtf8();
    qDebug() << "SMTP >>>" << buf;
    socket->write(buf);
    cmd.extra = 1;
}


void QwwSmtpClientPrivate::sendHelo() {
    SMTPCommand &cmd = commandqueue.head();
    QString domain = localName;
    if (socket->isEncrypted() && localNameEncrypted.isEmpty())
        domain = localNameEncrypted;
    QByteArray buf = QString("HELO "+domain+"\r\n").toUtf8();
    qDebug() << "SMTP >>>" << buf;
    socket->write(buf);
    cmd.extra = 1;
}


void QwwSmtpClientPrivate::sendQuit() {
    qDebug() << "SMTP >>> QUIT";
    socket->write("QUIT\r\n");
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();
    setState(QwwSmtpClient::Disconnecting);
}

void QwwSmtpClientPrivate::sendRcpt() {
    SMTPCommand &cmd = commandqueue.head();
    QVariantList vlist = cmd.data.toList();
    QList<QVariant> rcptlist = vlist.at(1).toList();
    QByteArray buf = QByteArray("RCPT TO:<").append(rcptlist.first().toByteArray()).append(">\r\n");
    qDebug() << "SMTP >>>" << buf;
    socket->write(buf);
    rcptlist.removeFirst();
    vlist[1] = rcptlist;
    cmd.data = vlist;

    if (rcptlist.isEmpty()) cmd.extra = 1;
}



void QwwSmtpClientPrivate::sendAuthPlain(const QString & username, const QString & password) {
    QByteArray ba;
    ba.append('\0');
    ba.append(username.toUtf8());
    ba.append('\0');
    ba.append(password.toUtf8());
    QByteArray encoded = ba.toBase64();
    qDebug() << "SMTP <<< [authentication data]";
    socket->write(encoded);
    socket->write("\r\n");
}

void QwwSmtpClientPrivate::sendAuthLogin(const QString & username, const QString & password, int stage) {
    if (stage==1) {
        qDebug() << "SMTP <<< [login username]";
        socket->write(username.toUtf8().toBase64());
        socket->write("\r\n");
    } else if (stage==2) {
        qDebug() << "SMTP <<< [login password]";
        socket->write(password.toUtf8().toBase64());
        socket->write("\r\n");
    }
}

void QwwSmtpClientPrivate::parseOption(const QString &buffer){
    if(buffer.toLower()=="pipelining"){                     options |= QwwSmtpClient::PipeliningOption;     }
    else if(buffer.toLower()=="starttls"){                  options |= QwwSmtpClient::StartTlsOption;       }
    else if(buffer.toLower()=="8bitmime"){                  options |= QwwSmtpClient::EightBitMimeOption;   }
    else if(buffer.toLower().startsWith("auth ")){          options |= QwwSmtpClient::AuthOption;
        // parse auth modes
        QStringList slist = buffer.mid(5).split(" ");
        foreach(const QString &s, slist){
            if(s.toLower()=="plain"){
                authModes |= QwwSmtpClient::AuthPlain;
            }
            if(s.toLower()=="login"){
                authModes |= QwwSmtpClient::AuthLogin;
            }
        }
    }
}


QwwSmtpClient::QwwSmtpClient(QObject *parent)
        : QObject(parent), d(new QwwSmtpClientPrivate(this)) {
    d->state = Disconnected;
    d->lastId = 0;
    d->inProgress = false;
    d->localName = "localhost";
    d->socket = new QSslSocket(this);
    connect(d->socket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(d->socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)) );
    connect(d->socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(d->socket, SIGNAL(readyRead()), this, SLOT(_q_readFromSocket()));
    connect(d->socket, SIGNAL(sslErrors(const QList<QSslError> &)), this, SIGNAL(sslErrors(const QList<QSslError>&)));
    connect(d->socket, SIGNAL(encrypted()), this, SLOT(_q_encrypted()));
}


QwwSmtpClient::~QwwSmtpClient() {
    delete d;
}

int QwwSmtpClient::connectToHost(const QString & hostName, quint16 port) {
    SMTPCommand cmd;
    cmd.type = SMTPCommand::Connect;
    cmd.data = QVariantList() << hostName << port << false;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

// int QwwSmtpClient::connectToHost(const QHostAddress & address, quint16 port) {
//     d->socket->connectToHost(address, port);
//     d->setState(Connecting);
// }


int QwwSmtpClient::connectToHostEncrypted(const QString & hostName, quint16 port)
{
    SMTPCommand cmd;
    cmd.type = SMTPCommand::Connect;
    cmd.data = QVariantList() << hostName << port << true;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if(!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

int QwwSmtpClient::disconnectFromHost() {
    SMTPCommand cmd;
    cmd.type = SMTPCommand::Disconnect;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

int QwwSmtpClient::startTls() {
    SMTPCommand cmd;
    cmd.type = SMTPCommand::StartTLS;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

void QwwSmtpClient::setLocalName(const QString & ln) {
    d->localName = ln;
}

void QwwSmtpClient::setLocalNameEncrypted(const QString & ln) {
    d->localNameEncrypted = ln;
}


int QwwSmtpClient::authenticate(const QString &user, const QString &password, AuthMode mode) {
    if(mode==AuthAny){
    	// negotiate
    	mode = AuthLogin;
    }
    SMTPCommand cmd;
    cmd.type = SMTPCommand::Authenticate;
    cmd.data = QVariantList() << (int)mode << user << password;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

int QwwSmtpClient::sendMail(const QString & from, const QString & to, const QString & content) {
    /* Any code sending to addresses that are still character-strings instead of
       byte strings is probably buggy, but we might as well have a convenience
       method here for it anyway. This method will frequently not do the right
       thing, but it will do the wrong thing conveniently. */
    QList<QByteArray> rcpts;
    rcpts.append(to.toUtf8());
    return sendMail(from.toUtf8(), rcpts, content);
}

int QwwSmtpClient::sendMail(const QByteArray &from, const QList<QByteArray> &to, const QString &content)
{
    QList<QVariant> rcpts;
    for(QList<QByteArray>::const_iterator it = to.begin(); it != to.end(); it ++) {
        rcpts.append(QVariant(*it));
    }
    SMTPCommand cmd;
    cmd.type = SMTPCommand::Mail;
    cmd.data = QVariantList() << from << QVariant(rcpts) << content;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

int QwwSmtpClient::sendMailBurl(const QByteArray &from, const QList<QByteArray> &to, const QString &url)
{
    QList<QVariant> rcpts;
    for(QList<QByteArray>::const_iterator it = to.begin(); it != to.end(); it ++) {
        rcpts.append(QVariant(*it));
    }
    SMTPCommand cmd;
    cmd.type = SMTPCommand::MailBurl;
    cmd.data = QVariantList() << from << QVariant(rcpts) << url;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}

int QwwSmtpClient::rawCommand(const QString & raw) {
    SMTPCommand cmd;
    cmd.type = SMTPCommand::RawCommand;
    cmd.data = raw;
    cmd.id = ++d->lastId;
    d->commandqueue.enqueue(cmd);
    if (!d->inProgress)
        d->processNextCommand();
    return cmd.id;
}


void QwwSmtpClientPrivate::abortDialog() {
    emit q->commandFinished(commandqueue.head().id, true);
    commandqueue.clear();
    sendQuit();
}

void QwwSmtpClient::ignoreSslErrors()
{d->socket->ignoreSslErrors();
}

QwwSmtpClient::AuthModes QwwSmtpClient::supportedAuthModes() const{
    return d->authModes;
}

QwwSmtpClient::Options QwwSmtpClient::options() const{
    return d->options;
}

QString QwwSmtpClient::errorString() const{
    return d->errorString;
}

#include "moc_qwwsmtpclient.cpp"
