/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>
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

namespace Composer
{

Submission::Submission(QObject *parent, Imap::Mailbox::Model *model, MSA::MSAFactory *msaFactory) :
    QObject(parent),
    m_appendUidReceived(false), m_appendUidValidity(0), m_appendUid(0), m_genUrlAuthReceived(false),
    m_saveToSentFolder(false), m_useBurl(false),
    m_composer(0), m_model(model), m_msaFactory(msaFactory)
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

void Submission::setImapOptions(const bool saveToSentFolder, const QString &sentFolderName, const QString &hostname)
{
    m_saveToSentFolder = saveToSentFolder;
    m_sentFolderName = sentFolderName;
    m_imapHostname = hostname;
}

void Submission::setSmtpOptions(const bool useBurl, const QString &smtpUsername)
{
    m_useBurl = useBurl;
    m_smtpUsername = smtpUsername;
}

void Submission::send()
{
    QByteArray rawMessageData;
    QBuffer buf(&rawMessageData);
    buf.open(QIODevice::WriteOnly);
    QString errorMessage;

    QPointer<Imap::Mailbox::AppendTask> appendTask = 0;
    m_appendUidReceived = false;
    m_genUrlAuthReceived = false;
    if (m_saveToSentFolder) {
        Q_ASSERT(m_model);

        if (m_model->capabilities().contains(QLatin1String("CATENATE"))) {
            QList<Imap::Mailbox::CatenatePair> catenateable;
            if (!m_composer->asCatenateData(catenateable, &errorMessage)) {
                gotError(tr("Cannot send right now -- saving (CATENATE) failed:\n %1").arg(errorMessage));
                return;
            }

            // FIXME: without UIDPLUS, there isn't much point in $SubmitPending...
            appendTask = QPointer<Imap::Mailbox::AppendTask>(
                        m_model->appendIntoMailbox(
                            m_sentFolderName,
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
                        m_model->appendIntoMailbox(
                            m_sentFolderName,
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

#if 0
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
#endif

    // Message uploading through IMAP cannot really be terminated
    emit progressMin(0);
    emit progressMax(3);
    emit updateCancellable(false);

    if (appendTask) {
        emit updateStatusMessage(tr("Saving message..."));
    }

    while (appendTask && !appendTask->isFinished()) {
        // FIXME: get rid of this busy wait, eventually
        QCoreApplication::processEvents();
    }

    if (!m_msaFactory) {
        // FIXME: this looks like a hack; null factory -> send via IMAP...
        if (!m_appendUidReceived) {
            emit failed(tr("Cannot send over IMAP, APPENDUID failed"));
            return;
        }
        Imap::Mailbox::UidSubmitOptionsList options;
        options.append(qMakePair<QByteArray,QVariant>("FROM", m_composer->rawFromAddress()));
        Q_FOREACH(const QByteArray &recipient, m_composer->rawRecipientAddresses()) {
            options.append(qMakePair<QByteArray,QVariant>("RECIPIENT", recipient));
        }
        Imap::Mailbox::UidSubmitTask *submitTask = m_model->sendMailViaUidSubmit(
                    m_sentFolderName, m_appendUidValidity, m_appendUid,
                    options);
        Q_ASSERT(submitTask);
        connect(submitTask, SIGNAL(completed(Imap::Mailbox::ImapTask*)), this, SLOT(sent()));
        connect(submitTask, SIGNAL(failed(QString)), this, SLOT(gotError(QString)));
        emit updateStatusMessage(tr("Sending mail..."));
        emit progress(2);
        while (submitTask && !submitTask->isFinished()) {
            QCoreApplication::processEvents();
        }
        emit succeeded();
        return;
    }

    QPointer<Imap::Mailbox::GenUrlAuthTask> genUrlAuthTask;
    if (m_appendUidReceived && m_useBurl && m_model->capabilities().contains(QLatin1String("GENURLAUTH"))) {
        emit progress(1);
        emit updateStatusMessage(tr("Generating IMAP URL..."));
        genUrlAuthTask = QPointer<Imap::Mailbox::GenUrlAuthTask>(
                    m_model->
                    generateUrlAuthForMessage(m_imapHostname,
                                              killDomainPartFromString(m_imapHostname),
                                              m_sentFolderName,
                                              m_appendUidValidity, m_appendUid, QString(),
                                              QString::fromUtf8("submit+%1").arg(
                        killDomainPartFromString(m_smtpUsername))
                                              ));
        connect(genUrlAuthTask.data(), SIGNAL(gotAuth(QString)), this, SLOT(slotGenUrlAuthReceived(QString)));
        while (genUrlAuthTask && !genUrlAuthTask->isFinished()) {
            // FIXME: get rid of this busy wait, eventually
            QCoreApplication::processEvents();
        }
    }
    emit updateStatusMessage(tr("Sending mail..."));
    emit progress(2);

    MSA::AbstractMSA *msa = m_msaFactory->create(this);
    connect(msa, SIGNAL(progressMax(int)), this, SIGNAL(progressMax(int)));
    connect(msa, SIGNAL(progress(int)), this, SIGNAL(progress(int)));
    connect(msa, SIGNAL(sent()), this, SLOT(sent()));
    connect(msa, SIGNAL(error(QString)), this, SLOT(gotError(QString)));

    if (m_genUrlAuthReceived && m_useBurl) {
        msa->sendBurl(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), m_urlauth.toUtf8());
    } else {
        msa->sendMail(m_composer->rawFromAddress(), m_composer->rawRecipientAddresses(), rawMessageData);
    }
}

void Submission::gotError(const QString &error)
{
    emit failed(error);
}

void Submission::sent()
{
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

    // FIXME: enable this again
#if 0
    if (m_replyingTo.isValid()) {
        m_model->setMessageFlags(QModelIndexList() << m_replyingTo,
                                                   QLatin1String("\\Answered"), Imap::Mailbox::FLAG_ADD);
    }
#endif

    // FIXME: move back to the currently selected mailbox

    QTimer::singleShot(0, this, SLOT(close()));
}

/** @short Remember the APPENDUID as reported by the APPEND operation */
void Submission::slotAppendUidKnown(const uint uidValidity, const uint uid)
{
    m_appendUidValidity = uidValidity;
    m_appendUid = uid;

    if (m_appendUid && m_appendUidValidity) {
        // Only ever consider valid UIDVALIDITY/UID pair
        m_appendUidReceived = true;
    }
}

/** @short Remember the GENURLAUTH response */
void Submission::slotGenUrlAuthReceived(const QString &url)
{
    m_urlauth = url;
    if (!m_urlauth.isEmpty()) {
        m_genUrlAuthReceived = true;
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
    // Unless all of URLAUTH, CATENATE and BURL is present and enabled, we will still have to download the data in the end
    return ! (m_model->capabilities().contains(QLatin1String("CATENATE"))
              && m_model->capabilities().contains(QLatin1String("GENURLAUTH"))
              && m_useBurl);
}

}

