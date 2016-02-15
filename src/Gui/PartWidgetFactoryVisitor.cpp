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

#include "PartWidgetFactoryVisitor.h"
#include <QLabel>
#include "AttachmentView.h"
#include "PartWidget.h"
#include "LoadablePartWidget.h"
#include "SimplePartWidget.h"
#include "MessageView.h"

namespace UiUtils {

template<typename Result, typename Context>
PartVisitor<Result, Context>::~PartVisitor()
{
}

template class PartVisitor<QWidget*, Gui::MessageView*>;

}

namespace Gui {

PartWidgetFactoryVisitor::~PartWidgetFactoryVisitor()
{
}

QWidget *PartWidgetFactoryVisitor::visitError(QString text, QWidget *parent)
{
    return new QLabel(text, parent);
}

QWidget *PartWidgetFactoryVisitor::visitLoadablePart(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager,
    const QModelIndex &part, PartWidgetFactory *factory, int recursionDepth, const UiUtils::PartLoadingOptions loadingMode)
{
    return new LoadablePartWidget(parent, manager, part, factory, recursionDepth, loadingMode);
}

QWidget *PartWidgetFactoryVisitor::visitAttachmentPart(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager,
    const QModelIndex &m_partIndex, MessageView *messageView, QWidget *contentView)
{
    return new AttachmentView(parent, manager, m_partIndex,
                              static_cast<MessageView *>(messageView),
                              contentView);
}

QWidget *PartWidgetFactoryVisitor::visitMultipartAlternative(QWidget *parent, PartWidgetFactory *factory,
    const QModelIndex &partIndex, const int recursionDepth, const UiUtils::PartLoadingOptions options)
{
    return new MultipartAlternativeWidget(parent, factory, partIndex, recursionDepth, options);
}

QWidget *PartWidgetFactoryVisitor::visitMultipartEncryptedView(QWidget *parent, PartWidgetFactory *factory,
    const QModelIndex &partIndex, const int recursionDepth, const UiUtils::PartLoadingOptions loadingOptions)
{
    return new MultipartSignedEncryptedWidget(parent, factory, partIndex, recursionDepth, loadingOptions);
}

QWidget *PartWidgetFactoryVisitor::visitMultipartSignedView(QWidget *parent, PartWidgetFactory *factory,
    const QModelIndex &partIndex, const int recursionDepth, const UiUtils::PartLoadingOptions loadingOptions)
{
    return new MultipartSignedEncryptedWidget(parent, factory, partIndex, recursionDepth, loadingOptions);
}

QWidget *PartWidgetFactoryVisitor::visitGenericMultipartView(QWidget *parent, PartWidgetFactory *factory,
    const QModelIndex &partIndex, int recursionDepth, const UiUtils::PartLoadingOptions options)
{
    return new GenericMultipartWidget(parent, factory, partIndex, recursionDepth, options);
}

QWidget *PartWidgetFactoryVisitor::visitMessage822View(QWidget *parent, PartWidgetFactory *factory,
    const QModelIndex &partIndex, int recursionDepth, const UiUtils::PartLoadingOptions options)
{
    return new Message822Widget(parent, factory, partIndex, recursionDepth, options);
}

QWidget *PartWidgetFactoryVisitor::visitSimplePartView(QWidget *parent,
    Imap::Network::MsgPartNetAccessManager *manager,
    const QModelIndex &partIndex, MessageView *messageView)
{
    return new SimplePartWidget(parent, manager, partIndex, messageView);
}

void PartWidgetFactoryVisitor::applySetHidden(QWidget *view)
{
    view->hide();
}

}
