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

#include <QMimeDatabase>
#include "UiUtils/PartWalker.h"
#include "Imap/Model/ItemRoles.h"

namespace UiUtils {

template<typename Result, typename Context>
PartWalker<Result, Context>::PartWalker(Imap::Network::MsgPartNetAccessManager *manager,
                               Context context, std::unique_ptr<PartVisitor<Result, Context> > visitor)
    : m_manager(manager), m_netWatcher(0), m_context(context)
{
    m_visitor = std::move(visitor);
}

template<typename Result, typename Context>
Result PartWalker<Result, Context>::walk(const QModelIndex &partIndex,int recursionDepth, const UiUtils::PartLoadingOptions loadingMode)
{
    using namespace Imap::Mailbox;
    Q_ASSERT(partIndex.isValid());

    if (recursionDepth > 1000) {
        return m_visitor->visitError(PartWalker::tr("This message contains too deep nesting of MIME message parts.\n"
                             "To prevent stack exhaustion and your head from exploding, only\n"
                             "the top-most thousand items or so are shown."), 0);
    }

    QString mimeType = partIndex.data(Imap::Mailbox::RolePartMimeType).toString().toLower();
    bool isMessageRfc822 = mimeType == QLatin1String("message/rfc822");
    bool isCompoundMimeType = mimeType.startsWith(QLatin1String("multipart/")) || isMessageRfc822;

    if (loadingMode & PART_IS_HIDDEN) {
        return m_visitor->visitLoadablePart(0, m_manager, partIndex, this, recursionDepth + 1,
                                      loadingMode | PART_IGNORE_CLICKTHROUGH);
    }

    // Check if we are dealing with encrypted data
    if (mimeType == QLatin1String("multipart/encrypted")) {
        return m_visitor->visitMultipartEncryptedView(0, this, partIndex, recursionDepth, loadingMode);
    }

    // Check whether we can render this MIME type at all
    QStringList allowedMimeTypes;
    allowedMimeTypes << QStringLiteral("text/html") << QStringLiteral("text/plain") << QStringLiteral("image/jpeg") <<
                     QStringLiteral("image/jpg") << QStringLiteral("image/pjpeg") << QStringLiteral("image/png") << QStringLiteral("image/gif");
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
                m_manager->registerMimeTypeTranslation(mimeType, candidate);
                break;
            }
        }
    }

    // Check whether it shall be wrapped inside an AttachmentView
    // From section 2.8 of RFC 2183: "Unrecognized disposition types should be treated as `attachment'."
    const QByteArray contentDisposition = partIndex.data(Imap::Mailbox::RolePartBodyDisposition).toByteArray().toLower();
    const bool isInline = (contentDisposition.isEmpty() || contentDisposition == "inline") && !(loadingMode & PART_IGNORE_INLINE);
    const bool looksLikeAttachment = !partIndex.data(Imap::Mailbox::RolePartFileName).toString().isEmpty();
    const bool wrapInAttachmentView = !(loadingMode & PART_IGNORE_DISPOSITION_ATTACHMENT)
            && (looksLikeAttachment || !isInline || !recognizedMimeType || isDerivedMimeType || isMessageRfc822);
    if (wrapInAttachmentView) {
        // The problem is that some nasty MUAs (hint hint Thunderbird) would
        // happily attach a .tar.gz and call it "inline"
        Result contentView = 0;
        if (recognizedMimeType) {
            PartLoadingOptions options = loadingMode | PART_IGNORE_DISPOSITION_ATTACHMENT;
            if (!isInline) {
                // The widget will be hidden by default, i.e. the "inline preview" will be deactivated.
                // If the user clicks that action in the AttachmentView, it makes sense to load the plugin without any further ado,
                // without requiring an extra clickthrough
                options |= PART_IS_HIDDEN;
            } else if (!isCompoundMimeType) {
                // This is to prevent a clickthrough when the data can be already shown
                partIndex.data(Imap::Mailbox::RolePartForceFetchFromCache);

                // This makes sure that clickthrough only affects big parts during "expensive network" mode
                if ( (m_netWatcher && m_netWatcher->desiredNetworkPolicy() != Imap::Mailbox::NETWORK_EXPENSIVE)
                        || partIndex.data(Imap::Mailbox::RolePartOctets).toULongLong() <= ExpensiveFetchThreshold) {
                    options |= PART_IGNORE_CLICKTHROUGH;
                }
            } else {
                // A compound type -> make sure we disable clickthrough
                options |= PART_IGNORE_CLICKTHROUGH;
            }

            if (m_netWatcher && m_netWatcher->effectiveNetworkPolicy() == Imap::Mailbox::NETWORK_OFFLINE) {
                // This is to prevent a clickthrough when offline
                options |= PART_IGNORE_CLICKTHROUGH;
            }
            contentView = m_visitor->visitLoadablePart(0, m_manager, partIndex, this, recursionDepth + 1, options);
            if (!isInline) {
                m_visitor->applySetHidden(contentView);
            }
        }
        // Previously, we would also hide an attachment if the current policy was "expensive network", the part was too big
        // and not fetched yet. Arguably, that's a bug -- an item which is marked online shall not be hidden.
        // Wrapping via a clickthrough is fine, though; the user is clearly informed that this item *should* be visible,
        // yet the bandwidth is not excessively trashed.
        return m_visitor->visitAttachmentPart(0, m_manager, partIndex, m_context, contentView);
    }

    // Now we know for sure that it's not supposed to be wrapped in an AttachmentView, cool.
    if (mimeType.startsWith(QLatin1String("multipart/"))) {
        // it's a compound part
        if (mimeType == QLatin1String("multipart/alternative")) {
            return m_visitor->visitMultipartAlternative(0, this, partIndex, recursionDepth, loadingMode);
        } else if (mimeType == QLatin1String("multipart/signed")) {
            return m_visitor->visitMultipartSignedView(0, this, partIndex, recursionDepth, loadingMode);
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
                mainPartIndex = Imap::Network::MsgPartNetAccessManager::cidToPart(partIndex, mainPartCID.toByteArray());
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
                    return walk(mainPartIndex, recursionDepth+1, loadingMode);
                } else {
                    // Sorry, but anything else than text/html is by definition suspicious here. Better than picking some random
                    // choice, let's just show everything.
                    return m_visitor->visitGenericMultipartView(0, this, partIndex, recursionDepth, loadingMode);
                }
            } else {
                // The RFC2387's wording is clear that in absence of an explicit START argument, the first part is the starting one.
                // On the other hand, I've seen real-world messages whose first part is some utter garbage (an image sent as
                // application/octet-stream, for example) and some *other* part is an HTML text. In that case (and if we somehow
                // failed to pick the HTML part by a heuristic), it's better to show everything.
                return m_visitor->visitGenericMultipartView(0, this, partIndex, recursionDepth, loadingMode);
            }
        } else {
            return m_visitor->visitGenericMultipartView(0, this, partIndex, recursionDepth, loadingMode);
        }
    } else if (mimeType == QLatin1String("message/rfc822")) {
        return m_visitor->visitMessage822View(0, this, partIndex, recursionDepth, loadingMode);
    } else {
        partIndex.data(Imap::Mailbox::RolePartForceFetchFromCache);

        if ((loadingMode & PART_IGNORE_CLICKTHROUGH) || (loadingMode & PART_IGNORE_LOAD_ON_SHOW) ||
                partIndex.data(Imap::Mailbox::RoleIsFetched).toBool() ||
                (m_netWatcher && m_netWatcher->desiredNetworkPolicy() != Imap::Mailbox::NETWORK_EXPENSIVE ) ||
                partIndex.data(Imap::Mailbox::RolePartOctets).toULongLong() < ExpensiveFetchThreshold) {
            // Show it directly without any fancy wrapping
            return m_visitor->visitSimplePartView(0, m_manager, partIndex, m_context);
        } else {
            return m_visitor->visitLoadablePart(0, m_manager, partIndex, this, recursionDepth + 1,
                                          (m_netWatcher && m_netWatcher->effectiveNetworkPolicy() != Imap::Mailbox::NETWORK_OFFLINE) ?
                                          loadingMode : loadingMode | PART_IGNORE_CLICKTHROUGH);
        }
    }
}

template<typename Result, typename Context>
Context PartWalker<Result, Context>::context() const
{
    return m_context;
}

template<typename Result, typename Context>
void PartWalker<Result, Context>::setNetworkWatcher(Imap::Mailbox::NetworkWatcher *netWatcher)
{
    m_netWatcher = netWatcher;
}

}
