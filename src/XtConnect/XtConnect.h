/*
    Certain enhancements (www.xtuple.com/trojita-enhancements)
    are copyright © 2010 by OpenMFG LLC, dba xTuple.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    - Neither the name of xTuple nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior
    written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef XTCONNECT_H
#define XTCONNECT_H

#include <QModelIndex>
#include "Imap/Model/Model.h"
#include "MailSynchronizer.h"

class QSettings;

namespace XtConnect {

class XtCache;

/** @short Handle storing the mails into the XTuple Connect database */
class XtConnect : public QObject
{
    Q_OBJECT
public:
    explicit XtConnect(QObject *parent, QSettings *s);

public slots:
    /** @short IMAP alerts */
    void alertReceived(const QString &alert);
    /** @short Error in connecting */
    void connectionError(const QString &error);
    /** @short Feed the auth data back to the Model */
    void authenticationRequested();
    /** @short Authentication error */
    void authenticationFailed(const QString &message);
    /** @short Refuse to work when SSL validation fails */
    void sslErrors(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &errors);
    /** @short Cache has encountered some error */
    void cacheError(const QString &error);
    /** @short Updating progress */
    void showConnectionStatus(QObject* parser, Imap::ConnectionState state);
    /** @short Go through all mailboxes and check for new stuff */
    void goTroughMailboxes();

    /** @short A decision is needed whether to download a message */
    void slotAboutToRequestMessage( const QString &mailbox, const QModelIndex &message, bool *shouldLoad );
    /** @short A message has been stored into the database */
    void slotMessageStored( const QString &mailbox, const QModelIndex &message );
    /** @short A message is already present in the database */
    void slotMessageIsDuplicate( const QString &mailbox, const QModelIndex &message );

    /** @short Dump some statistics about how is it going */
    void slotDumpStats();

    void slotSqlError(const QString &message);

private:
    void setupModels();

    Imap::Mailbox::Model *m_model;
    QSettings *m_settings;
    MailboxFinder *m_finder;
    QMap<QString, QPointer<MailSynchronizer> > m_syncers;
    QTimer *m_rotateMailboxes;
    XtCache *m_cache;
};

}

#endif // XTCONNECT_H
