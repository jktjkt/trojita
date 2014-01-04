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
#ifndef IMAP_PARSER_H
#define IMAP_PARSER_H
#include <QLinkedList>
#include <QSharedPointer>
#include "Command.h"
#include "Response.h"
#include "Sequence.h"
#include "../ConnectionState.h"
#include "../Exceptions.h"
#include "Imap/Model/CatenateData.h"
#include "Imap/Model/UidSubmitData.h"

/**
 * @file
 * A header file defining Parser class and various helpers.
 *
 * @author Jan Kundrát <jkt@flaska.net>
 */

class ImapParserParseTest;

namespace Streams {
class Socket;
}

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short A handle identifying a command sent to the server */
typedef QByteArray CommandHandle;

// this is required for clang 3.0
typedef QMap<QByteArray, quint64> MapByteArrayUint64;

/** @short Class that does all IMAP parsing */
class Parser : public QObject
{
    Q_OBJECT

    friend class ::ImapParserParseTest;

public:
    /** @short Constructor.
     *
     * Takes an QIODevice instance as a parameter. */
    Parser(QObject *parent, Streams::Socket *socket, const uint myId);

    ~Parser();

    /** @short Checks for waiting responses */
    bool hasResponse() const;

    /** @short De-queue and return parsed response */
    QSharedPointer<Responses::AbstractResponse> getResponse();

    /** @short Enable/Disable sending literals using the LITERAL+ extension */
    void enableLiteralPlus(const bool enabled=true);

    uint parserId() const;

public slots:

    /** @short CAPABILITY, RFC 3501 section 6.1.1 */
    CommandHandle capability();

    /** @short NOOP, RFC 3501 section 6.1.2 */
    CommandHandle noop();

    /** @short LOGOUT, RFC3501 section 6.1.3 */
    CommandHandle logout();


    /** @short STARTTLS, RFC3051 section 6.2.1 */
    CommandHandle startTls();

#if 0
    /** @short AUTHENTICATE, RFC3501 section 6.2.2 */
    CommandHandle authenticate(/* FIXME: parameter */);
#endif

    /** @short LOGIN, RFC3501 section 6.2.3 */
    CommandHandle login(const QString &user, const QString &pass);


    /** @short SELECT, RFC3501 section 6.3.1 */
    CommandHandle select(const QString &mailbox, const QList<QByteArray> &params = QList<QByteArray>());

    /** @short SELECT extended according to RFC 5162 section 3.1 */
    CommandHandle selectQresync(const QString &mailbox, const uint uidValidity,
                                const quint64 highestModSeq, const Sequence &knownUids = Sequence(),
                                const Sequence &sequenceSnapshot = Sequence(), const Sequence &uidSnapshot = Sequence());

    /** @short EXAMINE, RFC3501 section 6.3.2 */
    CommandHandle examine(const QString &mailbox, const QList<QByteArray> &params = QList<QByteArray>());

    /** @short CREATE, RFC3501 section 6.3.3 */
    CommandHandle create(const QString &mailbox);

    /** @short DELETE, RFC3501 section 6.3.4 */
    CommandHandle deleteMailbox(const QString &mailbox);

    /** @short RENAME, RFC3501 section 6.3.5 */
    CommandHandle rename(const QString &oldName, const QString &newName);

    /** @short SUBSCRIBE, RFC3501 section 6.3.6 */
    CommandHandle subscribe(const QString &mailbox);

    /** @short UNSUBSCRIBE, RFC3501 section 6.3.7 */
    CommandHandle unSubscribe(const QString &mailbox);

    /** @short LIST, RFC3501 section 6.3.8, as extended by RFC5258 */
    CommandHandle list(const QString &reference, const QString &mailbox, const QStringList &returnOptions = QStringList());

    /** @short LSUB, RFC3501 section 6.3.9 */
    CommandHandle lSub(const QString &reference, const QString &mailbox);

    /** @short STATUS, RFC3501 section 6.3.10 */
    CommandHandle status(const QString &mailbox, const QStringList &fields);

    /** @short APPEND, RFC3501 section 6.3.11 */
    CommandHandle append(const QString &mailbox, const QByteArray &message,
                         const QStringList &flags = QStringList(), const QDateTime &timestamp = QDateTime());

    /** @short APPEND CATENATE, RFC 4469 */
    CommandHandle appendCatenate(const QString &mailbox, const QList<Imap::Mailbox::CatenatePair> &data,
                                 const QStringList &flags = QStringList(), const QDateTime &timestamp = QDateTime());


    /** @short CHECK, RFC3501 sect 6.4.1 */
    CommandHandle check();

    /** @short CLOSE, RFC3501 sect 6.4.2 */
    CommandHandle close();

