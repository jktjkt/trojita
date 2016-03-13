/* Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>

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
#ifndef TEST_CRYPTOGRAPHY_MESSAGEMODEL
#define TEST_CRYPTOGRAPHY_MESSAGEMODEL

#include "Utils/LibMailboxSync.h"
#include <QtCore/QObject>

/** @short Unit tests for Imap::Network::MsgPartNetAccessManager and friends */
class CryptographyMessageModelTest : public LibMailboxSync
{
    Q_OBJECT
public:
    typedef QList<QPair<int,int> > pathList;
private Q_SLOTS:
    /* first test simple passthrough of IMAP messages */
    void testImapMessageParts();
    void testImapMessageParts_data();

    /* test custom messages */
    void testCustomMessageParts();

    /* test mixed messages with custom messages added to an existing IMAP message */
    void testMixedMessageParts();

    void testLocalMimeParsing();

    void testDelayedLoading();
};

Q_DECLARE_METATYPE(CryptographyMessageModelTest::pathList)

#endif /* TEST_CRYPTOGRAPHY_MESSAGEMODEL */
