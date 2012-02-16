/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@gentoo.org>

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

#include "TaskProgressIndicator.h"
#include <QApplication>
#include <QMouseEvent>
#include "Imap/Model/Model.h"
#include "Imap/Model/VisibleTasksModel.h"

namespace Gui {

TaskProgressIndicator::TaskProgressIndicator(QWidget *parent) :
    QProgressBar(parent), m_busy(false)
{
    setMinimum(0);
    setMaximum(0);
}

/** @short Connect to the specified IMAP model as the source of the activity information */
void TaskProgressIndicator::setImapModel(Imap::Mailbox::Model *model)
{
    if (model) {
        m_visibleTasksModel = new Imap::Mailbox::VisibleTasksModel(model, model->taskModel());
        connect(m_visibleTasksModel, SIGNAL(layoutChanged()), this, SLOT(updateActivityIndication()));
        connect(m_visibleTasksModel, SIGNAL(modelReset()), this, SLOT(updateActivityIndication()));
        connect(m_visibleTasksModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateActivityIndication()));
        connect(m_visibleTasksModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateActivityIndication()));
    }
}

/** @short Reflect a possible change in the activity in the GUI */
void TaskProgressIndicator::updateActivityIndication()
{
    if (!m_visibleTasksModel)
        return;

    bool busy = m_visibleTasksModel->hasChildren();
    setVisible(busy);
    if (!m_busy && busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else if (m_busy && !busy) {
        QApplication::restoreOverrideCursor();
    }

    if (busy) {
        setToolTip(tr("%1 ongoing actions").arg(QString::number(m_visibleTasksModel->rowCount())));
    } else {
        setToolTip(tr("IMAP connection idle"));
    }

    m_busy = busy;
}

/** @short Reimplemented from QProgressBar for launching the pop-up widgets with detailed activity */
void TaskProgressIndicator::mousePressEvent(QMouseEvent * event)
{
    if (!m_visibleTasksModel)
        return;

    if (event->buttons() == Qt::LeftButton) {
        event->accept();
    } else {
        QProgressBar::mousePressEvent(event);
    }
}

}
