/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include "SettingsDialog.h"
#include "SettingsNames.h"

namespace Gui {

SettingsDialog::SettingsDialog(): QDialog()
{
    setWindowTitle( tr("Settings") );
    QSettings s;

    QVBoxLayout* layout = new QVBoxLayout( this );
    stack = new QTabWidget( this );
    layout->addWidget( stack );

    identity = new IdentityPage( this, s );
    stack->addTab( identity, tr("Identity") );
    imap = new ImapPage( this, s );
    stack->addTab( imap, tr("IMAP") );
    cache = new CachePage( this, s );
    stack->addTab( cache, tr("Offline") );
    outgoing = new OutgoingPage( this, s );
    stack->addTab( outgoing, tr("SMTP") );

    QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this );
    connect( buttons, SIGNAL( accepted() ), this, SLOT( accept() ) );
    connect( buttons, SIGNAL( rejected() ), this, SLOT( reject() ) );
    layout->addWidget( buttons );
}

void SettingsDialog::accept()
{
    QSettings s;
    identity->save( s );
    imap->save( s );
    cache->save( s );
    outgoing->save( s );
    QDialog::accept();
}

IdentityPage::IdentityPage( QWidget* parent, QSettings& s ): QWidget(parent)
{
    QFormLayout* layout = new QFormLayout( this );
    realName = new QLineEdit( s.value( SettingsNames::realNameKey ).toString(), this );
    layout->addRow( tr("Real Name"), realName );
    address = new QLineEdit( s.value( SettingsNames::addressKey ).toString(), this );
    layout->addRow( tr("E-mail"), address );
}

void IdentityPage::save( QSettings& s )
{
    s.setValue( SettingsNames::realNameKey, realName->text() );
    s.setValue( SettingsNames::addressKey, address->text() );
}

ImapPage::ImapPage( QWidget* parent, QSettings& s ): QWidget(parent)
{
    QFormLayout* layout = new QFormLayout( this );
    method = new QComboBox( this );
    method->insertItem( 0, tr("TCP"), QVariant( TCP ) );
    method->insertItem( 1, tr("SSL"), QVariant( SSL ) );
    method->insertItem( 2, tr("Local Process"), QVariant( PROCESS ) );
    if ( QSettings().value( SettingsNames::imapMethodKey ).toString() == SettingsNames::methodTCP ) {
        method->setCurrentIndex( 0 );
    } else if ( QSettings().value( SettingsNames::imapMethodKey ).toString() == SettingsNames::methodSSL ) {
        method->setCurrentIndex( 1 );
    } else {
        method->setCurrentIndex( 2 );
    }
    layout->addRow( tr("Method"), method );

    imapHost = new QLineEdit( s.value( SettingsNames::imapHostKey ).toString(), this );
    layout->addRow( tr("IMAP Server"), imapHost );
    imapPort = new QLineEdit(s.value( SettingsNames::imapPortKey, 143 ).toString(), this );
    QValidator* validator = new QIntValidator( 1, 65535, this );
    imapPort->setValidator( validator );
    layout->addRow( tr("Port"), imapPort );
    startTls = new QCheckBox( this );
    startTls->setChecked( s.value( SettingsNames::imapStartTlsKey, true ).toBool() );
    layout->addRow( tr("Perform STARTTLS"), startTls );
    imapUser = new QLineEdit( s.value( SettingsNames::imapUserKey ).toString(), this );
    layout->addRow( tr("User"), imapUser );
    imapPass = new QLineEdit( s.value( SettingsNames::imapPassKey ).toString(), this );
    imapPass->setEchoMode( QLineEdit::Password );
    layout->addRow( tr("Password"), imapPass );

    processPath = new QLineEdit( s.value( SettingsNames::imapProcessKey ).toString(), this );
    layout->addRow( tr("Path to Server Binary"), processPath );

    startOffline = new QCheckBox( this );
    startOffline->setChecked( s.value( SettingsNames::imapStartOffline ).toBool() );
    layout->addRow( tr("Start in Offline Mode"), startOffline );

    connect( method, SIGNAL( currentIndexChanged( int ) ), this, SLOT( updateWidgets() ) );
    updateWidgets();
}

