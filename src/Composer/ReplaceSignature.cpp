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

#include "ReplaceSignature.h"
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace Composer {
namespace Util {

void replaceSignature(QTextDocument *document, const QString &newSignature)
{
    // The QTextEdit is set up in such a way as to treat a fully terminated line as a standalone text block,
    // hence no newlines in the signature separator
    const QLatin1String signatureSeperator("-- ");

    QTextBlock block = document->lastBlock();
    while (block.isValid() && block.blockNumber() > 0) {
        if (block.text() == signatureSeperator) {
            // So this block holds the last signature separator -- great!
            break;
        }
        block = block.previous();
    }

    QTextCursor cursor(block);
    if (block.text() == signatureSeperator) {
        // Remove everything till the end of the document since the end of the previous block
        if (block.previous().isValid()) {
            // Prevent adding newlines when switching signatures
            block = block.previous();
            cursor = QTextCursor(block);
            cursor.movePosition(QTextCursor::EndOfBlock);
        }
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
    } else {
        // We have not removed anything, so we have to "fake" an edit action so that we're adding the signature to a correct place
        block = document->lastBlock();
        cursor = QTextCursor(block);
        cursor.movePosition(QTextCursor::EndOfBlock);
        cursor.beginEditBlock();
        cursor.endEditBlock();
    }

    if (!newSignature.isEmpty()) {
        cursor.joinPreviousEditBlock();
        cursor.insertBlock();
        cursor.insertText(signatureSeperator);
        cursor.insertBlock();
        cursor.insertText(newSignature);
        cursor.endEditBlock();
    }
}

}
}


