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

#ifndef MESSAGEDOWNLOADER_H
#define MESSAGEDOWNLOADER_H

#include <QQueue>
#include "Imap/Model/Model.h"

class QTimer;

namespace XtConnect {

/** @short Download messages from the IMAP server

This class is responsible for requesting message structure, finding out the "most interesting part",
downloading all required parts and finally making the data retrieved so far available to other
parts of the application.
*/
class MessageDownloader : public QObject
{
    Q_OBJECT
public:
    /** @short Create a new downloader and inform it to ignore messages which belong to another mailbox */
    explicit MessageDownloader(QObject *parent, Imap::Mailbox::Model *model, const QString &mailboxName);
    /** @short Find out the body structure of a message and ask for relevant parts */
    void requestDownload( const QModelIndex &message );
    void reallyRequestDownload( const QModelIndex &message );
    int activeMessages() const;
    int pendingMessages() const;

    void requestDataDownload(const QModelIndex &message);
private slots:
    void slotDataChanged( const QModelIndex &a, const QModelIndex &b );
    void slotFreeProcessedMessages();
    void slotFetchQueuedMessages();

signals:
    /** @short All data for a message are available

This slot is emitted when the BODYSTRUCTURE is known, the main part is determined (or found not to
be supported) and all parts of the message were available from the Model, their data was retrieved
and are passed through the parameters here.
*/
    void messageDownloaded( const QModelIndex &message, const QByteArray &headers, const QByteArray &body, const QString &mainPart );

private:
    struct MessageMetadata {
        QPersistentModelIndex mainPart;
        bool hasHeader;
        bool hasBody;
        bool hasMessage;
        bool hasMainPart;
        bool mainPartFailed;
        QString partMessage;
        MessageMetadata(): hasHeader(false), hasBody(false), hasMessage(false), hasMainPart(false), mainPartFailed(false) {}
    };

    QMap<uint, MessageMetadata> m_parts;
    Imap::Mailbox::Model *m_model;

    /** @short A list of messages for which the Model shall be asked to free memory */
    QList<QPersistentModelIndex> m_messagesToBeFreed;

    /** @short List of envelopes which were requested, but are still waiting because too many messages are pending */
    QQueue<QPersistentModelIndex> m_queuedEnvelopes;

    QTimer *m_releasingTimer;
    QTimer *m_queuedTimer;

    /** @short Mailbox to which all messages got to belong */
    QString registeredMailbox;

    void log(const QString &message);

};

}

#endif // MESSAGEDOWNLOADER_H
