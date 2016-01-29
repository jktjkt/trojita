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

#include <QSet>

#include "SpecialFlagNames.h"

namespace Imap
{
namespace Mailbox
{

// Make sure to update the first-character check inside Model::normalizeFlags() when adding new flags here
const QString FlagNames::answered = QStringLiteral("\\Answered");
const QString FlagNames::seen = QStringLiteral("\\Seen");
const QString FlagNames::deleted = QStringLiteral("\\Deleted");
const QString FlagNames::forwarded = QStringLiteral("$Forwarded");
const QString FlagNames::recent = QStringLiteral("\\Recent");
const QString FlagNames::flagged = QStringLiteral("\\Flagged");
const QString FlagNames::junk = QStringLiteral("$Junk");
const QString FlagNames::notjunk = QStringLiteral("$NotJunk");
const QString FlagNames::mdnsent = QStringLiteral("$MDNSent");
const QString FlagNames::submitted = QStringLiteral("$Submitted");
const QString FlagNames::submitpending = QStringLiteral("$SubmitPending");

QHash<QString,QString> toCanonicalInit()
{
  QSet<QString> flagNames = QSet<QString>();
  flagNames << FlagNames::answered
            << FlagNames::seen
            << FlagNames::deleted
            << FlagNames::forwarded
            << FlagNames::recent
            << FlagNames::flagged
            << FlagNames::junk
            << FlagNames::notjunk
            << FlagNames::mdnsent
            << FlagNames::submitted
            << FlagNames::submitpending;
  QHash<QString,QString> flagNamesCanonical = QHash<QString,QString>();
  Q_FOREACH(const QString &flagName, flagNames) {
    flagNamesCanonical[flagName.toLower()] = flagName;
  }
  return flagNamesCanonical;
}
const QHash<QString,QString> FlagNames::toCanonical = toCanonicalInit();

}
}
