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


#include "UnSelectTask.h"
#include <QUuid>
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

UnSelectTask::UnSelectTask(Model *model, ImapTask *parentTask) :
    ImapTask(model)
{
    conn = parentTask;
    parser = conn->parser;
    Q_ASSERT(parser);
}

void UnSelectTask::perform()
{
    markAsActiveTask(TASK_PREPEND);

    if (_dead) {
        _failed("Asked to die");
        return;
    }
    // We really should ignore abort() -- we're a very important task

    if (model->accessParser(parser).maintainingTask) {
        model->accessParser(parser).maintainingTask->breakOrCancelPossibleIdle();
    }
    if (model->accessParser(parser).capabilities.contains("UNSELECT")) {
        unSelectTag = parser->unSelect();
    } else {
        doFakeSelect();
    }
}

void UnSelectTask::doFakeSelect()
{
    if (_dead) {
        _failed("Asked to die");
        return;
    }
    // Again, ignoring abort()

    if (model->accessParser(parser).maintainingTask) {
        model->accessParser(parser).maintainingTask->breakOrCancelPossibleIdle();
    }
    // The server does not support UNSELECT. Let's construct an unlikely-to-exist mailbox, then.
    selectMissingTag = parser->examine(QString("trojita non existing %1").arg(QUuid::createUuid().toString()));
}

bool UnSelectTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty()) {
        switch (resp->respCode) {
        case Responses::UNSEEN:
        case Responses::PERMANENTFLAGS:
        case Responses::UIDNEXT:
        case Responses::UIDVALIDITY:
        case Responses::NOMODSEQ:
        case Responses::HIGHESTMODSEQ:
        case Responses::CLOSED:
            return true;
        default:
            break;
        }
    }
    if (!resp->tag.isEmpty()) {
        if (resp->tag == unSelectTag) {
            if (resp->kind == Responses::OK) {
                // nothing should be needed here
                _completed();
            } else {
                // This is really bad.
                throw MailboxException("Attempted to unselect current mailbox, but the server denied our request. "
                                       "Can't continue, to avoid possible data corruption.", *resp);
            }
            return true;
        } else if (resp->tag == selectMissingTag) {
            if (resp->kind == Responses::OK) {
                QTimer::singleShot(0, this, SLOT(doFakeSelect()));
                log(QLatin1String("The emergency EXAMINE command has unexpectedly succeeded, trying to get out of here..."), Common::LOG_MAILBOX_SYNC);
            } else {
                // This is very good :)
                _completed();
            }
            return true;
        }
    }
    return false;
}

bool UnSelectTask::handleNumberResponse(const Imap::Responses::NumberResponse *const resp)
{
    Q_UNUSED(resp);
    log("UnSelectTask: ignoring numeric response", Common::LOG_MAILBOX_SYNC);
    return true;
}

bool UnSelectTask::handleFlags(const Imap::Responses::Flags *const resp)
{
    Q_UNUSED(resp);
    log("UnSelectTask: ignoring FLAGS response", Common::LOG_MAILBOX_SYNC);
    return true;
}

bool UnSelectTask::handleSearch(const Imap::Responses::Search *const resp)
{
    Q_UNUSED(resp);
    log("UnSelectTask: ignoring SEARCH response", Common::LOG_MAILBOX_SYNC);
    return true;
}

bool UnSelectTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    Q_UNUSED(resp);
    log("UnSelectTask: ignoring FETCH response", Common::LOG_MAILBOX_SYNC);
    return true;
}

/** @short Internal task */
QVariant UnSelectTask::taskData(const int role) const
{
    Q_UNUSED(role);
    return QVariant();
}

}
}
