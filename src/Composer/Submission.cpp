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
#include <QCoreApplication>
#include <QBuffer>
#include <QSettings>
#include "Composer/Submission.h"
#include "Composer/MessageComposer.h"
#include "Imap/Model/Model.h"
#include "Imap/Tasks/AppendTask.h"
#include "Imap/Tasks/GenUrlAuthTask.h"
#include "Imap/Tasks/UidSubmitTask.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"

namespace {
const int PROGRESS_MAX = 1000;
// We're very likely almost half-way there -- let's call it 45%
const int PROGRESS_SAVING_DONE = PROGRESS_MAX * 0.45;
// Updating flags might take roughly as much time as the URLAUTH
const int PROGRESS_DELIVERY_DONE = PROGRESS_MAX * 0.95;

const int PROGRESS_DELIVERY_START_WITHOUT_SAVING = PROGRESS_MAX * 0.10;
const int PROGRESS_DELIVERY_START_WITH_SAVING = PROGRESS_MAX * 0.5;
}

namespace Composer
{

QString submissionProgressToString(const Submission::SubmissionProgress progress)
{
    switch (progress) {
    case Submission::STATE_INIT:
        return QLatin1String("STATE_INIT");
    case Submission::STATE_BUILDING_MESSAGE:
        return QLatin1String("STATE_BUILDING_MESSAGE");
    case Submission::STATE_SAVING:
        return QLatin1String("STATE_SAVING");
    case Submission::STATE_PREPARING_URLAUTH:
        return QLatin1String("STATE_PREPARING_URLAUTH");
    case Submission::STATE_SUBMITTING:
        return QLatin1String("STATE_SUBMITTING");
    case Submission::STATE_UPDATING_FLAGS:
        return QLatin1String("STATE_UPDATING_FLAGS");
    case Submission::STATE_SENT:
        return QLatin1String("STATE_SENT");
    case Submission::STATE_FAILED:
        return QLatin1String("STATE_FAILED");
    }
    return QString::fromUtf8("[unknown: %1]").arg(QString::number(static_cast<int>(progress)));
}

Submission::Submission(QObject *parent, Imap::Mailbox::Model *model, MSA::MSAFactory *msaFactory) :
    QObject(parent),
    m_appendUidReceived(false), m_appendUidValidity(0), m_appendUid(0), m_genUrlAuthReceived(false),
    m_saveToSentFolder(false), m_useBurl(false), m_useImapSubmit(false), m_state(STATE_INIT),
    m_msaMaximalProgress(0),
    m_composer(0), m_model(model), m_msaFactory(msaFactory), m_updateReplyingToMessageFlagsTask(0)
{
    m_composer = new Composer::MessageComposer(model, this);
    m_composer->setPreloadEnabled(shouldBuildMessageLocally());
}

MessageComposer *Submission::composer()
{
    return m_composer;
}

Submission::~Submission()
{
}

void Submission::changeConnectionState(const SubmissionProgress state)
{
    m_state = state;
    if (m_model)
        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Submission"), submissionProgressToString(m_state));

    // Now broadcast a human-readable message and update the progress dialog
    switch (state) {
    case STATE_INIT:
        emit progressMin(0);
        emit progressMax(0);
        emit progress(0);
        emit updateStatusMessage(tr("Preparing to send"));
        break;
    case STATE_BUILDING_MESSAGE:
        emit progressMax(0);
        emit progress(0);
        emit updateStatusMessage(tr("Creating message"));
        break;
    case STATE_SAVING:
        emit progressMax(0);
        emit progress(0);
        emit updateStatusMessage(tr("Saving to the sent folder"));
        break;
    case STATE_PREPARING_URLAUTH:
        emit progressMax(PROGRESS_MAX);
        emit progress(PROGRESS_SAVING_DONE);
        emit updateStatusMessage(tr("Preparing message for delivery"));
        break;
    case STATE_SUBMITTING:
        emit progressMax(PROGRESS_MAX);
        emit progress(m_saveToSentFolder ? PROGRESS_DELIVERY_START_WITH_SAVING : PROGRESS_DELIVERY_START_WITHOUT_SAVING);
        emit updateStatusMessage(tr("Submitting message"));
        break;
    case STATE_UPDATING_FLAGS:
        emit progressMax(PROGRESS_MAX);
        emit progress(PROGRESS_DELIVERY_DONE);
        emit updateStatusMessage(tr("Updating message keywords"));
        break;
    case STATE_SENT:
        emit progressMax(PROGRESS_MAX);
        emit progress(PROGRESS_MAX);
        emit updateStatusMessage(tr("Message sent"));
        break;
    case STATE_FAILED:
        // revert to the busy indicator
        emit progressMin(0);
        emit progressMax(0);
        emit progress(0);
        emit updateStatusMessage(tr("Sending failed"));
        break;
    }
}

void Submission::setImapOptions(const bool saveToSentFolder, const QString &sentFolderName,
                                const QString &hostname, const QString &username, const bool useImapSubmit)
{
    m_saveToSentFolder = saveToSentFolder;
    m_sentFolderName = sentFolderName;
    m_imapHostname = hostname;
    m_imapUsername = username;
    m_useImapSubmit = useImapSubmit;
    m_composer->setPreloadEnabled(shouldBuildMessageLocally());
}

void Submission::setSmtpOptions(const bool useBurl, const QString &smtpUsername)
{
    m_useBurl = useBurl;
    if (m_useBurl && !m_model->isGenUrlAuthSupported()) {
        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Submission"), QLatin1String("Cannot BURL without the URLAUTH extension"));
        m_useBurl = false;
    }
    m_smtpUsername = smtpUsername;
    m_composer->setPreloadEnabled(shouldBuildMessageLocally());
}

void Submission::send()
{
    if (!m_model) {
        gotError(tr("The IMAP connection has disappeared. "
                    "You'll have close the composer, save the draft and re-open it later. "
                    "The attachments will have to be added later. Sorry for the trouble, "
                    "please see <a href=\"https://projects.flaska.net/issues/640\">https://projects.flaska.net/issues/640</a> "
                    "for details."));
        return;
    }

    // this double-updating is needed in case the same Submission attempts to send a message more than once
    changeConnectionState(STATE_INIT);
    changeConnectionState(STATE_BUILDING_MESSAGE);

    if (shouldBuildMessageLocally() && !m_composer->isReadyForSerialization()) {
        // we have to wait until the data arrive
        // FIXME: relax this to wait here
        gotError(tr("Some data are not available yet"));
    } else {
        slotMessageDataAvailable();
    }
}


void Submission::slotMessageDataAvailable()
{
    m_rawMessageData.clear();
    QBuffer buf(&m_rawMessageData);
    buf.open(QIODevice::WriteOnly);
    QString errorMessage;
    QList<Imap::Mailbox::CatenatePair> catenateable;

    if (shouldBuildMessageLocally() && !m_composer->asRawMessage(&buf, &errorMessage)) {
        gotError(tr("Cannot send right now -- saving failed:\n %1").arg(errorMessage));
        return;
    }
    if (m_model->isCatenateSupported() && !m_composer->asCatenateData(catenateable, &errorMessage)) {
        gotError(tr("Cannot send right now -- saving (CATENATE) failed:\n %1").arg(errorMessage));
        return;
    }

    if (m_saveToSentFolder) {
        Q_ASSERT(m_model);
        m_appendUidReceived = false;
        m_genUrlAuthReceived = false;

        changeConnectionState(STATE_SAVING);
        QPointer<Imap::Mailbox::AppendTask> appendTask = 0;

        if (m_model->isCatenateSupported()) {
            // FIXME: without UIDPLUS, there isn't much point in $SubmitPending...
            appendTask = QPointer<Imap::Mailbox::AppendTask>(
                        m_model->appendIntoMailbox(
                            m_sentFolderName,
                            catenateable,
                            QStringList() << QLatin1String("\\Seen"),
                            m_composer->timestamp()));
        } else {
            // FIXME: without UIDPLUS, there isn't much point in $SubmitPending...
            appendTask = QPointer<Imap::Mailbox::AppendTask>(
                        m_model->appendIntoMailbox(
                            m_sentFolderName,
                            m_rawMessageData,
                            QStringList() << QLatin1String("\\Seen"),
                            m_composer->timestamp()));
        }

        Q_ASSERT(appendTask);
        connect(appendTask.data(), SIGNAL(appendUid(uint,uint)), this, SLOT(slotAppendUidKnown(uint,uint)));
        connect(appendTask.data(), SIGNAL(completed(Imap::Mailbox::ImapTask*)), this, SLOT(slotAppendSucceeded()));
        connect(appendTask.data(), SIGNAL(failed(QString)), this, SLOT(slotAppendFailed(QString)));
    } else {
        slotInvokeMsaNow();
    }
}

void Submission::slotAskForUrl()
{
    Q_ASSERT(m_appendUidReceived && m_useBurl);
    changeConnectionState(STATE_PREPARING_URLAUTH);
    Imap::Mailbox::GenUrlAuthTask *genUrlAuthTask = QPointer<Imap::Mailbox::GenUrlAuthTask>(
                m_model->generateUrlAuthForMessage(m_imapHostname,
                                                   killDomainPartFromString(m_imapUsername),
                                                   m_sentFolderName,
                                                   m_appendUidValidity, m_appendUid, QString(),
                                                   QString::fromUtf8("submit+%1").arg(
                                                       killDomainPartFromString(m_smtpUsername))
                                                   ));
    connect(genUrlAuthTask, SIGNAL(gotAuth(QString)), this, SLOT(slotGenUrlAuthReceived(QString)));
    connect(genUrlAuthTask, SIGNAL(failed(QString)), this, SLOT(gotError(QString)));
}

void Submission::slotInvokeMsaNow()
{
    changeConnectionState(STATE_SUBMITTING);
    MSA::AbstractMSA *msa = m_msaFactory->create(this);
    connect(msa, SIGNAL(progressMax(int)), this, SLOT(onMsaProgressMaxChanged(int)));
    connect(msa, SIGNAL(progress(int)), this, SLOT(onMsaProgressCurrentChanged(int)));
    connect(msa, SIGNAL(sent()), this, SLOT(sent()));
    connect(msa, SIGNAL(error(QString)), this, SLOT(gotError(QString)));
    connect(msa, SIGNAL(passwordRequested(QString,QString)), this, SIGNAL(passwordRequested(QString,QString)));
    connect(this, SIGNAL(gotPassword(QString)), msa, SLOT(setPassword(QString)));
    connect(this, SIGNAL(canceled()), msa, SLOT(cancel()));

    if (m_useImapSubmit && msa->supportsImapSending() && m_appendUidReceived) {
        Imap::Mailbox::UidSubmitOptionsList options;
        options.append(qMakePair<QByteArray,QVariant>("FROM", m_composer->rawFromAddress()));
        Q_FOREACH(const QByteArray &recipient, m_composer->rawRecipientAddresses()) {
            options.append(qMakePair<QByteArray,QVariant>("RECIPIENT", recipient));
        }
        msa->sendImap(m_sentFolderName, m_appendUidValidity, m_appendUid, options);
    } else if (m_genUrlAuthReceived && m_useBurl) {
        msa->sendBurl(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), m_urlauth.toUtf8());
    } else {
        msa->sendMail(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), m_rawMessageData);
    }
}

