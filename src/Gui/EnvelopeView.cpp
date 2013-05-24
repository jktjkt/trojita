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
#include "EnvelopeView.h"
#include <QHeaderView>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <QTextDocument>
#else
#  include <QUrlQuery>
#endif
#include "MessageView.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Gui {

EnvelopeView::EnvelopeView(QWidget *parent): QLabel(parent)
{
    // we create a dummy header, pass it through the style and the use it's color roles so we
    // know what headers in general look like in the system
    QHeaderView helpingHeader(Qt::Horizontal);
    helpingHeader.ensurePolished();

    setBackgroundRole(helpingHeader.backgroundRole());
    setForegroundRole(helpingHeader.foregroundRole());
    setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    setIndent(5);
    setWordWrap(true);
    connect(this, SIGNAL(linkHovered(QString)), this, SLOT(onLinkHovered(QString)));
}

/** @short */
void EnvelopeView::setMessage(const QModelIndex &index)
{
    setText(headerText(index));
}

QString EnvelopeView::headerText(const QModelIndex &index)
{
    if (!index.isValid())
        return QString();

    const Imap::Message::Envelope e = index.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();

    QString res;
    if (!e.from.isEmpty())
        res += tr("<b>From:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(e.from, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!e.sender.isEmpty() && e.sender != e.from)
        res += tr("<b>Sender:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(e.sender, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!e.replyTo.isEmpty() && e.replyTo != e.from)
        res += tr("<b>Reply-To:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(e.replyTo, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    QVariantList headerListPost = index.data(Imap::Mailbox::RoleMessageHeaderListPost).toList();
    if (!headerListPost.isEmpty()) {
        QStringList buf;
        Q_FOREACH(const QVariant &item, headerListPost) {
            const QString scheme = item.toUrl().scheme().toLower();
            if (scheme == QLatin1String("http") || scheme == QLatin1String("https") || scheme == QLatin1String("mailto")) {
                QString target = item.toUrl().toString();
                QString caption = item.toUrl().toString(scheme == QLatin1String("mailto") ? QUrl::RemoveScheme : QUrl::None);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                target = Qt::escape(target);
                caption = Qt::escape(caption);
#else
                target = target.toHtmlEscaped();
                caption = caption.toHtmlEscaped();
#endif
                buf << tr("<a href=\"%1\">%2</a>").arg(target, caption);
            } else {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                buf << Qt::escape(item.toUrl().toString());
#else
                buf << item.toUrl().toString().toHtmlEscaped();
#endif
            }
        }
        res += tr("<b>List-Post:</b>&nbsp;%1<br/>").arg(buf.join(tr(", ")));
    }
    if (!e.to.isEmpty())
        res += tr("<b>To:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(e.to, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!e.cc.isEmpty())
        res += tr("<b>Cc:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(e.cc, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!e.bcc.isEmpty())
        res += tr("<b>Bcc:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(e.bcc, Imap::Message::MailAddress::FORMAT_CLICKABLE));
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    res += tr("<b>Subject:</b>&nbsp;%1").arg(Qt::escape(e.subject));
#else
    res += tr("<b>Subject:</b>&nbsp;%1").arg(e.subject.toHtmlEscaped());
#endif
    if (e.date.isValid())
        res += tr("<br/><b>Date:</b>&nbsp;%1").arg(e.date.toLocalTime().toString(Qt::SystemLocaleLongDate));
    return res;
}

void EnvelopeView::onLinkHovered(const QString &target)
{
    QUrl url(target);

    if (target.isEmpty() || url.scheme().toLower() != QLatin1String("mailto")) {
        setToolTip(QString());
        return;
    }

    QString frontOfAtSign, afterAtSign;
    if (url.path().indexOf(QLatin1String("@")) != -1) {
        QStringList chunks = url.path().split(QLatin1String("@"));
        frontOfAtSign = chunks[0];
        afterAtSign = QStringList(chunks.mid(1)).join(QLatin1String("@"));
    }
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    Imap::Message::MailAddress addr(url.queryItemValue(QLatin1String("X-Trojita-DisplayName")), QString(),
                                    frontOfAtSign, afterAtSign);
    setToolTip(Qt::escape(addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE)));
#else
    QUrlQuery q(url);
    Imap::Message::MailAddress addr(q.queryItemValue(QLatin1String("X-Trojita-DisplayName")), QString(),
                                    frontOfAtSign, afterAtSign);
    setToolTip(addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE).toHtmlEscaped());
#endif
}

void EnvelopeView::connectWithMessageView(MessageView *messageView)
{
    connect(this, SIGNAL(linkActivated(QString)), messageView, SLOT(headerLinkActivated(QString)));
}

}
