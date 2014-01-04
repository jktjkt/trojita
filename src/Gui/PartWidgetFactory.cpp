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
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QMimeDatabase>
#else
#  include "mimetypes-qt4/include/QMimeDatabase"
#endif
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

QWidget *PartWidgetFactory::create(const QModelIndex &partIndex, int recursionDepth, const PartLoadingOptions loadingMode)
{
    using namespace Imap::Mailbox;
    Q_ASSERT(partIndex.isValid());

    if (recursionDepth > 1000) {
        return new QLabel(tr("This message contains too deep nesting of MIME message parts.\n"
                             "To prevent stack exhaustion and your head from exploding, only\n"
                             "the top-most thousand items or so are shown."), 0);
    }

    QString mimeType = partIndex.data(Imap::Mailbox::RolePartMimeType).toString().toLower();
    bool isMessageRfc822 = mimeType == QLatin1String("message/rfc822");
    bool isCompoundMimeType = mimeType.startsWith(QLatin1String("multipart/")) || isMessageRfc822;

    if (loadingMode & PART_IS_HIDDEN) {
        return new LoadablePartWidget(0, manager, partIndex, m_messageView, this, recursionDepth + 1,
                                      loadingMode | PART_IGNORE_CLICKTHROUGH);
    }

    // Check whether we can render this MIME type at all
    QStringList allowedMimeTypes;
    allowedMimeTypes << "text/html" << "text/plain" << "image/jpeg" <<
                     "image/jpg" << "image/pjpeg" << "image/png" << "image/gif";
    bool recognizedMimeType = isCompoundMimeType || allowedMimeTypes.contains(mimeType);
    bool isDerivedMimeType = false;
    if (!recognizedMimeType) {
        // QMimeType's docs say that one shall use inherit() to check for "is this a recognized MIME type".
        // E.g. text/x-csrc inherits text/plain.
        QMimeType partType = QMimeDatabase().mimeTypeForName(mimeType);
        Q_FOREACH(const QString &candidate, allowedMimeTypes) {
            if (partType.isValid() && !partType.isDefault() && partType.inherits(candidate)) {
                // Looks like we shall be able to show this
                recognizedMimeType = true;
                // If it's a derived MIME type, it makes sense to not block inline display, yet still make it possible to hide it
                // using the regular attachment controls
                isDerivedMimeType = true;
                manager->registerMimeTypeTranslation(mimeType, candidate);
                break;
            }
        }
    }

    const Imap::Mailbox::Model *constModel = 0;
    Imap::Mailbox::TreeItemPart *part = dynamic_cast<Imap::Mailbox::TreeItemPart *>(Imap::Mailbox::Model::realTreeItem(partIndex, &constModel));
    Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model *>(constModel);
    Q_ASSERT(model);
    Q_ASSERT(part);

    // Check whether it shall be wrapped inside an AttachmentView
    // From section 2.8 of RFC 2183: "Unrecognized disposition types should be treated as `attachment'."
    const QByteArray contentDisposition = partIndex.data(Imap::Mailbox::RolePartBodyDisposition).toByteArray().toLower();
    const bool isInline = contentDisposition.isEmpty() || contentDisposition == "inline";
    const bool looksLikeAttachment = !partIndex.data(Imap::Mailbox::RolePartFileName).toString().isEmpty();
    const bool wrapInAttachmentView = !(loadingMode & PART_IGNORE_DISPOSITION_ATTACHMENT)
            && (looksLikeAttachment || !isInline || !recognizedMimeType || isDerivedMimeType || isMessageRfc822);
    if (wrapInAttachmentView) {
        // The problem is that some nasty MUAs (hint hint Thunderbird) would
        // happily attach a .tar.gz and call it "inline"
        QWidget *contentWidget = 0;
        if (recognizedMimeType) {
            PartLoadingOptions options = loadingMode | PART_IGNORE_DISPOSITION_ATTACHMENT;
            if (!isInline) {
                // The widget will be hidden by default, i.e. the "inline preview" will be deactivated.
                // If the user clicks that action in the AttachmentView, it makes sense to load the plugin without any further ado,
                // without requiring an extra clickthrough
                options |= PART_IS_HIDDEN;
            } else if (!isCompoundMimeType) {
                // This is to prevent a clickthrough when the data can be already shown
                part->fetchFromCache(model);

                // This makes sure that clickthrough only affects big parts during "expensive network" mode
                if (model->isNetworkOnline() || part->octets() <= ExpensiveFetchThreshold) {
                    options |= PART_IGNORE_CLICKTHROUGH;
                }
            } else {
                // A compound type -> make sure we disable clickthrough
                options |= PART_IGNORE_CLICKTHROUGH;
            }

            if (!model->isNetworkAvailable()) {
                // This is to prevent a clickthrough when offline
                options |= PART_IGNORE_CLICKTHROUGH;
            }
            contentWidget = new LoadablePartWidget(0, manager, partIndex, m_messageView, this, recursionDepth + 1, options);
            if (!isInline) {
                contentWidget->hide();
            }
        }
        // Previously, we would also hide an attachment if the current policy was "expensive network", the part was too big
        // and not fetched yet. Arguably, that's a bug -- an item which is marked online shall not be hidden.
        // Wrapping via a clickthrough is fine, though; the user is clearly informed that this item *should* be visible,
        // yet the bandwidth is not excessively trashed.
        return new AttachmentView(0, manager, partIndex, m_messageView, contentWidget);
    }

    // Now we know for sure that it's not supposed to be wrapped in an AttachmentView, cool.
    if (mimeType.startsWith(QLatin1String("multipart/"))) {
        // it's a compound part
        if (mimeType == QLatin1String("multipart/alternative")) {
            return new MultipartAlternativeWidget(0, this, partIndex, recursionDepth, loadingMode);
        } else if (mimeType == QLatin1String("multipart/signed")) {
            return new MultipartSignedWidget(0, this, partIndex, recursionDepth, loadingMode);
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
                    return PartWidgetFactory::create(mainPartIndex, recursionDepth+1, loadingMode);
                } else {
                    // Sorry, but anything else than text/html is by definition suspicious here. Better than picking some random
                    // choice, let's just show everything.
                    return new GenericMultipartWidget(0, this, partIndex, recursionDepth, loadingMode);
                }
            } else {
                // The RFC2387's wording is clear that in absence of an explicit START argument, the first part is the starting one.
                // On the other hand, I've seen real-world messages whose first part is some utter garbage (an image sent as
                // application/octet-stream, for example) and some *other* part is an HTML text. In that case (and if we somehow
                // failed to pick the HTML part by a heuristic), it's better to show everything.
                return new GenericMultipartWidget(0, this, partIndex, recursionDepth, loadingMode);
            }
        } else {
            return new GenericMultipartWidget(0, this, partIndex, recursionDepth, loadingMode);
        }
    } else if (mimeType == QLatin1String("message/rfc822")) {
        return new Message822Widget(0, this, partIndex, recursionDepth, loadingMode);
    } else {
        part->fetchFromCache(model);

        if ((loadingMode & PART_IGNORE_CLICKTHROUGH) || (loadingMode & PART_IGNORE_LOAD_ON_SHOW) ||
                part->fetched() || model->isNetworkOnline() || part->octets() < ExpensiveFetchThreshold) {
            // Show it directly without any fancy wrapping
            return new SimplePartWidget(0, manager, partIndex, m_messageView);
        } else {
            return new LoadablePartWidget(0, manager, partIndex, m_messageView, this, recursionDepth + 1,
                                          model->isNetworkAvailable() ? loadingMode : loadingMode | PART_IGNORE_CLICKTHROUGH);
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
