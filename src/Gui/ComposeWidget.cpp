/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2012 Peter Amidon <peter@picnicpark.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QAbstractProxyModel>
#include <QBuffer>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QToolButton>

#include "ui_ComposeWidget.h"
#include "Composer/MessageComposer.h"
#include "Composer/ReplaceSignature.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Composer/Submission.h"
#include "Common/InvokeMethod.h"
#include "Common/Paths.h"
#include "Common/SettingsNames.h"
#include "Gui/AbstractAddressbook.h"
#include "Gui/AutoCompletion.h"
#include "Gui/ComposeWidget.h"
#include "Gui/FromAddressProxyModel.h"
#include "Gui/IconLoader.h"
#include "Gui/LineEdit.h"
#include "Gui/OverlayWidget.h"
#include "Gui/PasswordDialog.h"
#include "Gui/ProgressPopUp.h"
#include "Gui/Util.h"
#include "Gui/Window.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Tasks/AppendTask.h"
#include "Imap/Tasks/GenUrlAuthTask.h"
#include "Imap/Tasks/UidSubmitTask.h"

namespace
{
enum { OFFSET_OF_FIRST_ADDRESSEE = 1, MIN_MAX_VISIBLE_RECIPIENTS = 4 };
}

namespace Gui
{

static const QString trojita_opacityAnimation = QLatin1String("trojita_opacityAnimation");

ComposeWidget::ComposeWidget(MainWindow *mainWindow, QSettings *settings, MSA::MSAFactory *msaFactory) :
    QWidget(0, Qt::Window),
    ui(new Ui::ComposeWidget),
    m_maxVisibleRecipients(MIN_MAX_VISIBLE_RECIPIENTS),
    m_sentMail(false),
    m_messageUpdated(false),
    m_messageEverEdited(false),
    m_explicitDraft(false),
    m_appendUidReceived(false), m_appendUidValidity(0), m_appendUid(0), m_genUrlAuthReceived(false),
    m_mainWindow(mainWindow),
    m_settings(settings)
{
    setAttribute(Qt::WA_DeleteOnClose, true);

    QIcon winIcon;
    winIcon.addFile(QLatin1String(":/icons/trojita-edit-big.png"), QSize(128, 128));
    winIcon.addFile(QLatin1String(":/icons/trojita-edit-small.png"), QSize(22, 22));
    setWindowIcon(winIcon);

    Q_ASSERT(m_mainWindow);
    m_submission = new Composer::Submission(this, m_mainWindow->imapModel(), msaFactory);
    connect(m_submission, SIGNAL(succeeded()), this, SLOT(sent()));
    connect(m_submission, SIGNAL(failed(QString)), this, SLOT(gotError(QString)));
    connect(m_submission, SIGNAL(passwordRequested(QString,QString)), this, SLOT(passwordRequested(QString,QString)), Qt::QueuedConnection);

    ui->setupUi(this);
    ui->attachmentsView->setComposer(m_submission->composer());
    sendButton = ui->buttonBox->addButton(tr("Send"), QDialogButtonBox::AcceptRole);
    connect(sendButton, SIGNAL(clicked()), this, SLOT(send()));
    cancelButton = ui->buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->attachButton, SIGNAL(clicked()), this, SLOT(slotAskForFileAttachment()));

    m_completionPopup = new QMenu(this);
    m_completionPopup->installEventFilter(this);
    connect(m_completionPopup, SIGNAL(triggered(QAction*)), SLOT(completeRecipient(QAction*)));

    // TODO: make this configurable?
    m_completionCount = 8;

    m_recipientListUpdateTimer = new QTimer(this);
    m_recipientListUpdateTimer->setSingleShot(true);
    m_recipientListUpdateTimer->setInterval(250);
    connect(m_recipientListUpdateTimer, SIGNAL(timeout()), SLOT(updateRecipientList()));

    connect(ui->verticalSplitter, SIGNAL(splitterMoved(int, int)), SLOT(calculateMaxVisibleRecipients()));
    calculateMaxVisibleRecipients();

