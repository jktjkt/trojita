/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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

Parser::Parser(QObject *parent, Socket *socket, const uint myId):
    QObject(parent), socket(socket), m_lastTagUsed(0), idling(false), waitForInitialIdle(false),
    literalPlus(false), waitingForContinuation(false), startTlsInProgress(false),
    waitingForConnection(true), waitingForEncryption(socket->isConnectingEncryptedSinceStart()), waitingForSslPolicy(false),
    readingMode(ReadingLine), oldLiteralPosition(0), m_parserId(myId)
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
                        Commands::PartOfCommand(username) << Commands::PartOfCommand(password));
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

CommandHandle Parser::selectQresync(const QString &mailbox, const uint uidValidity, const quint64 highestModSeq,
                                    const Sequence &knownUids, const Sequence &sequenceSnapshot, const Sequence &uidSnapshot)
{
    Commands::Command cmd = Commands::Command("SELECT") << encodeImapFolderName(mailbox);
    cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, QLatin1String(" (QRESYNC (")) <<
           Commands::PartOfCommand(Commands::ATOM, QString::number(uidValidity)) <<
           Commands::PartOfCommand(Commands::ATOM, QString::number(highestModSeq));
    if (knownUids.isValid()) {
        cmd << Commands::PartOfCommand(Commands::ATOM, knownUids.toString());
    }
    if (sequenceSnapshot.isValid()) {
        Q_ASSERT(uidSnapshot.isValid());
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, QLatin1String(" (")) <<
               Commands::PartOfCommand(Commands::ATOM, sequenceSnapshot.toString()) <<
               Commands::PartOfCommand(Commands::ATOM, uidSnapshot.toString()) <<
               Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, QLatin1String(")))"));
    } else {
        Q_ASSERT(uidSnapshot.isValid());
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, QLatin1String("))"));
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

CommandHandle Parser::list(const QString &reference, const QString &mailbox)
{
    return queueCommand(Commands::Command("LIST") << reference << encodeImapFolderName(mailbox));
}

CommandHandle Parser::lSub(const QString &reference, const QString &mailbox)
{
    return queueCommand(Commands::Command("LSUB") << reference << encodeImapFolderName(mailbox));
}

CommandHandle Parser::status(const QString &mailbox, const QStringList &fields)
{
    return queueCommand(Commands::Command("STATUS") << encodeImapFolderName(mailbox) <<
                        Commands::PartOfCommand(Commands::ATOM, "(" + fields.join(" ") +")")
                       );
}

