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
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include "Common/SettingsNames.h"
#include "XtCache.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/ItemRoles.h"
#include "MailboxFinder.h"
#include "MessageDownloader.h"
#include "SqlStorage.h"

namespace XtConnect {

XtConnect::XtConnect(QObject *parent, QSettings *s) :
    QObject(parent), m_model(0), m_settings(s), m_cache(0)
{
    Q_ASSERT(m_settings);
    m_settings->setParent(this);
    if ( ! m_settings->contains( Common::SettingsNames::xtConnectCacheDirectory ) ) {
        qFatal("The service is not configured yet. Please use the Trojita GUI for configuration.");
    }

    QString host = s->value( Common::SettingsNames::xtDbHost ).toString();
    int port = s->value( Common::SettingsNames::xtDbPort, QVariant(5432) ).toInt();
    QString dbname = s->value( Common::SettingsNames::xtDbDbName ).toString();
    QString username = s->value( Common::SettingsNames::xtDbUser ).toString();
    QString password;
    bool readstdin = false;

    QStringList args = QCoreApplication::arguments();
    for ( int i = 1; i < args.length(); i++ ) {
        if (args.at(i) == "-h" && args.length() > i)
            host = args.at(++i);
        else if (args.at(i) == "-d" && args.length() > i)
            dbname = args.at(++i);
        else if (args.at(i) == "-p" && args.length() > i)
            port = args.at(++i).toInt();
        else if (args.at(i) == "-U" && args.length() > i)
            username = args.at(++i);
        else if (args.at(i) == "-W")
            readstdin = true;
    }

    for ( int i = 0; i < 3 && password.isEmpty() && readstdin; i++ ) {
        QTextStream(stdout) << tr("Password: ");
        password = QTextStream(stdin).readLine();
    }

    setupModels();

    // Prepare the mailboxes
    m_finder = new MailboxFinder( this, m_model );
    SqlStorage *storage = new SqlStorage( this, host, port, dbname, username, password );
    storage->open();

    QTimer *statsDumper = new QTimer(this);
    connect( statsDumper, SIGNAL(timeout()), this, SLOT(slotDumpStats()) );
    statsDumper->setInterval( 5000 );
    statsDumper->start();

    Q_FOREACH( const QString &mailbox, s->value( Common::SettingsNames::xtSyncMailboxList ).toStringList() ) {
        MessageDownloader *downloader = new MessageDownloader( this, mailbox );
        MailSynchronizer *sync = new MailSynchronizer( this, m_model, m_finder, downloader, storage );
        connect( sync, SIGNAL(aboutToRequestMessage(QString,QModelIndex,bool*)), this, SLOT(slotAboutToRequestMessage(QString,QModelIndex,bool*)) );
        connect( sync, SIGNAL(messageSaved(QString,QModelIndex)), this, SLOT(slotMessageStored(QString,QModelIndex)) );
        connect( sync, SIGNAL(messageIsDuplicate(QString,QModelIndex)), this, SLOT(slotMessageIsDuplicate(QString,QModelIndex)) );
        m_syncers[ mailbox ] = sync;
        sync->setMailbox( mailbox );
    }

    m_rotateMailboxes = new QTimer(this);
    m_rotateMailboxes->setInterval( 1000 * 60 * 3 ); // every three minutes
    connect( m_rotateMailboxes, SIGNAL(timeout()), this, SLOT(goTroughMailboxes()) );
    m_rotateMailboxes->start();
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

    bool shouldUsePersistentCache = true;
    QString cacheDir = m_settings->value( Common::SettingsNames::xtConnectCacheDirectory).toString();

    if ( ! QDir().mkpath( cacheDir ) ) {
        qCritical() << "Failed to create directory" << cacheDir << " -- will not remember anything on restart!";
        shouldUsePersistentCache = false;
    }

    if ( shouldUsePersistentCache ) {
        m_cache = new XtCache( this, QLatin1String("trojita-imap-cache"), cacheDir );
        connect( m_cache, SIGNAL(error(QString)), this, SLOT(cacheError(QString)) );
        if ( ! m_cache->open() ) {
            // Error message was already shown by the cacheError() slot
            m_cache->deleteLater();
            m_cache = 0;
        }
    }

    m_model = new Imap::Mailbox::Model( this, m_cache ? static_cast<Imap::Mailbox::AbstractCache*>( m_cache ) :
                                                        static_cast<Imap::Mailbox::AbstractCache*>( new Imap::Mailbox::MemoryCache( this, QString() ) ),
                                        factory, taskFactory, m_settings->value( SettingsNames::imapStartOffline ).toBool() );
    m_model->setObjectName( QLatin1String("model") );
    // We want to wait longer to increase the potential of better grouping -- we don't care much about the latency
    m_model->setProperty( "trojita-imap-delayed-fetch-part", 300 );

    connect( m_model, SIGNAL( alertReceived( const QString& ) ), this, SLOT( alertReceived( const QString& ) ) );
    connect( m_model, SIGNAL( connectionError( const QString& ) ), this, SLOT( connectionError( const QString& ) ) );
    connect( m_model, SIGNAL( authRequested( QAuthenticator* ) ), this, SLOT( authenticationRequested( QAuthenticator* ) ) );
    connect(m_model, SIGNAL(authAttemptFailed(QString)), this, SLOT(authenticationFailed(QString)));
    connect( m_model, SIGNAL(connectionStateChanged(QObject*,Imap::ConnectionState)), this, SLOT(showConnectionStatus(QObject*,Imap::ConnectionState)) );
}

void XtConnect::alertReceived(const QString &alert)
{
    qCritical() << "ALERT: " << alert;
}

void XtConnect::authenticationRequested(QAuthenticator *auth)
{
    if ( ! m_settings->contains(Common::SettingsNames::imapPassKey) ) {
        qWarning() << "Warning: no IMAP password set in the configuration.";
        qWarning() << "Please remember to configure the synchronization service in Trojita GUI's settings dialog.";
    }
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

void XtConnect::authenticationFailed(const QString &message)
{
    qCritical() << "Cannot login to the IMAP server: " << message;
    m_model->setNetworkOffline();
    qFatal("Unable to login to the IMAP server");
}

void XtConnect::cacheError(const QString &error)
{
    qCritical() << "Cache error: " << error;
    if ( m_model ) {
        m_cache = 0;
        m_model->setCache( new Imap::Mailbox::MemoryCache( m_model, QString() ) );
    }
}

void XtConnect::showConnectionStatus( QObject* parser, Imap::ConnectionState state )
{
    Q_UNUSED( parser );
    using namespace Imap;

    switch ( state ) {
    case CONN_STATE_FETCHING_MSG_METADATA:
    case CONN_STATE_FETCHING_PART:
    case CONN_STATE_SELECTED:
    case CONN_STATE_SELECTING:
        return;
    default:
        // well, we're interested in the rest
        break;
    }

    qDebug() << "Connection status:" <<  Imap::connectionStateToString( state );
}

void XtConnect::goTroughMailboxes()
{
    Q_FOREACH( MailSynchronizer *sync, m_syncers ) {
        sync->switchHere();
    }
}

void XtConnect::slotAboutToRequestMessage( const QString &mailbox, const QModelIndex &message, bool *shouldLoad )
{
    Q_ASSERT( shouldLoad );
    if ( m_cache ) {
        XtCache::SavingState status = m_cache->messageSavingStatus( mailbox, message.data( Imap::Mailbox::RoleMessageUid ).toUInt() );
        switch ( status ) {
        case XtCache::STATE_DUPLICATE:
        case XtCache::STATE_SAVED:
            *shouldLoad = false;
            break;
        case XtCache::STATE_UNKNOWN:
            break;
        }
    }
}

void XtConnect::slotMessageStored( const QString &mailbox, const QModelIndex &message )
{
    if ( m_cache ) {
        m_cache->setMessageSavingStatus( mailbox,  message.data( Imap::Mailbox::RoleMessageUid ).toUInt(), XtCache::STATE_SAVED );
    }
}

void XtConnect::slotMessageIsDuplicate( const QString &mailbox, const QModelIndex &message )
{
    if ( m_cache ) {
        m_cache->setMessageSavingStatus( mailbox,  message.data( Imap::Mailbox::RoleMessageUid ).toUInt(), XtCache::STATE_DUPLICATE );
    }
}

void XtConnect::slotDumpStats()
{
    qDebug() << QDateTime::currentDateTime();
    Q_FOREACH( const QPointer<MailSynchronizer> item, m_syncers ) {
        item->debugStats();
    }
}

}