void Submission::setPassword(const QString &password)
{
    emit gotPassword(password);
}

void Submission::cancelPassword()
{
    emit canceled();
}

void Submission::gotError(const QString &error)
{
    if (m_model)
        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Submission"), QString::fromUtf8("gotError: %1").arg(error));
    changeConnectionState(STATE_FAILED);
    emit failed(error);
}

void Submission::sent()
{
    if (m_composer->replyingToMessage().isValid()) {
        m_updateReplyingToMessageFlagsTask = m_model->setMessageFlags(QModelIndexList() << m_composer->replyingToMessage(),
                                                                      QLatin1String("\\Answered"), Imap::Mailbox::FLAG_ADD);
        connect(m_updateReplyingToMessageFlagsTask, SIGNAL(completed(Imap::Mailbox::ImapTask*)),
                this, SLOT(onUpdatingFlagsOfReplyingToSucceded()));
        connect(m_updateReplyingToMessageFlagsTask, SIGNAL(failed(QString)),
                this, SLOT(onUpdatingFlagsOfReplyingToFailed()));
        changeConnectionState(STATE_UPDATING_FLAGS);
    } else {
        changeConnectionState(STATE_SENT);
        emit succeeded();
    }
#if 0
    if (m_appendUidReceived) {
        // FIXME: check the UIDVALIDITY!!!
        // FIXME: doesn't work at all; the messageIndexByUid() only works on already selected mailboxes
        QModelIndex message = m_mainWindow->imapModel()->
                messageIndexByUid(QSettings().value(Common::SettingsNames::composerImapSentKey, tr("Sent")).toString(), m_appendUid);
        if (message.isValid()) {
            m_mainWindow->imapModel()->setMessageFlags(QModelIndexList() << message,
                                                       QLatin1String("\\Seen $Submitted"), Imap::Mailbox::FLAG_USE_THESE);
        }
    }
#endif

    // FIXME: move back to the currently selected mailbox
}

