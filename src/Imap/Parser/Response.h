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
#ifndef IMAP_RESPONSE_H
#define IMAP_RESPONSE_H

#include <QMap>
#include <QPair>
#include <QSharedPointer>
#include <QStringList>
#include <QTextStream>
#include <QVariantList>
#include <QVector>
#include "Command.h"
#include "../Exceptions.h"
#include "Data.h"
#include "ThreadingNode.h"

#ifdef _MSC_VER
// Disable warnings about throw/nothrow
#pragma warning(disable: 4290)
#endif

class QSslCertificate;
class QSslError;

/**
 * @file
 * @short Various data structures related to IMAP responses
 *
 * @author Jan Kundrát <jkt@flaska.net>
 */

/** @short Namespace for IMAP interaction */
namespace Imap
{

namespace Mailbox
{
class Model;
class ImapTask;
}

class Parser;

/** @short IMAP server responses
 *
 * @ref AbstractResponse is an abstact parent of all classes. Each response
 * that might be received from the server is a child of this one.
 * */
namespace Responses
{

/** @short Result of a command */
enum Kind {
    OK /**< @short OK */,
    NO /**< @short NO */,
    BAD /**< @short BAD */,
    BYE,
    PREAUTH,
    EXPUNGE,
    FETCH,
    EXISTS,
    RECENT,
    CAPABILITY,
    LIST,
    LSUB,
    FLAGS,
    SEARCH,
    ESEARCH, /**< @short RFC 4731 ESEARCH */
    STATUS,
    NAMESPACE,
    SORT,
    THREAD,
    ID,
    ENABLED, /**< @short RFC 5161 ENABLE */
    VANISHED, /**< @short RFC 5162 VANISHED (for QRESYNC) */
    GENURLAUTH /**< @short GENURLAUTH, RFC 4467 */
}; // aren't those comments just sexy? :)

/** @short Response Code */
enum Code {
    NONE /**< @short No response code specified */,
    ATOM /**< @short Not recognized */,
    ALERT /**< @short ALERT */,
    BADCHARSET /**< @short BADCHARSET */,
    /** @short CAPABILITY.
     *
     * Yeah, it's different than the RFC3501 name for it.
     * Responses::Kind already defines a CAPABILITY and we aren't using
     * C++0x yet.
     *
     * */
    CAPABILITIES,
    PARSE /**< @short PARSE */,
    PERMANENTFLAGS /**< @short PERMANENTFLAGS */,
    READ_ONLY /**< @short READ-ONLY */,
    READ_WRITE /**< @short READ-WRITE */,
    TRYCREATE /**< @short TRYCREATE */,
    UIDNEXT /**< @short UIDNEXT */,
    UIDVALIDITY /**< @short UIDVALIDITY */,
    UNSEEN /**< @short UNSEEN */,

    // obsolete from RFC 2060
    NEWNAME /**< Obsolete NEWNAME from RFC 2060 */,

    // RFC 2221
    REFERRAL /**< REFERRAL from RFC 2221 */,

    // RFC 3516
    UNKNOWN_CTE /**< UNKNOWN-CTE from RFC 3516 */,

    // RFC 4315
    UIDNOTSTICKY /**< UIDNOTSTICKY from RFC 4315 */,
    APPENDUID /**< APPENDUID from RFC 4315 */,
    COPYUID /**< COPYUID from RFC 4315 */,

    // RFC 4467
    URLMECH /**< URLMECH from RFC 4467 */,

    // RFC 4469
    TOOBIG /**< RFC 4469's TOOBIG */,
    BADURL /**< BADURL from RFC 4469 */,

    // RFC 4551
    HIGHESTMODSEQ /**< HIGHESTMODSEQ from RFC 4451 */,
    NOMODSEQ /**< NOMODSEQ from RFC 4551 */,
    MODIFIED /**< MODIFIED from RFC 4551 */,

    // RFC 4978
    COMPRESSIONACTIVE /**< COMPRESSIONACTIVE from RFC 4978 */,

    // RFC 5162
    CLOSED /**< CLOSED from the QRESYNC RFC 5162 */,

    // RFC 5182
    NOTSAVED /**< NOTSAVED from RFC 5182 */,

    // RFC 5255
    BADCOMPARATOR /**< BADCOMPARATOR from RFC 5255 */,