void ImapPage::updateWidgets()
{
    QFormLayout* lay = qobject_cast<QFormLayout*>( layout() );
    Q_ASSERT( lay );

    switch ( method->itemData( method->currentIndex() ).toInt() ) {
        case TCP:
            imapHost->setEnabled( true );
            lay->labelForField( imapHost )->setEnabled( true );
            imapPort->setEnabled( true );
            imapPort->setText( QString::number( 143 ) );
            lay->labelForField( imapPort )->setEnabled( true );
            startTls->setEnabled( true );
            startTls->setChecked( QSettings().value( SettingsNames::imapStartTlsKey, true ).toBool() );
            lay->labelForField( startTls )->setEnabled( true );
            imapUser->setEnabled( true );
            lay->labelForField( imapUser )->setEnabled( true );
            imapPass->setEnabled( true );
            lay->labelForField( imapPass )->setEnabled( true );
            processPath->setEnabled( false );
            lay->labelForField( processPath )->setEnabled( false );
            break;
        case SSL:
            imapHost->setEnabled( true );
            lay->labelForField( imapHost )->setEnabled( true );
            imapPort->setEnabled( true );
            imapPort->setText( QString::number( 993 ) );
            lay->labelForField( imapPort )->setEnabled( true );
            startTls->setEnabled( false );
            startTls->setChecked( false );
            lay->labelForField( startTls )->setEnabled( false );
            imapUser->setEnabled( true );
            lay->labelForField( imapUser )->setEnabled( true );
            imapPass->setEnabled( true );
            lay->labelForField( imapPass )->setEnabled( true );
            processPath->setEnabled( false );
            lay->labelForField( processPath )->setEnabled( false );
            break;
        default:
            imapHost->setEnabled( false );
            lay->labelForField( imapHost )->setEnabled( false );
            imapPort->setEnabled( false );
            lay->labelForField( imapPort )->setEnabled( false );
            startTls->setEnabled( false );
            lay->labelForField( startTls )->setEnabled( false );
            imapUser->setEnabled( false );
            lay->labelForField( imapUser )->setEnabled( false );
            imapPass->setEnabled( false );
            lay->labelForField( imapPass )->setEnabled( false );
            processPath->setEnabled( true );
            lay->labelForField( processPath )->setEnabled( true );
    }
}

void ImapPage::save( QSettings& s )
{
    switch ( method->currentIndex() ) {
        case TCP:
            s.setValue( SettingsNames::imapMethodKey, SettingsNames::methodTCP );
            s.setValue( SettingsNames::imapHostKey, imapHost->text() );
            s.setValue( SettingsNames::imapPortKey, imapPort->text() );
            s.setValue( SettingsNames::imapStartTlsKey, startTls->isChecked() );
            s.setValue( SettingsNames::imapUserKey, imapUser->text() );
            s.setValue( SettingsNames::imapPassKey, imapPass->text() );
            break;
        case SSL:
            s.setValue( SettingsNames::imapMethodKey, SettingsNames::methodSSL );
            s.setValue( SettingsNames::imapHostKey, imapHost->text() );
            s.setValue( SettingsNames::imapPortKey, imapPort->text() );
            s.setValue( SettingsNames::imapStartTlsKey, startTls->isChecked() );
            s.setValue( SettingsNames::imapUserKey, imapUser->text() );
            s.setValue( SettingsNames::imapPassKey, imapPass->text() );
            break;
        default:
            s.setValue( SettingsNames::imapMethodKey, SettingsNames::methodProcess );
            s.setValue( SettingsNames::imapProcessKey, processPath->text() );
    }
    s.setValue( SettingsNames::imapStartOffline, startOffline->isChecked() );
}

