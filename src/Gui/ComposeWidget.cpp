/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#include <QFileDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>

#include "AbstractAddressbook.h"
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
#include "Imap/Tasks/UidSubmitTask.h"

namespace
{
enum { OFFSET_OF_FIRST_ADDRESSEE = 1 };
}

namespace Gui
{

ComposeWidget::ComposeWidget(MainWindow *parent) :
    QWidget(parent, Qt::Window),
    ui(new Ui::ComposeWidget),
    m_appendUidReceived(false), m_appendUidValidity(0), m_appendUid(0), m_genUrlAuthReceived(false),
    m_mainWindow(parent)
{
    Q_ASSERT(m_mainWindow);
    m_composer = new Imap::Mailbox::MessageComposer(m_mainWindow->imapModel(), this);
    m_composer->setPreloadEnabled(shouldBuildMessageLocally());

    ui->setupUi(this);
    sendButton = ui->buttonBox->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
    connect(sendButton, SIGNAL(clicked()), this, SLOT(send()));
    cancelButton = ui->buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(close()));
    ui->attachmentsView->setModel(m_composer);
    connect(ui->attachButton, SIGNAL(clicked()), this, SLOT(slotAskForFileAttachment()));
    ui->attachmentsView->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_actionRemoveAttachment = new QAction(tr("Remove"), this);
    connect(m_actionRemoveAttachment, SIGNAL(triggered()), this, SLOT(slotRemoveAttachment()));
    ui->attachmentsView->addAction(m_actionRemoveAttachment);

    connect(ui->attachmentsView, SIGNAL(itemDroppedOut()), SLOT(slotRemoveAttachment()));

    m_completionPopup = new QMenu(this);
    m_completionPopup->installEventFilter(this);
    connect (m_completionPopup, SIGNAL(triggered(QAction*)), SLOT(completeRecipient(QAction*)));

    // TODO: make this configurable?
    m_completionCount = 8;

    m_recipientListUpdateTimer = new QTimer(this);
    m_recipientListUpdateTimer->setSingleShot(true);
    m_recipientListUpdateTimer->setInterval(250);
    connect(m_recipientListUpdateTimer, SIGNAL(timeout()), SLOT(updateRecipientList()));

    // Ask for a fixed-width font. The problem is that these names wary acros platforms,
    // but the following works well -- at first, we come up with a made-up name, and then
    // let the Qt font substitution algorithm do its magic.
    QFont font(QLatin1String("x-trojita-terminus-like-fixed-width"));
    font.setStyleHint(QFont::TypeWriter);
    ui->mailText->setFont(font);

    connect(ui->mailText, SIGNAL(urlsAdded(QList<QUrl>)), SLOT(slotAttachFiles(QList<QUrl>)));
    connect(ui->mailText, SIGNAL(sendRequest()), SLOT(send()));
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
    QList<QPair<RecipientKind,Imap::Message::MailAddress> > recipients;
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