    // RFC 5257
    ANNOTATE /**< ANNOTATE from RFC 5257 */,
    ANNOTATIONS /**< ANNOTATIONS from RFC 5257 */,

    // RFC 5259
    TEMPFAIL /**< TEMPFAIL from RFC 5259 */,
    MAXCONVERTMESSAGES /**< MAXCONVERTMESSAGES from RFC 5259 */,
    MAXCONVERTPARTS /**< MAXCONVERTPARTS from RFC 5259 */,

    // RFC 5267
    NOUPDATE /**< NOUPDATE from RFC 5267 */,

    // RFC 5464
    METADATA /**< METADATA from RFC 5464 */,

    // RFC 5465
    NOTIFICATIONOVERFLOW /**< NOTIFICATIONOVERFLOW from RFC 5465 */,
    BADEVENT /**< BADEVENT from RFC 5465 */,

    // RFC 5466
    UNDEFINED_FILTER /**< UNDEFINED-FILTER from RFC 5466 */,

    // RFC5530
    UNAVAILABLE /**< UNAVAILABLE from RFC 5530 */,
    AUTHENTICATIONFAILED /**< AUTHENTICATIONFAILED from RFC 5530 */,
    AUTHORIZATIONFAILED /**< AUTHORIZATIONFAILED from RFC 5530 */,
    EXPIRED /**< EXPIRED from RFC 5530 */,
    PRIVACYREQUIRED /**< PRIVACYREQUIRED from RFC 5530 */,
    CONTACTADMIN /**< CONTACTADMIN from RFC 5530 */,
    NOPERM /**< NOPERM from RFC 5530 */,
    INUSE /**< INUSE from RFC 5530 */,
    EXPUNGEISSUED /**< EXPUNGEISSUED from RFC 5530 */,
    CORRUPTION /**< CORRUPTION from RFC 5530 */,
    SERVERBUG /**< SERVERBUG from RFC 5530 */,
    CLIENTBUG /**< CLIENTBUG from RFC 5530 */,
    CANNOT /**< CANNOT from RFC 5530 */,
    LIMIT /**< LIMIT from RFC 5530 */,
    OVERQUOTA /**< OVERQUOTA from RFC 5530 */,
    ALREADYEXISTS /**< ALREADYEXISTS from RFC 5530 */,
    NONEXISTENT /**< NONEXISTENT from RFC 5530 */,

    // draft-imap-sendmail by yours truly
    POLICYDENIED,
    SUBMISSIONRACE

}; // luvly comments, huh? :)

/** @short Parent class for all server responses */
class AbstractResponse
{
public:
    AbstractResponse() {}
    virtual ~AbstractResponse();
    /** @short Helper for operator<<() */
    virtual QTextStream &dump(QTextStream &) const = 0;
    /** @short Helper for operator==() */
    virtual bool eq(const AbstractResponse &other) const = 0;
    /** @short Helper for Imap::Mailbox::MailboxModel to prevent ugly
     * dynamic_cast<>s */
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const = 0;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const = 0;
};

/** @short Structure storing OK/NO/BAD/PREAUTH/BYE responses */
class State : public AbstractResponse
{
public:
    /** @short Tag name or QString::null if untagged */
    QByteArray tag;

    /** @short Kind of response
     *
     * A tagged status response might be either OK, NO or BAD.
     * Untagged status response might be either te same as tagged or BYE or
     * PREAUTH.
     * */
    Kind kind;

    /** @short Textual information embedded in the response
     *
     * While this information might be handy for correct understanding of
     * what happens at ther server, its value is not standardized so the
     * meaning is usually either duplicate to what's already said elsewhere
     * or only a hint to the user. Nevertheless, we decode and store it.
     * */
    QString message;

    /** @short Kind of optional Response Code
     *
     * For each supported value, type of ResponseCodeData stored in the
     * respCodeData is defined as follows:
     *
     *  ALERT, PARSE, READ_ONLY, READ_WRITE, TRYCREATE:
     *      Nothing else should be included, ie. void
     *
     *  UIDNEXT, UIDVALIDITY, UNSEEN:
     *      Only number, ie. unsigned int
     *
     *  BADCHARSET, PERMANENTFLAGS:
     *      List of strings, ie. QStringList
     *
     *  default:
     *      Any data, ie. QString
     * */
    Code respCode;

