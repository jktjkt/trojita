/*
    Certain enhancements (www.xtuple.com/trojita-enhancements)
    are copyright © 2010 by OpenMFG LLC, dba xTuple.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    - Neither the name of xTuple nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior
    written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "XtConnect.h"
#include <QAuthenticator>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include "Common/SettingsNames.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/MemoryCache.h"
#include "MailSynchronizer.h"
#include "MailboxFinder.h"

namespace XtConnect {

XtConnect::XtConnect(QObject *parent, QSettings *s) :
    QObject(parent), m_model(0), m_settings(s)
{
    Q_ASSERT(m_settings);
    m_settings->setParent(this);
    if ( ! m_settings->contains( Common::SettingsNames::xtConnectCacheDirectory ) ) {
        qFatal("The service is not configured yet. Please use the Trojita GUI for configuration.");
    }
    setupModels();

    // Prepare the mailboxes
    m_finder = new MailboxFinder( this, m_model );
    Q_FOREACH( const QString &mailbox, s->value( Common::SettingsNames::xtSyncMailboxList ).toStringList() ) {
        MailSynchronizer *sync = new MailSynchronizer( this, m_model, m_finder );
        sync->setMailbox( mailbox );
    }
}

void XtConnect::setupModels()
{
    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TaskFactory() );

    using Common::SettingsNames;
    if ( m_settings->value( SettingsNames::imapMethodKey ).toString() == SettingsNames::methodTCP ) {
        factory.reset( new Imap::Mailbox::TlsAbleSocketFactory(
                m_settings->value( SettingsNames::imapHostKey ).toString(),
                m_settings->value( SettingsNames::imapPortKey ).toUInt() ) );
        factory->setStartTlsRequired( m_settings->value( SettingsNames::imapStartTlsKey, true ).toBool() );
    } else if ( m_settings->value( SettingsNames::imapMethodKey ).toString() == SettingsNames::methodSSL ) {
        factory.reset( new Imap::Mailbox::SslSocketFactory(
                m_settings->value( SettingsNames::imapHostKey ).toString(),
                m_settings->value( SettingsNames::imapPortKey ).toUInt() ) );
    } else {
        QStringList args = m_settings->value( SettingsNames::imapProcessKey ).toString().split( QLatin1Char(' ') );
        if ( args.isEmpty() ) {
            qFatal("Invalid value found in the settings of imapProcessKey");
        }
        QString appName = args.takeFirst();
        factory.reset( new Imap::Mailbox::ProcessSocketFactory( appName, args ) );
    }

    QString cacheDir = m_settings->value( Common::SettingsNames::xtConnectCacheDirectory).toString();
    Imap::Mailbox::AbstractCache* cache = 0;

    bool shouldUsePersistentCache = false; // FIXME: enable later

    if ( ! QDir().mkpath( cacheDir ) ) {
        qCritical() << "Failed to create directory" << cacheDir << " -- will not remember anything on restart!";
        shouldUsePersistentCache = false;
    }

    if ( ! shouldUsePersistentCache ) {
        cache = new Imap::Mailbox::MemoryCache( this, QString() );
    } else {
        cache = new Imap::Mailbox::CombinedCache( this, QLatin1String("trojita-imap-cache"), cacheDir );
        connect( cache, SIGNAL(error(QString)), this, SLOT(cacheError(QString)) );
        if ( ! static_cast<Imap::Mailbox::CombinedCache*>( cache )->open() ) {
            // Error message was already shown by the cacheError() slot
            cache->deleteLater();
            cache = new Imap::Mailbox::MemoryCache( this, QString() );
        }
    }
    m_model = new Imap::Mailbox::Model( this, cache, factory, taskFactory, m_settings->value( SettingsNames::imapStartOffline ).toBool() );
    m_model->setObjectName( QLatin1String("model") );

    connect( m_model, SIGNAL( alertReceived( const QString& ) ), this, SLOT( alertReceived( const QString& ) ) );
    connect( m_model, SIGNAL( connectionError( const QString& ) ), this, SLOT( connectionError( const QString& ) ) );
    connect( m_model, SIGNAL( authRequested( QAuthenticator* ) ), this, SLOT( authenticationRequested( QAuthenticator* ) ) );
    connect( m_model, SIGNAL(connectionStateChanged(QObject*,Imap::ConnectionState)), this, SLOT(showConnectionStatus(QObject*,Imap::ConnectionState)) );

    // Actually bring us online
    m_model->rowCount( QModelIndex() );
}

void XtConnect::alertReceived(const QString &alert)
{
    qCritical() << "ALERT: " << alert;
}

void XtConnect::authenticationRequested(QAuthenticator *auth)
{
    QString user = m_settings->value( Common::SettingsNames::imapUserKey ).toString();
    QString pass = m_settings->value( Common::SettingsNames::imapPassKey ).toString();
    auth->setUser( user );
    auth->setPassword( pass );
}

void XtConnect::connectionError(const QString &error)
{
    qCritical() << "Connection error: " << error;
    m_model->setNetworkOffline();
    // FIXME: add some nice behavior for reconnecting. Also handle failed logins...
    qFatal("Reconnects not supported yet -> see you.");
}

void XtConnect::cacheError(const QString &error)
{
    qCritical() << "Cache error: " << error;
    if ( m_model ) {
        m_model->setCache( new Imap::Mailbox::MemoryCache( m_model, QString() ) );
    }
}

void XtConnect::showConnectionStatus( QObject* parser, Imap::ConnectionState state )
{
    Q_UNUSED( parser );
    using namespace Imap;

    qDebug() << "Connection status:" <<  Imap::connectionStateToString( state );
}

}