/** @short Remember the APPENDUID as reported by the APPEND operation */
void Submission::slotAppendUidKnown(const uint uidValidity, const uint uid)
{
    m_appendUidValidity = uidValidity;
    m_appendUid = uid;
}

void Submission::slotAppendFailed(const QString &error)
{
    gotError(tr("APPEND failed: %1").arg(error));
}

void Submission::slotAppendSucceeded()
{
    if (m_appendUid && m_appendUidValidity) {
        // Only ever consider valid UIDVALIDITY/UID pair
        m_appendUidReceived = true;

        if (m_useBurl) {
            slotAskForUrl();
        } else {
            slotInvokeMsaNow();
        }
    } else {
        m_useBurl = false;
        m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Submission"),
                          QLatin1String("APPEND does not contain APPENDUID or UIDVALIDITY, cannot use BURL or the SUBMIT command"));
        slotInvokeMsaNow();
    }
}

/** @short Remember the GENURLAUTH response */
void Submission::slotGenUrlAuthReceived(const QString &url)
{
    m_urlauth = url;
    if (!m_urlauth.isEmpty()) {
        m_genUrlAuthReceived = true;
        slotInvokeMsaNow();
    } else {
        gotError(tr("The URLAUTH response does not contain a proper URL"));
    }
}

/** @short Remove the "@domain" from a string */
QString Submission::killDomainPartFromString(const QString &s)
{
    return s.split(QLatin1Char('@'))[0];
}

