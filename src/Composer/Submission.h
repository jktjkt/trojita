/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef COMPOSER_SUBMISSION_H
#define COMPOSER_SUBMISSION_H

#include <QPersistentModelIndex>
#include <QPointer>

#include "Recipients.h"

namespace Imap {
namespace Mailbox {
class ImapTask;
class Model;
}
}

namespace MSA {
class MSAFactory;
}

namespace Composer {

class MessageComposer;

/** @short Handle submission of an e-mail via multiple ways

This class uses the MessageComposer for modelling a message and an MSA implementation for the actual submission.
The whole process is (trying to be) interruptable once started.
*/
class Submission : public QObject
{
    Q_OBJECT
public:
    explicit Submission(QObject *parent, Imap::Mailbox::Model *model, MSA::MSAFactory *msaFactory);
    virtual ~Submission();

    MessageComposer *composer();

    void setImapOptions(const bool saveToSentFolder, const QString &sentFolderName,
                        const QString &hostname, const QString &username, const bool useImapSubmit);
    void setSmtpOptions(const bool useBurl, const QString &smtpUsername);

    void send();

    /** @short Progress of the current submission */
    enum SubmissionProgress {
        STATE_INIT, /**< Nothing is happening yet */
        STATE_BUILDING_MESSAGE, /**< Waiting for data to become available */
        STATE_SAVING, /**< Saving the message to the Sent folder */
        STATE_PREPARING_URLAUTH, /**< Making the resulting message available via IMAP's URLAUTH */
        STATE_SUBMITTING, /**< Submitting the message via an MSA */
        STATE_UPDATING_FLAGS, /**< Updating flags of the relevant message(s) */
        STATE_SENT, /**< All done, succeeded */
        STATE_FAILED /**< Unable to send */
    };

public slots:
    void setPassword(const QString &password);
    void cancelPassword();

private slots:
    void gotError(const QString &error);
    void sent();

    void slotAppendUidKnown(const uint uidValidity, const uint uid);
    void slotGenUrlAuthReceived(const QString &url);
    void slotAppendSucceeded();
    void slotAppendFailed(const QString &error);
    void onUpdatingFlagsOfReplyingToSucceded();
    void onUpdatingFlagsOfReplyingToFailed();

    void slotMessageDataAvailable();
    void slotAskForUrl();
    void slotInvokeMsaNow();

    void onMsaProgressMaxChanged(const int max);
    void onMsaProgressCurrentChanged(const int value);

signals:
    void progressMin(const int min);
    void progressMax(const int max);
    void progress(const int progress);
    void updateStatusMessage(const QString &message);
    void failed(const QString &message);
    void succeeded();
    void passwordRequested(const QString &user, const QString &host);
    void gotPassword(const QString &password);
    void canceled();

private:
    bool shouldBuildMessageLocally() const;

    static QString killDomainPartFromString(const QString &s);

    void changeConnectionState(const SubmissionProgress state);

    bool m_appendUidReceived;
    uint m_appendUidValidity;
    uint m_appendUid;
    bool m_genUrlAuthReceived;
    QString m_urlauth;
    bool m_saveToSentFolder;
    QString m_sentFolderName;
    QString m_imapHostname;
    QString m_imapUsername;
    bool m_useBurl;
    QString m_smtpUsername;
    bool m_useImapSubmit;

    SubmissionProgress m_state;
    QByteArray m_rawMessageData;
    int m_msaMaximalProgress;

    MessageComposer *m_composer;
    QPointer<Imap::Mailbox::Model> m_model;
    MSA::MSAFactory *m_msaFactory;

    Imap::Mailbox::ImapTask *m_updateReplyingToMessageFlagsTask;

    Submission(const Submission &); // don't implement
    Submission &operator=(const Submission &); // don't implement
};

QString submissionProgressToString(const Submission::SubmissionProgress progress);

}

#endif // COMPOSER_SUBMISSION_H
