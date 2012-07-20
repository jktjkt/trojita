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
#include <QBuffer>
#include <QCompleter>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>

#include "AutoCompletion.h"
#include "ComposeWidget.h"
#include "Window.h"
#include "ui_ComposeWidget.h"

#include "Common/SettingsNames.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Imap/Model/Model.h"
#include "Imap/Tasks/AppendTask.h"
#include "Imap/Tasks/GenUrlAuthTask.h"

namespace
{
enum { OFFSET_OF_FIRST_ADDRESSEE = 2 };
}

namespace Gui
{

ComposeWidget::ComposeWidget(MainWindow *parent, QAbstractListModel *autoCompleteModel) :
    QWidget(parent, Qt::Window),
    ui(new Ui::ComposeWidget),
    m_appendUidReceived(false), m_appendUidValidity(0), m_appendUid(0), m_genUrlAuthReceived(false),
    m_mainWindow(parent),
    recipientCompleter(new QCompleter(this))
{
    Q_ASSERT(m_mainWindow);
    m_composer = new Imap::Mailbox::MessageComposer(m_mainWindow->imapModel(), this);
    m_composer->setPreloadEnabled(!m_mainWindow->isCatenateSupported());
    ui->setupUi(this);
    sendButton = ui->buttonBox->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
    connect(sendButton, SIGNAL(clicked()), this, SLOT(send()));
    cancelButton = ui->buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(close()));
    ui->attachmentsView->setModel(m_composer);
    connect(ui->attachButton, SIGNAL(clicked()), this, SLOT(slotAskForFileAttachment()));
    ui->attachmentsView->setAcceptDrops(true);
    ui->attachmentsView->setDropIndicatorShown(true);
    ui->attachmentsView->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_actionRemoveAttachment = new QAction(tr("Remove"), this);
    connect(m_actionRemoveAttachment, SIGNAL(triggered()), this, SLOT(slotRemoveAttachment()));
    ui->attachmentsView->addAction(m_actionRemoveAttachment);

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

bool ComposeWidget::buildMessageData()
{
    QList<QPair<Imap::Mailbox::MessageComposer::RecipientKind,Imap::Message::MailAddress> > recipients;
    if (!parseRecipients(recipients)) {
        gotError(tr("Cannot parse recipients"));
        return false;
    }
    if (recipients.isEmpty()) {
        gotError(tr("You haven't entered any recipients"));
        return false;
    }
    m_composer->setRecipients(recipients);

    Imap::Message::MailAddress fromAddress;
    if (!Imap::Message::MailAddress::fromPrettyString(fromAddress, ui->sender->currentText())) {
        gotError(tr("The From: address does not look like a valid one"));
        return false;
    }
    m_composer->setFrom(fromAddress);

    m_composer->setTimestamp(QDateTime::currentDateTime());
    m_composer->setSubject(ui->subject->text());
    m_composer->setText(ui->mailText->toPlainText());

    return m_composer->isReadyForSerialization();
}

void ComposeWidget::send()
{

    if (!buildMessageData())
        return;

    using Common::SettingsNames;
    QSettings s;

    QByteArray rawMessageData;
    QBuffer buf(&rawMessageData);
    buf.open(QIODevice::WriteOnly);
    QString errorMessage;

    QPointer<Imap::Mailbox::AppendTask> appendTask = 0;
    m_appendUidReceived = false;
    m_genUrlAuthReceived = false;
    if (s.value(SettingsNames::composerSaveToImapKey, true).toBool()) {
        Q_ASSERT(m_mainWindow->imapModel());

        if (m_mainWindow->isCatenateSupported()) {
            QList<Imap::Mailbox::CatenatePair> catenateable;
            if (!m_composer->asCatenateData(catenateable, &errorMessage)) {
                gotError(tr("Cannot send right now -- saving (CATENATE) failed:\n %1").arg(errorMessage));
                return;
            }

            // FIXME: without UIDPLUS, there isn't much point in $SubmitPending...
            appendTask = QPointer<Imap::Mailbox::AppendTask>(
                        m_mainWindow->imapModel()->appendIntoMailbox(
                            s.value(SettingsNames::composerImapSentKey, tr("Sent")).toString(),
                            catenateable,
                            QStringList() << QLatin1String("$SubmitPending") << QLatin1String("\\Seen"),
                            m_composer->timestamp()));
        } else {
            if (!m_composer->asRawMessage(&buf, &errorMessage)) {
                gotError(tr("Cannot send right now -- saving failed:\n %1").arg(errorMessage));
                return;
            }

            // FIXME: without UIDPLUS, there isn't much point in $SubmitPending...
            appendTask = QPointer<Imap::Mailbox::AppendTask>(
                        m_mainWindow->imapModel()->appendIntoMailbox(
                            s.value(SettingsNames::composerImapSentKey, tr("Sent")).toString(),
                            rawMessageData,
                            QStringList() << QLatin1String("$SubmitPending") << QLatin1String("\\Seen"),
                            m_composer->timestamp()));
        }

        Q_ASSERT(appendTask);
        connect(appendTask.data(), SIGNAL(appendUid(uint,uint)), this, SLOT(slotAppendUidKnown(uint,uint)));
    }

    if (rawMessageData.isEmpty()) {
        if (!m_composer->asRawMessage(&buf, &errorMessage)) {
            gotError(tr("Cannot send right now:\n %1").arg(errorMessage));
            return;
        }
    }

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

    QProgressDialog *progress = new QProgressDialog(
        tr("Sending mail"), tr("Abort"), 0, rawMessageData.size(), this);
    progress->setMinimumDuration(0);
    progress->setMaximum(3);

    setEnabled(false);
    progress->setEnabled(true);
    if (appendTask) {
        progress->setLabelText(tr("Saving message..."));
    }
    // Message uploading through IMAP cannot really be terminated
    while (appendTask && !appendTask->isFinished()) {
        QCoreApplication::processEvents();
    }
    QPointer<Imap::Mailbox::GenUrlAuthTask> genUrlAuthTask;
    if (m_appendUidReceived) {
        progress->setValue(1);
        progress->setLabelText(tr("Generating IMAP URL..."));
        genUrlAuthTask = QPointer<Imap::Mailbox::GenUrlAuthTask>(
                    m_mainWindow->imapModel()->
                    generateUrlAuthForMessage(m_mainWindow->imapModel(),
                                              s.value(SettingsNames::imapHostKey).toString(),
                                              killDomainPartFromString(s.value(SettingsNames::imapUserKey).toString()),
                                              s.value(SettingsNames::composerImapSentKey, tr("Sent")).toString(),
                                              m_appendUidValidity, m_appendUid, QString(),
                                              QString::fromAscii("submit+%1").arg(
                        killDomainPartFromString(s.value(SettingsNames::smtpUserKey).toString()))
                                              ));
        connect(genUrlAuthTask.data(), SIGNAL(gotAuth(QString)), this, SLOT(slotGenUrlAuthReceived(QString)));
        while (genUrlAuthTask && !genUrlAuthTask->isFinished()) {
            QCoreApplication::processEvents();
        }
    }
    progress->setLabelText(tr("Sending mail..."));
    progress->setValue(2);

    connect(msa, SIGNAL(progressMax(int)), progress, SLOT(setMaximum(int)));
    connect(msa, SIGNAL(progress(int)), progress, SLOT(setValue(int)));
    connect(msa, SIGNAL(error(QString)), progress, SLOT(close()));
    connect(msa, SIGNAL(sent()), progress, SLOT(close()));
    connect(msa, SIGNAL(sent()), this, SLOT(sent()));
    connect(msa, SIGNAL(sent()), this, SLOT(deleteLater()));
    connect(progress, SIGNAL(canceled()), msa, SLOT(cancel()));
    connect(msa, SIGNAL(error(QString)), this, SLOT(gotError(QString)));

    if (m_genUrlAuthReceived && s.value(SettingsNames::smtpUseBurlKey, false).toBool()) {
        msa->sendBurl(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), m_urlauth.toAscii());
    } else {
        return;
        msa->sendMail(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), rawMessageData);
    }
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
    m_composer->setInReplyTo(inReplyTo);
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