    connect(ui->recipientSlider, SIGNAL(valueChanged(int)), SLOT(scrollRecipients(int)));
    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), SLOT(handleFocusChange()));
    ui->recipientSlider->setMinimum(0);
    ui->recipientSlider->setMaximum(0);
    ui->recipientSlider->setVisible(false);
    ui->envelopeWidget->installEventFilter(this);

    m_markButton = new QToolButton(ui->buttonBox);
    m_markButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_markButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_markAsReply = new QActionGroup(m_markButton);
    m_markAsReply->setExclusive(true);
    auto *asReplyMenu = new QMenu(m_markButton);
    m_markButton->setMenu(asReplyMenu);
    m_actionStandalone = asReplyMenu->addAction(Gui::loadIcon(QLatin1String("mail-view-flat")), tr("New Thread"));
    m_actionStandalone->setActionGroup(m_markAsReply);
    m_actionStandalone->setCheckable(true);
    m_actionStandalone->setToolTip(tr("This mail will be sent as a standalone message.<hr/>Change to preserve the reply hierarchy."));
    m_actionInReplyTo = asReplyMenu->addAction(Gui::loadIcon(QLatin1String("mail-view-threaded")), tr("Threaded"));
    m_actionInReplyTo->setActionGroup(m_markAsReply);
    m_actionInReplyTo->setCheckable(true);

    // This is a "quick shortcut action". It shows the UI bits of the current option, but when the user clicks it,
    // the *other* action is triggered.
    m_actionToggleMarking = new QAction(m_markButton);
    connect(m_actionToggleMarking, SIGNAL(triggered()), this, SLOT(toggleReplyMarking()));
    m_markButton->setDefaultAction(m_actionToggleMarking);

    // Unfortunately, there's no signal for toggled(QAction*), so we'll have to call QAction::trigger() to have this working
    connect(m_markAsReply, SIGNAL(triggered(QAction*)), this, SLOT(updateReplyMarkingAction()));
    m_actionStandalone->trigger();
    // We want to have the button aligned to the left; the only "portable" way of this is the ResetRole
    // (thanks to TL for mentioning this, and for the Qt's doc for providing pretty pictures on different platforms)
    ui->buttonBox->addButton(m_markButton, QDialogButtonBox::ResetRole);
    m_markButton->hide();

    ui->mailText->setFont(Gui::Util::systemMonospaceFont());

    connect(ui->mailText, SIGNAL(urlsAdded(QList<QUrl>)), SLOT(slotAttachFiles(QList<QUrl>)));
    connect(ui->mailText, SIGNAL(sendRequest()), SLOT(send()));
    connect(ui->mailText, SIGNAL(textChanged()), SLOT(setMessageUpdated()));
    connect(ui->subject, SIGNAL(textChanged(QString)), SLOT(updateWindowTitle()));

    FromAddressProxyModel *proxy = new FromAddressProxyModel(this);
    proxy->setSourceModel(m_mainWindow->senderIdentitiesModel());
    ui->sender->setModel(proxy);

    connect(ui->sender, SIGNAL(currentIndexChanged(int)), SLOT(slotUpdateSignature()));
    connect(ui->sender, SIGNAL(editTextChanged(QString)), SLOT(setMessageUpdated()));
    connect(ui->sender->lineEdit(), SIGNAL(textChanged(QString)), SLOT(slotCheckAddress()));

    QTimer *autoSaveTimer = new QTimer(this);
    connect(autoSaveTimer, SIGNAL(timeout()), SLOT(autoSaveDraft()));
    autoSaveTimer->start(30*1000);

    // these are for the automatically saved drafts, i.e. no i18n for the dir name
    m_autoSavePath = QString(Common::writablePath(Common::LOCATION_CACHE) + QLatin1String("Drafts/"));
    QDir().mkpath(m_autoSavePath);

#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
    m_autoSavePath += QString::number(QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0)).secsTo(QDateTime::currentDateTime())) + QLatin1String(".draft");
#else
    m_autoSavePath += QString::number(QDateTime::currentMSecsSinceEpoch()) + QLatin1String(".draft");
#endif
}

ComposeWidget::~ComposeWidget()
{
    delete ui;
}

