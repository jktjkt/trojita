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
}

void ImapPage::save( QSettings& s )
{
}

OutgoingPage::OutgoingPage( QWidget* parent, QSettings& s ): QWidget(parent)
{
    QFormLayout* layout = new QFormLayout( this );
    method = new QComboBox( this );
    method->insertItem( 0, tr("SMTP"), QVariant( SMTP ) );
    method->insertItem( 1, tr("Local sendmail-compatible"), QVariant( SENDMAIL ) );
    if ( QSettings().value( SettingsNames::methodKey ).toString() == SettingsNames::methodSMTP ) {
        method->setCurrentIndex( 0 );
    } else {
        method->setCurrentIndex( 1 );
    }
    layout->addRow( tr("Method"), method );

    smtpHost = new QLineEdit( s.value( SettingsNames::smtpHostKey ).toString(), this );
    layout->addRow( tr("SMTP Server"), smtpHost );
    smtpPort = new QLineEdit(s.value( SettingsNames::smtpPortKey, 25 ).toString(), this );
    smtpPort->setInputMask( QLatin1String("D0") );
    layout->addRow( tr("Port"), smtpPort );
    smtpAuth = new QCheckBox( this );
    smtpAuth->setChecked( s.value( SettingsNames::smtpAuthKey, false ).toBool() );
    layout->addRow( tr("SMTP Auth"), smtpAuth );
    smtpUser = new QLineEdit( s.value( SettingsNames::smtpUserKey ).toString(), this );
    layout->addRow( tr("User"), smtpUser );
    smtpPass = new QLineEdit( s.value( SettingsNames::smtpPassKey ).toString(), this );
    smtpPass->setEchoMode( QLineEdit::Password );
    layout->addRow( tr("Password"), smtpPass );

    sendmail = new QLineEdit( s.value( SettingsNames::sendmailKey ).toString(), this );
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
    }
}

void OutgoingPage::save( QSettings& s )
{
    if ( method->currentIndex() == SMTP ) {
        s.setValue( SettingsNames::methodKey, SettingsNames::methodSMTP );
        s.setValue( SettingsNames::smtpHostKey, smtpHost->text() );
        s.setValue( SettingsNames::smtpPortKey, smtpPort->text() );
        s.setValue( SettingsNames::smtpAuthKey, smtpAuth->isChecked() );
        s.setValue( SettingsNames::smtpUserKey, smtpUser->text() );
        s.setValue( SettingsNames::smtpPassKey, smtpPass->text() );
    } else {
        s.setValue( SettingsNames::methodKey, SettingsNames::methodSENDMAIL );
        s.setValue( SettingsNames::sendmailKey, sendmail->text() );
    }
}

}

#include "SettingsDialog.moc"
