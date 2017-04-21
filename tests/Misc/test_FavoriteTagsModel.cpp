/*
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
#include "test_FavoriteTagsModel.h"
#include "Utils/ModelEvents.h"
#include "Common/MetaTypes.h"

#define COMPARE_ROWNUM(favoriteTag, rowNumber) \
  QVERIFY(model->index(rowNumber, FavoriteTagsModel::COLUMN_NAME).isValid()); \
  QCOMPARE(model->data(model->index(rowNumber, FavoriteTagsModel::COLUMN_NAME)).toString(), favoriteTag.name); \
  QCOMPARE(model->data(model->index(rowNumber, FavoriteTagsModel::COLUMN_COLOR)).toString(), favoriteTag.color);

#define COMPARE_INDEX(favoriteTag, indexName, indexColor) \
  QVERIFY(indexName.isValid()); \
  QVERIFY(indexColor.isValid()); \
  QCOMPARE(model->data(indexName).toString(), favoriteTag.name); \
  QCOMPARE(model->data(indexColor).toString(), favoriteTag.color);

using namespace Imap::Mailbox;

FavoriteTagsModelTest::FavoriteTagsModelTest() :
    favoriteTag1(QStringLiteral("test name #1"), QStringLiteral("red")),
    favoriteTag2(QStringLiteral("test name #2"), QStringLiteral("green")),
    favoriteTag3(QStringLiteral("test name #3"), QStringLiteral("blue"))
{
}

void FavoriteTagsModelTest::initTestCase()
{
    Common::registerMetaTypes();
}

void FavoriteTagsModelTest::init()
{
    model = new FavoriteTagsModel();
}
void FavoriteTagsModelTest::cleanup()
{
    delete model;
}

void FavoriteTagsModelTest::testPropertyStorage()
{
    QLatin1String name("Testing Tester");
    QLatin1String color("red");
    ItemFavoriteTagItem testFavoriteTag(name, color);
    QCOMPARE(testFavoriteTag.name, name);
    QCOMPARE(testFavoriteTag.color, color);
}

void FavoriteTagsModelTest::testAddTag()
{
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeInserted(QModelIndex, int, int)));
    QSignalSpy spy2(model, SIGNAL(rowsInserted(QModelIndex, int, int)));
    model->appendTag(favoriteTag1);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy2.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    COMPARE_ROWNUM(favoriteTag1, 0);
}

void FavoriteTagsModelTest::testRemoveFirstTag()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    QCOMPARE(model->rowCount(), 2);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)));
    QSignalSpy spy2(model, SIGNAL(rowsRemoved(QModelIndex, int, int)));
    model->removeTagAt(1);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy.at(0)), ModelInsertRemoveEvent(QModelIndex(), 1, 1));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy2.at(0)), ModelInsertRemoveEvent(QModelIndex(), 1, 1)) ;
    QCOMPARE(model->rowCount(), 1);
    COMPARE_ROWNUM(favoriteTag1, 0);
}

void FavoriteTagsModelTest::testRemoveLastTag()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    QCOMPARE(model->rowCount(), 2);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)));
    QSignalSpy spy2(model, SIGNAL(rowsRemoved(QModelIndex, int, int)));
    model->removeTagAt(0);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelInsertRemoveEvent(spy2.at(0)), ModelInsertRemoveEvent(QModelIndex(), 0, 0));
    QCOMPARE(model->rowCount(), 1);
    COMPARE_ROWNUM(favoriteTag2, 0);
}

void FavoriteTagsModelTest::testMoveFirstTag()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QPersistentModelIndex nameIndex =
        model->index(0, FavoriteTagsModel::COLUMN_NAME);
    QPersistentModelIndex colorIndex =
        model->index(0, FavoriteTagsModel::COLUMN_COLOR);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)));
    QSignalSpy spy2(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
    model->moveTag(0, 2);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 0, 0, QModelIndex(), 3));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 0, 0, QModelIndex(), 3));
    COMPARE_ROWNUM(favoriteTag2, 0);
    COMPARE_ROWNUM(favoriteTag1, 2);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
    COMPARE_ROWNUM(favoriteTag3, 1);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 4);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
    model->removeTagAt(0);
    QCOMPARE(model->rowCount(), 3);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
}

void FavoriteTagsModelTest::testMoveLastTag()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QPersistentModelIndex nameIndex =
        model->index(0, FavoriteTagsModel::COLUMN_NAME);
    QPersistentModelIndex colorIndex =
        model->index(0, FavoriteTagsModel::COLUMN_COLOR);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)));
    QSignalSpy spy2(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
    model->moveTag(2, 0);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 2, 2, QModelIndex(), 0));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 2, 2, QModelIndex(), 0));
    COMPARE_ROWNUM(favoriteTag3, 0);
    COMPARE_ROWNUM(favoriteTag1, 1);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
    COMPARE_ROWNUM(favoriteTag2, 2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 4);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
    model->removeTagAt(0);
    QCOMPARE(model->rowCount(), 3);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
}

void FavoriteTagsModelTest::testMoveMiddleTag()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QPersistentModelIndex nameIndex =
        model->index(0, FavoriteTagsModel::COLUMN_NAME);
    QPersistentModelIndex colorIndex =
        model->index(0, FavoriteTagsModel::COLUMN_COLOR);
    QSignalSpy spy(model, SIGNAL(rowsAboutToBeMoved(QModelIndex, int, int, QModelIndex, int)));
    QSignalSpy spy2(model, SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)));
    model->moveTag(1, 2);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 1, 1, QModelIndex(), 3));
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(ModelMoveEvent(spy.at(0)), ModelMoveEvent(QModelIndex(), 1, 1, QModelIndex(), 3));
    COMPARE_ROWNUM(favoriteTag3, 1);
    COMPARE_ROWNUM(favoriteTag1, 0);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
    COMPARE_ROWNUM(favoriteTag2, 2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 4);
    COMPARE_INDEX(favoriteTag1, nameIndex, colorIndex);
    model->removeTagAt(0);
    QCOMPARE(model->rowCount(), 3);
    QVERIFY(!nameIndex.isValid());
    QVERIFY(!colorIndex.isValid());
}

void FavoriteTagsModelTest::testFindBestColorForTags1()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QColor color = model->findBestColorForTags({favoriteTag1.name, favoriteTag2.name, favoriteTag3.name});
    QVERIFY(color.isValid());
    QCOMPARE(color.name(), QColor(favoriteTag1.color).name());
}

void FavoriteTagsModelTest::testFindBestColorForTags2()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QColor color = model->findBestColorForTags({favoriteTag3.name, favoriteTag2.name, favoriteTag1.name});
    QVERIFY(color.isValid());
    QCOMPARE(color.name(), QColor(favoriteTag1.color).name());
}

void FavoriteTagsModelTest::testTagNameByIndexNegative()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(model->tagNameByIndex(-1), QString());
}

void FavoriteTagsModelTest::testTagNameByIndexBeyondSize()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(model->tagNameByIndex(model->rowCount()), QString());
}

void FavoriteTagsModelTest::testTagNameByIndexLast()
{
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(model->tagNameByIndex(model->rowCount() - 1), favoriteTag3.name);
}

void FavoriteTagsModelTest::testLoadFromSettingsEmitsModelReset()
{
    QTemporaryFile tempFile;
    tempFile.open();
    QSignalSpy spyModelReset(model, SIGNAL(modelReset()));
    QSettings s(tempFile.fileName(), QSettings::NativeFormat);
    model->loadFromSettings(s);
    QCOMPARE(spyModelReset.count(), 1);
}

void FavoriteTagsModelTest::testLoadFromSettingsClearsModel()
{
    QTemporaryFile tempFile;
    tempFile.open();
    model->appendTag(favoriteTag1);
    QSettings s(tempFile.fileName(), QSettings::NativeFormat);
    model->loadFromSettings(s);
    QCOMPARE(model->rowCount(), 0);
}

void FavoriteTagsModelTest::testSaveLoadSettings()
{
    QTemporaryFile tempFile;
    model->appendTag(favoriteTag1);
    model->appendTag(favoriteTag2);
    model->appendTag(favoriteTag3);

    tempFile.open();
    QSettings s1(tempFile.fileName(), QSettings::NativeFormat);
    model->saveToSettings(s1);
    delete model;

    QSettings s2(tempFile.fileName(), QSettings::NativeFormat);
    model = new FavoriteTagsModel();
    model->loadFromSettings(s2);
    QCOMPARE(model->rowCount(), 3);
    COMPARE_ROWNUM(favoriteTag1, 0);
    COMPARE_ROWNUM(favoriteTag2, 1);
    COMPARE_ROWNUM(favoriteTag3, 2);

}


QTEST_GUILESS_MAIN( FavoriteTagsModelTest )
