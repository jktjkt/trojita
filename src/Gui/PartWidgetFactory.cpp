/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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
#include "PartWidgetFactory.h"
#include "AttachmentView.h"
#include "LoadablePartWidget.h"
#include "PartWidget.h"
#include "SimplePartWidget.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Gui {

PartWidgetFactory::PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager, QObject* _wheelEventFilter ):
        manager(_manager), wheelEventFilter(_wheelEventFilter)
{
}

QWidget* PartWidgetFactory::create(const QModelIndex &partIndex)
{
    return create(partIndex, 0);
}

QWidget* PartWidgetFactory::create(const QModelIndex &partIndex, int recursionDepth)
{
    using namespace Imap::Mailbox;
    Q_ASSERT(partIndex.isValid());

    if ( recursionDepth > 1000 ) {
        return new QLabel( tr("This message contains too deep nesting of MIME message parts.\n"
                              "To prevent stack exhaustion and your head from exploding, only\n"
                              "the top-most thousand items or so are shown."), 0 );
    }

    QString mimeType = partIndex.data(Imap::Mailbox::RolePartMimeType).toString();
    if (mimeType.startsWith( QLatin1String("multipart/"))) {
        // it's a compound part
        if (mimeType == QLatin1String("multipart/alternative")) {
            return new MultipartAlternativeWidget(0, this, partIndex, recursionDepth);
        } else if (mimeType == QLatin1String("multipart/signed")) {
            return new MultipartSignedWidget(0, this, partIndex, recursionDepth);
        } else {
            return new GenericMultipartWidget(0, this, partIndex, recursionDepth);
        }
    } else if (mimeType == QLatin1String("message/rfc822")) {
        return new Message822Widget(0, this, partIndex, recursionDepth);
    } else {
        QStringList allowedMimeTypes;
        allowedMimeTypes << "text/html" << "text/plain" << "image/jpeg" <<
                "image/jpg" << "image/pjpeg" << "image/png" << "image/gif";
        // The problem is that some nasty MUAs (hint hint Thunderbird) would
        // happily attach a .tar.gz and call it "inline"
        bool showInline = partIndex.data(Imap::Mailbox::RolePartBodyDisposition).toByteArray().toLower() != "attachment" &&
                          allowedMimeTypes.contains(mimeType);

        if ( showInline ) {
            const Imap::Mailbox::Model *constModel = 0;
            Imap::Mailbox::TreeItemPart *part = dynamic_cast<Imap::Mailbox::TreeItemPart*>(Imap::Mailbox::Model::realTreeItem(partIndex, &constModel));
            Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model*>(constModel);
            Q_ASSERT(model);
            Q_ASSERT(part);
            part->fetchFromCache(model);
            bool showDirectly = true;
            if (!part->fetched())
                showDirectly = model->isNetworkOnline() || part->octets() <= ExpensiveFetchThreshold;

            QWidget* widget = 0;
            if ( showDirectly ) {
                widget = new SimplePartWidget(0, manager, partIndex);
            } else if ( model->isNetworkAvailable() ) {
                widget = new LoadablePartWidget(0, manager, partIndex, wheelEventFilter);
            } else {
                widget = new QLabel(tr("Offline"), 0);
            }
            widget->installEventFilter(wheelEventFilter);
            return widget;
        } else {
            return new AttachmentView(0, manager, partIndex);
        }
    }
    QLabel* lbl = new QLabel(mimeType, 0);
    return lbl;
}

}
