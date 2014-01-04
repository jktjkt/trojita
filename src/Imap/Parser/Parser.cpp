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
#include <algorithm>
#include <QDebug>
#include <QStringList>
#include <QMutexLocker>
#include <QProcess>
#include <QSslError>
#include <QTime>
#include <QTimer>
#include "Parser.h"
#include "Imap/Encoders.h"
#include "LowLevelParser.h"
#include "../../Streams/IODeviceSocket.h"
#include "../Model/Utils.h"

//#define PRINT_TRAFFIC 100
//#define PRINT_TRAFFIC_TX 500
//#define PRINT_TRAFFIC_RX 25
//#define PRINT_TRAFFIC_SENSITIVE

#ifdef PRINT_TRAFFIC
# ifndef PRINT_TRAFFIC_TX
#  define PRINT_TRAFFIC_TX PRINT_TRAFFIC
# endif
# ifndef PRINT_TRAFFIC_RX
#  define PRINT_TRAFFIC_RX PRINT_TRAFFIC
# endif
#endif

/*
 * Parser interface considerations:
 *
 * - Parser receives comments and gives back some kind of ID for tracking the
 *   command state
 * - "High-level stuff" like "has this command already finished" should be
 *   implemented on higher level
 * - Due to command pipelining, there's no way to find out that this untagged
 *   reply we just received was triggered by FOO command
 *
 * Interface:
 *
 * - One function per command
 * - Each received reply emits a signal (Qt-specific stuff now)
 *
 *
 * Usage example FIXME DRAFT:
 *
 *  Imap::Parser parser;
 *  Imap::CommandHandle res = parser.deleteFolder( "foo mailbox/bar/baz" );
 *
 *
 *
 * How it works under the hood:
 *
 * - When there are any data available on the net, process them ASAP
 * - When user queues a command, process it ASAP
 * - You can't block the caller of the queueCommand()
 *
 * So, how to implement this?
 *
 * - Whenever something interesting happens (data/command/exit
 *   requested/available), we ask the worker thread to do something
 *
 * */

