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
#include <typeinfo>
#include <QSslError>
#include "Response.h"
#include "Message.h"
#include "LowLevelParser.h"
#include "../Model/Model.h"
#include "../Tasks/ImapTask.h"

namespace Imap
{
namespace Responses
{

AbstractResponse::~AbstractResponse()
{
}

static QString threadDumpHelper(const ThreadingNode &node);
static void threadingHelperInsertHere(ThreadingNode *where, const QVariantList &what);

QTextStream &operator<<(QTextStream &stream, const Code &r)
{
#define CASE(X) case X: stream << #X; break;
    switch (r) {
    case ATOM:
        stream << "<<ATOM>>'"; break;
        CASE(ALERT);
        CASE(BADCHARSET);
        CASE(CAPABILITIES);
        CASE(PARSE);
        CASE(PERMANENTFLAGS);
    case READ_ONLY:
        stream << "READ-ONLY"; break;
    case READ_WRITE:
        stream << "READ-WRITE"; break;
        CASE(TRYCREATE);
        CASE(UIDNEXT);
        CASE(UIDVALIDITY);
        CASE(UNSEEN);
    case NONE:
        stream << "<<NONE>>"; break;
        CASE(NEWNAME);
        CASE(REFERRAL);
    case UNKNOWN_CTE:
        stream << "UINKNOWN-CTE"; break;
        CASE(UIDNOTSTICKY);
        CASE(APPENDUID);
        CASE(COPYUID);
        CASE(URLMECH);
        CASE(TOOBIG);
        CASE(BADURL);
        CASE(HIGHESTMODSEQ);
        CASE(NOMODSEQ);
        CASE(MODIFIED);
        CASE(COMPRESSIONACTIVE);
        CASE(CLOSED);
        CASE(NOTSAVED);
        CASE(BADCOMPARATOR);
        CASE(ANNOTATE);
        CASE(ANNOTATIONS);
        CASE(TEMPFAIL);
        CASE(MAXCONVERTMESSAGES);
        CASE(MAXCONVERTPARTS);
        CASE(NOUPDATE);
        CASE(METADATA);
        CASE(NOTIFICATIONOVERFLOW);
        CASE(BADEVENT);
    case UNDEFINED_FILTER:
        stream << "UNDEFINED-FILTER"; break;
        CASE(UNAVAILABLE);
        CASE(AUTHENTICATIONFAILED);
        CASE(AUTHORIZATIONFAILED);
        CASE(EXPIRED);
        CASE(PRIVACYREQUIRED);
        CASE(CONTACTADMIN);
        CASE(NOPERM);
        CASE(INUSE);
        CASE(EXPUNGEISSUED);
        CASE(CORRUPTION);
        CASE(SERVERBUG);
        CASE(CLIENTBUG);
        CASE(CANNOT);
        CASE(LIMIT);
        CASE(OVERQUOTA);
        CASE(ALREADYEXISTS);
        CASE(NONEXISTENT);
        CASE(POLICYDENIED);
        CASE(SUBMISSIONRACE);
    }
    return stream;
#undef CASE
}

QTextStream &operator<<(QTextStream &stream, const Kind &res)
{
    switch (res) {
    case OK:
        stream << "OK";
        break;
    case NO:
        stream << "NO";
        break;
    case BAD:
        stream << "BAD";
        break;
    case BYE:
        stream << "BYE";
        break;
    case PREAUTH:
        stream << "PREAUTH";
        break;
    case EXPUNGE:
        stream << "EXPUNGE";
        break;
    case FETCH:
        stream << "FETCH";
        break;
    case EXISTS:
        stream << "EXISTS";
        break;
    case RECENT:
        stream << "RECENT";
        break;
    case CAPABILITY:
        stream << "CAPABILITY";
        break;
    case LIST:
        stream << "LIST";
        break;
    case LSUB:
        stream << "LSUB";
        break;
    case FLAGS:
        stream << "FLAGS";
        break;
    case SEARCH:
        stream << "SEARCH";
        break;
    case ESEARCH:
        stream << "ESEARCH";
        break;
    case STATUS:
        stream << "STATUS";
        break;
    case NAMESPACE:
        stream << "NAMESPACE";
        break;
    case SORT:
        stream << "SORT";
        break;
    case THREAD:
        stream << "THREAD";
        break;
    case ID:
        stream << "ID";
        break;
    case ENABLED:
        stream << "ENABLE";
        break;
    case VANISHED:
        stream << "VANISHED";
        break;
    case GENURLAUTH:
        stream << "GENURLAUTH";
        break;
    }
    return stream;
}

Kind kindFromString(QByteArray str) throw(UnrecognizedResponseKind)
{
    str = str.toUpper();

    if (str == "OK")
        return OK;
    if (str == "NO")
        return NO;
    if (str == "BAD")
        return BAD;
    if (str == "BYE")
        return BYE;
    if (str == "PREAUTH")
        return PREAUTH;
    if (str == "FETCH")
        return FETCH;
    if (str == "EXPUNGE")
        return EXPUNGE;
    if (str == "FETCH")
        return FETCH;
    if (str == "EXISTS")
        return EXISTS;
    if (str == "RECENT")
        return RECENT;
    if (str == "CAPABILITY")
        return CAPABILITY;
    if (str == "LIST")
        return LIST;
    if (str == "LSUB")
        return LSUB;
    if (str == "FLAGS")
        return FLAGS;
    if (str == "SEARCH" || str == "SEARCH\r\n")
        return SEARCH;
    if (str == "ESEARCH")
        return ESEARCH;
    if (str == "STATUS")
        return STATUS;
    if (str == "NAMESPACE")
        return NAMESPACE;
    if (str == "SORT")
        return SORT;
    if (str == "THREAD")
        return THREAD;
    if (str == "ID")
        return ID;
    if (str == "ENABLED")
        return ENABLED;
    if (str == "VANISHED")
        return VANISHED;
    if (str == "GENURLAUTH")
        return GENURLAUTH;
    throw UnrecognizedResponseKind(str.constData());
}

QTextStream &operator<<(QTextStream &stream, const Status::StateKind &kind)
{
    switch (kind) {
    case Status::MESSAGES:
        stream << "MESSAGES"; break;
    case Status::RECENT:
        stream << "RECENT"; break;
    case Status::UIDNEXT:
        stream << "UIDNEXT"; break;
    case Status::UIDVALIDITY:
        stream << "UIDVALIDITY"; break;
    case Status::UNSEEN:
        stream << "UNSEEN"; break;
    }
    return stream;
}

QTextStream &operator<<(QTextStream &stream, const NamespaceData &data)
{
    return stream << "( prefix \"" << data.prefix << "\", separator \"" << data.separator << "\")";
}

Status::StateKind Status::stateKindFromStr(QString s)
{
    s = s.toUpper();
    if (s == QLatin1String("MESSAGES"))
        return MESSAGES;
    if (s == QLatin1String("RECENT"))
        return RECENT;
    if (s == QLatin1String("UIDNEXT"))
        return UIDNEXT;
    if (s == QLatin1String("UIDVALIDITY"))
        return UIDVALIDITY;
    if (s == QLatin1String("UNSEEN"))
        return UNSEEN;
    throw UnrecognizedResponseKind(s.toStdString());
}

QTextStream &operator<<(QTextStream &stream, const AbstractResponse &res)
{
    return res.dump(stream);
}

State::State(const QByteArray &tag, const Kind kind, const QByteArray &line, int &start):
    tag(tag), kind(kind), respCode(NONE)
{
    if (!tag.isEmpty()) {
        // tagged
        switch (kind) {
        case OK:
        case NO:
        case BAD:
            break;
        default:
            throw UnexpectedHere(line, start);   // tagged response of weird kind
        }
    }

    QStringList list;
    QVariantList originalList;
    try {
        originalList = LowLevelParser::parseList('[', ']', line, start);
        list = QVariant(originalList).toStringList();
        ++start;
    } catch (UnexpectedHere &) {
        // this is perfectly possible
    }

    if (!list.isEmpty()) {
        const QString r = list.first().toUpper();
#define CASE(X) else if (r == QLatin1String(#X)) respCode = Responses::X;
        if (r == QLatin1String("ALERT"))
            respCode = Responses::ALERT;
        CASE(BADCHARSET)
        else if (r == QLatin1String("CAPABILITY"))
            respCode = Responses::CAPABILITIES;
        CASE(PARSE)
        CASE(PERMANENTFLAGS)
        else if (r == QLatin1String("READ-ONLY"))
            respCode = Responses::READ_ONLY;
        else if (r == QLatin1String("READ-WRITE"))
            respCode = Responses::READ_WRITE;
        CASE(TRYCREATE)
        CASE(UIDNEXT)
        CASE(UIDVALIDITY)
        CASE(UNSEEN)
        CASE(NEWNAME)
        CASE(REFERRAL)
        else if (r == QLatin1String("UNKNOWN-CTE"))
            respCode = Responses::UNKNOWN_CTE;
        CASE(UIDNOTSTICKY)
        CASE(APPENDUID)
        CASE(COPYUID)
        // FIXME: not implemented CASE(URLMECH)
        CASE(TOOBIG)
        CASE(BADURL)
        CASE(HIGHESTMODSEQ)
        CASE(NOMODSEQ)
        // FIXME: not implemented CASE(MODIFIED)
        CASE(COMPRESSIONACTIVE)
        CASE(CLOSED)
        CASE(NOTSAVED)
        CASE(BADCOMPARATOR)
        CASE(ANNOTATE)
        // FIXME: not implemented CASE(ANNOTATIONS)
        CASE(TEMPFAIL)
        CASE(MAXCONVERTMESSAGES)
        CASE(MAXCONVERTPARTS)
        CASE(NOUPDATE)
        // FIXME: not implemented CASE(METADATA)
        CASE(NOTIFICATIONOVERFLOW)
        CASE(BADEVENT)
        else if (r == QLatin1String("UNDEFINED-FILTER"))
            respCode = Responses::UNDEFINED_FILTER;
        CASE(UNAVAILABLE)
        CASE(AUTHENTICATIONFAILED)
        CASE(AUTHORIZATIONFAILED)
        CASE(EXPIRED)
        CASE(PRIVACYREQUIRED)
        CASE(CONTACTADMIN)
        CASE(NOPERM)
        CASE(INUSE)
        CASE(EXPUNGEISSUED)
        CASE(CORRUPTION)
        CASE(SERVERBUG)
        CASE(CLIENTBUG)
        CASE(CANNOT)
        CASE(LIMIT)
        CASE(OVERQUOTA)
        CASE(ALREADYEXISTS)
        CASE(NONEXISTENT)
        CASE(POLICYDENIED)
        CASE(SUBMISSIONRACE)
        else
            respCode = Responses::ATOM;

        if (respCode != Responses::ATOM)
            list.pop_front();

        // now perform validity check and construct & store the storage object
        switch (respCode) {
        case Responses::ALERT:
        case Responses::PARSE:
        case Responses::READ_ONLY:
        case Responses::READ_WRITE:
        case Responses::TRYCREATE:
        case Responses::UNAVAILABLE:
        case Responses::AUTHENTICATIONFAILED:
        case Responses::AUTHORIZATIONFAILED:
        case Responses::EXPIRED:
        case Responses::PRIVACYREQUIRED:
        case Responses::CONTACTADMIN:
        case Responses::NOPERM:
        case Responses::INUSE:
        case Responses::EXPUNGEISSUED:
        case Responses::CORRUPTION:
        case Responses::SERVERBUG:
        case Responses::CLIENTBUG:
        case Responses::CANNOT:
        case Responses::LIMIT:
        case Responses::OVERQUOTA:
        case Responses::ALREADYEXISTS:
        case Responses::NONEXISTENT:
        case Responses::UNKNOWN_CTE:
        case Responses::UIDNOTSTICKY:
        case Responses::TOOBIG:
        case Responses::NOMODSEQ:
        case Responses::COMPRESSIONACTIVE:
        case Responses::CLOSED:
        case Responses::NOTSAVED:
        case Responses::BADCOMPARATOR:
        case Responses::TEMPFAIL:
        case Responses::NOTIFICATIONOVERFLOW:
        case Responses::POLICYDENIED:
        case Responses::SUBMISSIONRACE:
            // check for "no more stuff"
            if (list.count())
                throw InvalidResponseCode("Got a response code with extra data inside the brackets",
                                          line, start);
            respCodeData = QSharedPointer<AbstractData>(new RespData<void>());
            break;
        case Responses::UIDNEXT:
        case Responses::UIDVALIDITY:
        case Responses::UNSEEN:
        case Responses::MAXCONVERTMESSAGES:
        case Responses::MAXCONVERTPARTS:
            // check for "number only"
        {
            if (list.count() != 1)
                throw InvalidResponseCode(line, start);
            bool ok;
            uint number = list.first().toUInt(&ok);
            if (!ok)
                throw InvalidResponseCode(line, start);
            respCodeData = QSharedPointer<AbstractData>(new RespData<uint>(number));
        }
        break;
        case Responses::HIGHESTMODSEQ:
            // similar to the above, but an unsigned 64bit int
        {
            if (list.count() != 1)
                throw InvalidResponseCode(line, start);
            bool ok;
            quint64 number = list.first().toULongLong(&ok);
            if (!ok)
                throw InvalidResponseCode(line, start);
            respCodeData = QSharedPointer<AbstractData>(new RespData<quint64>(number));
        }
        break;
        case Responses::BADCHARSET:
        case Responses::PERMANENTFLAGS:
        case Responses::BADEVENT:
            // The following text should be a parenthesized list of atoms
            if (originalList.size() == 1) {
                if (respCode != Responses::BADCHARSET) {
                    // BADCHARSET can be empty without any problem, but PERMANENTFLAGS or BADEVENT shouldn't
                    qDebug() << "Parser warning: empty PERMANENTFLAGS or BADEVENT";
                }
                respCodeData = QSharedPointer<AbstractData>(new RespData<QStringList>(QStringList()));
            } else if (originalList.size() == 2 && originalList[1].type() == QVariant::List) {
                respCodeData = QSharedPointer<AbstractData>(new RespData<QStringList>(originalList[1].toStringList()));
            } else {
                // Well, we used to accept "* OK [PERMANENTFLAGS foo bar] xyz" for quite long time,
                // so no need to break backwards compatibility here
                respCodeData = QSharedPointer<AbstractData>(new RespData<QStringList>(list));
                qDebug() << "Parser warning: obsolete format of BADCAHRSET/PERMANENTFLAGS/BADEVENT";
            }
            break;
        case Responses::CAPABILITIES:
            // no check here
            respCodeData = QSharedPointer<AbstractData>(new RespData<QStringList>(list));
            break;
        case Responses::ATOM: // no sanity check here, just make a string
        case Responses::NONE: // this won't happen, but if we omit it, gcc warns us about that
            respCodeData = QSharedPointer<AbstractData>(new RespData<QString>(list.join(QLatin1String(" "))));
            break;
        case Responses::NEWNAME:
        case Responses::REFERRAL:
        case Responses::BADURL:
            // FIXME: check if indeed won't eat the spaces
            respCodeData = QSharedPointer<AbstractData>(new RespData<QString>(list.join(QLatin1String(" "))));
            break;
        case Responses::NOUPDATE:
            // FIXME: check if indeed won't eat the spaces, and optionally verify that it came as a quoted string
            respCodeData = QSharedPointer<AbstractData>(new RespData<QString>(list.join(QLatin1String(" "))));
            break;
        case Responses::UNDEFINED_FILTER:
            // FIXME: check if indeed won't eat the spaces, and optionally verify that it came as an atom
            respCodeData = QSharedPointer<AbstractData>(new RespData<QString>(list.join(QLatin1String(" "))));
            break;
        case Responses::APPENDUID:
        {
            // FIXME: this is broken with MULTIAPPEND
            if (originalList.size() != 3)
                throw InvalidResponseCode("Malformed APPENDUID: wrong number of arguments", line, start);
            bool ok;
            uint uidValidity = originalList[1].toUInt(&ok);
            if (!ok)
                throw InvalidResponseCode("Malformed APPENDUID: cannot extract UIDVALIDITY", line, start);
            int pos = 0;
            QByteArray s1 = originalList[2].toByteArray();
            Sequence seq = Sequence::fromList(LowLevelParser::getSequence(s1, pos));
            if (!seq.isValid())
                throw InvalidResponseCode("Malformed APPENDUID: cannot extract UID or the list of UIDs", line, start);
            if (pos != s1.size())
                throw InvalidResponseCode("Malformed APPENDUID: garbage found after the list of UIDs", line, start);
            respCodeData = QSharedPointer<AbstractData>(new RespData<QPair<uint,Sequence> >(qMakePair(uidValidity, seq)));
            break;
        }
        case Responses::COPYUID:
        {
            // FIXME: don't return anything, this is broken with multiple messages, unfortunately
            break;
            if (originalList.size() != 4)
                throw InvalidResponseCode("Malformed COPYUID: wrong number of arguments", line, start);
            bool ok;
            uint uidValidity = originalList[1].toUInt(&ok);
            if (!ok)
                throw InvalidResponseCode("Malformed COPYUID: cannot extract UIDVALIDITY", line, start);
            int pos = 0;
            QByteArray s1 = originalList[2].toByteArray();
            Sequence seq1 = Sequence::fromList(LowLevelParser::getSequence(s1, pos));
            if (!seq1.isValid())
                throw InvalidResponseCode("Malformed COPYUID: cannot extract the first sequence", line, start);
            if (pos != s1.size())
                throw InvalidResponseCode("Malformed COPYUID: garbage found after the first sequence", line, start);
            pos = 0;
            QByteArray s2 = originalList[3].toByteArray();
            Sequence seq2 = Sequence::fromList(LowLevelParser::getSequence(s2, pos));
            if (!seq2.isValid())
                throw InvalidResponseCode("Malformed COPYUID: cannot extract the second sequence", line, start);
            if (pos != s2.size())
                throw InvalidResponseCode("Malformed COPYUID: garbage found after the second sequence", line, start);
            respCodeData = QSharedPointer<AbstractData>(new RespData<QPair<uint,QPair<Sequence, Sequence> > >(
                                                            qMakePair(uidValidity, qMakePair<Sequence, Sequence>(seq1, seq2))));
            break;
        }
        case Responses::URLMECH:
            // FIXME: implement me
            Q_ASSERT(false);
            break;
        case Responses::MODIFIED:
            // FIXME: clarify what "set" in the RFC 4551 means and implement it
            Q_ASSERT(false);
            break;
        case Responses::ANNOTATE:
        {
            if (list.count() != 1)
                throw InvalidResponseCode("ANNOTATE response code got weird number of elements", line, start);
            QString token = list.first().toUpper();
            if (token == QLatin1String("TOOBIG") || token == QLatin1String("TOOMANY"))
                respCodeData = QSharedPointer<AbstractData>(new RespData<QString>(token));
            else
                throw InvalidResponseCode("ANNOTATE response code with invalid value", line, start);
        }
        break;
        case Responses::ANNOTATIONS:
        case Responses::METADATA:
            // FIXME: implement me
            Q_ASSERT(false);
            break;
        }
    }

    if (start >= line.size() - 2) {
        if (originalList.isEmpty()) {
            qDebug() << "Response with no data at all, yuck" << line;
        } else {
            qDebug() << "Response with no data besides the response code, yuck" << line;
        }
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        message = QString::fromUtf8(line.mid(start));
#else
        message = QString::fromUtf8(line.mid(start).constData());
#endif
        Q_ASSERT(message.endsWith(QLatin1String("\r\n")));
        message.chop(2);
    }
}

NumberResponse::NumberResponse(const Kind kind, const uint number) throw(UnexpectedHere):
    kind(kind), number(number)
{
    if (kind != EXISTS && kind != EXPUNGE && kind != RECENT)
        throw UnexpectedHere("Attempted to create NumberResponse of invalid kind");
}

List::List(const Kind kind, const QByteArray &line, int &start):
    kind(kind)
{
    if (kind != LIST && kind != LSUB)
        throw UnexpectedHere(line, start);   // FIXME: well, "start" is too late here...

    flags = QVariant(LowLevelParser::parseList('(', ')', line, start)).toStringList();
    ++start;

    if (start >= line.size() - 5)
        throw NoData(line, start);   // flags and nothing else

    if (line.at(start) == '"') {
        ++start;
        if (line.at(start) == '\\')
            ++start;
        separator = QChar::fromLatin1(line.at(start));
        ++start;
        if (line.at(start) != '"')
            throw ParseError(line, start);
        ++start;
    } else if (line.mid(start, 3).toLower() == "nil") {
        separator = QString();
        start += 3;
    } else {
        throw ParseError(line, start);
    }

    ++start;

    if (start >= line.size() - 2)
        throw NoData(line, start);   // no mailbox

    mailbox = LowLevelParser::getMailbox(line, start);

    LowLevelParser::eatSpaces(line, start);
    if (start < line.size() - 4) {
        QVariantList extData = LowLevelParser::parseList('(', ')', line, start);
        if (extData.size() % 2) {
            throw ParseError("list-extended contains list with odd number of items", line, start);
        }

        for (int i = 0; i < extData.size(); i += 2) {
            QByteArray key = extData[i].toByteArray();
            QVariant data = extData[i+1];
            extendedData[key] = data;
        }
    }

    if (start > line.size() - 2)
        throw TooMuchData(line, start);
}

Flags::Flags(const QByteArray &line, int &start)
{
    flags = QVariant(LowLevelParser::parseList('(', ')', line, start)).toStringList();
    if (start >= line.size())
        throw TooMuchData(line, start);
}

Search::Search(const QByteArray &line, int &start)
{
    while (start < line.size() - 2) {
        try {
            uint number = LowLevelParser::getUInt(line, start);
            items << number;
            ++start;
        } catch (ParseError &) {
            throw UnexpectedHere(line, start);
        }
    }
}

ESearch::ESearch(const QByteArray &line, int &start): seqOrUids(SEQUENCE)
{
    LowLevelParser::eatSpaces(line, start);

    if (start >= line.size() - 2) {
        // an empty ESEARCH response; that shall be OK
        return;
    }

    if (line[start] == '(') {
        // Extract the search-correlator
        ++start;
        if (start >= line.size()) throw NoData(line, start);

        // extract the optional tag specifier
        QByteArray header = LowLevelParser::getAtom(line, start).toUpper();
        if (header != QByteArray("TAG")) {
            throw ParseError("ESEARCH response: malformed search-correlator", line, start);
        }

        LowLevelParser::eatSpaces(line, start);
        if (start >= line.size()) throw NoData(line, start);

        QPair<QByteArray,LowLevelParser::ParsedAs> astring = LowLevelParser::getAString(line, start);
        tag = astring.first;
        if (start >= line.size()) throw NoData(line, start);

        if (line[start] != ')')
            throw ParseError("ESEARCH: search-correlator not enclosed in parentheses", line, start);

        ++start;
        LowLevelParser::eatSpaces(line, start);
    }

    if (start >= line.size() - 2) {
        // So the search-correlator was given, but there isn't anything besides that. Well, let's accept that.
        return;
    }

    // Extract the "UID" specifier, if present
    try {
        int oldStart = start;
        QByteArray uid = LowLevelParser::getAtom(line, start);
        if (uid.toUpper() == QByteArray("UID")) {
            seqOrUids = UIDS;
        } else {
            // got to push the token "back"
            start = oldStart;
        }
    } catch (ParseError &) {
        seqOrUids = SEQUENCE;
    }

    LowLevelParser::eatSpaces(line, start);

    if (start >= line.size() - 2) {
        // No data -> again, accept
        return;
    }

    while (start < line.size() - 2) {
        QByteArray label = LowLevelParser::getAtom(line, start).toUpper();

        LowLevelParser::eatSpaces(line, start);

        if (label == "ADDTO" || label == "REMOVEFROM") {
            // These two are special, their structure is more complex and has to be parsed into a specialized storage

            // We don't really want to use generic LowLevelParser::parseList() here as it'd happily cut our sequence-se
            // into a list of numbers which we'd have to join back together and feed to LowLevelParser::getSequence.
            // This hand-crafted code is better (and isn't so longer, either).

            if (start >= line.size() - 2)
                throw NoData("CONTEXT ESEARCH: no incremental update found", line, start);

            if (line[start] != '(')
                throw UnexpectedHere("CONTEXT ESEARCH: missing '('", line, start);
            ++start;

            do {
                // Each ADDTO/REMOVEFROM record can contain many offsets-seq pairs

                uint offset = LowLevelParser::getUInt(line, start);
                LowLevelParser::eatSpaces(line, start);
                QList<uint> uids = LowLevelParser::getSequence(line, start);

                incrementalContextData.push_back(ContextIncrementalItem(
                                                     (label == "ADDTO" ?
                                                          ContextIncrementalItem::ADDTO :
                                                          ContextIncrementalItem::REMOVEFROM),
                                                     offset, uids));

                LowLevelParser::eatSpaces(line, start);

                if (start >= line.size() - 2)
                    throw NoData("CONTEXT ESEARCH: truncated update record?", line, start);

                if (line[start] == ')') {
                    // end of current ADDTO/REMOVEFROM group
                    ++start;
                    LowLevelParser::eatSpaces(line, start);
                    break;
                }

            } while (true);
        } else if (label == "INCTHREAD") {
            // another special case using completely different structure

            if (start >= line.size() - 2)
                throw NoData("ESEARCH THREAD: no data", line, start);

            const uint previousRoot = LowLevelParser::getUInt(line, start);
            LowLevelParser::eatSpaces(line, start);

            ThreadingNode node;
            while (start < line.size() - 2 && line[start] == '(') {
                QVariantList current = LowLevelParser::parseList('(', ')', line, start);
                threadingHelperInsertHere(&node, current);
            }
            incThreadData.push_back(IncrementalThreadingItem_t(previousRoot, node.children));
            LowLevelParser::eatSpaces(line, start);
        } else {
            // A generic case: be prepapred to accept a (sequence of) numbers

            QList<uint> numbers = LowLevelParser::getSequence(line, start);
            // There's no syntactic difference between a single-item sequence set and one number, which is why we always parse
            // such "sequences" as full blown sequences. That's better than deal with two nasties of the ListData_t kind -- one such
            // beast is more than enough, IMHO.
            listData.push_back(qMakePair<QByteArray, QList<uint> >(label, numbers));

            LowLevelParser::eatSpaces(line, start);
        }
    }
}

Status::Status(const QByteArray &line, int &start)
{
    mailbox = LowLevelParser::getMailbox(line, start);
    ++start;
    if (start >= line.size())
        throw NoData(line, start);
    QStringList items = QVariant(LowLevelParser::parseList('(', ')', line, start)).toStringList();
    if (start != line.size() - 2 && line.mid(start) != QByteArray(" \r\n"))
        throw TooMuchData("STATUS response contains data after the list of items", line, start);

    bool gotIdentifier = false;
    QString identifier;
    for (QStringList::const_iterator it = items.constBegin(); it != items.constEnd(); ++it) {
        if (gotIdentifier) {
            gotIdentifier = false;
            bool ok;
            uint number = it->toUInt(&ok);
            if (!ok)
                throw ParseError(line, start);
            StateKind kind;
            try {
                kind = stateKindFromStr(identifier);
            } catch (UnrecognizedResponseKind &e) {
                throw UnrecognizedResponseKind(e.what(), line, start);
            }
            states[kind] = number;
        } else {
            identifier = *it;
            gotIdentifier = true;
        }
    }
    if (gotIdentifier)
        throw ParseError(line, start);
}

QDateTime Fetch::dateify(QByteArray str, const QByteArray &line, const int start)
{
    // FIXME: all offsets in exceptions are broken here.
    if (str.size() == 25)
        str = QByteArray::number(0) + str;
    if (str.size() != 26)
        throw ParseError(line, start);

    QDateTime date = QLocale(QLocale::C).toDateTime(str.left(20), QLatin1String("d-MMM-yyyy HH:mm:ss"));
    const char sign = str[21];
    bool ok;
    int hours = str.mid(22, 2).toInt(&ok);
    if (!ok)
        throw ParseError(line, start);
    int minutes = str.mid(24, 2).toInt(&ok);
    if (!ok)
        throw ParseError(line, start);
    switch (sign) {
    case '+':
        date = date.addSecs(-3600*hours - 60*minutes);
        break;
    case '-':
        date = date.addSecs(+3600*hours + 60*minutes);
        break;
    default:
        throw ParseError(line, start);
    }
    date.setTimeSpec(Qt::UTC);
    return date;
}

Fetch::Fetch(const uint number, const QByteArray &line, int &start): number(number)
{
    ++start;

    if (start >= line.size())
        throw NoData(line, start);

    if (line[start++] != '(')
        throw UnexpectedHere("FETCH response should consist of a parenthesized list", line, start);

    while (start < line.size() && line[start] != ')') {
        int posBeforeIdentifier = start;
        QByteArray identifier = LowLevelParser::getAtom(line, start).toUpper();
        if (identifier.contains('[')) {
            // special case: these identifiers can contain spaces
            int pos = line.indexOf(']', posBeforeIdentifier);
            if (pos == -1)
                throw UnexpectedHere("FETCH identifier contains \"[\", but no matching \"]\" was found", line, posBeforeIdentifier);
            identifier = line.mid(posBeforeIdentifier, pos - posBeforeIdentifier + 1).toUpper();
            start = pos + 1;
        }

        if (data.contains(identifier))
            throw UnexpectedHere("FETCH response contains duplicate data", line, start);

        if (start >= line.size())
            throw NoData(line, start);

        LowLevelParser::eatSpaces(line, start);

        if (identifier == "MODSEQ") {
            if (line[start++] != '(')
                throw UnexpectedHere("FETCH MODSEQ must be a list");
            data[identifier] = QSharedPointer<AbstractData>(new RespData<quint64>(LowLevelParser::getUInt64(line, start)));
            if (start >= line.size())
                throw NoData(line, start);
            if (line[start++] != ')')
                throw UnexpectedHere("FETCH MODSEQ must be a list");
        } else if (identifier == "FLAGS") {
            if (line[start++] != '(')
                throw UnexpectedHere("FETCH FLAGS must be a list");
            QStringList flags;
            while (start < line.size() && line[start] != ')') {
                flags << QString::fromUtf8(LowLevelParser::getPossiblyBackslashedAtom(line, start));
                LowLevelParser::eatSpaces(line, start);
            }
            data[identifier] = QSharedPointer<AbstractData>(new RespData<QStringList>(flags));
            if (start >= line.size())
                throw NoData(line, start);
            if (line[start++] != ')')
                throw UnexpectedHere("FETCH FLAGS must be a list");
        } else if (identifier == "UID" || identifier == "RFC822.SIZE") {
            data[identifier] = QSharedPointer<AbstractData>(new RespData<uint>(LowLevelParser::getUInt(line, start)));
        } else if (identifier.startsWith("BODY[") || identifier.startsWith("BINARY[") || identifier.startsWith("RFC822")) {
            data[identifier] = QSharedPointer<AbstractData>(new RespData<QByteArray>(LowLevelParser::getNString(line, start).first));
        } else if (identifier == "ENVELOPE") {
            QVariantList list = LowLevelParser::parseList('(', ')', line, start);
            data[identifier] = QSharedPointer<AbstractData>(new RespData<Message::Envelope>(Message::Envelope::fromList(list, line, start)));
        } else if (identifier == "INTERNALDATE") {
            QByteArray buf = LowLevelParser::getNString(line, start).first;
            data[identifier] = QSharedPointer<AbstractData>(new RespData<QDateTime>(dateify(buf, line, start)));
        } else if (identifier == "BODY" || identifier == "BODYSTRUCTURE") {
            QVariantList list = LowLevelParser::parseList('(', ')', line, start);
            data[identifier] = Message::AbstractMessage::fromList(list, line, start);
            QByteArray buffer;
            QDataStream stream(&buffer, QIODevice::WriteOnly);
            stream.setVersion(QDataStream::Qt_4_6);
            stream << list;
            data["x-trojita-bodystructure"] = QSharedPointer<AbstractData>(new RespData<QByteArray>(buffer));
        } else {
            // Unrecognized identifier, let's treat it as QByteArray so that we don't break needlessly
            data[identifier] = QSharedPointer<AbstractData>(new RespData<QByteArray>(LowLevelParser::getNString(line, start).first));
        }

        if (start >= line.size())
            throw NoData(line, start);

        LowLevelParser::eatSpaces(line, start);
    }

    if (start >= line.size())
        throw NoData(line, start);
    if (line[start] == ')')
        ++start;
    if (start != line.size() - 2)
        throw TooMuchData(line, start);
}

Fetch::Fetch(const uint number, const Fetch::dataType &data): number(number), data(data)
{
}

QList<NamespaceData> NamespaceData::listFromLine(const QByteArray &line, int &start)
{
    QList<NamespaceData> result;
    try {
        QVariantList list = LowLevelParser::parseList('(', ')', line, start);
        for (QVariantList::const_iterator it = list.constBegin(); it != list.constEnd(); ++it) {
            if (it->type() != QVariant::List)
                throw UnexpectedHere("Malformed data found when processing one item "
                                     "in NAMESPACE record (not a list)", line, start);
            QStringList list = it->toStringList();
            if (list.size() != 2)
                throw UnexpectedHere("Malformed data found when processing one item "
                                     "in NAMESPACE record (list of weird size)", line, start);
            result << NamespaceData(list[0], list[1]);
        }
    } catch (UnexpectedHere &) {
        // must be a NIL, then
        QPair<QByteArray,LowLevelParser::ParsedAs> res = LowLevelParser::getNString(line, start);
        if (res.second != LowLevelParser::NIL) {
            throw UnexpectedHere("Top-level NAMESPACE record is neither list nor NIL", line, start);
        }
    }
    ++start;
    return result;
}

Namespace::Namespace(const QByteArray &line, int &start)
{
    personal = NamespaceData::listFromLine(line, start);
    users = NamespaceData::listFromLine(line, start);
    other = NamespaceData::listFromLine(line, start);
}

Sort::Sort(const QByteArray &line, int &start)
{
    while (start < line.size() - 2) {
        try {
            uint number = LowLevelParser::getUInt(line, start);
            numbers << number;
            ++start;
        } catch (ParseError &) {
            throw UnexpectedHere(line, start);
        }
    }
}


Thread::Thread(const QByteArray &line, int &start)
{
    ThreadingNode node;
    while (start < line.size() - 2) {
        QVariantList current = LowLevelParser::parseList('(', ')', line, start);
        threadingHelperInsertHere(&node, current);
    }
    rootItems = node.children;
}

/** @short Helper for parsing the brain-dead RFC5256 THREAD response

Please note that the syntax for the THREAD untagged response, as defined in
RFC5256, is rather counter-intuitive -- items placed at the *same* level in the
list are actually parent/child, not siblings.
 */
static void threadingHelperInsertHere(ThreadingNode *where, const QVariantList &what)
{
    bool first = true;
    for (QVariantList::const_iterator it = what.begin(); it != what.end(); ++it) {
        if (it->type() == QVariant::UInt) {
            where->children.append(ThreadingNode());
            where = &(where->children.last());
            where->num = it->toUInt();
        } else if (it->type() == QVariant::List) {
            if (first) {
                where->children.append(ThreadingNode());
                where = &(where->children.last());
            }
            threadingHelperInsertHere(where, it->toList());
        } else {
            throw UnexpectedHere("THREAD structure contains garbage; expecting fancy list of uints");
        }
        first = false;
    }
}

Id::Id(const QByteArray &line, int &start)
{
    try {
        QVariantList list = LowLevelParser::parseList('(', ')', line, start);
        if (list.size() % 2) {
            throw ParseError("ID response with invalid number of entries", line, start);
        }
        for (int i = 0; i < list.size() - 1; i += 2) {
            data[list[i].toByteArray()] = list[i+1].toByteArray();
        }
    } catch (UnexpectedHere &) {
        // Check for NIL
        QPair<QByteArray,LowLevelParser::ParsedAs> nString = LowLevelParser::getNString(line, start);
        if (nString.second != LowLevelParser::NIL) {
            // Let's propagate the original exception from here
            throw;
        } else {
            // It's a NIL, which is explicitly OK, so let's just accept it
        }
    }
}

Enabled::Enabled(const QByteArray &line, int &start)
{
    LowLevelParser::eatSpaces(line, start);
    while (start < line.size() - 2) {
        QByteArray extension = LowLevelParser::getAtom(line, start);
        extensions << extension;
        LowLevelParser::eatSpaces(line, start);
    }
}

Vanished::Vanished(const QByteArray &line, int &start): earlier(NOT_EARLIER)
{
    LowLevelParser::eatSpaces(line, start);

    if (start >= line.size() - 2) {
        throw NoData(line, start);
    }

    const int prefixLength = strlen("(EARLIER)");
    if (start < line.size() - prefixLength && line.mid(start, prefixLength).toUpper() == "(EARLIER)") {
        earlier = EARLIER;
        start += prefixLength + 1; // one for the required space
    }

    uids = LowLevelParser::getSequence(line, start);

    if (start != line.size() - 2)
        throw TooMuchData(line, start);
}

GenUrlAuth::GenUrlAuth(const QByteArray &line, int &start)
{
    url = QString::fromUtf8(LowLevelParser::getAString(line, start).first);
    if (start != line.size() - 2)
        throw TooMuchData(line, start);
}

SocketEncryptedResponse::SocketEncryptedResponse(const QList<QSslCertificate> &sslChain, const QList<QSslError> &sslErrors):
    sslChain(sslChain), sslErrors(sslErrors)
{
}

QTextStream &State::dump(QTextStream &stream) const
{
    if (!tag.isEmpty())
        stream << tag;
    else
        stream << '*';
    stream << ' ' << kind;
    if (respCode != NONE)
        stream << " [" << respCode << ' ' << *respCodeData << ']';
    return stream << ' ' << message;
}

QTextStream &Capability::dump(QTextStream &stream) const
{
    return stream << "* CAPABILITY " << capabilities.join(", ");
}

QTextStream &NumberResponse::dump(QTextStream &stream) const
{
    return stream << kind << " " << number;
}

QTextStream &List::dump(QTextStream &stream) const
{
    stream << kind << " '" << mailbox << "' (" << flags.join(", ") << "), sep '" << separator << "'";
    if (!extendedData.isEmpty()) {
        stream << " (";
        for (QMap<QByteArray,QVariant>::const_iterator it = extendedData.constBegin(); it != extendedData.constEnd(); ++it) {
            stream << it.key() << " " << it.value().toString() << " ";
        }
        stream << ")";
    }
    return stream;
}

QTextStream &Flags::dump(QTextStream &stream) const
{
    return stream << "FLAGS " << flags.join(", ");
}

QTextStream &Search::dump(QTextStream &stream) const
{
    stream << "SEARCH";
    for (QList<uint>::const_iterator it = items.begin(); it != items.end(); ++it)
        stream << " " << *it;
    return stream;
}

QTextStream &ESearch::dump(QTextStream &stream) const
{
    stream << "ESEARCH ";
    if (!tag.isEmpty())
        stream << "TAG " << tag << " ";
    if (seqOrUids == UIDS)
        stream << "UID ";
    for (ListData_t::const_iterator it = listData.constBegin(); it != listData.constEnd(); ++it) {
        stream << it->first << " (";
        Q_FOREACH(const uint number, it->second) {
            stream << number << " ";
        }
        stream << ") ";
    }
    for (IncrementalContextData_t::const_iterator it = incrementalContextData.constBegin();
         it != incrementalContextData.constEnd(); ++it) {
        switch (it->modification) {
        case ContextIncrementalItem::ADDTO:
            stream << "ADDTO (";
            break;
        case ContextIncrementalItem::REMOVEFROM:
            stream << "REMOVEFROM (";
            break;
        default:
            Q_ASSERT(false);
        }
        stream << it->offset << " ";
        Q_FOREACH(const uint num, it->uids) {
            stream << num << ' ';
        }
        stream << ") ";
    }
    for (IncrementalThreadingData_t::const_iterator it = incThreadData.constBegin();
         it != incThreadData.constEnd(); ++it) {
        ThreadingNode node;
        node.children = it->thread;
        stream << "INCTHREAD " << it->previousThreadRoot << " [THREAD parsed-into-sane-form follows] " << threadDumpHelper(node) << " ";
    }
    return stream;
}

QTextStream &Status::dump(QTextStream &stream) const
{
    stream << "STATUS " << mailbox;
    for (stateDataType::const_iterator it = states.begin(); it != states.end(); ++it) {
        stream << " " << it.key() << " " << it.value();
    }
    return stream;
}

QTextStream &Fetch::dump(QTextStream &stream) const
{
    stream << "FETCH " << number << " (";
    for (dataType::const_iterator it = data.begin();
         it != data.end(); ++it)
        stream << ' ' << it.key() << " \"" << *it.value() << '"';
    return stream << ')';
}

QTextStream &Namespace::dump(QTextStream &stream) const
{
    stream << "NAMESPACE (";
    QList<NamespaceData>::const_iterator it;
    for (it = personal.begin(); it != personal.end(); ++it)
        stream << *it << ",";
    stream << ") (";
    for (it = users.begin(); it != users.end(); ++it)
        stream << *it << ",";
    stream << ") (";
    for (it = other.begin(); it != other.end(); ++it)
        stream << *it << ",";
    return stream << ")";
}

QTextStream &Sort::dump(QTextStream &stream) const
{
    stream << "SORT ";
    for (QList<uint>::const_iterator it = numbers.begin(); it != numbers.end(); ++it)
        stream << " " << *it;
    return stream;
}

QTextStream &Thread::dump(QTextStream &stream) const
{
    ThreadingNode node;
    node.children = rootItems;
    return stream << "THREAD {parsed-into-sane-form}" << threadDumpHelper(node);
}

static QString threadDumpHelper(const ThreadingNode &node)
{
    if (node.children.isEmpty()) {
        return QString::number(node.num);
    } else {
        QStringList res;
        for (QVector<ThreadingNode>::const_iterator it = node.children.begin(); it != node.children.end(); ++it) {
            res << threadDumpHelper(*it);
        }
        return QString::fromUtf8("%1: {%2}").arg(node.num).arg(res.join(QLatin1String(", ")));
    }
}

QTextStream &Id::dump(QTextStream &s) const
{
    if (data.isEmpty()) {
        return s << "ID NIL";
    } else {
        s << "ID (";
        for (QMap<QByteArray,QByteArray>::const_iterator it = data.constBegin(); it != data.constEnd(); ++it) {
            if (it != data.constBegin())
                s << " ";
            s << it.key() << " " << it.value();
        }
        return s << ")";
    }
}

QTextStream &Enabled::dump(QTextStream &s) const
{
    s << "ENABLE ";
    Q_FOREACH(const QByteArray &extension, extensions) {
        s << extension << ' ';
    }
    return s;
}

QTextStream &Vanished::dump(QTextStream &s) const
{
    s << "VANISHED ";
    if (earlier == EARLIER)
        s << "(EARLIER) ";
    s << "(";
    Q_FOREACH(const uint &uid, uids) {
        s << " " << uid;
    }
    return s << ")";
}

QTextStream &GenUrlAuth::dump(QTextStream &s) const
{
    return s << "GENURLAUTH " << url;
}

QTextStream &SocketEncryptedResponse::dump(QTextStream &s) const
{
    s << "[Socket is encrypted now; ";
    if (sslErrors.isEmpty()) {
        s << "no errors]";
    } else {
        s << sslErrors.size() << " errors: ";
        Q_FOREACH(const QSslError &e, sslErrors) {
            if (e.certificate().isNull()) {
                s << e.errorString() << " ";
            } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                s << e.errorString() << " (CN: " << e.certificate().subjectInfo(QSslCertificate::CommonName).join(", ") << ") ";
#else
                s << e.errorString() << " (CN: " << e.certificate().subjectInfo(QSslCertificate::CommonName) << ") ";
#endif
            }
        }
        s << "]";
    }
    return s;
}

QTextStream &SocketDisconnectedResponse::dump(QTextStream &s) const
{
    return s << "[Socket disconnected: " << message << "]";
}

QTextStream &ParseErrorResponse::dump(QTextStream &s) const
{
    return s << "[Parse error (" << exceptionClass << "): " << message << "\n" << line << "\n" << offset << "]";
}


template<class T> QTextStream &RespData<T>::dump(QTextStream &stream) const
{
    return stream << data;
}

template<> QTextStream &RespData<QStringList>::dump(QTextStream &stream) const
{
    return stream << data.join(" ");
}

template<> QTextStream &RespData<QDateTime>::dump(QTextStream &stream) const
{
    return stream << data.toString();
}

template<> QTextStream &RespData<QPair<uint,Sequence> >::dump(QTextStream &stream) const
{
    return stream << "UIDVALIDITY " << data.first << " UIDs" << data.second;
}

template<> QTextStream &RespData<QPair<uint,QPair<Sequence, Sequence> > >::dump(QTextStream &stream) const
{
    return stream << "UIDVALIDITY " << data.first << " UIDs-1" << data.second.first << " UIDs-2" << data.second.second;
}

bool RespData<void>::eq(const AbstractData &other) const
{
    try {
        // There's no data involved, so we just want to compare the type
        (void) dynamic_cast<const RespData<void>&>(other);
        return true;
    } catch (std::bad_cast &) {
        return false;
    }
}

template<class T> bool RespData<T>::eq(const AbstractData &other) const
{
    try {
        const RespData<T> &r = dynamic_cast<const RespData<T>&>(other);
        return data == r.data;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Capability::eq(const AbstractResponse &other) const
{
    try {
        const Capability &cap = dynamic_cast<const Capability &>(other);
        return capabilities == cap.capabilities;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool NumberResponse::eq(const AbstractResponse &other) const
{
    try {
        const NumberResponse &num = dynamic_cast<const NumberResponse &>(other);
        return kind == num.kind && number == num.number;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool State::eq(const AbstractResponse &other) const
{
    try {
        const State &s = dynamic_cast<const State &>(other);
        if (kind == s.kind && tag == s.tag && message == s.message && respCode == s.respCode)
            if (respCode == NONE)
                return true;
            else
                return *respCodeData == *s.respCodeData;
        else
            return false;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool List::eq(const AbstractResponse &other) const
{
    try {
        const List &r = dynamic_cast<const List &>(other);
        return kind == r.kind && mailbox == r.mailbox && flags == r.flags && separator == r.separator && extendedData == r.extendedData;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Flags::eq(const AbstractResponse &other) const
{
    try {
        const Flags &fl = dynamic_cast<const Flags &>(other);
        return flags == fl.flags;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Search::eq(const AbstractResponse &other) const
{
    try {
        const Search &s = dynamic_cast<const Search &>(other);
        return items == s.items;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool ESearch::eq(const AbstractResponse &other) const
{
    try {
        const ESearch &s = dynamic_cast<const ESearch &>(other);
        return tag == s.tag && seqOrUids == s.seqOrUids && listData == s.listData &&
                incrementalContextData == s.incrementalContextData && incThreadData == s.incThreadData;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Status::eq(const AbstractResponse &other) const
{
    try {
        const Status &s = dynamic_cast<const Status &>(other);
        return mailbox == s.mailbox && states == s.states;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Fetch::eq(const AbstractResponse &other) const
{
    try {
        const Fetch &f = dynamic_cast<const Fetch &>(other);
        if (number != f.number)
            return false;
        if (data.keys() != f.data.keys())
            return false;
        for (dataType::const_iterator it = data.begin();
             it != data.end(); ++it)
            if (!f.data.contains(it.key()) || *it.value() != *f.data[ it.key() ])
                return false;
        return true;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Namespace::eq(const AbstractResponse &otherResp) const
{
    try {
        const Namespace &ns = dynamic_cast<const Namespace &>(otherResp);
        return ns.personal == personal && ns.users == users && ns.other == other;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Sort::eq(const AbstractResponse &other) const
{
    try {
        const Sort &s = dynamic_cast<const Sort &>(other);
        return numbers == s.numbers;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Thread::eq(const AbstractResponse &other) const
{
    try {
        const Thread &t = dynamic_cast<const Thread &>(other);
        return rootItems == t.rootItems;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Id::eq(const AbstractResponse &other) const
{
    try {
        const Id &r = dynamic_cast<const Id &>(other);
        return data == r.data;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Enabled::eq(const AbstractResponse &other) const
{
    try {
        const Enabled &r = dynamic_cast<const Enabled &>(other);
        return extensions == r.extensions;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool Vanished::eq(const AbstractResponse &other) const
{
    try {
        const Vanished &r = dynamic_cast<const Vanished &>(other);
        return earlier == r.earlier && uids == r.uids;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool GenUrlAuth::eq(const AbstractResponse &other) const
{
    try {
        const GenUrlAuth &r = dynamic_cast<const GenUrlAuth &>(other);
        return url == r.url;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool SocketEncryptedResponse::eq(const AbstractResponse &other) const
{
    try {
        const SocketEncryptedResponse &r = dynamic_cast<const SocketEncryptedResponse &>(other);
        return sslChain == r.sslChain && sslErrors == r.sslErrors;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool SocketDisconnectedResponse::eq(const AbstractResponse &other) const
{
    try {
        const SocketDisconnectedResponse &r = dynamic_cast<const SocketDisconnectedResponse &>(other);
        return message == r.message;
    } catch (std::bad_cast &) {
        return false;
    }
}

ParseErrorResponse::ParseErrorResponse(const ImapException &e):
    message(QString::fromUtf8(e.msg().c_str())), exceptionClass(e.exceptionClass().c_str()), line(e.line()), offset(e.offset())
{
}

bool ParseErrorResponse::eq(const AbstractResponse &other) const
{
    try {
        const ParseErrorResponse &r = dynamic_cast<const ParseErrorResponse &>(other);
        return message == r.message && exceptionClass == r.exceptionClass && line == r.line && offset == r.offset;
    } catch (std::bad_cast &) {
        return false;
    }
}

bool NamespaceData::operator==(const NamespaceData &other) const
{
    return separator == other.separator && prefix == other.prefix;
}

bool NamespaceData::operator!=(const NamespaceData &other) const
{
    return !(*this == other);
}

#define PLUG(X) void X::plug( Imap::Parser* parser, Imap::Mailbox::Model* model ) const \
{ model->handle##X( parser, this ); } \
bool X::plug( Imap::Mailbox::ImapTask* task ) const \
{ return task->handle##X( this ); }

PLUG(State)
PLUG(Capability)
PLUG(NumberResponse)
PLUG(List)
PLUG(Flags)
PLUG(Search)
PLUG(ESearch)
PLUG(Status)
PLUG(Fetch)
PLUG(Namespace)
PLUG(Sort)
PLUG(Thread)
PLUG(Id)
PLUG(Enabled)
PLUG(Vanished)
PLUG(GenUrlAuth)
PLUG(SocketEncryptedResponse)
PLUG(SocketDisconnectedResponse)
PLUG(ParseErrorResponse)

#undef PLUG


}
}

