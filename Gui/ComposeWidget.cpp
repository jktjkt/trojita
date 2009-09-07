#include "ComposeWidget.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include "SettingsNames.h"
#include "RecipientsWidget.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Imap/Parser/3rdparty/kmime_util.h"


namespace Gui {

ComposeWidget::ComposeWidget( QWidget* parent, const QString& from,
                              const QList<QPair<QString, QString> >& recipients,
                              const QString& subject ):
QWidget(parent, Qt::Window)
{
    setupWidgets( from, recipients, subject );
    setWindowTitle( tr("Compose Mail") );
}

void ComposeWidget::setupWidgets( const QString& from,
                                  const QList<QPair<QString, QString> >& recipients,
                                  const QString& subject )
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    QHBoxLayout* hLayout = new QHBoxLayout();
    QLabel* lbl = new QLabel( tr("From"), this );
    hLayout->addWidget( lbl );
    fromField = new QLineEdit( this );
    fromField->setText( from );
    hLayout->addWidget( fromField );
    layout->addLayout( hLayout );
    recipientsField = new RecipientsWidget( this, recipients );
    layout->addWidget( recipientsField );
    hLayout = new QHBoxLayout();
    lbl = new QLabel( tr("Subject"), this );
    hLayout->addWidget( lbl );
    subjectField = new QLineEdit( this );
    subjectField->setText( subject );
    hLayout->addWidget( subjectField );
    layout->addLayout( hLayout );
    bodyField = new QTextEdit( this );
    bodyField->setMinimumSize( 600, 400 );
    layout->addWidget( bodyField );
    sendButton = new QPushButton( tr("Send"), this );
    //sendButton->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ) );
    layout->addWidget( sendButton );
    connect( sendButton, SIGNAL(clicked()), this, SLOT(send()) );
    recipientsField->setFocus();
}

void ComposeWidget::send()
{
    QSettings s;
    MSA::AbstractMSA* msa = 0;
    if ( s.value( SettingsNames::msaMethodKey ).toString() == SettingsNames::methodSMTP ) {
        msa = new MSA::SMTP( this, s.value( SettingsNames::smtpHostKey ).toString(),
                             s.value( SettingsNames::smtpPortKey ).toInt(),
                             false, false, // FIXME: encryption & startTls
                             s.value( SettingsNames::smtpAuthKey ).toBool(),
                             s.value( SettingsNames::smtpUserKey ).toString(),
                             s.value( SettingsNames::smtpPassKey ).toString() );
    } else {
        QStringList args = s.value( SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd ).toString().split( QLatin1Char(' ') );
        Q_ASSERT( ! args.isEmpty() ); // FIXME
        QString appName = args.takeFirst();
        msa = new MSA::Sendmail( this, appName, args );
    }

    QList<QPair<QString,QString> > recipients = recipientsField->recipients();
    QList<QString> mailDestinations;
    QByteArray recipientHeaders;
    for ( QList<QPair<QString,QString> >::const_iterator it = recipients.begin();
          it != recipients.end(); ++it ) {
        if ( ! it->second.isEmpty() ) {
            bool ok;
            mailDestinations << extractMailAddress( it->second, ok );
            if ( ! ok ) {
                QMessageBox::critical( this, tr("Invalid Address"),
                                       tr("Can't parse \"%1\" as an e-mail address").arg( it->second ) );
                return;
            }
            if ( it->first != QLatin1String("Bcc") ) {
                recipientHeaders.append( it->first ).append( ": " ).append(
                        encodeHeaderField( it->second ) ).append( "\r\n" );
            }
        }
    }

    if ( mailDestinations.isEmpty() ) {
        QMessageBox::critical( this, tr("No Way"), tr("You haven't entered any recipients") );
        return;
    }

    QByteArray mailData;
    mailData.append( "From: " ).append( encodeHeaderField( fromField->text() ) ).append( "\r\n" );
    mailData.append( recipientHeaders );
    mailData.append( "Subject: " ).append( encodeHeaderField( subjectField->text() ) ).append( "\r\n" );
    mailData.append( "Content-Type: text/plain; charset=utf-8\r\n"
                     "Content-Transfer-Encoding: 8bit\r\n");

    //mailData.append( "Message-ID: <4A00C5A4.1080301@fzu.cz>\r\n" ); // FIXME
    QDateTime now = QDateTime::currentDateTime().toUTC(); // FIXME: will neeed proper timzeone...
    mailData.append( "Date: " ).append( now.toString(
            QString::fromAscii( "ddd, dd MMM yyyy hh:mm:ss" ) ).append(
                    " +0000\r\n" ) );
    mailData.append( "User-Agent: ").append( qApp->applicationName() ).append(
            "/").append( qApp->applicationVersion() ).append( "\r\n");
    mailData.append( "MIME-Version: 1.0\r\n" );
    mailData.append( "\r\n" );
    mailData.append( bodyField->toPlainText().toUtf8() );

    QProgressDialog* progress = new QProgressDialog(
            tr("Sending mail"), tr("Abort"), 0, mailData.size(), this );
    progress->setMinimumDuration( 0 );
    connect( msa, SIGNAL(progressMax(int)), progress, SLOT(setMaximum(int)) );
    connect( msa, SIGNAL(progress(int)), progress, SLOT(setValue(int)) );
    connect( msa, SIGNAL(error(QString)), progress, SLOT(close()) );
    connect( msa, SIGNAL(sent()), progress, SLOT(close()) );
    connect( msa, SIGNAL(sent()), this, SLOT(sent()) );
    connect( msa, SIGNAL(sent()), this, SLOT(deleteLater()) );
    connect( progress, SIGNAL(canceled()), msa, SLOT(cancel()) );
    connect( msa, SIGNAL(error(QString)), this, SLOT(gotError(QString)) );

    setEnabled( false );
    progress->setEnabled( true );

    // FIXME: parse the From: address and use it instead of hard-coded stub
    bool senderMailIsCorrect = false;
    QString senderMail = extractMailAddress( fromField->text(), senderMailIsCorrect );
    if ( ! senderMailIsCorrect ) {
        gotError( tr("The From: address does not look like a valid one") );
        return;
    }
    msa->sendMail( senderMail, mailDestinations, mailData );
}

void ComposeWidget::gotError( const QString& error )
{
    QMessageBox::critical( this, tr("Failed to Send Mail"), error );
    setEnabled( true );
}

void ComposeWidget::sent()
{
    QMessageBox::information( this, tr("OK"), tr("Message Sent") );
    setEnabled( true );
}

QByteArray ComposeWidget::encodeHeaderField( const QString& text )
{
    return KMime::encodeRFC2047String( text, "utf-8" );
}

QByteArray ComposeWidget::extractMailAddress( const QString& text, bool& ok )
{
    // This is of course far from complete, but at least catches "Real Name" <foo@bar>
    int pos1 = text.lastIndexOf( QChar('<') );
    int pos2 = text.lastIndexOf( QChar('>') );
    if ( pos1 == -1 || pos2 == -1 || pos2 < pos1 ) {
        ok = true;
        return text.toUtf8();
    } else {
        ok = true;
        return text.mid( pos1 + 1, pos2 - pos1 - 1 ).toUtf8();
    }
}

}

#include "ComposeWidget.moc"