CachePage::CachePage( QWidget* parent, QSettings& s ): QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    QGroupBox* box = new QGroupBox( tr("Caching"), this );
    QVBoxLayout* boxLayout = new QVBoxLayout( box );
    metadataPersistentCache = new QRadioButton( tr("Use persistent cache"), box );
    boxLayout->addWidget( metadataPersistentCache );
    metadataMemoryCache = new QRadioButton( tr("Cache metadata only for session duration"), box );
    boxLayout->addWidget( metadataMemoryCache );
    layout->addWidget( box );
    box = new QGroupBox( tr("Offline mode"), this );
    boxLayout = new QVBoxLayout( box );
    offlineNope = new QRadioButton( tr("Don't store messages for offline access"), box );
    boxLayout->addWidget( offlineNope );
    offlineXDays = new QRadioButton( tr("Remember messages from X recent days"), box );
    boxLayout->addWidget( offlineXDays );
    offlineNumberOfDays = new QSpinBox( box );
    offlineNumberOfDays->setRange( 1, 365 * 1000 ); // more than 1000 years -> unlimited
    boxLayout->addWidget( offlineNumberOfDays );
    offlineXMessages = new QRadioButton( tr("Remember X recent messages"), box );
    boxLayout->addWidget( offlineXMessages );
    offlineNumberOfMessages = new QSpinBox( box );
    boxLayout->addWidget( offlineNumberOfMessages );
    offlineNumberOfMessages->setRange( 1, 100 * 1000 ); // more than 100k messages -> unlimited
    offlineEverything = new QRadioButton( tr("Remember all messages"), box );
    boxLayout->addWidget( offlineEverything );
    layout->addWidget( box );

    QString val = s.value( SettingsNames::cacheMetadataKey ).toString();
    if ( val == SettingsNames::cacheMetadataPersistent )
        metadataPersistentCache->setChecked( true );
    else
        metadataMemoryCache->setChecked( true );

    val = s.value( SettingsNames::cacheOfflineKey ).toString();
    if ( val == SettingsNames::cacheOfflineAll )
        offlineEverything->setChecked( true );
    else if ( val == SettingsNames::cacheOfflineXDays )
        offlineXDays->setChecked( true );
    else if ( val == SettingsNames::cacheOfflineXMessages )
        offlineXMessages->setChecked( true );
    else
        offlineNope->setChecked( true );

    offlineNumberOfDays->setValue( s.value( SettingsNames::cacheOfflineNumberDaysKey, QVariant( 30 ) ).toInt() );
    offlineNumberOfDays->setValue( s.value( SettingsNames::cacheOfflineNumberMessagesKey, QVariant( 100 ) ).toInt() );

    updateWidgets();
    connect( offlineNope, SIGNAL(clicked()), this, SLOT(updateWidgets()) );
    connect( offlineXDays, SIGNAL(clicked()), this, SLOT(updateWidgets()) );
    connect( offlineXMessages, SIGNAL(clicked()), this, SLOT(updateWidgets()) );
    connect( offlineEverything, SIGNAL(clicked()), this, SLOT(updateWidgets()) );
    connect( metadataMemoryCache, SIGNAL(clicked()), this, SLOT(updateWidgets()) );
    connect( metadataPersistentCache, SIGNAL(clicked()), this, SLOT(updateWidgets()) );
}

void CachePage::updateWidgets()
{
    if ( metadataMemoryCache->isChecked() ) {
        offlineNope->setChecked( true );
        offlineXDays->setEnabled( false );
        offlineXMessages->setEnabled( false );
        offlineEverything->setEnabled( false );
    } else {
        offlineXDays->setEnabled( true );
        offlineXMessages->setEnabled( true );
        offlineEverything->setEnabled( true );
    }
    offlineNumberOfDays->setEnabled( offlineXDays->isChecked() );
    offlineNumberOfMessages->setEnabled( offlineXMessages->isChecked() );
}

void CachePage::save( QSettings& s )
{
    if ( metadataPersistentCache->isChecked() )
        s.setValue( SettingsNames::cacheMetadataKey, SettingsNames::cacheMetadataPersistent );
    else
        s.setValue( SettingsNames::cacheMetadataKey, SettingsNames::cacheMetadataMemory );

    if ( offlineEverything->isChecked() )
        s.setValue( SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineAll );
    else if ( offlineXDays->isChecked() )
        s.setValue( SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineXDays );
    else if ( offlineXMessages->isChecked() )
        s.setValue( SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineXMessages );
    else
        s.setValue( SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineNone );

    s.setValue( SettingsNames::cacheOfflineNumberDaysKey, offlineNumberOfDays->value() );
    s.setValue( SettingsNames::cacheOfflineNumberMessagesKey, offlineNumberOfMessages->value() );
}