/** @short Return true if the message payload shall be built locally */
bool Submission::shouldBuildMessageLocally() const
{
    if (!m_useImapSubmit) {
        // sending via SMTP or Sendmail
        // Unless all of URLAUTH, CATENATE and BURL is present and enabled, we will still have to download the data in the end
        return ! (m_useBurl && m_model->isCatenateSupported() && m_model->isGenUrlAuthSupported());
    } else {
        return ! m_model->isCatenateSupported();
    }
}

void Submission::onUpdatingFlagsOfReplyingToSucceded()
{
    m_updateReplyingToMessageFlagsTask = 0;
    changeConnectionState(STATE_SENT);
    emit succeeded();
}

void Submission::onUpdatingFlagsOfReplyingToFailed()
{
    m_updateReplyingToMessageFlagsTask = 0;
    m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("Submission"),
                      QLatin1String("Cannot update flags of the message we replied to -- interesting, but we cannot do anything at this point anyway"));
    changeConnectionState(STATE_SENT);
    emit succeeded();
}

void Submission::onMsaProgressCurrentChanged(const int value)
{
    if (m_msaMaximalProgress > 0) {
        // prevent division by zero or performing operations which do not make any sense
        int low = m_saveToSentFolder ? PROGRESS_DELIVERY_START_WITH_SAVING : PROGRESS_DELIVERY_START_WITHOUT_SAVING;
        int high = PROGRESS_DELIVERY_DONE;
        emit progress(1.0 * value / m_msaMaximalProgress * (high - low) + low);
    }
}

void Submission::onMsaProgressMaxChanged(const int max)
{
    m_msaMaximalProgress = max;
}

}

