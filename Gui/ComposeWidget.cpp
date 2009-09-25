#include <QDateTime>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>

#include "ComposeWidget.h"
#include "ui_ComposeWidget.h"

#include "SettingsNames.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Imap/Encoders.h"

namespace {
    enum { OFFSET_OF_FIRST_ADDRESSEE = 1 };
}

namespace Gui {

ComposeWidget::ComposeWidget(QWidget *parent) :
    QWidget(parent, Qt::Window),
    ui(new Ui::ComposeWidget)
{
    ui->setupUi(this);
    connect( ui->sendButton, SIGNAL(clicked()), this, SLOT(send()) );
}

ComposeWidget::~ComposeWidget()
{
    delete ui;
}

void ComposeWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
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
        if ( args.isEmpty() ) {
            QMessageBox::critical( this, tr("Error"), tr("Please configure the SMTP or sendmail settings in application settings.") );
            return;
        }
        QString appName = args.takeFirst();
        msa = new MSA::Sendmail( this, appName, args );
    }

    QList<QPair<QString,QString> > recipients = _parseRecipients();
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
    mailData.append( "From: " ).append( encodeHeaderField( ui->sender->currentText() ) ).append( "\r\n" );
    mailData.append( recipientHeaders );
    mailData.append( "Subject: " ).append( encodeHeaderField( ui->subject->text() ) ).append( "\r\n" );
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
    mailData.append( ui->mailText->toPlainText().toUtf8() );

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
    QString senderMail = extractMailAddress( ui->sender->currentText(), senderMailIsCorrect );
    if ( ! senderMailIsCorrect ) {
        gotError( tr("The From: address does not look like a valid one") );
        return;
    }
    msa->sendMail( senderMail, mailDestinations, mailData );
}

void ComposeWidget::setData( const QString& from, const QList<QPair<QString, QString> >& recipients,
                             const QString& subject, const QString& body )
{
    // FIXME: combobox for from...
    ui->sender->addItem( from );
    for ( int i = 0; i < recipients.size(); ++i ) {
        addRecipient( i, recipients[i].first, recipients[i].second );
    }
    if ( recipients.isEmpty() )
        addRecipient( 0, tr("To"), QString() );
    else
        addRecipient( recipients.size(), recipients.last().first, QString() );
    ui->subject->setText( subject );
    ui->mailText->setText( body );
}

void ComposeWidget::addRecipient( int position, const QString& kind, const QString& address )
{
    QComboBox* combo = new QComboBox( this );
    QStringList toCcBcc = QStringList() << tr("To") << tr("Cc") << tr("Bcc");
    combo->addItems( toCcBcc );
    combo->setCurrentIndex( toCcBcc.indexOf( kind ) );
    QLineEdit* edit = new QLineEdit( this );
    _recipientsAddress.insert( position, edit );
    _recipientsKind.insert( position, combo );
    connect( edit, SIGNAL(editingFinished()), this, SLOT(handleRecipientAddressChange()) );
    ui->formLayout->insertRow( position + OFFSET_OF_FIRST_ADDRESSEE, combo, edit );
}

void ComposeWidget::handleRecipientAddressChange()
{
    QLineEdit* item = qobject_cast<QLineEdit*>( sender() );
    Q_ASSERT( item );
    int index = _recipientsAddress.indexOf( item );
    Q_ASSERT( index >= 0 );

    if ( index == _recipientsAddress.size() - 1 ) {
        if ( ! item->text().isEmpty() ) {
            addRecipient( index + 1, _recipientsKind[ index ]->currentText(), QString() );
            _recipientsAddress[index + 1]->setFocus();
        }
    } else if ( item->text().isEmpty() && _recipientsAddress.size() != 1 ) {
        delete _recipientsAddress.takeAt( index );
        delete _recipientsKind.takeAt( index );

        delete ui->formLayout;
        ui->formLayout = new QFormLayout( this );

        // the first line
        ui->formLayout->addRow( ui->fromLabel, ui->sender );

        // note: number of layout items before this one has to match OFFSET_OF_FIRST_ADDRESSEE
        for ( int i = 0; i < _recipientsAddress.size(); ++i ) {
            ui->formLayout->addRow( _recipientsKind[ i ], _recipientsAddress[ i ] );
        }

        // all other stuff
        ui->formLayout->addRow( ui->subjectLabel, ui->subject );
        ui->formLayout->addRow( ui->mailText );
        ui->formLayout->addRow( 0, ui->sendButton );
        setLayout( ui->formLayout );
    }
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
    return Imap::encodeRFC2047String( text );
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

QList<QPair<QString, QString> > ComposeWidget::_parseRecipients()
{
    QList<QPair<QString, QString> > res;
    Q_ASSERT( _recipientsAddress.size() == _recipientsKind.size() );
    for ( int i = 0; i < _recipientsAddress.size(); ++i ) {
        QString kind = QLatin1String("To");
        if ( _recipientsKind[i]->currentText() == tr("Cc") )
            kind = QLatin1String("Cc");
        else if ( _recipientsKind[i]->currentText() == tr("Bcc") )
            kind = QLatin1String("Bcc");
        res << qMakePair( kind, _recipientsAddress[i]->text() );
    }
    return res;
}

}

#include "ComposeWidget.moc"