OutgoingPage::OutgoingPage( QWidget* parent, QSettings& s ): QWidget(parent)
{
    QFormLayout* layout = new QFormLayout( this );
    method = new QComboBox( this );
    method->insertItem( 0, tr("SMTP"), QVariant( SMTP ) );
    method->insertItem( 1, tr("Local sendmail-compatible"), QVariant( SENDMAIL ) );
    if ( QSettings().value( SettingsNames::msaMethodKey ).toString() == SettingsNames::methodSMTP ) {
        method->setCurrentIndex( 0 );
    } else {
        method->setCurrentIndex( 1 );
    }
    layout->addRow( tr("Method"), method );

    smtpHost = new QLineEdit( s.value( SettingsNames::smtpHostKey ).toString(), this );
    layout->addRow( tr("SMTP Server"), smtpHost );
    smtpPort = new QLineEdit(s.value( SettingsNames::smtpPortKey, 25 ).toString(), this );
    QValidator* validator = new QIntValidator( 1, 65535, this );
    smtpPort->setValidator( validator );
    layout->addRow( tr("Port"), smtpPort );
    smtpAuth = new QCheckBox( this );
    smtpAuth->setChecked( s.value( SettingsNames::smtpAuthKey, false ).toBool() );
    layout->addRow( tr("SMTP Auth"), smtpAuth );
    smtpUser = new QLineEdit( s.value( SettingsNames::smtpUserKey ).toString(), this );
    layout->addRow( tr("User"), smtpUser );
    smtpPass = new QLineEdit( s.value( SettingsNames::smtpPassKey ).toString(), this );
    smtpPass->setEchoMode( QLineEdit::Password );
    layout->addRow( tr("Password"), smtpPass );

    sendmail = new QLineEdit( s.value( SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd ).toString(), this );
    layout->addRow( tr("Sendmail-compatible Executable"), sendmail );

    connect( method, SIGNAL( currentIndexChanged( int ) ), this, SLOT( updateWidgets() ) );
    connect( smtpAuth, SIGNAL( toggled(bool) ), this, SLOT( updateWidgets() ) );
    updateWidgets();
}

void OutgoingPage::updateWidgets()
{
    QFormLayout* lay = qobject_cast<QFormLayout*>( layout() );
    Q_ASSERT( lay );
    switch ( method->itemData( method->currentIndex() ).toInt() ) {
        case SMTP:
        {
            smtpHost->setEnabled( true );
            lay->labelForField( smtpHost )->setEnabled( true );
            smtpPort->setEnabled( true );
            lay->labelForField( smtpPort )->setEnabled( true );
            smtpAuth->setEnabled( true );
            lay->labelForField( smtpAuth )->setEnabled( true );
            bool authEnabled = smtpAuth->isChecked();
            smtpUser->setEnabled( authEnabled );
            lay->labelForField( smtpUser )->setEnabled( authEnabled );
            smtpPass->setEnabled( authEnabled );
            lay->labelForField( smtpPass )->setEnabled( authEnabled );
            sendmail->setEnabled( false );
            lay->labelForField( sendmail )->setEnabled( false );
            break;
        }
        default:
            smtpHost->setEnabled( false );
            lay->labelForField( smtpHost )->setEnabled( false );
            smtpPort->setEnabled( false );
            lay->labelForField( smtpPort )->setEnabled( false );
            smtpAuth->setEnabled( false );
            lay->labelForField( smtpAuth )->setEnabled( false );
            smtpUser->setEnabled( false );
            lay->labelForField( smtpUser )->setEnabled( false );
            smtpPass->setEnabled( false );
            lay->labelForField( smtpPass )->setEnabled( false );
            sendmail->setEnabled( true );
            lay->labelForField( sendmail )->setEnabled( true );
            if ( sendmail->text().isEmpty() )
                sendmail->setText( SettingsNames::sendmailDefaultCmd );
    }
}

void OutgoingPage::save( QSettings& s )
{
    if ( method->currentIndex() == SMTP ) {
        s.setValue( SettingsNames::msaMethodKey, SettingsNames::methodSMTP );
        s.setValue( SettingsNames::smtpHostKey, smtpHost->text() );
        s.setValue( SettingsNames::smtpPortKey, smtpPort->text() );
        s.setValue( SettingsNames::smtpAuthKey, smtpAuth->isChecked() );
        s.setValue( SettingsNames::smtpUserKey, smtpUser->text() );
        s.setValue( SettingsNames::smtpPassKey, smtpPass->text() );
    } else {
        s.setValue( SettingsNames::msaMethodKey, SettingsNames::methodSENDMAIL );
        s.setValue( SettingsNames::sendmailKey, sendmail->text() );
    }
}

}