    if (rawMessageData.isEmpty() && shouldBuildMessageLocally()) {
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
    } else if (method == SettingsNames::methodSENDMAIL) {
        QStringList args = s.value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("Please configure the SMTP or sendmail settings in application settings."));
            return;
        }
        QString appName = args.takeFirst();
        msa = new MSA::Sendmail(this, appName, args);
    } else if (method == SettingsNames::methodImapSendmail) {
        if (!m_mainWindow->isImapSubmissionSupported()) {
            QMessageBox::critical(this, tr("Error"), tr("The IMAP server does not support mail submission. Please reconfigure the application."));
            return;
        }
        // no particular preparation needed here
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Please configure e-mail delivery method in application settings."));
        return;
    }

    QProgressDialog *progress = new QProgressDialog(
        tr("Sending mail"), tr("Abort"), 0, rawMessageData.size(), this);
    progress->setMinimumDuration(0);
    progress->setMaximum(3);

    // Message uploading through IMAP cannot really be terminated
    setEnabled(false);
    progress->setEnabled(true);

    if (appendTask) {
        progress->setLabelText(tr("Saving message..."));
    }

    while (appendTask && !appendTask->isFinished()) {
        // FIXME: get rid of this busy wait, eventually
        QCoreApplication::processEvents();
    }

    if (method == SettingsNames::methodImapSendmail) {
        if (!m_appendUidReceived) {
            QMessageBox::critical(this, tr("Error"), tr("Cannot send over IMAP, APPENDUID failed"));
            return;
        }
        Imap::Mailbox::UidSubmitOptionsList options;
        options.append(qMakePair<QByteArray,QVariant>("FROM", m_composer->rawFromAddress()));
        Q_FOREACH(const QByteArray recipient, m_composer->rawRecipientAddresses()) {
            options.append(qMakePair<QByteArray,QVariant>("RECIPIENT", recipient));
        }
        Imap::Mailbox::UidSubmitTask *submitTask = m_mainWindow->imapModel()->sendMailViaUidSubmit(
                    s.value(SettingsNames::composerImapSentKey, tr("Sent")).toString(), m_appendUidValidity, m_appendUid,
                    options);
        Q_ASSERT(submitTask);
        connect(submitTask, SIGNAL(completed(ImapTask*)), this, SLOT(sent()));
        connect(submitTask, SIGNAL(failed(QString)), this, SLOT(gotError(QString)));
        progress->setLabelText(tr("Sending mail..."));
        progress->setValue(2);
        while (submitTask && !submitTask->isFinished()) {
            QCoreApplication::processEvents();
        }
        progress->cancel();
        return;
    }

    QPointer<Imap::Mailbox::GenUrlAuthTask> genUrlAuthTask;
    if (m_appendUidReceived && s.value(SettingsNames::smtpUseBurlKey, false).toBool() && m_mainWindow->isGenUrlAuthSupported()) {
        progress->setValue(1);
        progress->setLabelText(tr("Generating IMAP URL..."));
        genUrlAuthTask = QPointer<Imap::Mailbox::GenUrlAuthTask>(
                    m_mainWindow->imapModel()->
                    generateUrlAuthForMessage(s.value(SettingsNames::imapHostKey).toString(),
                                              killDomainPartFromString(s.value(SettingsNames::imapUserKey).toString()),
                                              s.value(SettingsNames::composerImapSentKey, tr("Sent")).toString(),
                                              m_appendUidValidity, m_appendUid, QString(),
                                              QString::fromUtf8("submit+%1").arg(
                        killDomainPartFromString(s.value(SettingsNames::smtpUserKey).toString()))
                                              ));
        connect(genUrlAuthTask.data(), SIGNAL(gotAuth(QString)), this, SLOT(slotGenUrlAuthReceived(QString)));
        while (genUrlAuthTask && !genUrlAuthTask->isFinished()) {
            // FIXME: get rid of this busy wait, eventually
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
        msa->sendBurl(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), m_urlauth.toUtf8());
    } else {
        msa->sendMail(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), rawMessageData);
    }
}



void ComposeWidget::setData(const QString &from, const QList<QPair<RecipientKind, QString> > &recipients,
                            const QString &subject, const QString &body, const QList<QByteArray> &inReplyTo,
                            const QList<QByteArray> &references)
{
    // FIXME: combobox for from...
    ui->sender->addItem(from);
    for (int i = 0; i < recipients.size(); ++i) {
        addRecipient(i, recipients.at(i).first, recipients.at(i).second);
    }
    if (recipients.isEmpty())
        addRecipient(0, Imap::Mailbox::MessageComposer::Recipient_To, QString());
    else
        addRecipient(recipients.size(), recipients.last().first, QString());
    ui->subject->setText(subject);
    ui->mailText->setText(body);
    m_composer->setInReplyTo(inReplyTo);
    m_composer->setReferences(references);
}

//BEGIN QFormLayout workarounds

/** First issue: QFormLayout messes up rows by never removing them
 * ----------------------------------------------------------------
 * As a result insertRow(int pos, .) does not pick the expected row, but usually minor
 * (if you ever removed all items of a row in this layout)
 *
 * Solution: we count all rows non empty rows and when we have enough, return the row suitable for
 * QFormLayout (which is usually behind the requested one)
 */
static int actualRow(QFormLayout *form, int row)
{
    for (int i = 0, c = 0; i < form->rowCount(); ++i) {
        if (c == row) {
            return i;
        }
        if (form->itemAt(i, QFormLayout::LabelRole) || form->itemAt(i, QFormLayout::FieldRole) ||
            form->itemAt(i, QFormLayout::SpanningRole))
            ++c;
    }
    return form->rowCount(); // append
}

