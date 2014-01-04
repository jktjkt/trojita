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

#ifndef GUI_TASKPROGRESSINDICATOR_H
#define GUI_TASKPROGRESSINDICATOR_H

#include <QPointer>
#include <QProgressBar>

namespace Imap
{
namespace Mailbox
{
class Model;
class VisibleTasksModel;
}
}

namespace Gui
{

/** @short A GUI element for showing whether anything is "using" the IMAP connection */
class TaskProgressIndicator : public QProgressBar
{
    Q_OBJECT
public:
    explicit TaskProgressIndicator(QWidget *parent = 0);

    void setImapModel(Imap::Mailbox::Model *model);

public slots:
    void updateActivityIndication();

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    /** @short Model for a list of "visible tasks" */
    QPointer<Imap::Mailbox::VisibleTasksModel> m_visibleTasksModel;

    /** @short Is there any ongoing activity? */
    bool m_busy;
};

}

#endif // GUI_TASKPROGRESSINDICATOR_H