    /** @short Response Code Data
     *
     * Format is explained in the respCode documentation.
     * We have to use pointer indirection because virtual methods wouldn't
     * work otherwise.
     * */
    QSharedPointer<AbstractData> respCodeData;

    /** @short Default constructor
     *
     * No error checking takes place, we assume _respCodeData's type
     * actually corresponds to all invariants we declare as per respCode's
     * documentation.
     * */
    State(const QByteArray &tag, const Kind kind, const QString &message, const Code respCode, const QSharedPointer<AbstractData> respCodeData):
        tag(tag), kind(kind), message(message), respCode(respCode), respCodeData(respCodeData) {}

    /** @short "Smart" constructor that parses a response out of a QByteArray */
    State(const QByteArray &tag, const Kind kind, const QByteArray &line, int &start);

    /** @short Default destructor that makes containers and QtTest happy */
    State(): respCode(NONE) {}

    /** @short helper for operator<<( QTextStream& ) */
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing a CAPABILITY untagged response */
class Capability : public AbstractResponse
{
public:
    /** @short List of capabilities */
    QStringList capabilities;
    Capability(const QStringList &capabilities): capabilities(capabilities) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure for EXISTS/EXPUNGE/RECENT responses */
class NumberResponse : public AbstractResponse
{
public:
    Kind kind;
    /** @short Number that we're storing */
    uint number;
    NumberResponse(const Kind kind, const uint number) throw(UnexpectedHere);
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing a LIST untagged response */
class List : public AbstractResponse
{
public:
    /** @short LIST or LSUB */
    Kind kind;
    /** @short Flags for this particular mailbox */
    QStringList flags;
    /** @short Hierarchy separator
     *
     * QString::null in case original response containded NIL
     * */
    QString separator;
    /** @short Mailbox name */
    QString mailbox;

    /** @short Extended LIST data (mbox-list-extended from RFC5258) */
    QMap<QByteArray, QVariant> extendedData;

    /** @short Parse line and construct List object from it */
    List(const Kind kind, const QByteArray &line, int &start);
    List(const Kind kind, const QStringList &flags, const QString &separator, const QString &mailbox,
         const QMap<QByteArray, QVariant> &extendedData):
        kind(kind), flags(flags), separator(separator), mailbox(mailbox), extendedData(extendedData) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

struct NamespaceData {
    QString prefix;
    QString separator;
    NamespaceData(const QString &prefix, const QString &separator): prefix(prefix), separator(separator) {};
    bool operator==(const NamespaceData &other) const;
    bool operator!=(const NamespaceData &other) const;
    static QList<NamespaceData> listFromLine(const QByteArray &line, int &start);
};

/** @short Structure storing a NAMESPACE untagged response */
class Namespace : public AbstractResponse
{
public:
    QList<NamespaceData> personal, users, other;
    /** @short Parse line and construct List object from it */
    Namespace(const QByteArray &line, int &start);
    Namespace(const QList<NamespaceData> &personal, const QList<NamespaceData> &users,
              const QList<NamespaceData> &other):
        personal(personal), users(users), other(other) {};
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};


/** @short Structure storing a FLAGS untagged response */
class Flags : public AbstractResponse
{
public:
    /** @short List of flags */
    QStringList flags;
    Flags(const QStringList &flags) : flags(flags) {};
    Flags(const QByteArray &line, int &start);
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing a SEARCH untagged response */
class Search : public AbstractResponse
{
public:
    /** @short List of matching messages */
    QList<uint> items;
    Search(const QByteArray &line, int &start);
    Search(const QList<uint> &items) : items(items) {};
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing an ESEARCH untagged response */
class ESearch : public AbstractResponse
{
public:
    /** @short Is the response given in UIDs, or just in sequence numbers */
    typedef enum {
        SEQUENCE, /**< @short In sequence numbers */
        UIDS /**< @short In UIDs */
    } SequencesOrUids;

    /** @short Convenience typedef for the received data of the list type */
    typedef QList<QPair<QByteArray, QList<uint> > > ListData_t;

