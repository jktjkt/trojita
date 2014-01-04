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
#include "LoadablePartWidget.h"
#include "Gui/MessageView.h" // so that the compiler knows that it's a QObject
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"

#include <QPushButton>

namespace Gui
{

LoadablePartWidget::LoadablePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &part,
                                       MessageView *messageView, PartWidgetFactory *factory, int recursionDepth,
                                       const PartWidgetFactory::PartLoadingOptions loadingMode):
    QStackedWidget(parent), manager(manager), partIndex(part), m_messageView(messageView), m_factory(factory),
    m_recursionDepth(recursionDepth), m_loadingMode(loadingMode), realPart(0), loadButton(0), m_loadOnShow(false)
{
    Q_ASSERT(partIndex.isValid());

    QString mimeType = partIndex.data(Imap::Mailbox::RolePartMimeType).toString().toLower();

    if ((loadingMode & PartWidgetFactory::PART_IS_HIDDEN) ||
            (loadingMode & PartWidgetFactory::PART_IGNORE_CLICKTHROUGH) ||
            partIndex.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        m_loadOnShow = true;
    } else {
        loadButton = new QPushButton(tr("Load %1 (%2)").arg(partIndex.data(Imap::Mailbox::RolePartMimeType).toString(),
                                     Imap::Mailbox::PrettySize::prettySize(partIndex.data(Imap::Mailbox::RolePartOctets).toUInt())),
                                     this);
        connect(loadButton, SIGNAL(clicked()), this, SLOT(loadClicked()));
        addWidget(loadButton);
    }
}

void LoadablePartWidget::loadClicked()
{
    if (!partIndex.isValid()) {
        if (loadButton) {
            loadButton->setEnabled(false);
        }
        return;
    }
    if (loadButton) {
        loadButton->deleteLater();
        loadButton = 0;
    }

    // We have to disable any flags which might cause recursion here
    realPart = m_factory->create(partIndex, m_recursionDepth + 1,
                                 (m_loadingMode | PartWidgetFactory::PART_IGNORE_CLICKTHROUGH
                                  | PartWidgetFactory::PART_IGNORE_LOAD_ON_SHOW) ^ PartWidgetFactory::PART_IS_HIDDEN);
    addWidget(realPart);
    setCurrentIndex(1);
}

QString LoadablePartWidget::quoteMe() const
{
    AbstractPartWidget *part = dynamic_cast<AbstractPartWidget*>(realPart);
    return part ? part->quoteMe() : QString();
}

void LoadablePartWidget::showEvent(QShowEvent *event)
{
    QStackedWidget::showEvent(event);
    if (m_loadOnShow) {
        m_loadOnShow = false;
        loadClicked();
    }
}

void LoadablePartWidget::reloadContents()
{
    if (AbstractPartWidget *w = dynamic_cast<AbstractPartWidget*>(realPart))
        w->reloadContents();
}

}