bool ComposeWidget::parseRecipients(QList<QPair<Imap::Mailbox::MessageComposer::RecipientKind, Imap::Message::MailAddress> > &results)
{
    Q_ASSERT(recipientsAddress.size() == recipientsKind.size());
    for (int i = 0; i < recipientsAddress.size(); ++i) {
        Imap::Mailbox::MessageComposer::RecipientKind kind = Imap::Mailbox::MessageComposer::Recipient_To;
        if (recipientsKind[i]->currentText() == tr("Cc"))
            kind = Imap::Mailbox::MessageComposer::Recipient_Cc;
        else if (recipientsKind[i]->currentText() == tr("Bcc"))
            kind = Imap::Mailbox::MessageComposer::Recipient_Bcc;

        int offset = 0;
        QString text = recipientsAddress[i]->text();
        for(;;) {
            Imap::Message::MailAddress addr;
            bool ok = Imap::Message::MailAddress::parseOneAddress(addr, text, offset);
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

void ComposeWidget::slotAskForFileAttachment()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Attach File..."), QString(), QString(), 0,
                                                    QFileDialog::DontResolveSymlinks);
    if (!fileName.isEmpty()) {
        m_composer->addFileAttachment(fileName);
    }
}

void ComposeWidget::slotRemoveAttachment()
{
    m_composer->removeAttachment(ui->attachmentsView->currentIndex());
}

void ComposeWidget::slotAppendUidKnown(const uint uidValidity, const uint uid)
{
    m_appendUidValidity = uidValidity;
    m_appendUid = uid;

    if (m_appendUid && m_appendUidValidity) {
        // Only ever consider valid UIDVALIDITY/UID pair
        m_appendUidReceived = true;
    }
}

void ComposeWidget::slotGenUrlAuthReceived(const QString &url)
{
    m_urlauth = url;
    if (!m_urlauth.isEmpty()) {
        m_genUrlAuthReceived = true;
    }
}

QString ComposeWidget::killDomainPartFromString(const QString &s)
{
    return s.split(QLatin1Char('@'))[0];
}

}