    /** @short EXPUNGE, RFC3501 sect 6.4.3 */
    CommandHandle expunge();

    /** @short SEARCH, RFC3501 sect 6.4.4 */
    CommandHandle search(const QStringList &criteria, const QByteArray &charset = QByteArray()) {
        return searchHelper("SEARCH", criteria, charset);
    };

    /** @short FETCH, RFC3501 sect 6.4.5 */
    CommandHandle fetch(const Sequence &seq, const QStringList &items,
                        const QMap<QByteArray, quint64> &uint64Modifiers = MapByteArrayUint64());

    /** @short STORE, RFC3501 sect 6.4.6 */
    CommandHandle store(const Sequence &seq, const QString &item, const QString &value);

    /** @short COPY, RFC3501 sect 6.4.7 */
    CommandHandle copy(const Sequence &seq, const QString &mailbox);

    /** @short UID command (FETCH), RFC3501 sect 6.4.8 */
    CommandHandle uidFetch(const Sequence &seq, const QStringList &items);

    /** @short UID command (STORE), RFC3501 sect 6.4.8 */
    CommandHandle uidStore(const Sequence &seq, const QString &item, const QString &value);

    /** @short UID command (COPY), RFC3501 sect 6.4.8 */
    CommandHandle uidCopy(const Sequence &seq, const QString &mailbox);

    /** @short UID XMOVE, draft-gulbrandsen-imap-move-01 as implemented by fastmail.fm */
    CommandHandle uidMove(const Sequence &seq, const QString &mailbox);

    /** @short UID EXPUNGE from the UIDPLUS extension, RFC 2359 section 4.1 */
    CommandHandle uidExpunge(const Sequence &seq);

    /** @short UID command (SEARCH), RFC3501 sect 6.4.8 */
    CommandHandle uidSearch(const QStringList &criteria, const QByteArray &charset = QByteArray()) {
        return searchHelper("UID SEARCH", criteria, charset);
    }

    /** @short A special case of the "UID SEARCH UID" command */
    CommandHandle uidSearchUid(const QByteArray &sequence);

    /** @short Perform the UID ESEARCH command with the specified UID set */
    CommandHandle uidESearchUid(const QByteArray &sequence);


    /** @short X<atom>, RFC3501 sect 6.5.1 */
    CommandHandle xAtom(const Commands::Command &commands);


    /** @short UNSELECT, RFC3691 */
    CommandHandle unSelect();

    /** @short IDLE, RFC2177

      The IDLE command has to be explicitly terminated by calling idleDone().
     */
    CommandHandle idle();

    /** @short The DONE for terminating the IDLE state, RFC2177 */
    void idleDone();

    /** @short Don't wait for the initial continuation prompt, it won't come */
    void idleContinuationWontCome();

    /** @short The IDLE command got terminated by the server after it sent the continuation request but before we got a chance to break it */
    void idleMagicallyTerminatedByServer();


    /** @short NAMESPACE, RFC 2342 */
    CommandHandle namespaceCommand();

    /** SORT, RFC5256 */
    CommandHandle sort(const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria);
    /** UID SORT, RFC5256 */
    CommandHandle uidSort(const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria);
    /** THREAD, RFC5256 */
    CommandHandle thread(const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria);
    /** UID THREAD, RFC5256 */
    CommandHandle uidThread(const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria);

    /** @short ESORT, the extended UID SORT from RFC 5267, section 3.1 */
    CommandHandle uidESort(const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria,
                           const QStringList &returnOptions);

    /** @short ESEARCH, the extended UID SEARCH with support for ESEARCH return options from RFC 5267 */
    CommandHandle uidESearch(const QByteArray &charset, const QStringList &searchCriteria, const QStringList &returnOptions);


    CommandHandle uidEThread(const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria,
                             const QStringList &returnOptions);

    /** @short CANCELUPDATE, tell the server that it shall stop sending any ESEARCH responses associated with the given tag */
    CommandHandle cancelUpdate(const CommandHandle &tag);

    /** @short ID, RFC 2971 section 3.1

    This variant will send the ID NIL command.
    */
    CommandHandle idCommand();

    /** @short ID, RFC 2971 section 3.1

    This variant of the idCommand() sends an arbitrary list of arguments to the server.
    */
    CommandHandle idCommand(const QMap<QByteArray,QByteArray> &args);

    /** @short ENABLE command, RFC 6151 */
    CommandHandle enable(const QList<QByteArray> &extensions);

    /** @short COMPRESS DEFLATE, RFC 4978 */
    CommandHandle compressDeflate();

