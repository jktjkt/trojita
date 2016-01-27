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
#include "EnvelopeView.h"
#include <QHeaderView>
#include <QFontMetrics>
#include <QLabel>
#include <QUrlQuery>
#include <QVBoxLayout>
#include "Gui/AddressRowWidget.h"
#include "Gui/MessageView.h"
#include "Gui/Util.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Gui {

EnvelopeView::EnvelopeView(QWidget *parent, MessageView *messageView): QWidget(parent), m_messageView(messageView)
{
    // we create a dummy header, pass it through the style and the use it's color roles so we
    // know what headers in general look like in the system
    QHeaderView helpingHeader(Qt::Horizontal);
    helpingHeader.ensurePolished();

    setBackgroundRole(helpingHeader.backgroundRole());
    setForegroundRole(helpingHeader.foregroundRole());

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setSpacing(0);
    lay->setContentsMargins(0, 0, 0, 0);
    setLayout(lay);

}

#define SET_LABEL_OPTIONS(LBL) \
{ \
    LBL->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); \
    LBL->setWordWrap(true); \
    LBL->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse); \
}

/** @short */
void EnvelopeView::setMessage(const QModelIndex &index)
{
    Q_FOREACH(QWidget *w, findChildren<QWidget*>()) {
        w->deleteLater();
    }

    if (!index.isValid()) {
        return;
    }

    const Imap::Message::Envelope e = index.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();

    if (!e.from.isEmpty()) {
        layout()->addWidget(new AddressRowWidget(this, tr("From"), e.from, m_messageView));
    }
    if (!e.sender.isEmpty() && e.sender != e.from) {
        layout()->addWidget(new AddressRowWidget(this, tr("Sender"), e.sender, m_messageView));
    }
    if (!e.replyTo.isEmpty() && e.replyTo != e.from) {
        layout()->addWidget(new AddressRowWidget(this, tr("Reply-To"), e.replyTo, m_messageView));
    }
    QVariantList headerListPost = index.data(Imap::Mailbox::RoleMessageHeaderListPost).toList();
    if (!headerListPost.isEmpty()) {
        QStringList buf;
        Q_FOREACH(const QVariant &item, headerListPost) {
            const QString scheme = item.toUrl().scheme().toLower();
            if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("mailto")) {
                QString target = item.toUrl().toString();
                QString caption = item.toUrl().toString(scheme == QLatin1String("mailto") ? QUrl::RemoveScheme : QUrl::None);
                buf << tr("<a href=\"%1\">%2</a>").arg(target.toHtmlEscaped(), caption.toHtmlEscaped());
            } else {
                buf << item.toUrl().toString().toHtmlEscaped();
            }
        }
        auto lbl = new QLabel(tr("<b>Mailing List:</b>&nbsp;%1").arg(buf.join(tr(", "))));
        SET_LABEL_OPTIONS(lbl)
        layout()->addWidget(lbl);
    }
    if (!e.to.isEmpty()) {
        layout()->addWidget(new AddressRowWidget(this, tr("To"), e.to, m_messageView));
    }
    if (!e.cc.isEmpty()) {
        layout()->addWidget(new AddressRowWidget(this, tr("Cc"), e.cc, m_messageView));
    }
    if (!e.bcc.isEmpty()) {
        layout()->addWidget(new AddressRowWidget(this, tr("Bcc"), e.bcc, m_messageView));
    }
    auto lbl = new QLabel(tr("<b>Subject:</b>&nbsp;%1").arg(e.subject.toHtmlEscaped()), this);
    SET_LABEL_OPTIONS(lbl)
    layout()->addWidget(lbl);
    if (e.date.isValid()) {
        const QString &date = e.date.toLocalTime().toString(Qt::SystemLocaleLongDate);
        auto lbl = new QLabel(tr("<b>Date:</b>&nbsp;%1").arg(date.toHtmlEscaped()), this);
        SET_LABEL_OPTIONS(lbl)
        layout()->addWidget(lbl);
    }
}

}
