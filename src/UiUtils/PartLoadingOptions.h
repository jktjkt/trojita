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

#ifndef UIUTILS_PARTLOADINGOPTIONS_H
#define UIUTILS_PARTLOADINGOPTIONS_H

#include <QFlags>

namespace UiUtils {

/** @short Are the parts supposed to be visible immediately, or only after their respective View is shown? */
typedef enum {
    PART_IGNORE_DISPOSITION_ATTACHMENT = 1 << 0, /**< @short Don't wrap the part in an AttachmentView */
    PART_IGNORE_CLICKTHROUGH = 1 << 1, /**< @short Ignore any heuristics which lead to wrapping in an LoadablePartView with a clickthrough */
    PART_IGNORE_LOAD_ON_SHOW = 1 << 2, /**< @short Ignore wrapping in a LoadablePartView set up to load on first show event */
    PART_IS_HIDDEN = 1 << 3, /**< @short Request wrapping this part in a LoadablePartView */
    PART_PREFER_PLAINTEXT_OVER_HTML = 1 << 4, /**< @short The user's preferences indicate that a text/plain part shall be shown instead of a text/html if available */
    PART_IGNORE_INLINE = 1 << 5, /**< @short Do not auto-expand this attachment */
} PartLoadingFlag;
/** @short Which of these options shall be propagated to child Views when embedding them, and which shall be filtered? */
enum {
    MASK_PROPAGATE_WHEN_EMBEDDING =
        PART_IS_HIDDEN | PART_PREFER_PLAINTEXT_OVER_HTML
};
typedef QFlags<PartLoadingFlag> PartLoadingOptions;

}

#endif