CommandHandle Parser::append(const QString &mailbox, const QString &message, const QStringList &flags, const QDateTime &timestamp)
{
    Commands::Command command("APPEND");
    command << encodeImapFolderName(mailbox);
    if (flags.count())
        command << Commands::PartOfCommand(Commands::ATOM, "(" + flags.join(" ") + ")");
    if (timestamp.isValid())
        command << Commands::PartOfCommand(timestamp.toString());
    command << Commands::PartOfCommand(Commands::LITERAL, message);

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

CommandHandle Parser::searchHelper(const QString &command, const QStringList &criteria, const QString &charset)
{
    Commands::Command cmd(command);

    if (!charset.isEmpty())
        cmd << "CHARSET" << charset;

    for (QStringList::const_iterator it = criteria.begin(); it != criteria.end(); ++it)
        cmd << *it;

    return queueCommand(cmd);
}

CommandHandle Parser::uidSearchUid(const QString &sequence)
{
    Commands::Command command("UID SEARCH");
    command << Commands::PartOfCommand(Commands::ATOM, sequence);
    return queueCommand(command);
}

CommandHandle Parser::uidESearchUid(const QString &sequence)
{
    Commands::Command command("UID SEARCH RETURN ()");
    command << Commands::PartOfCommand(Commands::ATOM, sequence);
    return queueCommand(command);
}

CommandHandle Parser::sortHelper(const QString &command, const QStringList &sortCriteria, const QString &charset, const QStringList &searchCriteria)
{
    Q_ASSERT(! sortCriteria.isEmpty());
    Commands::Command cmd;

    cmd << Commands::PartOfCommand(Commands::ATOM, command) <<
        Commands::PartOfCommand(Commands::ATOM, QString::fromAscii("(%1)").arg(sortCriteria.join(QString(' ')))) <<
        charset;

    for (QStringList::const_iterator it = searchCriteria.begin(); it != searchCriteria.end(); ++it)
        cmd << *it;

    return queueCommand(cmd);
}

CommandHandle Parser::sort(const QStringList &sortCriteria, const QString &charset, const QStringList &searchCriteria)
{
    return sortHelper(QLatin1String("SORT"), sortCriteria, charset, searchCriteria);
}

CommandHandle Parser::uidSort(const QStringList &sortCriteria, const QString &charset, const QStringList &searchCriteria)
{
    return sortHelper(QLatin1String("UID SORT"), sortCriteria, charset, searchCriteria);
}

CommandHandle Parser::threadHelper(const QString &command, const QString &algo, const QString &charset, const QStringList &searchCriteria)
{
    Commands::Command cmd;

    cmd << Commands::PartOfCommand(Commands::ATOM, command) << algo << charset;

    for (QStringList::const_iterator it = searchCriteria.begin(); it != searchCriteria.end(); ++it)
        cmd << *it;

    return queueCommand(cmd);
}

CommandHandle Parser::thread(const QString &algo, const QString &charset, const QStringList &searchCriteria)
{
    return threadHelper(QLatin1String("THREAD"), algo, charset, searchCriteria);
}

CommandHandle Parser::uidThread(const QString &algo, const QString &charset, const QStringList &searchCriteria)
{
    return threadHelper(QLatin1String("UID THREAD"), algo, charset, searchCriteria);
}

CommandHandle Parser::fetch(const Sequence &seq, const QStringList &items, const QMap<QByteArray, quint64> &uint64Modifiers)
{
    Commands::Command cmd = Commands::Command("FETCH") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        Commands::PartOfCommand(Commands::ATOM, '(' + items.join(" ") + ')');
    if (!uint64Modifiers.isEmpty()) {
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, QLatin1String(" ("));
        for (QMap<QByteArray, quint64>::const_iterator it = uint64Modifiers.constBegin(); it != uint64Modifiers.constEnd(); ++it) {
            cmd << Commands::PartOfCommand(Commands::ATOM, QString::fromAscii(it.key())) <<
                   Commands::PartOfCommand(Commands::ATOM, QString::number(it.value()));
        }
        cmd << Commands::PartOfCommand(Commands::ATOM_NO_SPACE_AROUND, QLatin1String(")"));
    }
    return queueCommand(cmd);
}