/** Second (related) issue: QFormLayout messes the tab order
 * ----------------------------------------------------------
 * "Inserted" rows just get appended to the present ones and by this to the tab focus order
 * It's therefore necessary to fix this forcing setTabOrder()
 *
 * Approach: traverse all rows until we have the widget that shall be inserted in tab order and
 * return it's predecessor
 */

static QWidget* formPredecessor(QFormLayout *form, QWidget *w)
{
    QWidget *pred = 0;
    QWidget *runner = 0;
    QLayoutItem *item = 0;
    for (int i = 0; i < form->rowCount(); ++i) {
        if ((item = form->itemAt(i, QFormLayout::LabelRole))) {
            runner = item->widget();
            if (runner == w)
                return pred;
            else if (runner)
                pred = runner;
        }
        if ((item = form->itemAt(i, QFormLayout::FieldRole))) {
            runner = item->widget();
            if (runner == w)
                return pred;
            else if (runner)
                pred = runner;
        }
        if ((item = form->itemAt(i, QFormLayout::SpanningRole))) {
            runner = item->widget();
            if (runner == w)
                return pred;
            else if (runner)
                pred = runner;
        }
    }
    return pred;
}

//END QFormLayout workarounds


void ComposeWidget::addRecipient(int position, RecipientKind kind, const QString &address)
{
    QComboBox *combo = new QComboBox(this);
    combo->addItem(tr("To"), Imap::Mailbox::MessageComposer::Recipient_To);
    combo->addItem(tr("Cc"), Imap::Mailbox::MessageComposer::Recipient_Cc);
    combo->addItem(tr("Bcc"), Imap::Mailbox::MessageComposer::Recipient_Bcc);
    combo->setCurrentIndex(combo->findData(kind));
    QLineEdit *edit = new QLineEdit(address, this);
    connect(edit, SIGNAL(textEdited(QString)), SLOT(completeRecipients(QString)));
    connect(edit, SIGNAL(editingFinished()), SLOT(collapseRecipients()));
    connect(edit, SIGNAL(textChanged(QString)), m_recipientListUpdateTimer, SLOT(start()));
    m_recipients.insert(position, Recipient(combo, edit));
    ui->envelopeLayout->insertRow(actualRow(ui->envelopeLayout, position + OFFSET_OF_FIRST_ADDRESSEE), combo, edit);
    setTabOrder(formPredecessor(ui->envelopeLayout, combo), combo);
    setTabOrder(combo, edit);
}

void ComposeWidget::removeRecipient(int pos)
{
    // removing the widgets from the layout is important
    // a) not doing so leaks (minor)
    // b) deleteLater() crosses the evenchain and so our actualRow funtion would be tricked
    ui->envelopeLayout->removeWidget(m_recipients.at(pos).first);
    ui->envelopeLayout->removeWidget(m_recipients.at(pos).second);
    m_recipients.at(pos).first->deleteLater();
    m_recipients.at(pos).second->deleteLater();
    m_recipients.removeAt(pos);
}

static inline ComposeWidget::RecipientKind currentRecipient(const QComboBox *box)
{
    return ComposeWidget::RecipientKind(box->itemData(box->currentIndex()).toInt());
}

void ComposeWidget::updateRecipientList()
{
    // we ensure there's always one empty available
    bool haveEmpty = false;
    for (int i = 0; i < m_recipients.count(); ++i) {
        if (m_recipients.at(i).second->text().isEmpty()) {
            if (haveEmpty) {
                removeRecipient(i);
            }
            haveEmpty = true;
        }
    }
    if (!haveEmpty)
        addRecipient(m_recipients.count(), currentRecipient(m_recipients.last().first), QString());
}