    /** @short Compare identifiers of the ListData_t list */
    template <typename T>
    class CompareListDataIdentifier: public std::unary_function<const typename T::value_type&, bool> {
        QByteArray keyOne;
        QByteArray keyTwo;
        bool hasKeyTwo;
    public:
        /** @short Find a record which matches the given key */
        explicit CompareListDataIdentifier(const QByteArray &key): keyOne(key), hasKeyTwo(false) {}

        /** @short Find a record matching any of the two passed keys */
        explicit CompareListDataIdentifier(const QByteArray &keyOne, const QByteArray &keyTwo):
            keyOne(keyOne), keyTwo(keyTwo), hasKeyTwo(true) {}

        bool operator() (const typename T::value_type & item) {
            if (hasKeyTwo) {
                return item.first == keyOne || item.first == keyTwo;
            } else {
                return item.first == keyOne;
            }
        }
    };

    /** @short Represent one item to be added/removed by an incremental SEARCH/SORT response */
    struct ContextIncrementalItem {

        /** @short Is this incremental update record about adding an item, or removing it?

        These correspond to RFC 5267's ADDTO and REMOVEFROM identifiers.
        */
        typedef enum {
            ADDTO, /**< ADDTO, adding items to the search/sort criteria */
            REMOVEFROM /**< REMOVEFROM, removing items from the list of matches */
        } Modification;

        /** @short Type of modification */
        Modification modification;

        /** @short Offset at which the modification shall be performed */
        uint offset;

        /** @short Sequence of UIDs to apply */
        QList<uint> uids;

        ContextIncrementalItem(const Modification modification, const uint offset, const QList<uint> &uids):
            modification(modification), offset(offset), uids(uids) {}

        bool operator==(const ContextIncrementalItem &other) const {
            return modification == other.modification && offset == other.offset && uids == other.uids;
        }
    };

    typedef QList<ContextIncrementalItem> IncrementalContextData_t;

    /** @short The tag of the command which requested in this operation */
    QByteArray tag;

    /** @short Are the numbers given in UIDs, or as sequence numbers? */
    SequencesOrUids seqOrUids;

    /** @short The received data: list of numbers */
    ListData_t listData;

    /** @short The received data: incremental updates to SEARCH/SORT according to RFC 5267 */
    IncrementalContextData_t incrementalContextData;

    /** @short Incremental threading information along its identifier and the preceding thread root in an ESEARCH response */
    struct IncrementalThreadingItem_t {
        /** @short UID of the previous thread root's item or 0 if there's no previous item */
        uint previousThreadRoot;

        /** @short A complete subthread */
        QVector<ThreadingNode> thread;

        IncrementalThreadingItem_t(const uint previousThreadRoot, const QVector<ThreadingNode> &thread):
            previousThreadRoot(previousThreadRoot), thread(thread) {}

        bool operator==(const IncrementalThreadingItem_t &other) const {
            return previousThreadRoot == other.previousThreadRoot && thread == other.thread;
        }
    };

    /** @short Typedef for all threading data sent over ESEARCH */
    typedef QList<IncrementalThreadingItem_t> IncrementalThreadingData_t;

    /** @short The threading information, draft-imap-incthread */
    IncrementalThreadingData_t incThreadData;

    // Other forms of returned data are quite explicitly not supported.

    ESearch(const QByteArray &line, int &start);
    ESearch(const QByteArray &tag, const SequencesOrUids seqOrUids, const ListData_t &listData) :
        tag(tag), seqOrUids(seqOrUids), listData(listData) {}
    ESearch(const QByteArray &tag, const SequencesOrUids seqOrUids, const IncrementalContextData_t &incrementalContextData) :
        tag(tag), seqOrUids(seqOrUids), incrementalContextData(incrementalContextData) {}
    ESearch(const QByteArray &tag, const SequencesOrUids seqOrUids, const IncrementalThreadingData_t &incThreadData):
        tag(tag), seqOrUids(seqOrUids), incThreadData(incThreadData) {}
    virtual QTextStream &dump(QTextStream &stream) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing a STATUS untagged response */
class Status : public AbstractResponse
{
public:
    /** @short Indentifies type of status data */
    enum StateKind {
        MESSAGES,
        RECENT,
        UIDNEXT,
        UIDVALIDITY,
        UNSEEN
    };

    typedef QMap<StateKind,uint> stateDataType;

    /** @short Mailbox name */
    QString mailbox;
    /** @short Associative array of states */
    stateDataType states;

