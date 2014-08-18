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

#ifndef GUI_PARTWIDGETFACTORYVISITOR_H
#define GUI_PARTWIDGETFACTORYVISITOR_H

#include "Gui/PartWalker.h"
#include "UiUtils/PartVisitor.h"

namespace Imap {
namespace Network {
class MsgPartNetAccessManager;
}
}

namespace Gui {

class MessageView;

/** @short Build widget displaying message parts

This class defines how different message parts are displayed, returning QWidget.
It is used by PartWalker which traverses the mail tree and builds up a messageView.
 */
class PartWidgetFactoryVisitor: public UiUtils::PartVisitor<QWidget *, MessageView *>
{
public:
    virtual ~PartWidgetFactoryVisitor();
    virtual QWidget *visitError(QString text, QWidget *parent);
    virtual QWidget *visitLoadablePart(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &part,
                                            PartWidgetFactory *factory, int recursionDepth,
                                            const UiUtils::PartLoadingOptions loadingMode);
    virtual QWidget *visitAttachmentPart(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &m_partIndex,
                                      MessageView *messageView, QWidget *contentView);
    virtual QWidget *visitMultipartAlternative(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex,
                                                  const int recursionDepth, const UiUtils::PartLoadingOptions options);
    virtual QWidget *visitMultipartEncryptedView(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                                              const UiUtils::PartLoadingOptions loadingOptions);
    virtual QWidget *visitMultipartSignedView(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                                              const UiUtils::PartLoadingOptions loadingOptions);
    virtual QWidget *visitGenericMultipartView(QWidget *parent,
                                              PartWidgetFactory *factory, const QModelIndex &partIndex,
                                              int recursionDepth, const UiUtils::PartLoadingOptions options);
    virtual QWidget *visitMessage822View(QWidget *parent,
                                 PartWidgetFactory *factory, const QModelIndex &partIndex,
                                 int recursionDepth, const UiUtils::PartLoadingOptions options);
    virtual QWidget *visitSimplePartView(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex,
                                 MessageView *messageView);
    virtual void applySetHidden(QWidget *view);
};

}

#endif // GUI_PARTWIDGETFACTORYVISITOR_H
