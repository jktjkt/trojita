/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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
#include <QCompleter>
#include <QDateTime>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>

#include "AutoCompletion.h"
#include "ComposeWidget.h"
#include "ui_ComposeWidget.h"

#include "Common/SettingsNames.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Imap/Encoders.h"
#include "Imap/Model/Utils.h"

namespace
{
enum { OFFSET_OF_FIRST_ADDRESSEE = 2 };
}

namespace Gui
{

ComposeWidget::ComposeWidget(QWidget *parent, QAbstractListModel *autoCompleteModel) :
    QWidget(parent, Qt::Window),
    ui(new Ui::ComposeWidget),
    recipientCompleter(new QCompleter(this))
{
    ui->setupUi(this);
    sendButton = ui->buttonBox->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
    connect(sendButton, SIGNAL(clicked()), this, SLOT(send()));
    cancelButton = ui->buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(close()));

    // Ask for a fixed-width font. The problem is that these names wary acros platforms,
    // but the following works well -- at first, we come up with a made-up name, and then
    // let the Qt font substitution algorithm do its magic.
    QFont font(QString::fromAscii("x-trojita-terminus-like-fixed-width"));
    font.setStyleHint(QFont::TypeWriter);
    ui->mailText->setFont(font);

    if (autoCompleteModel)
        recipientCompleter->setModel(autoCompleteModel);
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
    using Common::SettingsNames;
    QSettings s;
    MSA::AbstractMSA *msa = 0;
    QString method = s.value(SettingsNames::msaMethodKey).toString();
    if (method == SettingsNames::methodSMTP || method == SettingsNames::methodSSMTP) {
        msa = new MSA::SMTP(this, s.value(SettingsNames::smtpHostKey).toString(),
                            s.value(SettingsNames::smtpPortKey).toInt(),
                            (method == SettingsNames::methodSSMTP),
                            (method == SettingsNames::methodSMTP)
                            && s.value(SettingsNames::smtpStartTlsKey).toBool(),
                            s.value(SettingsNames::smtpAuthKey).toBool(),
                            s.value(SettingsNames::smtpUserKey).toString(),
                            s.value(SettingsNames::smtpPassKey).toString());
    } else {
        QStringList args = s.value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("Please configure the SMTP or sendmail settings in application settings."));
            return;
        }
        QString appName = args.takeFirst();
        msa = new MSA::Sendmail(this, appName, args);
    }

    QList<QPair<RecipientKind,Imap::Message::MailAddress> > recipients;
    if (!parseRecipients(recipients)) {
        return;
    }
    QList<QByteArray> mailDestinations;
    QByteArray recipientHeaders;
    for (QList<QPair<RecipientKind,Imap::Message::MailAddress> >::const_iterator it = recipients.begin();
         it != recipients.end(); ++it) {
        mailDestinations << it->second.asSMTPMailbox();
        switch(it->first) {
        case Recipient_To:
            recipientHeaders.append("To: ").append(it->second.asMailHeader()).append("\r\n");
            break;
        case Recipient_Cc:
            recipientHeaders.append("Cc: ").append(it->second.asMailHeader()).append("\r\n");
            break;
        case Recipient_Bcc:
            break;
        }
    }

    if (mailDestinations.isEmpty()) {
        QMessageBox::critical(this, tr("No Way"), tr("You haven't entered any recipients"));
        return;
    }

    Imap::Message::MailAddress fromAddress;
    if (!parseOneAddress(fromAddress, ui->sender->currentText())) {
        gotError(tr("The From: address does not look like a valid one"));
        return;
    }

    QByteArray mailData;
    mailData.append("From: ").append(fromAddress.asMailHeader()).append("\r\n");
    mailData.append(recipientHeaders);
    mailData.append("Subject: ").append(encodeHeaderField(ui->subject->text())).append("\r\n");
    mailData.append("Content-Type: text/plain; charset=utf-8\r\n"
                    "Content-Transfer-Encoding: 8bit\r\n");

    //mailData.append( "Message-ID: <4A00C5A4.1080301@fzu.cz>\r\n" ); // FIXME
    QDateTime now = QDateTime::currentDateTime().toUTC(); // FIXME: will neeed proper timzeone...
    mailData.append("Date: ").append(now.toString(
                                         QString::fromAscii("ddd, dd MMM yyyy hh:mm:ss")).append(
                                         " +0000\r\n"));
    mailData.append("User-Agent: ").append(
        QString::fromAscii("%1/%2; %3")
                .arg(qApp->applicationName(), qApp->applicationVersion(), Imap::Mailbox::systemPlatformVersion()).toAscii()
    ).append("\r\n");
    mailData.append("MIME-Version: 1.0\r\n");
    if (!m_inReplyTo.isEmpty()) {
        mailData.append("In-Reply-To: ").append(m_inReplyTo).append("\r\n");
    }
    mailData.append("\r\n");
    mailData.append(ui->mailText->toPlainText().toUtf8());

    QProgressDialog *progress = new QProgressDialog(
        tr("Sending mail"), tr("Abort"), 0, mailData.size(), this);
    progress->setMinimumDuration(0);
    connect(msa, SIGNAL(progressMax(int)), progress, SLOT(setMaximum(int)));
    connect(msa, SIGNAL(progress(int)), progress, SLOT(setValue(int)));
    connect(msa, SIGNAL(error(QString)), progress, SLOT(close()));
    connect(msa, SIGNAL(sent()), progress, SLOT(close()));
    connect(msa, SIGNAL(sent()), this, SLOT(sent()));
    connect(msa, SIGNAL(sent()), this, SLOT(deleteLater()));
    connect(progress, SIGNAL(canceled()), msa, SLOT(cancel()));
    connect(msa, SIGNAL(error(QString)), this, SLOT(gotError(QString)));

    setEnabled(false);
    progress->setEnabled(true);

    msa->sendMail(fromAddress.asSMTPMailbox(), mailDestinations, mailData);
}

