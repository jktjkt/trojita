/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

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

#include <QList>
#include <QListView>
#include <QUrl>

namespace Composer {
class MessageComposer;
}

/** @short
    This class is used inside the ComposeWidget. @see GUI::ComposeWidget. It is positioned below
    the "Attach" button at the right of the ComposeWidget. When an item is dropped inside it an
    attachement is added to the @see Imap::Mailbox::MessageComposer model.
  */

class ComposerAttachmentsList : public QListView {
    Q_OBJECT
public:
    explicit ComposerAttachmentsList(QWidget *parent);
    void setComposer(Composer::MessageComposer *composer);
signals:
    void itemDroppedOut();
protected:
    void startDrag(Qt::DropActions da);
    void dragEnterEvent(QDragEnterEvent *de);
    void dragLeaveEvent(QDragLeaveEvent *de);
public slots:
    void slotRemoveAttachment();
    void slotToggledContentDispositionInline(bool checked);
    void slotRenameAttachment();
    void onAttachmentNumberChanged();
    void onCurrentChanged();
    void showContextMenu(const QPoint &pos);
private:
    bool m_dragging, m_dragInside;
    Composer::MessageComposer *m_composer;
    QAction *m_actionRemoveAttachment;
    QAction *m_actionSendInline;
    QAction *m_actionRename;
};