void ComposeWidget::passwordRequested(const QString &user, const QString &host)
{
    bool ok;
    const QString &password = Gui::PasswordDialog::getPassword(this, tr("Authentication Required"),
                                           tr("<p>Please provide SMTP password for user <b>%1</b> on <b>%2</b>:</p>").arg(
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                                               Qt::escape(user),
                                               Qt::escape(host)
#else
                                               user.toHtmlEscaped(),
                                               host.toHtmlEscaped()
#endif
                                               ),
                                           QString(), &ok);
    if (ok)
        m_submission->setPassword(password);
    else
        m_submission->cancelPassword();
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

/**
 * We capture the close event and check whether there's something to save
 * (not sent, not up-to-date or persistent autostore)
 * The offer the user to store or omit the message or not close at all
 */

void ComposeWidget::closeEvent(QCloseEvent *ce)
{
    const bool noSaveRequired = m_sentMail || ui->mailText->document()->isEmpty() || !m_messageEverEdited ||
                                (m_explicitDraft && !m_messageUpdated); // autosave to permanent draft and no update
    if (!noSaveRequired) {  // save is required
        QMessageBox msgBox(this);
        msgBox.setWindowModality(Qt::WindowModal);
        msgBox.setWindowTitle(tr("Save Draft?"));
        QString message(tr("The mail has not been sent.<br>Do you want to save the draft?"));
        if (ui->attachmentsView->model()->rowCount() > 0)
            message += tr("<br><span style=\"color:red\">Warning: Attachments are <b>not</b> saved with the draft!</span>");
        msgBox.setText(message);
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        if (ret == QMessageBox::Save) {
            if (m_explicitDraft) { // editing a present draft - override it
                saveDraft(m_autoSavePath);
            } else {
                // Explicitly stored drafts should be saved in a location with proper i18n support, so let's make sure both main
                // window and this code uses the same tr() calls
                QString path(Common::writablePath(Common::LOCATION_DATA) + Gui::MainWindow::tr("Drafts"));
                QDir().mkpath(path);
                QString filename = ui->subject->text();
                if (filename.isEmpty()) {
                    filename = QDateTime::currentDateTime().toString(Qt::ISODate);
                }
                // Some characters are best avoided in file names. This is probably not a definitive list, but the hope is that
                // it's going to be more readable than an unformatted hash or similar stuff.  The list of characters was taken
                // from http://en.wikipedia.org/wiki/Filename#Reserved_characters_and_words .
                filename.replace(QRegExp(QLatin1String("[/\\\\:\"|<>*?]")), QLatin1String("_"));
                path = QFileDialog::getSaveFileName(this, tr("Save as"), path + QLatin1Char('/') + filename + QLatin1String(".draft"),
                                                    tr("Drafts") + QLatin1String(" (*.draft)"));
                if (path.isEmpty()) { // cancelled save
                    ret = QMessageBox::Cancel;
                } else {
                    m_explicitDraft = true;
                    saveDraft(path);
                    if (path != m_autoSavePath) // we can remove the temp save
                        QFile::remove(m_autoSavePath);
                }
            }
        }
        if (ret == QMessageBox::Cancel) {
            ce->ignore(); // don't close the window
            return;
        }
    }
    if (m_sentMail || !m_explicitDraft) // is the mail has been sent or the user does not want to store it
        QFile::remove(m_autoSavePath); // get rid of draft
    ce->accept(); // ultimately close the window
}



bool ComposeWidget::buildMessageData()
{
    QList<QPair<Composer::RecipientKind,Imap::Message::MailAddress> > recipients;
    QString errorMessage;
    if (!parseRecipients(recipients, errorMessage)) {
        gotError(tr("Cannot parse recipients:\n%1").arg(errorMessage));
        return false;
    }
    if (recipients.isEmpty()) {
        gotError(tr("You haven't entered any recipients"));
        return false;
    }
    m_submission->composer()->setRecipients(recipients);

    Imap::Message::MailAddress fromAddress;
    if (!Imap::Message::MailAddress::fromPrettyString(fromAddress, ui->sender->currentText())) {
        gotError(tr("The From: address does not look like a valid one"));
        return false;
    }
    if (ui->subject->text().isEmpty()) {
        gotError(tr("You haven't entered any subject. Cannot send such a mail, sorry."));
        ui->subject->setFocus();
        return false;
    }
    m_submission->composer()->setFrom(fromAddress);

    m_submission->composer()->setTimestamp(QDateTime::currentDateTime());
    m_submission->composer()->setSubject(ui->subject->text());

    QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(ui->sender->model());
    Q_ASSERT(proxy);

    if (ui->sender->findText(ui->sender->currentText()) != -1) {
        QModelIndex proxyIndex = ui->sender->model()->index(ui->sender->currentIndex(), 0, ui->sender->rootModelIndex());
        Q_ASSERT(proxyIndex.isValid());
        m_submission->composer()->setOrganization(
                    proxy->mapToSource(proxyIndex).sibling(proxyIndex.row(), Composer::SenderIdentitiesModel::COLUMN_ORGANIZATION)
                    .data().toString());
    }
    m_submission->composer()->setText(ui->mailText->toPlainText());

    if (m_actionInReplyTo->isChecked()) {
        m_submission->composer()->setInReplyTo(m_inReplyTo);
        m_submission->composer()->setReferences(m_references);
        m_submission->composer()->setReplyingToMessage(m_replyingToMessage);
    } else {
        m_submission->composer()->setInReplyTo(QList<QByteArray>());
        m_submission->composer()->setReferences(QList<QByteArray>());
        m_submission->composer()->setReplyingToMessage(QModelIndex());
    }

    return m_submission->composer()->isReadyForSerialization();
}

void ComposeWidget::send()
{
    // Well, Trojita is of course rock solid and will never ever crash :), but experience has shown that every now and then,
    // there is a subtle issue $somewhere. This means that it's probably a good idea to save the draft explicitly -- better
    // than losing some work. It's cheap anyway.
    saveDraft(m_autoSavePath);

    if (!buildMessageData())
        return;

    m_submission->setImapOptions(m_settings->value(Common::SettingsNames::composerSaveToImapKey, true).toBool(),
                                 m_settings->value(Common::SettingsNames::composerImapSentKey, tr("Sent")).toString(),
                                 m_settings->value(Common::SettingsNames::imapHostKey).toString(),
                                 m_settings->value(Common::SettingsNames::imapUserKey).toString(),
                                 m_settings->value(Common::SettingsNames::msaMethodKey).toString() == Common::SettingsNames::methodImapSendmail);
    m_submission->setSmtpOptions(m_settings->value(Common::SettingsNames::smtpUseBurlKey, false).toBool(),
                                 m_settings->value(Common::SettingsNames::smtpUserKey).toString());


    ProgressPopUp *progress = new ProgressPopUp();
    OverlayWidget *overlay = new OverlayWidget(progress, this);
    overlay->show();
    setUiWidgetsEnabled(false);

    connect(m_submission, SIGNAL(progressMin(int)), progress, SLOT(setMinimum(int)));
    connect(m_submission, SIGNAL(progressMax(int)), progress, SLOT(setMaximum(int)));
    connect(m_submission, SIGNAL(progress(int)), progress, SLOT(setValue(int)));
    connect(m_submission, SIGNAL(updateStatusMessage(QString)), progress, SLOT(setLabelText(QString)));
    connect(m_submission, SIGNAL(succeeded()), overlay, SLOT(deleteLater()));
    connect(m_submission, SIGNAL(failed(QString)), overlay, SLOT(deleteLater()));

    m_submission->send();
}

void ComposeWidget::setUiWidgetsEnabled(const bool enabled)
{
    ui->splitter->setEnabled(enabled);
    ui->buttonBox->setEnabled(enabled);
}


void ComposeWidget::setData(const QList<QPair<Composer::RecipientKind, QString> > &recipients,
                            const QString &subject, const QString &body, const QList<QByteArray> &inReplyTo,
                            const QList<QByteArray> &references, const QModelIndex &replyingToMessage)
{
    for (int i = 0; i < recipients.size(); ++i) {
        addRecipient(i, recipients.at(i).first, recipients.at(i).second);
    }
    updateRecipientList();
    ui->envelopeLayout->itemAt(OFFSET_OF_FIRST_ADDRESSEE, QFormLayout::FieldRole)->widget()->setFocus();
    ui->subject->setText(subject);
    const bool wasEdited = m_messageEverEdited;
    ui->mailText->setText(body);
    m_messageEverEdited = wasEdited;
    m_inReplyTo = inReplyTo;
    m_references = references;
    m_replyingToMessage = replyingToMessage;
    if (m_replyingToMessage.isValid()) {
        m_markButton->show();
        // Got to use trigger() so that the default action of the QToolButton is updated
        m_actionInReplyTo->setToolTip(tr("This mail will be marked as a response<hr/>%1").arg(
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                                          m_replyingToMessage.data(Imap::Mailbox::RoleMessageSubject).toString().toHtmlEscaped()
#else
                                          Qt::escape(m_replyingToMessage.data(Imap::Mailbox::RoleMessageSubject).toString())
#endif
                                          ));
        m_actionInReplyTo->trigger();
    } else {
        m_markButton->hide();
        m_actionInReplyTo->setToolTip(QString());
        m_actionStandalone->trigger();
    }

    int row = -1;
    bool ok = Composer::Util::chooseSenderIdentityForReply(m_mainWindow->senderIdentitiesModel(), replyingToMessage, row);
    if (ok) {
        Q_ASSERT(row >= 0 && row < m_mainWindow->senderIdentitiesModel()->rowCount());
        ui->sender->setCurrentIndex(row);
    }

    slotUpdateSignature();
}

/** @short Find out what type of recipient to use for the last row */
Composer::RecipientKind ComposeWidget::recipientKindForNextRow(const Composer::RecipientKind kind)
{
    using namespace Imap::Mailbox;
    switch (kind) {
    case Composer::ADDRESS_TO:
        // Heuristic: if the last one is "to", chances are that the next one shall not be "to" as well.
        // Cc is reasonable here.
        return Composer::ADDRESS_CC;
    case Composer::ADDRESS_CC:
    case Composer::ADDRESS_BCC:
        // In any other case, it is probably better to just reuse the type of the last row
        return kind;
    case Composer::ADDRESS_FROM:
    case Composer::ADDRESS_SENDER:
    case Composer::ADDRESS_REPLY_TO:
        // shall never be used here
        Q_ASSERT(false);
        return kind;
    }
    Q_ASSERT(false);
    return Composer::ADDRESS_TO;
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

void ComposeWidget::calculateMaxVisibleRecipients()
{
    const int oldMaxVisibleRecipients = m_maxVisibleRecipients;
    int spacing, bottom;
    ui->envelopeLayout->getContentsMargins(&spacing, &spacing, &spacing, &bottom);
    // we abuse the fact that there's always an addressee and that they all look the same
    QRect itemRects[2];
    for (int i = 0; i < 2; ++i) {
        if (QLayoutItem *li = ui->envelopeLayout->itemAt(OFFSET_OF_FIRST_ADDRESSEE - i, QFormLayout::LabelRole)) {
            itemRects[i] |= li->geometry();
        }
        if (QLayoutItem *li = ui->envelopeLayout->itemAt(OFFSET_OF_FIRST_ADDRESSEE - i, QFormLayout::FieldRole)) {
            itemRects[i] |= li->geometry();
        }
        if (QLayoutItem *li = ui->envelopeLayout->itemAt(OFFSET_OF_FIRST_ADDRESSEE - i, QFormLayout::SpanningRole)) {
            itemRects[i] |= li->geometry();
        }
    }
    int itemHeight = itemRects[0].height();
    spacing = qMax(0, itemRects[0].top() - itemRects[1].bottom() - 1); // QFormLayout::[vertical]spacing() is useless ...
    int firstTop = itemRects[0].top();
    const int subjectHeight = ui->subject->height();
    const int height = ui->verticalSplitter->sizes().at(0) - // entire splitter area
                       firstTop - // offset of first recipient
                       (subjectHeight + spacing) - // for the subject
                       bottom - // layout bottom padding
                       2; // extra pixels padding to detect that the user wants to shrink
    if (itemHeight + spacing == 0) {
        m_maxVisibleRecipients = MIN_MAX_VISIBLE_RECIPIENTS;
    } else {
        m_maxVisibleRecipients = height / (itemHeight + spacing);
    }
    if (m_maxVisibleRecipients < MIN_MAX_VISIBLE_RECIPIENTS)
        m_maxVisibleRecipients = MIN_MAX_VISIBLE_RECIPIENTS; // allow up to 4 recipients w/o need for a sliding
    if (oldMaxVisibleRecipients != m_maxVisibleRecipients) {
        const int max = qMax(0, m_recipients.count() - m_maxVisibleRecipients);
        int v = qRound(1.0f*(ui->recipientSlider->value()*m_maxVisibleRecipients)/oldMaxVisibleRecipients);
        ui->recipientSlider->setMaximum(max);
        ui->recipientSlider->setVisible(max > 0);
        scrollRecipients(qMin(qMax(0, v), max));
    }
}

void ComposeWidget::addRecipient(int position, Composer::RecipientKind kind, const QString &address)
{
    QComboBox *combo = new QComboBox(this);
    combo->addItem(tr("To"), Composer::ADDRESS_TO);
    combo->addItem(tr("Cc"), Composer::ADDRESS_CC);
    combo->addItem(tr("Bcc"), Composer::ADDRESS_BCC);
    combo->setCurrentIndex(combo->findData(kind));
    LineEdit *edit = new LineEdit(address, this);
    slotCheckAddress(edit);
    connect(edit, SIGNAL(textChanged(QString)), this, SLOT(slotCheckAddress()));
    connect(edit, SIGNAL(textEdited(QString)), SLOT(completeRecipients(QString)));
    connect(edit, SIGNAL(editingFinished()), SLOT(collapseRecipients()));
    connect(edit, SIGNAL(textChanged(QString)), m_recipientListUpdateTimer, SLOT(start()));
    m_recipients.insert(position, Recipient(combo, edit));
    ui->envelopeWidget->setUpdatesEnabled(false);
    ui->envelopeLayout->insertRow(actualRow(ui->envelopeLayout, position + OFFSET_OF_FIRST_ADDRESSEE), combo, edit);
    setTabOrder(formPredecessor(ui->envelopeLayout, combo), combo);
    setTabOrder(combo, edit);
    const int max = qMax(0, m_recipients.count() - m_maxVisibleRecipients);
    ui->recipientSlider->setMaximum(max);
    ui->recipientSlider->setVisible(max > 0);
    if (ui->recipientSlider->isVisible()) {
        const int v = ui->recipientSlider->value();
        int keepInSight = ++position;
        for (int i = 0; i < m_recipients.count(); ++i) {
            if (m_recipients.at(i).first->hasFocus() || m_recipients.at(i).second->hasFocus()) {
                keepInSight = i;
                break;
            }
        }
        if (qAbs(keepInSight - position) < m_maxVisibleRecipients)
            ui->recipientSlider->setValue(position*max/m_recipients.count());
        if (v == ui->recipientSlider->value()) // force scroll update
            scrollRecipients(v);
    }
    ui->envelopeWidget->setUpdatesEnabled(true);
}

void ComposeWidget::slotCheckAddress()
{
    QLineEdit *edit = qobject_cast<QLineEdit*>(sender());
    Q_ASSERT(edit);
    slotCheckAddress(edit);
}

void ComposeWidget::slotCheckAddress(QLineEdit *edit)
{
    Imap::Message::MailAddress addr;
    if (edit->text().isEmpty() || Imap::Message::MailAddress::fromPrettyString(addr, edit->text())) {
        edit->setPalette(QPalette());
    } else {
        QPalette p;
        p.setColor(QPalette::Base, Gui::Util::tintColor(p.color(QPalette::Base), QColor(0xff, 0, 0, 0x20)));
        edit->setPalette(p);
    }
}

void ComposeWidget::removeRecipient(int pos)
{
    // removing the widgets from the layout is important
    // a) not doing so leaks (minor)
    // b) deleteLater() crosses the evenchain and so our actualRow function would be tricked
    QWidget *formerFocus = QApplication::focusWidget();
    if (!formerFocus)
        formerFocus = m_lastFocusedRecipient;

    if (pos + 1 < m_recipients.count()) {
        if (m_recipients.at(pos).first == formerFocus) {
            m_recipients.at(pos + 1).first->setFocus();
            formerFocus = m_recipients.at(pos + 1).first;
        } else if (m_recipients.at(pos).second == formerFocus) {
            m_recipients.at(pos + 1).second->setFocus();
            formerFocus = m_recipients.at(pos + 1).second;
        }
    } else if (m_recipients.at(pos).first == formerFocus || m_recipients.at(pos).second == formerFocus) {
            formerFocus = 0;
    }

    ui->envelopeLayout->removeWidget(m_recipients.at(pos).first);
    ui->envelopeLayout->removeWidget(m_recipients.at(pos).second);
    m_recipients.at(pos).first->deleteLater();
    m_recipients.at(pos).second->deleteLater();
    m_recipients.removeAt(pos);
    const int max = qMax(0, m_recipients.count() - m_maxVisibleRecipients);
    ui->recipientSlider->setMaximum(max);
    ui->recipientSlider->setVisible(max > 0);
    if (formerFocus) {
        // skip event loop, remove might be triggered by imminent focus loss
        CALL_LATER_NOARG(formerFocus, setFocus);
    }
}

static inline Composer::RecipientKind currentRecipient(const QComboBox *box)
{
    return Composer::RecipientKind(box->itemData(box->currentIndex()).toInt());
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
    if (!haveEmpty) {
        addRecipient(m_recipients.count(),
                     m_recipients.isEmpty() ?
                         Composer::ADDRESS_TO :
                         recipientKindForNextRow(currentRecipient(m_recipients.last().first)),
                     QString());
    }
}

void ComposeWidget::handleFocusChange()
{
    // got explicit focus on other widget - don't restore former focused recipient on scrolling
    m_lastFocusedRecipient = QApplication::focusWidget();

    if (m_lastFocusedRecipient)
        QTimer::singleShot(150, this, SLOT(scrollToFocus())); // give user chance to notice the focus change disposition
}

void ComposeWidget::scrollToFocus()
{
    if (!ui->recipientSlider->isVisible())
        return;

    QWidget *focus = QApplication::focusWidget();
    if (focus == ui->envelopeWidget)
        focus = m_lastFocusedRecipient;
    if (!focus)
        return;

    // if this is the first or last visible recipient, show one more (to hint there's more and allow tab progression)
    for (int i = 0, pos = 0; i < m_recipients.count(); ++i) {
        if (m_recipients.at(i).first->isVisible())
            ++pos;
        if (focus == m_recipients.at(i).first || focus == m_recipients.at(i).second) {
            if (pos > 1 && pos < m_maxVisibleRecipients) // prev & next are in sight
                break;
            if (pos == 1)
                ui->recipientSlider->setValue(i - 1); // scroll to prev
            else
                ui->recipientSlider->setValue(i + 2 - m_maxVisibleRecipients);  // scroll to next
            break;
        }
    }
    if (focus == m_lastFocusedRecipient)
        focus->setFocus(); // in case we scrolled to m_lastFocusedRecipient
}

void ComposeWidget::fadeIn(QWidget *w)
{
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(effect);
    QPropertyAnimation *animation = new QPropertyAnimation(effect, "opacity", w);
    connect(animation, SIGNAL(finished()), SLOT(slotFadeFinished()));
    animation->setObjectName(trojita_opacityAnimation);
    animation->setDuration(333);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ComposeWidget::slotFadeFinished()
{
    Q_ASSERT(sender());
    QWidget *animatedEffectWidget = qobject_cast<QWidget*>(sender()->parent());
    Q_ASSERT(animatedEffectWidget);
    animatedEffectWidget->setGraphicsEffect(0); // deletes old one
}

void ComposeWidget::scrollRecipients(int value)
{
    // ignore focus changes caused by "scrolling"
    disconnect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(handleFocusChange()));

    QList<QWidget*> visibleWidgets;
    for (int i = 0; i < m_recipients.count(); ++i) {
        // remove all widgets from the form because of vspacing - causes spurious padding

        QWidget *toCC = m_recipients.at(i).first;
        QWidget *lineEdit = m_recipients.at(i).second;
        if (!m_lastFocusedRecipient) { // apply only _once_
            if (toCC->hasFocus())
                m_lastFocusedRecipient = toCC;
            else if (lineEdit->hasFocus())
                m_lastFocusedRecipient = lineEdit;
        }
        if (toCC->isVisible())
            visibleWidgets << toCC;
        if (lineEdit->isVisible())
            visibleWidgets << lineEdit;
        ui->envelopeLayout->removeWidget(toCC);
        ui->envelopeLayout->removeWidget(lineEdit);
        toCC->hide();
        lineEdit->hide();
    }

    const int begin = qMin(m_recipients.count(), value);
    const int end   = qMin(m_recipients.count(), value + m_maxVisibleRecipients);
    for (int i = begin, j = 0; i < end; ++i, ++j) {
        const int pos = actualRow(ui->envelopeLayout, j + OFFSET_OF_FIRST_ADDRESSEE);
        QWidget *toCC = m_recipients.at(i).first;
        QWidget *lineEdit = m_recipients.at(i).second;
        ui->envelopeLayout->insertRow(pos, toCC, lineEdit);
        if (!visibleWidgets.contains(toCC))
            fadeIn(toCC);
        visibleWidgets.removeOne(toCC);
        if (!visibleWidgets.contains(lineEdit))
            fadeIn(lineEdit);
        visibleWidgets.removeOne(lineEdit);
        toCC->show();
        lineEdit->show();
        setTabOrder(formPredecessor(ui->envelopeLayout, toCC), toCC);
        setTabOrder(toCC, lineEdit);
        if (toCC == m_lastFocusedRecipient)
            toCC->setFocus();
        else if (lineEdit == m_lastFocusedRecipient)
            lineEdit->setFocus();
    }

    if (m_lastFocusedRecipient && !m_lastFocusedRecipient->hasFocus() && QApplication::focusWidget())
        ui->envelopeWidget->setFocus();

    Q_FOREACH (QWidget *w, visibleWidgets) {
        // was visible, is no longer -> stop animation so it won't conflict later ones
        w->setGraphicsEffect(0); // deletes old one
        if (QPropertyAnimation *pa = w->findChild<QPropertyAnimation*>(trojita_opacityAnimation))
            pa->stop();
    }
    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), SLOT(handleFocusChange()));
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
    Composer::RecipientKind carriedKind = recipientKindForNextRow(Composer::ADDRESS_TO);
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
    setUiWidgetsEnabled(true);
}