void ComposeWidget::setData(const QString &from, const QList<QPair<QString, QString> > &recipients,
                            const QString &subject, const QString &body, const QByteArray &inReplyTo)
{
    // FIXME: combobox for from...
    ui->sender->addItem(from);
    for (int i = 0; i < recipients.size(); ++i) {
        addRecipient(i, recipients[i].first, recipients[i].second);
    }
    if (recipients.isEmpty())
        addRecipient(0, tr("To"), QString());
    else
        addRecipient(recipients.size(), recipients.last().first, QString());
    ui->subject->setText(subject);
    ui->mailText->setText(body);
    m_inReplyTo = inReplyTo;
}

void ComposeWidget::addRecipient(int position, const QString &kind, const QString &address)
{
    QComboBox *combo = new QComboBox(this);
    QStringList toCcBcc = QStringList() << tr("To") << tr("Cc") << tr("Bcc");
    combo->addItems(toCcBcc);
    combo->setCurrentIndex(toCcBcc.indexOf(kind));
    QLineEdit *edit = new QLineEdit(address, this);
    edit->setCompleter(recipientCompleter);
    recipientsAddress.insert(position, edit);
    recipientsKind.insert(position, combo);
    connect(edit, SIGNAL(editingFinished()), this, SLOT(handleRecipientAddressChange()));
    ui->formLayout->insertRow(position + OFFSET_OF_FIRST_ADDRESSEE, combo, edit);
}

void ComposeWidget::handleRecipientAddressChange()
{
    QLineEdit *item = qobject_cast<QLineEdit *>(sender());
    Q_ASSERT(item);
    int index = recipientsAddress.indexOf(item);
    Q_ASSERT(index >= 0);

    if (index == recipientsAddress.size() - 1) {
        if (! item->text().isEmpty()) {
            addRecipient(index + 1, recipientsKind[ index ]->currentText(), QString());
            recipientsAddress[index + 1]->setFocus();
        }
    } else if (item->text().isEmpty() && recipientsAddress.size() != 1) {
        recipientsAddress.takeAt(index)->deleteLater();
        recipientsKind.takeAt(index)->deleteLater();

        delete ui->formLayout;
        ui->formLayout = new QFormLayout(this);

        // the first line
        ui->formLayout->addRow(ui->developmentWarning);
        ui->formLayout->addRow(ui->fromLabel, ui->sender);

        // note: number of layout items before this one has to match OFFSET_OF_FIRST_ADDRESSEE
        for (int i = 0; i < recipientsAddress.size(); ++i) {
            ui->formLayout->addRow(recipientsKind[ i ], recipientsAddress[ i ]);
        }

        // all other stuff
        ui->formLayout->addRow(ui->subjectLabel, ui->subject);
        ui->formLayout->addRow(ui->mailText);
        ui->formLayout->addRow(0, ui->buttonBox);
        setLayout(ui->formLayout);
    }
}

