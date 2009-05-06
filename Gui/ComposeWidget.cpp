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
    layout->addWidget( bodyField );
    sendButton = new QPushButton( tr("Send"), this );
    layout->addWidget( sendButton );
    connect( sendButton, SIGNAL(clicked()), this, SLOT(send()) );
}

void ComposeWidget::send()
{
    QSettings s;
    MSA::AbstractMSA* msa = 0;
    if ( s.value( SettingsNames::msaMethodKey ).toString() == SettingsNames::methodSMTP ) {
        qFatal( "SMTP not implemented yet, sorry" );
        return;
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
            mailDestinations << it->second;
            recipientHeaders.append( it->first ).append( ": " ).append( "" /*to string*/ ).append( "\r\n" );
        }
    }

    if ( mailDestinations.isEmpty() ) {
        QMessageBox::critical( this, tr("No Way"), tr("You haven't entered any recipients") );
        return;
    }

    QByteArray mailData;
    mailData.append( "From: foo bar <trojita@flaska.net>\r\n" );
    mailData.append( recipientHeaders );

    mailData.append( "Subject: testovaci mail\r\n"
                     "Content-Type: text/plain; charset=utf-8\r\n"
                     "Content-Transfer-Encoding: 8bit");
    mailData.append( "\r\n" );

    mailData.append( "Message-ID: <4A00C5A4.1080301@fzu.cz>\r\n" );
    QDateTime now = QDateTime::currentDateTime().toUTC(); // FIXME: will neeed proper timzeone...
    mailData.append( "Date: " ).append( now.toString(
            QString::fromAscii( "ddd, dd MMM yyyy hh:mm:ss" ) ).append(
                    " +0000\r\n" ) );
    mailData.append( "User-Agent: ").append( qApp->applicationName() ).append(
            "/").append( qApp->applicationVersion() ).append( "\r\n");
    mailData.append( "MIME-Version: 1.0\r\n" );
    mailData.append( bodyField->toPlainText().toUtf8() );

    QProgressDialog* progress = new QProgressDialog(
            tr("Sending mail"), tr("Abort"), 0, mailData.size(), this );
    connect( msa, SIGNAL(progressMax(int)), progress, SLOT(setMaximum(int)) );
    connect( msa, SIGNAL(progress(int)), progress, SLOT(setValue(int)) );
    connect( msa, SIGNAL(error(QString)), progress, SLOT(close()) );
    connect( msa, SIGNAL(sent()), progress, SLOT(close()) );
    connect( progress, SIGNAL(canceled()), msa, SLOT(cancel()) );
    connect( msa, SIGNAL(error(QString)), this, SLOT(gotError(QString)) );

    qDebug() << "now really sending";
    qDebug() << mailDestinations;
    qDebug() << mailData;
    msa->sendMail( mailDestinations, mailData );
}

void ComposeWidget::gotError( const QString& error )
{
    QMessageBox::critical( this, tr("Failed to Send Mail"), error );
}

void ComposeWidget::sent()
{
    QMessageBox::information( this, tr("OK"), tr("Message Sent") );
}

}

#include "ComposeWidget.moc"
