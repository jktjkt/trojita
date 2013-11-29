/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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

#include <QTest>
#include "test_SqlCache.h"
#include "Utils/headless_test.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/SQLCache.h"

Q_DECLARE_METATYPE(QList<Imap::Mailbox::MailboxMetadata>)

namespace QTest {
template<>
char *toString(const QList<Imap::Mailbox::MailboxMetadata> &items)
{
    QString buf;
    QDebug dbg(&buf);
    dbg << "[";
    Q_FOREACH(const Imap::Mailbox::MailboxMetadata &metadata, items) {
        dbg << "MailboxMetadata(" << metadata.mailbox << metadata.separator << metadata.flags << "),";
    }
    dbg << "]";
    return qstrdup(buf.toUtf8().constData());
}
}

void TestSqlCache::init()
{
    cache = new Imap::Mailbox::SQLCache(this);
    errorSpy = new QSignalSpy(cache, SIGNAL(error(QString)));
    QCOMPARE(static_cast<Imap::Mailbox::SQLCache*>(cache)->open(QLatin1String("meh"), QLatin1String(":memory:")), true);
}

void TestSqlCache::cleanup()
{
    delete cache;
    cache = 0;
    delete errorSpy;
    errorSpy = 0;
}

#define CHECK_CACHE_ERRORS \
    if (!errorSpy->isEmpty()) { \
        QCOMPARE(errorSpy->first()[0].toString(), QString()); \
    }

void TestSqlCache::testMailboxOperation()
{
    using namespace Imap::Mailbox;

    QCOMPARE(cache->childMailboxesFresh(QString()), false);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->childMailboxesFresh(QLatin1String("foo")), false);
    CHECK_CACHE_ERRORS;

    QList<MailboxMetadata> toplevel;
    toplevel << MailboxMetadata(QLatin1String("INBOX"), QString("."), QStringList());
    toplevel << MailboxMetadata(QLatin1String("a"), QString(), QStringList());
    toplevel << MailboxMetadata(QLatin1String("b"), QString(), QStringList() << QLatin1String("\\HASNOCHILDREN"));
    toplevel << MailboxMetadata(QLatin1String("c"), QString(), QStringList() << QLatin1String("\\HASCHILDREN"));

    cache->setChildMailboxes(QString(), toplevel);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->childMailboxes(QString()), toplevel);
    CHECK_CACHE_ERRORS;

    // Check that the cache really removes items
    toplevel.takeLast();
    cache->setChildMailboxes(QString(), toplevel);
    CHECK_CACHE_ERRORS;
    QCOMPARE(cache->childMailboxes(QString()), toplevel);
    CHECK_CACHE_ERRORS;

    QVERIFY(errorSpy->isEmpty());
}

/** @short Measure how fast our flag updates and lookups are */
void TestSqlCache::testFlagBenchmark()
{
    using namespace Imap::Mailbox;

    const int messages = 100000;
    const QString mailbox = QLatin1String("blesmrt.333");

    QBENCHMARK_ONCE {
        for (auto i = 0; i < messages; ++i) {
            cache->setMsgFlags(mailbox, i + 1,
                               i % 2 ?
                                   QStringList() << QLatin1String("\\Seen") :
                                   QStringList() << QLatin1String("\\Seen") << QLatin1String("\\Answered"));
        }
        cache->prepareStreamedFlags(mailbox);
        for (auto i = 0; i < messages; ++i) {
            auto res = cache->iterateStreamedFlags();
            QVERIFY(res.isValid);
            QCOMPARE(res.uid, static_cast<uint>(i) + 1);
            QCOMPARE(res.flags, i % 2 ?
                                   QStringList() << QLatin1String("\\Seen") :
                                   QStringList() << QLatin1String("\\Seen") << QLatin1String("\\Answered"));
        }
        cache->freeStreamedFlags();
    }
    cache->clearAllMessages(mailbox);
}

/** @short Test streaming flags with the memory cache */
void TestSqlCache::testStreamedCachingMemory()
{
    helperStreamedCaching(true);
}

/** @short Test streaming flags with the SQL cache */
void TestSqlCache::testStreamedCachingSql()
{
    helperStreamedCaching(false);
}

/** @short Check whether the cache streaming works reasonably */
void TestSqlCache::helperStreamedCaching(bool useMemoryCache)
{
    if (useMemoryCache) {
        cleanup();
        cache = new Imap::Mailbox::MemoryCache(this);
        errorSpy = new QSignalSpy(cache, SIGNAL(error(QString)));
    }
    QString mbox = QLatin1String("a");
    QStringList flags3 = QStringList() << QLatin1String("x");
    QStringList flags66 = QStringList() << QLatin1String("yy");
    QStringList flags333 = QStringList() << QLatin1String("zzz");
    cache->setMsgFlags(mbox, 333, flags333);
    cache->setMsgFlags(mbox, 3, flags3);
    cache->setMsgFlags(mbox, 66, flags66);

    cache->prepareStreamedFlags(mbox);
    auto item = cache->iterateStreamedFlags();
    QVERIFY(item.isValid);
    QCOMPARE(item.uid, 3u);
    QCOMPARE(item.flags, flags3);

    item = cache->iterateStreamedFlags();
    QVERIFY(item.isValid);
    QCOMPARE(item.uid, 66u);
    QCOMPARE(item.flags, flags66);

    item = cache->iterateStreamedFlags();
    QVERIFY(item.isValid);
    QCOMPARE(item.uid, 333u);
    QCOMPARE(item.flags, flags333);

    item = cache->iterateStreamedFlags();
    QVERIFY(!item.isValid);
    item = cache->iterateStreamedFlags();
    QVERIFY(!item.isValid);
}

TROJITA_HEADLESS_TEST(TestSqlCache)