void ComposeWidget::gotError(const QString &error)
{
    QMessageBox::critical(this, tr("Failed to Send Mail"), error);
    setEnabled(true);
}

void ComposeWidget::sent()
{
    QMessageBox::information(this, tr("OK"), tr("Message Sent"));
    setEnabled(true);
}

QByteArray ComposeWidget::encodeHeaderField(const QString &text)
{
    /* This encodes an "unstructured" header field */
    /* FIXME: Don't apply RFC2047 if it isn't needed */
    return Imap::encodeRFC2047String(text);
}

/* A simple regexp to match an address typed into the input field. */
static QRegExp mailishRx("(?:\\b|\\<)(\\w+)\\s*\\@\\s*([\\w_.-]+|(?:\\[[^][\\\\\\\"\\s]+\\]))(?:\\b|\\>)");

/*
   This is of course far from complete, but at least catches "Real
   Name" <foo@bar>.  It needs to recognize the things people actually
   type, and it should also recognize anything that's a valid
   rfc2822 address.
*/
bool ComposeWidget::parseOneAddress(Imap::Message::MailAddress &into,
                                    const QString &address,
                                    int &startOffset)
{
    int offset;
    static QRegExp commaRx("^\\s*(?:,\\s*)*");

    offset = mailishRx.indexIn(address, startOffset);
    if (offset < 0) {
        /* Try stripping a leading comma? */
        offset = commaRx.indexIn(address, startOffset, QRegExp::CaretAtOffset);
        if (offset < startOffset)
            return false;
        offset += commaRx.matchedLength();
        startOffset = offset;
        offset = mailishRx.indexIn(address, offset);
        if (offset < 0)
            return false;
    }
    
    QString before = address.mid(startOffset, offset - startOffset);
    into = Imap::Message::MailAddress(before.simplified(), NULL,
                                      mailishRx.cap(1),
                                      mailishRx.cap(2));
    
    offset += mailishRx.matchedLength();
        
    int comma = commaRx.indexIn(address, offset, QRegExp::CaretAtOffset);
    if (comma >= offset)
        offset = comma + commaRx.matchedLength();
    
    startOffset = offset;
    return true;
}
bool ComposeWidget::parseOneAddress(Imap::Message::MailAddress &into,
                                    const QString &address)
{
    int offset = 0;

    if (!parseOneAddress(into, address, offset))
        return false;
    
    if (offset < address.size())
        return false;

    return true;
}

bool ComposeWidget::parseRecipients(QList<QPair<ComposeWidget::RecipientKind, Imap::Message::MailAddress> > &results)
{
    Q_ASSERT(recipientsAddress.size() == recipientsKind.size());
    for (int i = 0; i < recipientsAddress.size(); ++i) {
        RecipientKind kind = Recipient_To;
        if (recipientsKind[i]->currentText() == tr("Cc"))
            kind = Recipient_Cc;
        else if (recipientsKind[i]->currentText() == tr("Bcc"))
            kind = Recipient_Bcc;

        int offset = 0;
        QString text = recipientsAddress[i]->text();
        for(;;) {
            Imap::Message::MailAddress addr;
            bool ok = parseOneAddress(addr, text, offset);
            if (ok) {
                maybeAddNewKnownRecipient(addr);
                results << qMakePair(kind, addr);
            } else if (offset < text.size()) {
                QMessageBox::critical(this, tr("Invalid Address"),
                                      tr("Can't parse \"%1\" as an e-mail address").arg(text.mid(offset)));
                return false;
            } else {
                /* Successfully parsed the field. */
                break;
            }
        }
    }
    return true;
}

void ComposeWidget::maybeAddNewKnownRecipient(const Imap::Message::MailAddress &recipientAddress)
{
    // TODO: Could we store parsed addresses in the completer, and complete more intelligently against the realname/username parts?
    QString recipient = recipientAddress.prettyName(Imap::Message::MailAddress::FORMAT_READABLE);

    // let completer look for recipient
    recipientCompleter->setCompletionPrefix(recipient);
    // and if not found, insert it into completer's model
    if (recipientCompleter->currentCompletion().isEmpty()) {
        QAbstractItemModel *acMdl = recipientCompleter->model();
        acMdl->insertRow(0);
        QModelIndex idx = acMdl->index(0,0);
        acMdl->setData(idx,recipient);
    }
}

}