void ComposeWidget::collapseRecipients()
{
    QLineEdit *edit = qobject_cast<QLineEdit*>(sender());
    Q_ASSERT(edit);
    if (edit->hasFocus() || !edit->text().isEmpty())
        return; // nothing to clean up

    // an empty recipient line just lost focus -> we "place it at the end", ie. simply remove it
    // and append a clone
    bool needEmpty = false;
    RecipientKind carriedKind = Imap::Mailbox::MessageComposer::Recipient_To;
    for (int i = 0; i < m_recipients.count() - 1; ++i) { // sic! on the -1, no action if it trails anyway
        if (m_recipients.at(i).second == edit) {
            carriedKind = currentRecipient(m_recipients.last().first);
            removeRecipient(i);
            needEmpty = true;
            break;
        }
    }
    if (needEmpty)
        addRecipient(m_recipients.count(), carriedKind, QString());
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

bool ComposeWidget::parseRecipients(QList<QPair<RecipientKind, Imap::Message::MailAddress> > &results)
{
    for (int i = 0; i < m_recipients.size(); ++i) {
        RecipientKind kind = currentRecipient(m_recipients.at(i).first);

        int offset = 0;
        QString text = m_recipients.at(i).second->text();
        for(;;) {
            Imap::Message::MailAddress addr;
            bool ok = Imap::Message::MailAddress::parseOneAddress(addr, text, offset);
            if (ok) {
                // TODO: should we *really* learn every junk entered into a recipient field?
                // m_mainWindow->addressBook()->learn(addr);
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

void ComposeWidget::completeRecipients(const QString &text)
{
    if (text.isEmpty()) {
        if (m_completionPopup) {
            // if there's a popup close it and set back the receiver
            m_completionPopup->close();
            m_completionReceiver = 0;
        }
        return; // we do not suggest "nothing"
    }
    Q_ASSERT(sender());
    QLineEdit *toEdit = static_cast<QLineEdit*>(sender());
    QStringList contacts = m_mainWindow->addressBook()->complete(text, QStringList(), m_completionCount);
    if (contacts.isEmpty() && m_completionPopup) {
        m_completionPopup->close();
        m_completionReceiver = 0;
    }
    else {
        m_completionReceiver = toEdit;
        m_completionPopup->setUpdatesEnabled(false);
        m_completionPopup->clear();
        Q_FOREACH(const QString &s, contacts)
            m_completionPopup->addAction(s);
        if (m_completionPopup->isHidden())
            m_completionPopup->popup(toEdit->mapToGlobal(QPoint(0, toEdit->height())));
        m_completionPopup->setUpdatesEnabled(true);
    }
}

void ComposeWidget::completeRecipient(QAction *act)
{
    if (act->text().isEmpty())
        return;
    m_completionReceiver->setText(act->text());
    if (m_completionPopup) {
        m_completionPopup->close();
        m_completionReceiver = 0;
    }
}

bool ComposeWidget::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_completionPopup) {
        if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            if (!(  ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down || // Navigation
                    ke->key() == Qt::Key_Escape || // "escape"
                    ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)) { // selection
                QCoreApplication::sendEvent(m_completionReceiver, e);
                return true;
            }
        }
        return false;
    }
    return false;
}


void ComposeWidget::slotAskForFileAttachment()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Attach File..."), QString(), QString(), 0,
                                                    QFileDialog::DontResolveSymlinks);
    if (!fileName.isEmpty()) {
        m_composer->addFileAttachment(fileName);
    }
}

void ComposeWidget::slotAttachFiles(QList<QUrl> urls)
{
    foreach (const QUrl &url, urls) {

#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
        if (url.isLocalFile()) {
#else
        if (url.scheme() == QLatin1String("file")) {
#endif
            m_composer->addFileAttachment(url.path());
        }
    }
}

void ComposeWidget::slotRemoveAttachment()
{
    m_composer->removeAttachment(ui->attachmentsView->currentIndex());
}

/** @short Remember the APPENDUID as reported by the APPEND operation */
void ComposeWidget::slotAppendUidKnown(const uint uidValidity, const uint uid)
{
    m_appendUidValidity = uidValidity;
    m_appendUid = uid;

    if (m_appendUid && m_appendUidValidity) {
        // Only ever consider valid UIDVALIDITY/UID pair
        m_appendUidReceived = true;
    }
}

/** @short Remember the GENURLAUTH response */
void ComposeWidget::slotGenUrlAuthReceived(const QString &url)
{
    m_urlauth = url;
    if (!m_urlauth.isEmpty()) {
        m_genUrlAuthReceived = true;
    }
}

/** @short Remove the "@domain" from a string */
QString ComposeWidget::killDomainPartFromString(const QString &s)
{
    return s.split(QLatin1Char('@'))[0];
}

/** @short Return true if the message payload shall be built locally */
bool ComposeWidget::shouldBuildMessageLocally() const
{
    // Unless all of URLAUTH, CATENATE and BURL is present and enabled, we will still have to download the data in the end
    return ! (m_mainWindow->isCatenateSupported() && m_mainWindow->isGenUrlAuthSupported()
              && QSettings().value(Common::SettingsNames::smtpUseBurlKey, false).toBool());
}

}

