/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#include "PartWidgetFactory.h"
#include "AttachmentView.h"
#include "MessageView.h" // so that the compiler knows that it's a QObject
#include "LoadablePartWidget.h"
#include "PartWidget.h"
#include "SimplePartWidget.h"
#include "Common/SettingsNames.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Gui
{

PartWidgetFactory::PartWidgetFactory(Imap::Network::MsgPartNetAccessManager *manager, MessageView *messageView):
    manager(manager), m_messageView(messageView)
{
}

QWidget *PartWidgetFactory::create(const QModelIndex &partIndex)
{
    return create(partIndex, 0);
}

QWidget *PartWidgetFactory::create(const QModelIndex &partIndex, int recursionDepth, const PartLoadingMode loadingMode)
{
    using namespace Imap::Mailbox;
    Q_ASSERT(partIndex.isValid());

    if (recursionDepth > 1000) {
        return new QLabel(tr("This message contains too deep nesting of MIME message parts.\n"
                             "To prevent stack exhaustion and your head from exploding, only\n"
                             "the top-most thousand items or so are shown."), 0);
    }

    bool userPrefersPlaintext = QSettings().value(Common::SettingsNames::guiPreferPlaintextRendering, QVariant(true)).toBool();

    QString mimeType = partIndex.data(Imap::Mailbox::RolePartMimeType).toString();
    if (mimeType.startsWith(QLatin1String("multipart/"))) {
        // it's a compound part
        if (mimeType == QLatin1String("multipart/alternative")) {
            return new MultipartAlternativeWidget(0, this, partIndex, recursionDepth,
                                                  userPrefersPlaintext ?
                                                      QLatin1String("text/plain") :
                                                      QLatin1String("text/html"));
        } else if (mimeType == QLatin1String("multipart/signed")) {
            return new MultipartSignedWidget(0, this, partIndex, recursionDepth);
        } else if (mimeType == QLatin1String("multipart/related")) {
            // The purpose of this section is to find a text/html e-mail, along with its associated body parts, and hide
            // everything else than the HTML widget.

            // At this point, it might be interesting to somehow respect the user's preference about using text/plain
            // instead of text/html. However, things are a bit complicated; the widget used at this point really wants
            // to either show just a single part or alternatively all of them in a sequence.
            // Furthermore, if someone sends a text/plain and a text/html together inside a multipart/related, they're
            // just wrong.

            // Let's see if we know what the root part is
            QModelIndex mainPartIndex;
            QVariant mainPartCID = partIndex.data(RolePartMultipartRelatedMainCid);
            if (mainPartCID.isValid()) {
                const Imap::Mailbox::Model *constModel = 0;
                Imap::Mailbox::TreeItemPart *part = dynamic_cast<Imap::Mailbox::TreeItemPart *>(Imap::Mailbox::Model::realTreeItem(partIndex, &constModel));
                Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model *>(constModel);
                Imap::Mailbox::TreeItemPart *mainPartPtr = Imap::Network::MsgPartNetAccessManager::cidToPart(mainPartCID.toByteArray(), model, part);
                if (mainPartPtr) {
                    mainPartIndex = mainPartPtr->toIndex(model);
                }
            }

            if (!mainPartIndex.isValid()) {
                // The Content-Type-based start parameter was not terribly useful. Let's find the HTML part manually.
                QModelIndex candidate = partIndex.child(0, 0);
                while (candidate.isValid()) {
                    if (candidate.data(RolePartMimeType).toString() == QLatin1String("text/html")) {
                        mainPartIndex = candidate;
                        break;
                    }
                    candidate = candidate.sibling(candidate.row() + 1, 0);
                }
            }

            if (mainPartIndex.isValid()) {
                if (mainPartIndex.data(RolePartMimeType).toString() == QLatin1String("text/html")) {
                    return PartWidgetFactory::create(mainPartIndex, recursionDepth+1);
                } else {
                    // Sorry, but anything else than text/html is by definition suspicious here. Better than picking some random
                    // choice, let's just show everything.
                    return new GenericMultipartWidget(0, this, partIndex, recursionDepth);
                }
            } else {
                // The RFC2387's wording is clear that in absence of an explicit START argument, the first part is the starting one.
                // On the other hand, I've seen real-world messages whose first part is some utter garbage (an image sent as
                // application/octet-stream, for example) and some *other* part is an HTML text. In that case (and if we somehow
                // failed to pick the HTML part by a heuristic), it's better to show everything.
                return new GenericMultipartWidget(0, this, partIndex, recursionDepth);
            }
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

        if (showInline) {
            const Imap::Mailbox::Model *constModel = 0;
            Imap::Mailbox::TreeItemPart *part = dynamic_cast<Imap::Mailbox::TreeItemPart *>(Imap::Mailbox::Model::realTreeItem(partIndex, &constModel));
            Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model *>(constModel);
            Q_ASSERT(model);
            Q_ASSERT(part);
            part->fetchFromCache(model);

            bool showDirectly = loadingMode == LOAD_IMMEDIATELY;
            if (!part->fetched())
                showDirectly &= model->isNetworkOnline() || part->octets() <= ExpensiveFetchThreshold;

            QWidget *widget = 0;
            if (showDirectly) {
                widget = new SimplePartWidget(0, manager, partIndex, m_messageView);
            } else if (model->isNetworkAvailable() || part->fetched()) {
                widget = new LoadablePartWidget(0, manager, partIndex, m_messageView,
                                                loadingMode == LOAD_ON_SHOW && part->octets() <= ExpensiveFetchThreshold ?
                                                    LoadablePartWidget::LOAD_ON_SHOW :
                                                    LoadablePartWidget::LOAD_ON_CLICK);
            } else {
                widget = new QLabel(tr("Offline"), 0);
            }
            return widget;
        } else {
            return new AttachmentView(0, manager, partIndex);
        }
    }
    QLabel *lbl = new QLabel(mimeType, 0);
    return lbl;
}

MessageView *PartWidgetFactory::messageView() const
{
    return m_messageView;
}

}
