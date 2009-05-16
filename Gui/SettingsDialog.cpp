#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
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

#include "SettingsDialog.moc"