void ComposeWidget::sent()
{
    // FIXME: move back to the currently selected mailbox

    m_sentMail = true;
    QTimer::singleShot(0, this, SLOT(close()));
}

bool ComposeWidget::parseRecipients(QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress> > &results, QString &errorMessage)
{
    for (int i = 0; i < m_recipients.size(); ++i) {
        Composer::RecipientKind kind = currentRecipient(m_recipients.at(i).first);

        QString text = m_recipients.at(i).second->text();
        if (text.isEmpty())
            continue;
        Imap::Message::MailAddress addr;
        bool ok = Imap::Message::MailAddress::fromPrettyString(addr, text);
        if (ok) {
            // TODO: should we *really* learn every junk entered into a recipient field?
            // m_mainWindow->addressBook()->learn(addr);
            results << qMakePair(kind, addr);
        } else {
            errorMessage = tr("Can't parse \"%1\" as an e-mail address.").arg(text);
            return false;
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

    if (o == ui->envelopeWidget)  {
        if (e->type() == QEvent::Wheel) {
            int v = ui->recipientSlider->value();
            if (static_cast<QWheelEvent*>(e)->delta() > 0)
                --v;
            else
                ++v;
            // just QApplication::sendEvent(ui->recipientSlider, e) will cause a recursion if
            // ui->recipientSlider ignores the event (eg. because it would lead to an invalid value)
            // since ui->recipientSlider is child of ui->envelopeWidget
            // my guts tell me to not send events to children if it can be avoided, but its just a gut feeling
            ui->recipientSlider->setValue(v);
            e->accept();
            return true;
        }
        if (e->type() == QEvent::KeyPress && ui->envelopeWidget->hasFocus()) {
            scrollToFocus();
            QWidget *focus = QApplication::focusWidget();
            if (focus && focus != ui->envelopeWidget) {
                int key = static_cast<QKeyEvent*>(e)->key();
                if (!(key == Qt::Key_Tab || key == Qt::Key_Backtab)) // those alter the focus again
                    QApplication::sendEvent(focus, e);
            }
            return true;
        }
        if (e->type() == QEvent::Resize) {
            QResizeEvent *re = static_cast<QResizeEvent*>(e);
            if (re->size().height() != re->oldSize().height())
                calculateMaxVisibleRecipients();
            return false;
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
        m_submission->composer()->addFileAttachment(fileName);
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
            m_submission->composer()->addFileAttachment(url.path());
        }
    }
}

void ComposeWidget::slotUpdateSignature()
{
    QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(ui->sender->model());
    Q_ASSERT(proxy);
    QModelIndex proxyIndex = ui->sender->model()->index(ui->sender->currentIndex(), 0, ui->sender->rootModelIndex());

    if (!proxyIndex.isValid()) {
        // This happens when the settings dialog gets closed and the SenderIdentitiesModel reloads data from the on-disk cache
        return;
    }

    QString newSignature = proxy->mapToSource(proxyIndex).sibling(proxyIndex.row(),
                                                                  Composer::SenderIdentitiesModel::COLUMN_SIGNATURE)
            .data().toString();

    const bool wasEdited = m_messageEverEdited;
    Composer::Util::replaceSignature(ui->mailText->document(), newSignature);
    m_messageEverEdited = wasEdited;
}

/** @short Massage the list of recipients so that they match the desired type of reply

In case of an error, the original list of recipients is left as is.
*/
bool ComposeWidget::setReplyMode(const Composer::ReplyMode mode)
{
    if (!m_replyingToMessage.isValid())
        return false;

    // Determine the new list of recipients
    Composer::RecipientList list;
    if (!Composer::Util::replyRecipientList(mode, m_mainWindow->senderIdentitiesModel(),
                                            m_replyingToMessage, list)) {
        return false;
    }

    while (!m_recipients.isEmpty())
        removeRecipient(0);

    Q_FOREACH(Composer::RecipientList::value_type recipient, list) {
        if (!recipient.second.hasUsefulDisplayName())
            recipient.second.name.clear();
        addRecipient(m_recipients.size(), recipient.first, recipient.second.asPrettyString());
    }

    updateRecipientList();
    ui->mailText->setFocus();

    return true;
}

/** local draft serializaton:
 * Version (int)
 * Whether this draft was stored explicitly (bool)
 * The sender (QString)
 * Amount of recipients (int)
 * n * (RecipientKind ("int") + recipient (QString))
 * Subject (QString)
 * The message text (QString)
 */

void ComposeWidget::saveDraft(const QString &path)
{
    static const int trojitaDraftVersion = 3;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return; // TODO: error message?
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_4_6);
    stream << trojitaDraftVersion << m_explicitDraft << ui->sender->currentText();
    stream << m_recipients.count();
    for (int i = 0; i < m_recipients.count(); ++i) {
        stream << m_recipients.at(i).first->itemData(m_recipients.at(i).first->currentIndex()).toInt();
        stream << m_recipients.at(i).second->text();
    }
    stream << m_submission->composer()->timestamp() << m_inReplyTo << m_references;
    stream << m_actionInReplyTo->isChecked();
    stream << ui->subject->text();
    stream << ui->mailText->toPlainText();
    // we spare attachments
    // a) serializing isn't an option, they could be HUUUGE
    // b) storing urls only works for urls
    // c) the data behind the url or the url validity might have changed
    // d) nasty part is writing mails - DnD a file into it is not a problem
    file.close();
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
}

/**
 * When loading a draft we omit the present autostorage (content is replaced anyway) and make
 * the loaded path the autosave path, so all further automatic storage goes into the present
 * draft file
 */

void ComposeWidget::loadDraft(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    if (m_autoSavePath != path) {
        QFile::remove(m_autoSavePath);
        m_autoSavePath = path;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_4_6);
    QString string;
    int version, recipientCount;
    stream >> version;
    stream >> m_explicitDraft;
    stream >> string >> recipientCount; // sender / amount of recipients
    int senderIndex = ui->sender->findText(string);
    if (senderIndex != -1) {
        ui->sender->setCurrentIndex(senderIndex);
    } else {
        ui->sender->setEditText(string);
    }
    for (int i = 0; i < recipientCount; ++i) {
        int kind;
        stream >> kind >> string;
        if (!string.isEmpty())
            addRecipient(i, static_cast<Composer::RecipientKind>(kind), string);
    }
    if (version >= 2) {
        QDateTime timestamp;
        stream >> timestamp >> m_inReplyTo >> m_references;
        m_submission->composer()->setTimestamp(timestamp);
        if (!m_inReplyTo.isEmpty()) {
            m_markButton->show();

            // We do not have the message index at this point, but we can at least show the Message-Id here
            QStringList inReplyTo;
            Q_FOREACH(auto item, m_inReplyTo) {
                // There's no HTML escaping to worry about
                inReplyTo << QLatin1Char('<') + QString::fromUtf8(item.constData()) + QLatin1Char('>');
            }
            m_actionInReplyTo->setToolTip(tr("This mail will be marked as a response<hr/>%1").arg(
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                                              inReplyTo.join(tr("<br/>")).toHtmlEscaped()
#else
                                              Qt::escape(inReplyTo.join(tr("<br/>")))
#endif
                                              ));
            if (version == 2) {
                // it is always marked as a reply in v2
                m_actionInReplyTo->trigger();
            }
        }
    }
    if (version >= 3) {
        bool replyChecked;
        stream >> replyChecked;
        // Got to use trigger() so that the default action of the QToolButton is updated
        if (replyChecked) {
            m_actionInReplyTo->trigger();
        } else {
            m_actionStandalone->trigger();
        }
    }
    stream >> string;
    ui->subject->setText(string);
    stream >> string;
    ui->mailText->setPlainText(string);
    m_messageUpdated = false; // this is now the most up-to-date one
    file.close();
}

void ComposeWidget::autoSaveDraft()
{
    if (m_messageUpdated) {
        m_messageUpdated = false;
        saveDraft(m_autoSavePath);
    }
}

void ComposeWidget::setMessageUpdated()
{
    m_messageEverEdited = m_messageUpdated = true;
}

void ComposeWidget::updateWindowTitle()
{
    if (ui->subject->text().isEmpty()) {
        setWindowTitle(tr("Compose Mail"));
    } else {
        setWindowTitle(tr("%1 - Compose Mail").arg(ui->subject->text()));
    }
}

void ComposeWidget::toggleReplyMarking()
{
    (m_actionInReplyTo->isChecked() ? m_actionStandalone : m_actionInReplyTo)->trigger();
}

void ComposeWidget::updateReplyMarkingAction()
{
    auto action = m_markAsReply->checkedAction();
    m_actionToggleMarking->setText(action->text());
    m_actionToggleMarking->setIcon(action->icon());
    m_actionToggleMarking->setToolTip(action->toolTip());
}

}

