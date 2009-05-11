#include "SMTP.h"

namespace MSA {

SMTP::SMTP( QObject* parent, const QString& host, quint16 port,
          bool encryptedConnect, bool startTls, bool auth,
          const QString& user, const QString& pass ):
AbstractMSA(parent), _host(host), _port(port),
_encryptedConnect(encryptedConnect), _startTls(startTls), _auth(auth),
_user(user), _pass(pass)
{
    _qwwSmtp = new QwwSmtpClient( this );
    // FIXME: handle SSL errors properly
    connect( _qwwSmtp, SIGNAL(sslErrors(QList<QSslError>)), _qwwSmtp, SLOT(ignoreSslErrors()) );
    connect( _qwwSmtp, SIGNAL(connected()), this, SIGNAL(sending()) );
    connect( _qwwSmtp, SIGNAL(done(bool)), this, SLOT(handleDone(bool)) );
}

void SMTP::cancel()
{
    _qwwSmtp->disconnectFromHost();
}

void SMTP::handleDone( bool ok )
{
    qDebug() << "handleDone" << ok;
    if ( ok )
        emit sent();
    else
        emit error( "blesmrt" );
}

void SMTP::sendMail( const QStringList& to, const QByteArray& data )
{
    emit progressMax( data.size() );
    emit progress( 0 );
    emit connecting();
    if ( _encryptedConnect )
        _qwwSmtp->connectToHostEncrypted( _host, _port );
    else
        _qwwSmtp->connectToHost( _host, _port );
    if ( _startTls )
        _qwwSmtp->startTls();
    if ( _auth )
        _qwwSmtp->authenticate( _user, _pass );
    emit sending(); // FIXME: later
    _qwwSmtp->sendMail( /*FIXME*/ "kundratj@fzu.cz", to, data );
    _qwwSmtp->disconnectFromHost();
}

}

#include "SMTP.moc"