CommandHandle Parser::store(const Sequence &seq, const QString &item, const QString &value)
{
    return queueCommand(Commands::Command("STORE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        Commands::PartOfCommand(Commands::ATOM, item) <<
                        Commands::PartOfCommand(Commands::ATOM, value)
                       );
}

CommandHandle Parser::copy(const Sequence &seq, const QString &mailbox)
{
    return queueCommand(Commands::Command("COPY") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        encodeImapFolderName(mailbox));
}

CommandHandle Parser::uidFetch(const Sequence &seq, const QStringList &items)
{
    return queueCommand(Commands::Command("UID FETCH") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        Commands::PartOfCommand(Commands::ATOM, '(' + items.join(" ") + ')'));
}

CommandHandle Parser::uidStore(const Sequence &seq, const QString &item, const QString &value)
{
    return queueCommand(Commands::Command("UID STORE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        Commands::PartOfCommand(Commands::ATOM, item) <<
                        Commands::PartOfCommand(Commands::ATOM, value));
}

CommandHandle Parser::uidCopy(const Sequence &seq, const QString &mailbox)
{
    return queueCommand(Commands::Command("UID COPY") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        encodeImapFolderName(mailbox));
}

CommandHandle Parser::uidXMove(const Sequence &seq, const QString &mailbox)
{
    return queueCommand(Commands::Command("UID XMOVE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()) <<
                        encodeImapFolderName(mailbox));
}

CommandHandle Parser::uidExpunge(const Sequence &seq)
{
    return queueCommand(Commands::Command("UID EXPUNGE") <<
                        Commands::PartOfCommand(Commands::ATOM, seq.toString()));
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

CommandHandle Parser::queueCommand(Commands::Command command)
{
    QString tag = generateTag();
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

QString Parser::generateTag()
{
    return QString("y%1").arg(m_lastTagUsed++);
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
            throw CantHappen("canReadLine() returned true, but following readLine() failed");
        }
    } catch (ParserException &e) {
        emit parseError(this, QString::fromStdString(e.exceptionClass()), QString::fromStdString(e.msg()), e.line(), e.offset());
    }
}

void Parser::executeCommands()
{
    while (! waitingForContinuation && ! waitForInitialIdle &&
           ! waitingForConnection && ! waitingForEncryption && ! waitingForSslPolicy &&
           ! cmdQueue.isEmpty() && ! startTlsInProgress)
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

    bool sensitiveCommand = (cmd.cmds.size() > 2 && cmd.cmds[1].text == QLatin1String("LOGIN"));
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
            QString item = part.text;
            item.replace(QChar('\\'), QString::fromAscii("\\\\"));
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
    if (line.startsWith("* ")) {
        queueResponse(parseUntagged(line));
    } else if (line.startsWith("+ ")) {
        if (waitingForContinuation) {
            waitingForContinuation = false;
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
            capabilities << str;
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
                   new Responses::State(QString::null, kind, line, start));
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

        // Those already handled above follow here
    case Responses::EXPUNGE:
    case Responses::FETCH:
    case Responses::EXISTS:
    case Responses::RECENT:
        throw UnexpectedHere("Mallformed response: the number should go first", line, start);
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

    return QSharedPointer<Responses::AbstractResponse>(
               new Responses::State(tag, kind, line, pos));
}

void Parser::enableLiteralPlus(const bool enabled)
{
    literalPlus = enabled;
}

void Parser::handleDisconnected(const QString &reason)
{
    emit lineReceived(this, "*** Socket disconnected: " + reason.toLocal8Bit());
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
    }
    emit lineReceived(this, "*** " + message.toLocal8Bit());
    emit connectionStateChanged(this, connState);
}

Sequence::Sequence(const uint num): kind(DISTINCT)
{
    list << num;
}

Sequence Sequence::startingAt(const uint lo)
{
    Sequence res(lo);
    res.lo = lo;
    res.kind = UNLIMITED;
    return res;
}

QString Sequence::toString() const
{
    switch (kind) {
    case DISTINCT:
    {
        Q_ASSERT(! list.isEmpty());

        QStringList res;
        int i = 0;
        while (i < list.size()) {
            int old = i;
            while (i < list.size() - 1 &&
                   list[i] == list[ i + 1 ] - 1)
                ++i;
            if (old != i) {
                // we've found a sequence
                res << QString::number(list[old]) + QLatin1Char(':') + QString::number(list[i]);
            } else {
                res << QString::number(list[i]);
            }
            ++i;
        }
        return res.join(QLatin1String(","));
        break;
    }
    case RANGE:
        Q_ASSERT(lo <= hi);
        if (lo == hi)
            return QString::number(lo);
        else
            return QString::number(lo) + QLatin1Char(':') + QString::number(hi);
    case UNLIMITED:
        return QString::number(lo) + QLatin1String(":*");
    }
    // fix gcc warning
    Q_ASSERT(false);
    return QString();
}

Sequence &Sequence::add(uint num)
{
    Q_ASSERT(kind == DISTINCT);
    QList<uint>::iterator it = qLowerBound(list.begin(), list.end(), num);
    if (it == list.end() || *it != num)
        list.insert(it, num);
    return *this;
}

Sequence Sequence::fromList(QList<uint> numbers)
{
    Q_ASSERT(!numbers.isEmpty());
    qSort(numbers);
    Sequence seq(numbers.first());
    for (int i = 1; i < numbers.size(); ++i) {
        seq.add(numbers[i]);
    }
    return seq;
}

bool Sequence::isValid() const
{
    if (kind == DISTINCT && list.isEmpty())
        return false;
    else
        return true;
}

QTextStream &operator<<(QTextStream &stream, const Sequence &s)
{
    return stream << s.toString();
}

}
