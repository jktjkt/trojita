/* Copyright (C) 2012 - 2013 Peter Amidon <peter@picnicpark.org>

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


#include <QtTest>
#include "test_SenderIdentitiesModel.h"
#include "Utils/ModelEvents.h"
#include "Utils/headless_test.h"
#include "Common/MetaTypes.h"

#define COMPARE_ROWNUM(identity, rowNumber) \
  QVERIFY(model->index(rowNumber, SenderIdentitiesModel::COLUMN_NAME).isValid()); \
  QCOMPARE(model->data(model->index(rowNumber, SenderIdentitiesModel::COLUMN_NAME)).toString(), identity.realName); \
  QCOMPARE(model->data(model->index(rowNumber, SenderIdentitiesModel::COLUMN_SIGNATURE)).toString(), identity.signature);

#define COMPARE_INDEX(identity, indexName, indexSignature) \
  QVERIFY(indexName.isValid()); \
  QVERIFY(indexSignature.isValid()); \
  QCOMPARE(model->data(indexName).toString(), identity.realName); \
  QCOMPARE(model->data(indexSignature).toString(), identity.signature);

using namespace Composer;

SenderIdentitiesModelTest::SenderIdentitiesModelTest() :
    identity1("test name #1", "test1@mail.org", "testing", "tester 1"),
    identity2("test name #2", "test2@mail.org", "testing", "tester 2"),
    identity3("test name #3", "test3@mail.org", "testing", "tester 3")
{
}

void SenderIdentitiesModelTest::initTestCase()
{
    Common::registerMetaTypes();
}

void SenderIdentitiesModelTest::init()
{
    model = new SenderIdentitiesModel();
}

void SenderIdentitiesModelTest::testPropertyStorage()
{
    QLatin1String realName("Testing Tester");
    QLatin1String emailAddress("test@testing.org");
    QLatin1String organization("Testing Inc.");
    QLatin1String signature("A tester working for Testing Inc.");
    ItemSenderIdentity testIdentity(realName, emailAddress, organization, signature);
    QCOMPARE(testIdentity.realName, realName);
    QCOMPARE(testIdentity.emailAddress, emailAddress);
    QCOMPARE(testIdentity.organisation, organization);
    QCOMPARE(testIdentity.signature, signature);
}

void SenderIdentitiesModelTest::testAddIdentity()
{
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeInserted(QModelIndex, int, int)));
    QSignalSpy spy2(model, SIGNAL(rowsInserted(QModelIndex, int, int)));
    model->appendIdentity(identity1);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy2.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    COMPARE_ROWNUM(identity1, 0);
}

void SenderIdentitiesModelTest::testRemoveIdentity1()
{
    model->appendIdentity(identity1);
    model->appendIdentity(identity2);
    QCOMPARE(model->rowCount(), 2);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)));
    QSignalSpy spy2(model, SIGNAL(rowsRemoved(QModelIndex, int, int)));
    model->removeIdentityAt(1);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy.at(0)), ModelInsertRemoveEvent(QModelIndex(), 1, 1));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy2.at(0)), ModelInsertRemoveEvent(QModelIndex(), 1, 1)) ;
    QCOMPARE(model->rowCount(), 1);
    COMPARE_ROWNUM(identity1, 0);
}

void SenderIdentitiesModelTest::testRemoveIdentity2()
{
    model->appendIdentity(identity1);
    model->appendIdentity(identity2);
    QCOMPARE(model->rowCount(), 2);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)));
    QSignalSpy spy2(model, SIGNAL(rowsRemoved(QModelIndex, int, int)));
    model->removeIdentityAt(0);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy2.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    QCOMPARE(model->rowCount(), 1);
    COMPARE_ROWNUM(identity2, 0);
}

void SenderIdentitiesModelTest::testMoveFirstIdentity()
{
    model->appendIdentity(identity1);
    model->appendIdentity(identity2);
    model->appendIdentity(identity3);
    QCOMPARE(model->rowCount(), 3);
    QPersistentModelIndex nameIndex =
        model->index(0, SenderIdentitiesModel::COLUMN_NAME);
    QPersistentModelIndex signatureIndex =
        model->index(0, SenderIdentitiesModel::COLUMN_SIGNATURE);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)));
    QSignalSpy spy2(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
    model->moveIdentity(0, 2);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 0, 0, QModelIndex(), 3));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 0, 0, QModelIndex(), 3));
    COMPARE_ROWNUM(identity2, 0);
    COMPARE_ROWNUM(identity1, 2);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
    COMPARE_ROWNUM(identity3, 1);
    model->appendIdentity(identity3);
    QCOMPARE(model->rowCount(), 4);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
    model->removeIdentityAt(0);
    QCOMPARE(model->rowCount(), 3);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
}

void SenderIdentitiesModelTest::testMoveLastIdentity()
{
    model->appendIdentity(identity1);
    model->appendIdentity(identity2);
    model->appendIdentity(identity3);
    QCOMPARE(model->rowCount(), 3);
    QPersistentModelIndex nameIndex =
        model->index(0, SenderIdentitiesModel::COLUMN_NAME);
    QPersistentModelIndex signatureIndex =
        model->index(0, SenderIdentitiesModel::COLUMN_SIGNATURE);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)));
    QSignalSpy spy2(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
    model->moveIdentity(2, 0);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 2, 2, QModelIndex(), 0));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 2, 2, QModelIndex(), 0));
    COMPARE_ROWNUM(identity3, 0);
    COMPARE_ROWNUM(identity1, 1);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
    COMPARE_ROWNUM(identity2, 2);
    model->appendIdentity(identity3);
    QCOMPARE(model->rowCount(), 4);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
    model->removeIdentityAt(0);
    QCOMPARE(model->rowCount(), 3);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
}

void SenderIdentitiesModelTest::testMoveMiddleIdentity()
{
    model->appendIdentity(identity1);
    model->appendIdentity(identity2);
    model->appendIdentity(identity3);
    QCOMPARE(model->rowCount(), 3);
    QPersistentModelIndex nameIndex =
        model->index(0, SenderIdentitiesModel::COLUMN_NAME);
    QPersistentModelIndex signatureIndex =
        model->index(0, SenderIdentitiesModel::COLUMN_SIGNATURE);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)));
    QSignalSpy spy2(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
    model->moveIdentity(1, 2);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 1, 1, QModelIndex(), 3));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 1, 1, QModelIndex(), 3));
    COMPARE_ROWNUM(identity3, 1);
    COMPARE_ROWNUM(identity1, 0);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
    COMPARE_ROWNUM(identity2, 2);
    model->appendIdentity(identity3);
    QCOMPARE(model->rowCount(), 4);
    COMPARE_INDEX(identity1, nameIndex, signatureIndex);
    model->removeIdentityAt(0);
    QCOMPARE(model->rowCount(), 3);
    QVERIFY(!nameIndex.isValid());
    QVERIFY(!signatureIndex.isValid());
}

void SenderIdentitiesModelTest::cleanup()
{
    delete model;
}



TROJITA_HEADLESS_TEST( SenderIdentitiesModelTest )
