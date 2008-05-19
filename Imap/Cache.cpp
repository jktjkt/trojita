/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "Imap/Cache.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

NoCache::NoCache():
    _uidNext(0), _uidValidity(0), _exists(0)
{}

void NoCache::setNewNumbers( const uint uidValidity, const uint uidNext, const uint exists )
{
    qDebug() << "setUidNext: " << uidNext;
    _uidNext = uidNext;
    qDebug() << "setUidValidity: " << uidValidity;
    _uidValidity = uidValidity;
    qDebug() << "setExists: " << exists;
    _exists = exists;
}

void NoCache::forget()
{
    qDebug() << "NoCache::forget()";
    forgetSeqUid();
}

void NoCache::forgetSeqUid()
{
    qDebug() << "NoCache::forgetSeqUid()";
}

uint NoCache::getUidNext()
{
    return _uidNext;
}

uint NoCache::getExists()
{
    return _exists;
}

uint NoCache::getUidValidity()
{
    return _uidValidity;
}

bool NoCache::seqToUid( const uint seq, uint& uid )
{
    qDebug() << "seqToUid( " << seq << ") = ?";
    return false;
}

bool NoCache::uidToSeq( const uint uid, uint& seq )
{
    qDebug() << "uidToSeq( " << uid << ") = ?";
    return false;
}

void NoCache::addSeqUid( const uint seq, const uint uid )
{
    qDebug() << "addSeqUid: seq " << seq << " uid " << uid;
}


}
}