    /** @short GENURLAUTH, RFC 4467 */
    CommandHandle genUrlAuth(const QByteArray &url, const QByteArray mechanism);

    /** @short UID SENDMAIL, jkt's draft-imap-sendmail */
    CommandHandle uidSendmail(const uint uid, const Mailbox::UidSubmitOptionsList &submissionOptions);

    void slotSocketStateChanged(const Imap::ConnectionState connState, const QString &message);

    void unfreezeAfterEncryption();

    void closeConnection();


signals:
    /** @short New response received */
    void responseReceived(Imap::Parser *parser);

    /** @short A full line was received from the remote IMAP server

    This signal is emitted when a full line, including all embedded literals, have
    been received from the remote IMAP server, but before it was attempted to parse
    it. However,
    */
    void lineReceived(Imap::Parser *parser, const QByteArray &line);

    /** @short A full line has been sent to the remote IMAP server */
    void lineSent(Imap::Parser *parser, const QByteArray &line);

    /** @short There's been a non-fatal error when parsing given line

    Detailed information is available in the @arg message with @arg line and @arg position
    containing the original line and indicating the troublesome position, or -1 if not applciable.
    */
    void parserWarning(Imap::Parser *parser, const QString &message, const QByteArray *line, uint position);

    void commandQueued();

    /** @short The socket's state has changed */
    void connectionStateChanged(Imap::Parser *parser, Imap::ConnectionState);

private slots:
    void handleReadyRead();
    void handleDisconnected(const QString &reason);
    void executeACommand();
    void executeCommands();
    void finishStartTls();
    void handleSocketEncrypted();
    void handleCompressionPossibleActivated();

private:
    /** @short Private copy constructor */
    Parser(const Parser &);
    /** @short Private assignment operator */
    Parser &operator=(const Parser &);

    /** @short Queue command for execution.*/
    CommandHandle queueCommand(Commands::Command command);

    /** @short Shortcut function; works exactly same as above mentioned queueCommand() */
    CommandHandle queueCommand(const Commands::TokenType kind, const QByteArray &text) {
        return queueCommand(Commands::Command() << Commands::PartOfCommand(kind, text));
    };

    /** @short Helper for handleReadyRead() -- actually read & parse the data */
    void reallyReadLine();

    /** @short Helper for search() and uidSearch() */
    CommandHandle searchHelper(const QByteArray &command, const QStringList &criteria,
                               const QByteArray &charset = QByteArray());

    CommandHandle sortHelper(const QByteArray &command, const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria);
    CommandHandle threadHelper(const QByteArray &command, const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria);

    /** @short Generate tag for next command */
    QByteArray generateTag();

    void processLine(QByteArray line);

    /** @short Parse line for untagged reply */
    QSharedPointer<Responses::AbstractResponse> parseUntagged(const QByteArray &line);

    /** @short Parse line for tagged reply */
    QSharedPointer<Responses::AbstractResponse> parseTagged(const QByteArray &line);

    /** @short helper for parseUntagged() */
    QSharedPointer<Responses::AbstractResponse> parseUntaggedNumber(
        const QByteArray &line, int &start, const uint number);

    /** @short helper for parseUntagged() */
    QSharedPointer<Responses::AbstractResponse> parseUntaggedText(
        const QByteArray &line, int &start);

    /** @short Add parsed response to the internal queue, emit notification signal */
    void queueResponse(const QSharedPointer<Responses::AbstractResponse> &resp);

    /** @short Connection to the IMAP server */
    Streams::Socket *socket;

    /** @short Keeps track of the last-used command tag */
    unsigned int m_lastTagUsed;

    /** @short Queue storing commands that are about to be executed */
    QLinkedList<Commands::Command> cmdQueue;

    /** @short Queue storing parsed replies from the IMAP server */
    QLinkedList<QSharedPointer<Responses::AbstractResponse> > respQueue;

    bool idling;
    bool waitForInitialIdle;

    bool literalPlus;
    bool waitingForContinuation;
    bool startTlsInProgress;
    bool compressDeflateInProgress;
    bool waitingForConnection;
    bool waitingForEncryption;
    bool waitingForSslPolicy;
    bool m_expectsInitialGreeting;

    enum { ReadingLine, ReadingNumberOfBytes } readingMode;
    QByteArray currentLine;
    int oldLiteralPosition;
    uint readingBytes;
    QByteArray startTlsCommand;
    QByteArray startTlsReply;
    QByteArray compressDeflateCommand;
    QByteArray literalCommandTag;

    /** @short Unique-id for debugging purposes */
    uint m_parserId;
};

QTextStream &operator<<(QTextStream &stream, const Sequence &s);

}
#endif /* IMAP_PARSER_H */
