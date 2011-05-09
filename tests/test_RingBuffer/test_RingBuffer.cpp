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

#include <QDebug>
#include <QTest>
#include "test_RingBuffer.h"
#include "../headless_test.h"
#include "Imap/Model/RingBuffer.h"

using namespace Imap;

void RingBufferTest::testOne()
{
    typedef RingBuffer<int>::const_iterator It;
    RingBuffer<int> rb(5);
    rb.append(3);
    for(It it = rb.begin(); it != rb.end(); ++it) {
    }
}

TROJITA_HEADLESS_TEST( RingBufferTest )
