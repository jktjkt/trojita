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
#ifndef UIUTILS_PARTWALKER_H
#define UIUTILS_PARTWALKER_H

#include <memory>
#include <QCoreApplication>
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Imap/Model/NetworkWatcher.h"
#include "UiUtils/PartLoadingOptions.h"
#include "UiUtils/PartVisitor.h"

namespace UiUtils {

template <typename Result, typename Context>
class PartVisitor;

/** @short walk through a message and display it.

This class traverses a message, puts together all its parts and displays the message.
It owns a PartVistor that defines how each part in a message is displayed.
It traverses message with function walk() and outputs a UI Object;
 */
template<typename Result, typename Context>
class PartWalker
{
    Q_DECLARE_TR_FUNCTIONS(PartWalker)
    enum { ExpensiveFetchThreshold = 50*1024 };
public:

    PartWalker(Imap::Network::MsgPartNetAccessManager *manager,
               Context context, std::unique_ptr<PartVisitor<Result, Context> > visitor);
    Result walk(const QModelIndex &partIndex, int recursionDepth, const UiUtils::PartLoadingOptions loadingMode);
    Context context() const;
    void setNetworkWatcher(Imap::Mailbox::NetworkWatcher *netWatcher);

private:
    Imap::Network::MsgPartNetAccessManager *m_manager;
    Imap::Mailbox::NetworkWatcher *m_netWatcher;
    Context m_context;
    std::unique_ptr<PartVisitor<Result, Context>> m_visitor;

};

}

#endif //UIUTILS_PARTWALKER_H