    Status(const QString &mailbox, const stateDataType &states) :
        mailbox(mailbox), states(states) {};
    Status(const QByteArray &line, int &start);
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    static StateKind stateKindFromStr(QString s);
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short FETCH response */
class Fetch : public AbstractResponse
{
public:
    typedef QMap<QByteArray,QSharedPointer<AbstractData> > dataType;

    /** @short Sequence number of message that we're working with */
    uint number;

    /** @short Fetched items */
    dataType data;

    Fetch(const uint number, const QByteArray &line, int &start);
    Fetch(const uint number, const dataType &data);
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
private:
    static QDateTime dateify(QByteArray str, const QByteArray &line, const int start);
};

/** @short Structure storing a SORT untagged response */
class Sort : public AbstractResponse
{
public:
    /** @short List of sequence/UID numbers as returned by the server */
    QList<uint> numbers;
    Sort(const QByteArray &line, int &start);
    Sort(const QList<uint> &items): numbers(items) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing a THREAD untagged response */
class Thread : public AbstractResponse
{
public:
    /** @short List of "top-level" messages */
    QVector<ThreadingNode> rootItems;
    Thread(const QByteArray &line, int &start);
    Thread(const QVector<ThreadingNode> &items): rootItems(items) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing the result of the ID command */
class Id : public AbstractResponse
{
public:
    /** @short List of sequence/UID numbers as returned by the server */
    QMap<QByteArray,QByteArray> data;
    Id(const QByteArray &line, int &start);
    Id(const QMap<QByteArray,QByteArray> &items): data(items) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short Structure storing each enabled extension */
class Enabled: public AbstractResponse
{
public:
    QList<QByteArray> extensions;
    Enabled(const QByteArray &line, int &start);
    Enabled(const QList<QByteArray> &extensions): extensions(extensions) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short VANISHED contains information about UIDs of removed messages */
class Vanished: public AbstractResponse
{
public:
    typedef enum {EARLIER, NOT_EARLIER} EarlierOrNow;
    EarlierOrNow earlier;
    QList<uint> uids;
    Vanished(const QByteArray &line, int &start);
    Vanished(EarlierOrNow earlier, const QList<uint> &uids): earlier(earlier), uids(uids) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short The GENURLAUTH response */
class GenUrlAuth: public AbstractResponse
{
public:
    QString url;
    GenUrlAuth(const QByteArray &line, int &start);
    GenUrlAuth(const QString &url): url(url) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short A fake response for passing along the SSL state */
class SocketEncryptedResponse : public AbstractResponse
{
public:
    QList<QSslCertificate> sslChain;
    QList<QSslError> sslErrors;
    /** @short List of sequence/UID numbers as returned by the server */
    SocketEncryptedResponse(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &sslErrors);
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short A fake response saying that the socket got disconnected */
class SocketDisconnectedResponse : public AbstractResponse
{
public:
    QString message;
    explicit SocketDisconnectedResponse(const QString &message): message(message) {}
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

/** @short A fake response about a parsing error */
class ParseErrorResponse : public AbstractResponse
{
public:
    QString message;
    QByteArray exceptionClass;
    QByteArray line;
    int offset;
    explicit ParseErrorResponse(const ImapException &e);
    virtual QTextStream &dump(QTextStream &s) const;
    virtual bool eq(const AbstractResponse &other) const;
    virtual void plug(Imap::Parser *parser, Imap::Mailbox::Model *model) const;
    virtual bool plug(Imap::Mailbox::ImapTask *task) const;
};

QTextStream &operator<<(QTextStream &stream, const Code &r);
QTextStream &operator<<(QTextStream &stream, const Kind &res);
QTextStream &operator<<(QTextStream &stream, const Status::StateKind &kind);
QTextStream &operator<<(QTextStream &stream, const AbstractResponse &res);
QTextStream &operator<<(QTextStream &stream, const NamespaceData &res);

inline bool operator==(const AbstractResponse &first, const AbstractResponse &other)
{
    return first.eq(other);
}

inline bool operator!=(const AbstractResponse &first, const AbstractResponse &other)
{
    return !first.eq(other);
}

/** @short Build Responses::Kind from textual value */
Kind kindFromString(QByteArray str) throw(UnrecognizedResponseKind);

}

}

#endif // IMAP_RESPONSE_H
