#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>
#include "SettingsDialog.h"

namespace Gui {

SettingsDialog::SettingsDialog(): QDialog()
{
    setWindowTitle( tr("Settings") );

    QVBoxLayout* layout = new QVBoxLayout( this );
    stack = new QTabWidget( this );
    layout->addWidget( stack );

    identity = new IdentityPage( this );
    stack->addTab( identity, tr("Identity") );
    imap = new ImapPage( this );
    stack->addTab( imap, tr("IMAP") );
    outgoing = new OutgoingPage( this );
    stack->addTab( outgoing, tr("SMTP") );

    QDialogButtonBox* buttons = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this );
    connect( buttons, SIGNAL( accepted() ), this, SLOT( accept() ) );
    connect( buttons, SIGNAL( rejected() ), this, SLOT( reject() ) );
    layout->addWidget( buttons );
}

void SettingsDialog::accept()
{
    identity->save();
    imap->save();
    outgoing->save();
    QDialog::accept();
}

IdentityPage::IdentityPage( QWidget* parent ): QWidget(parent),
    realNameKey(QLatin1String("identity.realName")), addressKey(QLatin1String("identity.address"))
{
    QFormLayout* layout = new QFormLayout( this );
    realName = new QLineEdit( QSettings().value( realNameKey ).toString(), this );
    layout->addRow( tr("Real Name"), realName );
    address = new QLineEdit( QSettings().value( addressKey ).toString(), this );
    layout->addRow( tr("E-mail"), address );
}

void IdentityPage::save()
{
    QSettings().setValue( realNameKey, realName->text() );
    QSettings().setValue( addressKey, address->text() );
}

ImapPage::ImapPage( QWidget* parent ): QWidget(parent)
{
}

void ImapPage::save()
{
}

OutgoingPage::OutgoingPage( QWidget* parent ): QWidget(parent)
{
}

void OutgoingPage::save()
{
}

}

#include "SettingsDialog.moc"
