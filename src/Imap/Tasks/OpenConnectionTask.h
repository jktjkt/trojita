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

#ifndef IMAP_OPENCONNECTIONTASK_H
#define IMAP_OPENCONNECTIONTASK_H

#include "ImapTask.h"
#include <QSslError>
#include "../Model/Model.h"

namespace Imap
{
namespace Mailbox
{

/** @short Create new connection and make sure it reaches authenticated state

Use this task if you want to obtain a new connection which ends up in the authenticated
state. It will establish a new connection and baby-sit it until it reaches the request
authenticated state.

Obtaining capabilities, issuing STARTTLS and logging in are all handled here.
*/
class OpenConnectionTask : public ImapTask
{
    Q_OBJECT
public:
    explicit OpenConnectionTask(Model *model);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    // FIXME: reimplement handleCapability(), add some guards against "unexpected changes" to Model's implementation
    virtual bool handleSocketEncryptedResponse(const Responses::SocketEncryptedResponse *const resp);

    /** @short Inform the task that the auth credentials are now available and can be used */
    void authCredentialsNowAvailable();

    /** @short A decision about the future whereabouts of the conneciton has been made */
    void sslConnectionPolicyDecided(bool ok);

    /** @short Return the peer's chain of digital certificates, or an empty list of certificates */
    QList<QSslCertificate> sslCertificateChain() const;

    /** @short Return a list of SSL errors which the underlying socket has encountered since its start */
    QList<QSslError> sslErrors() const;

    virtual QString debugIdentification() const;
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return false;}

protected:
    /** @short A special, internal constructor used only by Fake_OpenConnectionTask */
    OpenConnectionTask(Model *model, void *dummy);

private:
    bool stateMachine(const Imap::Responses::State *const resp);

    void startTlsOrLoginNow();

    bool checkCapabilitiesResult(const Imap::Responses::State *const resp);

    /** @short Wrapper around the _completed() call for optionally launching the ID command */
    void onComplete();

    void logout(const QString &message);

    void askForAuth();

private:
    CommandHandle startTlsCmd;
    CommandHandle capabilityCmd;
    CommandHandle loginCmd;
    CommandHandle compressCmd;
    QList<QSslCertificate> m_sslChain;
    QList<QSslError> m_sslErrors;
};

}
}

#endif // IMAP_OPENCONNECTIONTASK_H
