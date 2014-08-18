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
#ifndef UIUTILS_PARTVISITOR_H
#define UIUTILS_PARTVISITOR_H

#include "PartWalker.h"
#include "PartLoadingOptions.h"
#include <QModelIndex>

namespace Imap {
namespace Network {
class MsgPartNetAccessManager;
}
}

namespace UiUtils {

template <typename Result, typename Context>
class PartWalker;

/** @short define how a message part is displayed

This class is used in PartWalker to display a message part.
Inherit this class and implement functions to display a message part for a specific UI environment.
 */
template <typename Result, typename Context>
class PartVisitor{

public:
    virtual ~PartVisitor();
    virtual Result visitError(QString text, Result parent) = 0;
    virtual Result visitLoadablePart(Result parent,
                                        Imap::Network::MsgPartNetAccessManager *manager,
                                        const QModelIndex &part,
                                        PartWalker<Result, Context> *factory,
                                        int recursionDepth,
                                        const PartLoadingOptions loadingMode) = 0;
    virtual Result visitAttachmentPart(Result parent,
                                        Imap::Network::MsgPartNetAccessManager *manager,
                                        const QModelIndex &m_partIndex,
                                        Context context, Result contentView) = 0;
    virtual Result visitMultipartAlternative(Result parent,
                                        PartWalker<Result, Context> *factory,
                                        const QModelIndex &partIndex,
                                        const int recursionDepth,
                                        const PartLoadingOptions options) = 0;
    virtual Result visitMultipartEncryptedView(Result parent, PartWalker<Result, Context> *factory,
                                        const QModelIndex &partIndex, const int recursionDepth,
                                        const PartLoadingOptions loadingOptions) = 0;
    virtual Result visitMultipartSignedView(Result parent, PartWalker<Result, Context> *factory,
                                        const QModelIndex &partIndex, const int recursionDepth,
                                        const PartLoadingOptions loadingOptions) = 0;
    virtual Result visitGenericMultipartView(Result parent,
                                        PartWalker<Result, Context> *factory,
                                        const QModelIndex &partIndex,
                                        int recursionDepth, const PartLoadingOptions options) = 0;
    virtual Result visitMessage822View(Result parent,
                                        PartWalker<Result, Context> *factory, const QModelIndex &partIndex,
                                        int recursionDepth, const PartLoadingOptions options) = 0;
    virtual Result visitSimplePartView(Result parent, Imap::Network::MsgPartNetAccessManager *manager,
                                        const QModelIndex &partIndex,
                                        Context messageView) = 0;
    virtual void applySetHidden(Result view) = 0;
};

}

#endif // UIUTILS_PARTVISITOR_H