namespace Imap
{

Parser::Parser(QObject *parent, Streams::Socket *socket, const uint myId):
    QObject(parent), socket(socket), m_lastTagUsed(0), idling(false), waitForInitialIdle(false),
    literalPlus(false), waitingForContinuation(false), startTlsInProgress(false), compressDeflateInProgress(false),
    waitingForConnection(true), waitingForEncryption(socket->isConnectingEncryptedSinceStart()), waitingForSslPolicy(false),
    m_expectsInitialGreeting(true), readingMode(ReadingLine), oldLiteralPosition(0), m_parserId(myId)
{
    connect(socket, SIGNAL(disconnected(const QString &)),
            this, SLOT(handleDisconnected(const QString &)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
    connect(socket, SIGNAL(stateChanged(Imap::ConnectionState,QString)), this, SLOT(slotSocketStateChanged(Imap::ConnectionState,QString)));
    connect(socket, SIGNAL(encrypted()), this, SLOT(handleSocketEncrypted()));
}

CommandHandle Parser::noop()
{
    return queueCommand(Commands::ATOM, "NOOP");
}

CommandHandle Parser::logout()
{
    return queueCommand(Commands::Command("LOGOUT"));

    // Queue a request for closing the socket. It'll get closed after a short while.
    QTimer::singleShot(1000, this, SLOT(closeConnection()));
}

/** @short Close the underlying conneciton */
void Parser::closeConnection()
{
    socket->close();
}

CommandHandle Parser::capability()
{
    // CAPABILITY should take precedence over LOGIN, because we have to check for LOGINDISABLED
    return queueCommand(Commands::Command() <<
                        Commands::PartOfCommand(Commands::ATOM, "CAPABILITY"));
}

CommandHandle Parser::startTls()
{
    return queueCommand(Commands::Command() <<
                        Commands::PartOfCommand(Commands::STARTTLS, "STARTTLS"));
}

CommandHandle Parser::compressDeflate()
{
    return queueCommand(Commands::Command() <<
                        Commands::PartOfCommand(Commands::COMPRESS_DEFLATE, "COMPRESS DEFLATE"));
}

#if 0
CommandHandle Parser::authenticate(/*Authenticator FIXME*/)
{
    // FIXME: needs higher priority
    return queueCommand(Commands::ATOM, "AUTHENTICATE");
}
#endif

CommandHandle Parser::login(const QString &username, const QString &password)
{
    return queueCommand(Commands::Command("LOGIN") <<
                        Commands::PartOfCommand(username.toUtf8()) << Commands::PartOfCommand(password.toUtf8()));
}

CommandHandle Parser::select(const QString &mailbox, const QList<QByteArray> &params)
{
    Commands::Command cmd = Commands::Command("SELECT") << encodeImapFolderName(mailbox);
    if (!params.isEmpty()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (");
        Q_FOREACH(const QByteArray &param, params) {
            cmd << Commands::PartOfCommand(Commands::ATOM, param);
        }
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
    }
    return queueCommand(cmd);
}

CommandHandle Parser::selectQresync(const QString &mailbox, const uint uidValidity,
                                    const quint64 highestModSeq, const Sequence &knownUids, const Sequence &sequenceSnapshot,
                                    const Sequence &uidSnapshot)
{
    Commands::Command cmd = Commands::Command("SELECT") << encodeImapFolderName(mailbox) <<
           Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (QRESYNC (") <<
           Commands::PartOfCommand(Commands::ATOM, QByteArray::number(uidValidity)) <<
           Commands::PartOfCommand(Commands::ATOM, QByteArray::number(highestModSeq));
    if (knownUids.isValid()) {
        cmd << Commands::PartOfCommand(Commands::ATOM, knownUids.toByteArray());
    }
    Q_ASSERT(uidSnapshot.isValid() == sequenceSnapshot.isValid());
    if (sequenceSnapshot.isValid()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (") <<
               Commands::PartOfCommand(Commands::ATOM, sequenceSnapshot.toByteArray()) <<
               Commands::PartOfCommand(Commands::ATOM, uidSnapshot.toByteArray()) <<
               Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")))");
    } else {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, "))");
    }
    return queueCommand(cmd);
}

CommandHandle Parser::examine(const QString &mailbox, const QList<QByteArray> &params)
{
    Commands::Command cmd = Commands::Command("EXAMINE") << encodeImapFolderName(mailbox);
    if (!params.isEmpty()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (");
        Q_FOREACH(const QByteArray &param, params) {
            cmd << Commands::PartOfCommand(Commands::ATOM, param);
        }
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
    }
    return queueCommand(cmd);
}

CommandHandle Parser::deleteMailbox(const QString &mailbox)
{
    return queueCommand(Commands::Command("DELETE") << encodeImapFolderName(mailbox));
}

CommandHandle Parser::create(const QString &mailbox)
{
    return queueCommand(Commands::Command("CREATE") << encodeImapFolderName(mailbox));
}

CommandHandle Parser::rename(const QString &oldName, const QString &newName)
{
    return queueCommand(Commands::Command("RENAME") <<
                        encodeImapFolderName(oldName) <<
                        encodeImapFolderName(newName));
}

CommandHandle Parser::subscribe(const QString &mailbox)
{
    return queueCommand(Commands::Command("SUBSCRIBE") << encodeImapFolderName(mailbox));
}

CommandHandle Parser::unSubscribe(const QString &mailbox)
{
    return queueCommand(Commands::Command("UNSUBSCRIBE") << encodeImapFolderName(mailbox));
}

CommandHandle Parser::list(const QString &reference, const QString &mailbox, const QStringList &returnOptions)
{
    Commands::Command cmd("LIST");
    cmd << reference.toUtf8() << encodeImapFolderName(mailbox);
    if (!returnOptions.isEmpty()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " RETURN (");
        Q_FOREACH(const QString &option, returnOptions) {
            cmd << Commands::PartOfCommand(Commands::ATOM, option.toUtf8());
        }
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
    }
    return queueCommand(cmd);
}

CommandHandle Parser::lSub(const QString &reference, const QString &mailbox)
{
    return queueCommand(Commands::Command("LSUB") << reference.toUtf8() << encodeImapFolderName(mailbox));
}

CommandHandle Parser::status(const QString &mailbox, const QStringList &fields)
{
    return queueCommand(Commands::Command("STATUS") << encodeImapFolderName(mailbox) <<
                        Commands::PartOfCommand(Commands::ATOM, "(" + fields.join(QLatin1String(" ")).toUtf8() + ")")
                       );
}

CommandHandle Parser::append(const QString &mailbox, const QByteArray &message, const QStringList &flags, const QDateTime &timestamp)
{
    Commands::Command command("APPEND");
    command << encodeImapFolderName(mailbox);
    if (flags.count())
        command << Commands::PartOfCommand(Commands::ATOM, "(" + flags.join(QLatin1String(" ")).toUtf8() + ")");
    if (timestamp.isValid())
        command << Commands::PartOfCommand(Imap::dateTimeToInternalDate(timestamp).toUtf8());
    command << Commands::PartOfCommand(Commands::LITERAL, message);

    return queueCommand(command);
}

CommandHandle Parser::appendCatenate(const QString &mailbox, const QList<Imap::Mailbox::CatenatePair> &data,
                                     const QStringList &flags, const QDateTime &timestamp)
{
    Commands::Command command("APPEND");
    command << encodeImapFolderName(mailbox);
    if (flags.count())
        command << Commands::PartOfCommand(Commands::ATOM, "(" + flags.join(QLatin1String(" ")).toUtf8() + ")");
    if (timestamp.isValid())
        command << Commands::PartOfCommand(Imap::dateTimeToInternalDate(timestamp).toUtf8());
    command << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " CATENATE (");
    Q_FOREACH(const Imap::Mailbox::CatenatePair &item, data) {
        switch (item.first) {
        case Imap::Mailbox::CATENATE_TEXT:
            command << Commands::PartOfCommand(Commands::ATOM, "TEXT");
            command << Commands::PartOfCommand(Commands::LITERAL, item.second);
            break;
        case Imap::Mailbox::CATENATE_URL:
            command << Commands::PartOfCommand(Commands::ATOM, "URL");
            command << Commands::PartOfCommand(item.second);
            break;
        }
    }
    command << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");

    return queueCommand(command);
}

CommandHandle Parser::check()
{
    return queueCommand(Commands::ATOM, "CHECK");
}

CommandHandle Parser::close()
{
    return queueCommand(Commands::ATOM, "CLOSE");
}

CommandHandle Parser::expunge()
{
    return queueCommand(Commands::ATOM, "EXPUNGE");
}

CommandHandle Parser::searchHelper(const QByteArray &command, const QStringList &criteria, const QByteArray &charset)
{
    Commands::Command cmd(command);

    if (!charset.isEmpty())
        cmd << "CHARSET" << charset;

    // FIXME: we don't really support anything else but utf-8 here

    if (criteria.size() == 1) {
        // Hack: if it's just a single item, let's assume it's already well-formatted by the caller.
        // This is required in the current shape of the API if we want to allow the user to type in their queries directly.
        cmd << Commands::PartOfCommand(Commands::ATOM, criteria.front().toUtf8());
    } else {
        for (QStringList::const_iterator it = criteria.begin(); it != criteria.end(); ++it)
            cmd << it->toUtf8();
    }

    return queueCommand(cmd);
}

CommandHandle Parser::uidSearchUid(const QByteArray &sequence)
{
    Commands::Command command("UID SEARCH");
    command << Commands::PartOfCommand(Commands::ATOM, sequence);
    return queueCommand(command);
}

CommandHandle Parser::uidESearchUid(const QByteArray &sequence)
{
    Commands::Command command("UID SEARCH RETURN (ALL)");
    command << Commands::PartOfCommand(Commands::ATOM, sequence);
    return queueCommand(command);
}

CommandHandle Parser::sortHelper(const QByteArray &command, const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria)
{
    Q_ASSERT(! sortCriteria.isEmpty());
    Commands::Command cmd;

    cmd << Commands::PartOfCommand(Commands::ATOM, command) <<
        Commands::PartOfCommand(Commands::ATOM, "(" + sortCriteria.join(QLatin1String(" ")).toUtf8() + ")" ) <<
        charset;

    if (searchCriteria.size() == 1) {
        // Hack: if it's just a single item, let's assume it's already well-formatted by the caller.
        // This is required in the current shape of the API if we want to allow the user to type in their queries directly.
        cmd << Commands::PartOfCommand(Commands::ATOM, searchCriteria.front().toUtf8());
    } else {
        for (QStringList::const_iterator it = searchCriteria.begin(); it != searchCriteria.end(); ++it)
            cmd << it->toUtf8();
    }

    return queueCommand(cmd);
}

CommandHandle Parser::sort(const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria)
{
    return sortHelper("SORT", sortCriteria, charset, searchCriteria);
}

CommandHandle Parser::uidSort(const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria)
{
    return sortHelper("UID SORT", sortCriteria, charset, searchCriteria);
}

CommandHandle Parser::uidESort(const QStringList &sortCriteria, const QByteArray &charset, const QStringList &searchCriteria,
                               const QStringList &returnOptions)
{
    return sortHelper("UID SORT RETURN (" + returnOptions.join(QLatin1String(" ")).toUtf8() + ")",
                      sortCriteria, charset, searchCriteria);
}

CommandHandle Parser::uidESearch(const QByteArray &charset, const QStringList &searchCriteria, const QStringList &returnOptions)
{
    return searchHelper("UID SEARCH RETURN (" + returnOptions.join(QLatin1String(" ")).toUtf8() + ")",
                        searchCriteria, charset);
}

CommandHandle Parser::cancelUpdate(const CommandHandle &tag)
{
    Commands::Command command("CANCELUPDATE");
    command << Commands::PartOfCommand(Commands::QUOTED_STRING, tag);
    return queueCommand(command);
}

CommandHandle Parser::threadHelper(const QByteArray &command, const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria)
{
    Commands::Command cmd;

    cmd << Commands::PartOfCommand(Commands::ATOM, command) << algo << charset;

    for (QStringList::const_iterator it = searchCriteria.begin(); it != searchCriteria.end(); ++it) {
        // FIXME: this is another place which needs proper structure for this searching stuff...
        cmd << Commands::PartOfCommand(Commands::ATOM, it->toUtf8());
    }

    return queueCommand(cmd);
}

CommandHandle Parser::thread(const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria)
{
    return threadHelper("THREAD", algo, charset, searchCriteria);
}

CommandHandle Parser::uidThread(const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria)
{
    return threadHelper("UID THREAD", algo, charset, searchCriteria);
}

CommandHandle Parser::uidEThread(const QByteArray &algo, const QByteArray &charset, const QStringList &searchCriteria,
                                 const QStringList &returnOptions)
{
    return threadHelper("UID THREAD RETURN (" + returnOptions.join(QLatin1String(" ")).toUtf8() + ")",
                        algo, charset, searchCriteria);
}

CommandHandle Parser::fetch(const Sequence &seq, const QStringList &items, const QMap<QByteArray, quint64> &uint64Modifiers)
{
    Commands::Command cmd = Commands::Command("FETCH") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        Commands::PartOfCommand(Commands::ATOM, '(' + items.join(QLatin1String(" ")).toUtf8() + ')');
    if (!uint64Modifiers.isEmpty()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (");
        for (QMap<QByteArray, quint64>::const_iterator it = uint64Modifiers.constBegin(); it != uint64Modifiers.constEnd(); ++it) {
            cmd << Commands::PartOfCommand(Commands::ATOM, it.key()) <<
                   Commands::PartOfCommand(Commands::ATOM, QByteArray::number(it.value()));
        }
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
    }
    return queueCommand(cmd);
}

CommandHandle Parser::store(const Sequence &seq, const QString &item, const QString &value)
{
    return queueCommand(Commands::Command("STORE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        Commands::PartOfCommand(Commands::ATOM, item.toUtf8()) <<
                        Commands::PartOfCommand(Commands::ATOM, value.toUtf8())
                       );
}

CommandHandle Parser::copy(const Sequence &seq, const QString &mailbox)
{
    return queueCommand(Commands::Command("COPY") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        encodeImapFolderName(mailbox));
}

CommandHandle Parser::uidFetch(const Sequence &seq, const QStringList &items)
{
    return queueCommand(Commands::Command("UID FETCH") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        Commands::PartOfCommand(Commands::ATOM, '(' + items.join(QLatin1String(" ")).toUtf8() + ')'));
}

CommandHandle Parser::uidStore(const Sequence &seq, const QString &item, const QString &value)
{
    return queueCommand(Commands::Command("UID STORE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        Commands::PartOfCommand(Commands::ATOM, item.toUtf8()) <<
                        Commands::PartOfCommand(Commands::ATOM, value.toUtf8()));
}

CommandHandle Parser::uidCopy(const Sequence &seq, const QString &mailbox)
{
    return queueCommand(Commands::Command("UID COPY") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        encodeImapFolderName(mailbox));
}

CommandHandle Parser::uidMove(const Sequence &seq, const QString &mailbox)
{
    return queueCommand(Commands::Command("UID MOVE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()) <<
                        encodeImapFolderName(mailbox));
}

CommandHandle Parser::uidExpunge(const Sequence &seq)
{
    return queueCommand(Commands::Command("UID EXPUNGE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toByteArray()));
}

CommandHandle Parser::xAtom(const Commands::Command &cmd)
{
    return queueCommand(cmd);
}

CommandHandle Parser::unSelect()
{
    return queueCommand(Commands::ATOM, "UNSELECT");
}

CommandHandle Parser::idle()
{
    return queueCommand(Commands::IDLE, "IDLE");
}

void Parser::idleDone()
{
    // This is not a new "command", so we don't go via queueCommand()
    // which would allocate a new tag for us, but submit directly
    Commands::Command cmd;
    cmd << Commands::PartOfCommand(Commands::IDLE_DONE, "DONE");
    cmdQueue.append(cmd);
    QTimer::singleShot(0, this, SLOT(executeCommands()));
}

void Parser::idleContinuationWontCome()
{
    Q_ASSERT(waitForInitialIdle);
    waitForInitialIdle = false;
    idling = false;
    QTimer::singleShot(0, this, SLOT(executeCommands()));
}

void Parser::idleMagicallyTerminatedByServer()
{
    Q_ASSERT(! waitForInitialIdle);
    Q_ASSERT(idling);
    idling = false;
}

CommandHandle Parser::namespaceCommand()
{
    return queueCommand(Commands::ATOM, "NAMESPACE");
}

CommandHandle Parser::idCommand()
{
    return queueCommand(Commands::Command("ID NIL"));
}

CommandHandle Parser::idCommand(const QMap<QByteArray,QByteArray> &args)
{
    Commands::Command cmd("ID ");
    cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, "(");
    for (QMap<QByteArray,QByteArray>::const_iterator it = args.constBegin(); it != args.constEnd(); ++it) {
        cmd << Commands::PartOfCommand(Commands::QUOTED_STRING, it.key()) << Commands::PartOfCommand(Commands::QUOTED_STRING, it.value());
    }
    cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
    return queueCommand(cmd);
}

CommandHandle Parser::enable(const QList<QByteArray> &extensions)
{
    Commands::Command cmd("ENABLE");
    Q_FOREACH(const QByteArray &item, extensions) {
        cmd << Commands::PartOfCommand(Commands::ATOM, item);
    }
    return queueCommand(cmd);
}

CommandHandle Parser::genUrlAuth(const QByteArray &url, const QByteArray mechanism)
{
    Commands::Command cmd("GENURLAUTH");
    cmd << Commands::PartOfCommand(Commands::QUOTED_STRING, url);
    cmd << Commands::PartOfCommand(Commands::ATOM, mechanism);
    return queueCommand(cmd);
}

CommandHandle Parser::uidSendmail(const uint uid, const Mailbox::UidSubmitOptionsList &submissionOptions)
{
    Commands::Command cmd("UID SENDMAIL");
    cmd << Commands::PartOfCommand(QByteArray::number(uid));
    if (!submissionOptions.isEmpty()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (");
        for (Mailbox::UidSubmitOptionsList::const_iterator it = submissionOptions.begin(); it != submissionOptions.end(); ++it) {
            cmd << Commands::PartOfCommand(Commands::ATOM, it->first);
            switch (it->second.type()) {
            case QVariant::ByteArray:
                cmd << Commands::PartOfCommand(it->second.toByteArray());
                break;
            case QVariant::List:
                cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, " (");
                Q_FOREACH(const QVariant &item, it->second.toList()) {
                    cmd << Commands::PartOfCommand(Commands::ATOM, item.toByteArray());
                }
                cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
                break;
            case QVariant::Invalid:
                cmd << Commands::PartOfCommand(Commands::ATOM, "NIL");
                break;
            default:
                throw InvalidArgument("Internal error: Malformed data for the UID SEND command.");
            }
        }
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, ")");
    }
    return queueCommand(cmd);
}

CommandHandle Parser::queueCommand(Commands::Command command)
{
    CommandHandle tag = generateTag();
    command.addTag(tag);
    cmdQueue.append(command);
    QTimer::singleShot(0, this, SLOT(executeCommands()));
    return tag;
}

void Parser::queueResponse(const QSharedPointer<Responses::AbstractResponse> &resp)
{
    respQueue.push_back(resp);
    // Try to limit the signal rate -- when there are multiple items in the queue, there's no point in sending more signals
    if (respQueue.size() == 1) {
        emit responseReceived(this);
    }

    if (waitingForContinuation) {
        // Check whether this is the server's way of informing us that the continuation request is not going to arrive
        QSharedPointer<Responses::State> stateResponse = resp.dynamicCast<Responses::State>();
        Q_ASSERT(!literalCommandTag.isEmpty());
        if (stateResponse && stateResponse->tag == literalCommandTag) {
            literalCommandTag.clear();
            waitingForContinuation = false;
            cmdQueue.pop_front();
            QTimer::singleShot(0, this, SLOT(executeCommands()));
            if (stateResponse->kind != Responses::NO && stateResponse->kind != Responses::BAD) {
                // FIXME: use parserWarning when it's adapted throughout the code
                qDebug() << "Synchronized literal rejected but response is neither NO nor BAD";
            }
        }
    }
}

bool Parser::hasResponse() const
{
    return ! respQueue.empty();
}

QSharedPointer<Responses::AbstractResponse> Parser::getResponse()
{
    QSharedPointer<Responses::AbstractResponse> ptr;
    if (respQueue.empty())
        return ptr;
    ptr = respQueue.front();
    respQueue.pop_front();
    return ptr;
}

QByteArray Parser::generateTag()
{
    return QString::fromUtf8("y%1").arg(m_lastTagUsed++).toUtf8();
}

void Parser::handleReadyRead()
{
    while (!waitingForEncryption && !waitingForSslPolicy) {
        switch (readingMode) {
        case ReadingLine:
            if (socket->canReadLine()) {
                reallyReadLine();
            } else {
                // Not enough data yet, let's try again later
                return;
            }
            break;
        case ReadingNumberOfBytes:
        {
            QByteArray buf = socket->read(readingBytes);
            readingBytes -= buf.size();
            currentLine += buf;
            if (readingBytes == 0) {
                // we've read the literal
                readingMode = ReadingLine;
            } else {
                return;
            }
        }
        break;
        }
    }
}

void Parser::reallyReadLine()
{
    try {
        currentLine += socket->readLine();
        if (currentLine.endsWith("}\r\n")) {
            int offset = currentLine.lastIndexOf('{');
            if (offset < oldLiteralPosition)
                throw ParseError("Got unmatched '}'", currentLine, currentLine.size() - 3);
            bool ok;
            int number = currentLine.mid(offset + 1, currentLine.size() - offset - 4).toInt(&ok);
            if (!ok)
                throw ParseError("Can't parse numeric literal size", currentLine, offset);
            if (number < 0)
                throw ParseError("Negative literal size", currentLine, offset);
            oldLiteralPosition = offset;
            readingMode = ReadingNumberOfBytes;
            readingBytes = number;
        } else if (currentLine.endsWith("\r\n")) {
            // it's complete
            if (startTlsInProgress && currentLine.startsWith(startTlsCommand)) {
                startTlsCommand.clear();
                startTlsReply = currentLine;
                currentLine.clear();
                oldLiteralPosition = 0;
                QTimer::singleShot(0, this, SLOT(finishStartTls()));
                return;
            }
            processLine(currentLine);
            currentLine.clear();
            oldLiteralPosition = 0;
        } else {
            throw ParseError("Received line doesn't end with any of \"}\\r\\n\" and \"\\r\\n\"", currentLine, 0);
        }
    } catch (ParserException &e) {
        queueResponse(QSharedPointer<Responses::AbstractResponse>(new Responses::ParseErrorResponse(e)));
    }
}

void Parser::executeCommands()
{
    while (! waitingForContinuation && ! waitForInitialIdle &&
           ! waitingForConnection && ! waitingForEncryption && ! waitingForSslPolicy &&
           ! cmdQueue.isEmpty() && ! startTlsInProgress && !compressDeflateInProgress)
        executeACommand();
}

void Parser::finishStartTls()
{
    emit lineSent(this, "*** STARTTLS");
#ifdef PRINT_TRAFFIC_TX
    qDebug() << m_parserId << "*** STARTTLS";
#endif
    cmdQueue.pop_front();
    socket->startTls(); // warn: this might invoke event loop
    startTlsInProgress = false;
    waitingForEncryption = true;
    processLine(startTlsReply);
}

void Parser::handleSocketEncrypted()
{
    waitingForEncryption = false;
    waitingForConnection = false;
    waitingForSslPolicy = true;
    QSharedPointer<Responses::AbstractResponse> resp(
                new Responses::SocketEncryptedResponse(socket->sslChain(), socket->sslErrors()));
    QByteArray buf;
    QTextStream ss(&buf);
    ss << "*** " << *resp;
    ss.flush();
#ifdef PRINT_TRAFFIC_RX
    qDebug() << m_parserId << "***" << buf;
#endif
    emit lineReceived(this, buf);
    handleReadyRead();
    queueResponse(resp);
    executeCommands();
}

/** @short We've previously frozen the command queue, so it's time to kick it a bit and keep the usual sending/receiving again */
void Parser::handleCompressionPossibleActivated()
{
    handleReadyRead();
    executeCommands();
}

void Parser::unfreezeAfterEncryption()
{
    Q_ASSERT(waitingForSslPolicy);
    waitingForSslPolicy = false;
    handleReadyRead();
    executeCommands();
}

void Parser::executeACommand()
{
    Q_ASSERT(! cmdQueue.isEmpty());
    Commands::Command &cmd = cmdQueue.first();

    QByteArray buf;

    bool sensitiveCommand = (cmd.cmds.size() > 2 && cmd.cmds[1].text == "LOGIN");
    QByteArray privateMessage = sensitiveCommand ? QByteArray("[LOGIN command goes here]") : QByteArray();

#ifdef PRINT_TRAFFIC_TX
#ifdef PRINT_TRAFFIC_SENSITIVE
    bool printThisCommand = true;
#else
    bool printThisCommand = ! sensitiveCommand;
#endif
#endif

    if (cmd.cmds[ cmd.currentPart ].kind == Commands::IDLE_DONE) {
        // Handling of the IDLE_DONE is a bit special, as we have to check and update the idling flag...
        Q_ASSERT(idling);
        buf.append("DONE\r\n");
#ifdef PRINT_TRAFFIC_TX
        qDebug() << m_parserId << ">>>" << buf.left(PRINT_TRAFFIC_TX).trimmed();
#endif
        socket->write(buf);
        idling = false;
        cmdQueue.pop_front();
        emit lineSent(this, buf);
        buf.clear();
        return;
    }

    Q_ASSERT(! idling);

    while (1) {
        Commands::PartOfCommand &part = cmd.cmds[ cmd.currentPart ];
        switch (part.kind) {
        case Commands::ATOM:
        case Commands::ATOM_NO_SPACE_AROUND:
            buf.append(part.text);
            break;
        case Commands::QUOTED_STRING:
        {
            QByteArray item = part.text;
            item.replace('\\', "\\\\");
            buf.append('"');
            buf.append(item);
            buf.append('"');
        }
        break;
        case Commands::LITERAL:
            if (literalPlus) {
                buf.append('{');
                buf.append(QByteArray::number(part.text.size()));
                buf.append("+}\r\n");
                buf.append(part.text);
            } else if (part.numberSent) {
                buf.append(part.text);
            } else {
                buf.append('{');
                buf.append(QByteArray::number(part.text.size()));
                buf.append("}\r\n");
#ifdef PRINT_TRAFFIC_TX
                if (printThisCommand)
                    qDebug() << m_parserId << ">>>" << buf.left(PRINT_TRAFFIC_TX).trimmed();
                else
                    qDebug() << m_parserId << ">>> [sensitive command] -- added literal";
#endif
                socket->write(buf);
                part.numberSent = true;
                waitingForContinuation = true;
                Q_ASSERT(literalCommandTag.isEmpty());
                literalCommandTag = cmd.cmds.first().text;
                Q_ASSERT(!literalCommandTag.isEmpty());
                emit lineSent(this, sensitiveCommand ? privateMessage : buf);
                return; // and wait for continuation request
            }
            break;
        case Commands::IDLE_DONE:
            Q_ASSERT(false); // is handled above
            break;
        case Commands::IDLE:
            buf.append("IDLE\r\n");
#ifdef PRINT_TRAFFIC_TX
            qDebug() << m_parserId << ">>>" << buf.left(PRINT_TRAFFIC_TX).trimmed();
#endif
            socket->write(buf);
            idling = true;
            waitForInitialIdle = true;
            cmdQueue.pop_front();
            emit lineSent(this, buf);
            return;
            break;
        case Commands::STARTTLS:
            startTlsCommand = buf;
            buf.append("STARTTLS\r\n");
#ifdef PRINT_TRAFFIC_TX
            qDebug() << m_parserId << ">>>" << buf.left(PRINT_TRAFFIC_TX).trimmed();
#endif
            socket->write(buf);
            startTlsInProgress = true;
            emit lineSent(this, buf);
            return;
            break;
        case Commands::COMPRESS_DEFLATE:
            compressDeflateCommand = buf;
            buf.append("COMPRESS DEFLATE\r\n");
#ifdef PRINT_TRAFFIC_TX
            qDebug() << m_parserId << ">>>" << buf.left(PRINT_TRAFFIC_TX).trimmed();
#endif
            socket->write(buf);
            compressDeflateInProgress = true;
            cmdQueue.pop_front();
            emit lineSent(this, buf);
            return;
            break;
        }
        if (cmd.currentPart == cmd.cmds.size() - 1) {
            // finalize
            buf.append("\r\n");
#ifdef PRINT_TRAFFIC_TX
            if (printThisCommand)
                qDebug() << m_parserId << ">>>" << buf.left(PRINT_TRAFFIC_TX).trimmed();
            else
                qDebug() << m_parserId << ">>> [sensitive command]";
#endif
            socket->write(buf);
            cmdQueue.pop_front();
            emit lineSent(this, sensitiveCommand ? privateMessage : buf);
            break;
        } else {
            if (part.kind == Commands::ATOM_NO_SPACE_AROUND || cmd.cmds[cmd.currentPart + 1].kind == Commands::ATOM_NO_SPACE_AROUND) {
                // Skip the extra space if asked to do so
            } else {
                buf.append(' ');
            }
            ++cmd.currentPart;
        }
    }
}

/** @short Process a line from IMAP server */
void Parser::processLine(QByteArray line)
{
#ifdef PRINT_TRAFFIC_RX
    QByteArray debugLine = line.trimmed();
    if (debugLine.size() > PRINT_TRAFFIC_RX)
        qDebug() << m_parserId << "<<<" << debugLine.left(PRINT_TRAFFIC_RX) << "...";
    else
        qDebug() << m_parserId << "<<<" << debugLine;
#endif
    emit lineReceived(this, line);
    if (m_expectsInitialGreeting && !line.startsWith("* ")) {
        throw NotAnImapServerError(std::string(), line, -1);
    } else if (line.startsWith("* ")) {
        m_expectsInitialGreeting = false;
        queueResponse(parseUntagged(line));
    } else if (line.startsWith("+ ")) {
        if (waitingForContinuation) {
            waitingForContinuation = false;
            literalCommandTag.clear();
            QTimer::singleShot(0, this, SLOT(executeCommands()));
        } else if (waitForInitialIdle) {
            waitForInitialIdle = false;
            QTimer::singleShot(0, this, SLOT(executeCommands()));
        } else {
            throw ContinuationRequest(line.constData());
        }
    } else {
        queueResponse(parseTagged(line));
    }
}

QSharedPointer<Responses::AbstractResponse> Parser::parseUntagged(const QByteArray &line)
{
    int pos = 2;
    uint number;
    try {
        number = LowLevelParser::getUInt(line, pos);
        ++pos;
    } catch (ParseError &) {
        return parseUntaggedText(line, pos);
    }
    return parseUntaggedNumber(line, pos, number);
}

QSharedPointer<Responses::AbstractResponse> Parser::parseUntaggedNumber(
    const QByteArray &line, int &start, const uint number)
{
    if (start == line.size())
        // number and nothing else
        throw NoData(line, start);

    QByteArray kindStr = LowLevelParser::getAtom(line, start);
    Responses::Kind kind;
    try {
        kind = Responses::kindFromString(kindStr);
    } catch (UnrecognizedResponseKind &e) {
        throw UnrecognizedResponseKind(e.what(), line, start);
    }

    switch (kind) {
    case Responses::EXISTS:
    case Responses::RECENT:
    case Responses::EXPUNGE:
        // no more data should follow
        if (start >= line.size())
            throw TooMuchData(line, start);
        else if (line.mid(start) != QByteArray("\r\n"))
            throw UnexpectedHere(line, start);   // expected CRLF
        else
            try {
                return QSharedPointer<Responses::AbstractResponse>(
                           new Responses::NumberResponse(kind, number));
            } catch (UnexpectedHere &e) {
                throw UnexpectedHere(e.what(), line, start);
            }
        break;

    case Responses::FETCH:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Fetch(number, line, start));
        break;

    default:
        break;
    }
    throw UnexpectedHere(line, start);
}

QSharedPointer<Responses::AbstractResponse> Parser::parseUntaggedText(
    const QByteArray &line, int &start)
{
    Responses::Kind kind;
    try {
        kind = Responses::kindFromString(LowLevelParser::getAtom(line, start));
    } catch (UnrecognizedResponseKind &e) {
        throw UnrecognizedResponseKind(e.what(), line, start);
    }
    ++start;
    if (start == line.size() && kind != Responses::SEARCH && kind != Responses::SORT)
        throw NoData(line, start);
    switch (kind) {
    case Responses::CAPABILITY:
    {
        QStringList capabilities;
        QList<QByteArray> list = line.mid(start).split(' ');
        for (QList<QByteArray>::const_iterator it = list.constBegin(); it != list.constEnd(); ++it) {
            QByteArray str = *it;
            if (str.endsWith("\r\n"))
                str.chop(2);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
            capabilities << QString::fromUtf8(str);
#else
            capabilities << QString::fromUtf8(str.constData());
#endif
        }
        if (!capabilities.count())
            throw NoData(line, start);
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Capability(capabilities));
    }
    case Responses::OK:
    case Responses::NO:
    case Responses::BAD:
    case Responses::PREAUTH:
    case Responses::BYE:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::State(QByteArray(), kind, line, start));
    case Responses::LIST:
    case Responses::LSUB:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::List(kind, line, start));
    case Responses::FLAGS:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Flags(line, start));
    case Responses::SEARCH:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Search(line, start));
    case Responses::ESEARCH:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::ESearch(line, start));
    case Responses::STATUS:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Status(line, start));
    case Responses::NAMESPACE:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Namespace(line, start));
    case Responses::SORT:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Sort(line, start));
    case Responses::THREAD:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Thread(line, start));
    case Responses::ID:
        return QSharedPointer<Responses::AbstractResponse>(
                   new Responses::Id(line, start));
    case Responses::ENABLED:
        return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::Enabled(line, start));
    case Responses::VANISHED:
        return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::Vanished(line, start));
    case Responses::GENURLAUTH:
        return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::GenUrlAuth(line, start));


        // Those already handled above follow here
    case Responses::EXPUNGE:
    case Responses::FETCH:
    case Responses::EXISTS:
    case Responses::RECENT:
        throw UnexpectedHere("Malformed response: the number should go first", line, start);
    }
    throw UnexpectedHere(line, start);
}

