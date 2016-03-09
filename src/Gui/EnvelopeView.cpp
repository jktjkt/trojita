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
#include <QFontMetrics>
#include <QLabel>
#include <QUrlQuery>
#include <QGridLayout>
#include <QLayoutItem>
#include <QVBoxLayout>
#include "Gui/AddressRowWidget.h"
#include "Gui/MessageView.h"
#include "Gui/Util.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "UiUtils/Formatting.h"

namespace Gui {

EnvelopeView::EnvelopeView(QWidget *parent, MessageView *messageView): QWidget(parent), m_messageView(messageView)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    setLayout(lay);

}

#define SET_LABEL_OPTIONS(LBL) \
{ \
    LBL->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); \
    LBL->setWordWrap(true); \
    LBL->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse); \
}

#define ADD_ROW(LBL, WDG) \
{ \
    QLabel *l = new QLabel(LBL, this); \
    l->setAlignment(Qt::AlignRight); \
    form->addWidget(l, form->rowCount(), 0, Qt::AlignTop); \
    form->addWidget(WDG, form->rowCount()-1, 1, Qt::AlignTop); \
}

/** @short */
void EnvelopeView::setMessage(const QModelIndex &index)
{
    while (QLayoutItem *item = layout()->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        } else if (item->layout()) {
            item->layout()->deleteLater();
        } else {
            delete item;
        }
    }
    Q_FOREACH(QWidget *w, findChildren<QWidget*>()) {
        w->deleteLater();
    }

    if (!index.isValid()) {
        return;
    }

    const Imap::Message::Envelope e = index.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();

    // Subject & date
    QString subDate;
    // Date
    if (e.date.isValid()) {
        subDate = QStringLiteral("<table style=\"margin:0px; margin-left:4em; float:right;\"><tr style=\"margin:0px;\"><td style=\"margin:0px;\">%1</td></tr></table>").arg(e.date.toLocalTime().toString(Qt::SystemLocaleLongDate));
    }
    subDate += QStringLiteral("<span style=\"font:bold large;\">%1</span>").arg(e.subject.toHtmlEscaped());
    auto lbl = new QLabel(subDate, this);
    SET_LABEL_OPTIONS(lbl)
    layout()->addWidget(lbl);

    QGridLayout *form = new QGridLayout();
    form->setSpacing(0);
    form->setContentsMargins(0, 0, 0, 0);
    static_cast<QBoxLayout*>(layout())->addLayout(form);

    // Sender
    AddressRowWidget *senderWidget = 0;
    QString senderLabel;
    if (!e.from.isEmpty()) {
        senderLabel = tr("From");
        senderWidget = new AddressRowWidget(this, QString(), e.from, m_messageView);
    }
    if (!e.sender.isEmpty() && e.sender != e.from) {
        if (senderWidget) {
            senderWidget->addAddresses(QStringLiteral(" %1").arg(tr("sent via")), e.sender, m_messageView);
        } else {
            senderLabel = tr("Sender");
            senderWidget = new AddressRowWidget(this, QString(), e.sender, m_messageView);
        }
    }
    if (!e.replyTo.isEmpty() && e.replyTo != e.from) {
        if (senderWidget) {
            senderWidget->addAddresses(QStringLiteral(", %1").arg(tr("replies to")), e.replyTo, m_messageView);
        } else {
            senderLabel = tr("Reply-To");
            senderWidget = new AddressRowWidget(this, QString(), e.replyTo, m_messageView);
        }
    }
    if (senderWidget)
        ADD_ROW(senderLabel, senderWidget)

    // Receiver
    AddressRowWidget *receiverWidget = 0;
    QString receiverLabel;
    if (!e.to.isEmpty()) {
        receiverLabel = tr("To");
        receiverWidget = new AddressRowWidget(this, QString(), e.to, m_messageView);
    }
    if (!e.cc.isEmpty()) {
        if (receiverWidget) {
            receiverWidget->addAddresses(QStringLiteral(" %1").arg(tr("CC'd to")), e.cc, m_messageView);
        } else {
            receiverLabel = tr("Cc");
            receiverWidget = new AddressRowWidget(this, QString(), e.cc, m_messageView);
        }
    }
    if (!e.bcc.isEmpty()) {
        if (receiverWidget) {
            receiverWidget->addAddresses(QStringLiteral(" %1").arg(tr("Bcc'd to")), e.bcc, m_messageView);
        } else {
            receiverLabel = tr("Bcc");
            receiverWidget = new AddressRowWidget(this, QString(), e.bcc, m_messageView);
        }
    }
    if (receiverWidget)
        ADD_ROW(receiverLabel, receiverWidget)

    // Mailing list
    QVariantList headerListPost = index.data(Imap::Mailbox::RoleMessageHeaderListPost).toList();
    if (!headerListPost.isEmpty()) {
        QStringList buf;
        bool elided = false;
        Q_FOREACH(const QVariant &item, headerListPost) {
            const QString scheme = item.toUrl().scheme().toLower();
            const bool isMailTo = scheme == QLatin1String("mailto");
            if (isMailTo || scheme == QLatin1String("http") || scheme == QLatin1String("https")) {
                QString target = item.toUrl().toString();
                // eg. github uses reply+<some hash>@reply.github.com
                QString caption = isMailTo ? item.toUrl().toString(QUrl::RemoveScheme) : target;
                elided = elided || UiUtils::elideAddress(caption);
                target = target.toHtmlEscaped();
                buf << tr("<a href=\"%1\">%2</a>").arg(target, caption.toHtmlEscaped());
            } else {
                buf << item.toUrl().toString().toHtmlEscaped();
            }
        }
        auto lbl = new QLabel(QString(QLatin1String("<html>&nbsp;%1</html>")).arg(buf.join(tr(", "))));
        SET_LABEL_OPTIONS(lbl)
        if (elided)
            connect(lbl, &QLabel::linkHovered, lbl, &QLabel::setToolTip);
        ADD_ROW(tr("Mailing List"), lbl)
    }

    // separating the message
    QFrame *line = new QFrame(this);
    line->setFrameStyle(QFrame::HLine|QFrame::Plain);
    layout()->addWidget(line);
}


}
