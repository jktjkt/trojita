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

#ifndef TEST_SENDERIDENTITIESMODEL
#define TEST_SENDERIDENTITIESMODEL

#include <QTest>
#include "Composer/SenderIdentitiesModel.h"

using namespace Composer;

class SenderIdentitiesModelTest : public QObject
{
    Q_OBJECT

 public:
    SenderIdentitiesModelTest();

private slots:
    void init();
    void initTestCase();
    void testPropertyStorage();
    void testAddIdentity();
    void testRemoveIdentity1();
    void testRemoveIdentity2();
    void testMoveFirstIdentity();
    void testMoveLastIdentity();
    void testMoveMiddleIdentity();
    void cleanup();
private:
    SenderIdentitiesModel* model;
    ItemSenderIdentity identity1;
    ItemSenderIdentity identity2;
    ItemSenderIdentity identity3;
};



#endif