QSharedPointer<Responses::AbstractResponse> Parser::parseTagged(const QByteArray &line)
{
    int pos = 0;
    const QByteArray tag = LowLevelParser::getAtom(line, pos);
    ++pos;
    const Responses::Kind kind = Responses::kindFromString(LowLevelParser::getAtom(line, pos));
    ++pos;

    if (compressDeflateInProgress && compressDeflateCommand == tag + ' ') {
        switch (kind) {
        case Responses::OK:
            socket->startDeflate();
            compressDeflateInProgress = false;
            compressDeflateCommand.clear();
            break;
        default:
            // do nothing
            break;
        }
        compressDeflateInProgress = false;
        compressDeflateCommand.clear();
        QTimer::singleShot(0, this, SLOT(handleCompressionPossibleActivated()));
    }

    return QSharedPointer<Responses::AbstractResponse>(
               new Responses::State(tag, kind, line, pos));
}

void Parser::enableLiteralPlus(const bool enabled)
{
    literalPlus = enabled;
}

void Parser::handleDisconnected(const QString &reason)
{
    emit lineReceived(this, "*** Socket disconnected: " + reason.toUtf8());
#ifdef PRINT_TRAFFIC_TX
    qDebug() << m_parserId << "*** Socket disconnected";
#endif
    queueResponse(QSharedPointer<Responses::AbstractResponse>(new Responses::SocketDisconnectedResponse(reason)));
}

Parser::~Parser()
{
    // We want to prevent nasty signals from the underlying socket from
    // interfering with this object -- some of our local data might have
    // been already destroyed!
    socket->disconnect(this);
    socket->close();
    socket->deleteLater();
}

uint Parser::parserId() const
{
    return m_parserId;
}

void Parser::slotSocketStateChanged(const Imap::ConnectionState connState, const QString &message)
{
    if (connState == CONN_STATE_CONNECTED_PRETLS_PRECAPS) {
#ifdef PRINT_TRAFFIC_TX
        qDebug() << m_parserId << "*** Connection established";
#endif
        emit lineReceived(this, "*** Connection established");
        waitingForConnection = false;
        QTimer::singleShot(0, this, SLOT(executeCommands()));
    } else if (connState == CONN_STATE_AUTHENTICATED) {
        // unit tests: don't wait for the initial untagged response greetings
        m_expectsInitialGreeting = false;
    }
    emit lineReceived(this, "*** " + message.toUtf8());
    emit connectionStateChanged(this, connState);
}

}
