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

#include <QTimer>
#include "IdleLauncher.h"
#include "Imap/Model/Model.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

IdleLauncher::IdleLauncher(KeepMailboxOpenTask *parent):
    QObject(parent), task(parent), m_idling(false), m_idleCommandRunning(false)
{
    delayedEnter = new QTimer(this);
    delayedEnter->setObjectName(QString::fromUtf8("%1-IdleLauncher-delayedEnter").arg(task->objectName()));
    delayedEnter->setSingleShot(true);
    // It's a question about what timeout to set here -- if it's too long, we enter IDLE too soon, before the
    // user has a chance to click on a message, but if we set it too long, we needlessly wait too long between
    // we receive updates, and also between terminating one IDLE and starting another.
    // 6 seconds is a compromise here.
    bool ok;
    int timeout = parent->model->property("trojita-imap-idle-delayedEnter").toUInt(&ok);
    if (! ok)
        timeout = 6 * 1000;
    delayedEnter->setInterval(timeout);
    connect(delayedEnter, SIGNAL(timeout()), this, SLOT(slotEnterIdleNow()));
    renewal = new QTimer(this);
    renewal->setObjectName(QString::fromUtf8("%1-IdleLauncher-renewal").arg(task->objectName()));
    renewal->setSingleShot(true);
    timeout = parent->model->property("trojita-imap-idle-renewal").toUInt(&ok);
    if (! ok)
        timeout = 1000 * 29 * 60; // 29 minutes -- that's the longest allowed time to IDLE
    renewal->setInterval(timeout);
    connect(renewal, SIGNAL(timeout()), this, SLOT(slotTerminateLongIdle()));
}

void IdleLauncher::slotEnterIdleNow()
{
    delayedEnter->stop();
    renewal->stop();

    if (m_idleCommandRunning) {
        enterIdleLater();
        return;
    }

    Q_ASSERT(task->parser);
    Q_ASSERT(! m_idling);
    Q_ASSERT(! m_idleCommandRunning);
    Q_ASSERT(task->tagIdle.isEmpty());
    task->tagIdle = task->parser->idle();
    renewal->start();
    m_idling = true;
    m_idleCommandRunning = true;
}

void IdleLauncher::finishIdle()
{
    Q_ASSERT(task->parser);
    if (m_idling) {
        renewal->stop();
        task->parser->idleDone();
        m_idling = false;
    } else if (delayedEnter->isActive()) {
        delayedEnter->stop();
    }
}

void IdleLauncher::slotTerminateLongIdle()
{
    if (m_idling)
        finishIdle();
}

void IdleLauncher::enterIdleLater()
{
    if (m_idling)
        return;

    delayedEnter->start();
}

void IdleLauncher::die()
{
    delayedEnter->stop();
    delayedEnter->disconnect();
    renewal->stop();
    renewal->disconnect();
}

bool IdleLauncher::idling() const
{
    return m_idling;
}

bool IdleLauncher::waitingForIdleTaggedTermination() const
{
    return m_idleCommandRunning;
}

void IdleLauncher::idleCommandCompleted()
{
    // FIXME: these asseerts could be triggered by a rogue server...
    if (m_idling) {
        task->log("Warning: IDLE completed before we could ask for its termination...", Common::LOG_MAILBOX_SYNC);
        m_idling = false;
        renewal->stop();
        task->parser->idleMagicallyTerminatedByServer();
    }
    Q_ASSERT(m_idleCommandRunning);
    m_idleCommandRunning = false;
}

void IdleLauncher::idleCommandFailed()
{
    // FIXME: these asseerts could be triggered by a rogue server...
    Q_ASSERT(m_idling);
    Q_ASSERT(m_idleCommandRunning);
    renewal->stop();
    m_idleCommandRunning = false;
    task->parser->idleContinuationWontCome();
    die();
}

}
}
