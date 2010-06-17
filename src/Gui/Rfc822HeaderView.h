/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#ifndef GUI_RFC822HEADERVIEW_H
#define GUI_RFC822HEADERVIEW_H

#include <QLabel>

class QModelIndex;

namespace Imap {
namespace Mailbox {
class Model;
class TreeItemPart;
}
}

namespace Gui {

class Rfc822HeaderView : public QLabel
{
    Q_OBJECT
public:
    Rfc822HeaderView( QWidget* parent, Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemPart* _part );
private slots:
    void handleDataChanged(const QModelIndex&, const QModelIndex& );
    void setCorrectText();
private:
    Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemPart* part;

    Rfc822HeaderView(const Rfc822HeaderView&); // don't implement
    Rfc822HeaderView& operator=(const Rfc822HeaderView&); // don't implement
};

}

#endif // GUI_RFC822HEADERVIEW_H
