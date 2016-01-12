/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

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
#include "Gui/AddressRowWidget.h"
#include "Gui/FlowLayout.h"
#include "Gui/OneEnvelopeAddress.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTextDocument>

namespace Gui {

static int plainChars(const QLabel *l)
{
    static QTextDocument converter;
    converter.setHtml(l->text());
    return converter.toPlainText().length();
}

AddressRowWidget::AddressRowWidget(QWidget *parent, const QString &headerName,
                                   const QList<Imap::Message::MailAddress> &addresses, MessageView *messageView):
    QWidget(parent), m_expander(0)
{
    FlowLayout *lay = new FlowLayout(this, 0, 0, -1);
    setLayout(lay);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QLabel *title = new QLabel(QStringLiteral("<b>%1:</b>").arg(headerName.toHtmlEscaped()), this);
    title->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    lay->addWidget(title);
    int chars = 0;
    bool collapse = false;
    for (int i = 0; i < addresses.size(); ++i) {
        auto *w = new OneEnvelopeAddress(this, addresses[i], messageView,
                                         i == addresses.size() - 1 ?
                                         OneEnvelopeAddress::Position::Last :
                                         OneEnvelopeAddress::Position::Middle);
        chars += plainChars(w);
        lay->addWidget(w);
        if (i > 1 && chars > 66) {
            w->hide();
            collapse = true;
        }
    }
    if (collapse) {
        lay->addWidget(m_expander = new Expander(this));
        connect(m_expander, &Expander::clicked, this, &AddressRowWidget::toggle);
    }
}

void AddressRowWidget::toggle()
{
    Q_ASSERT(m_expander);
    if (m_expander->isExpanding()) {
        m_expander->setExpanding(false);
        for (int i = 0; i < layout()->count(); ++i) {
            if (QWidget *w = layout()->itemAt(i)->widget())
                w->show();
        }
    } else {
        int chars = 0, addresses = 0;
        int collapse = 0;
        for (int i = 0; i < layout()->count(); ++i) {
            if (OneEnvelopeAddress *w =
                qobject_cast<OneEnvelopeAddress*>(layout()->itemAt(i)->widget())) {
                ++addresses;
                chars += plainChars(w);
                if (addresses > 1 && chars > 66) {
                    ++collapse;
                    w->hide();
                }
            }
        }
        m_expander->setExpanding(collapse);
    }
}

Expander::Expander(QWidget *parent, Expander::Direction d) : QLabel(parent)
{
    setCursor(Qt::PointingHandCursor);
    setExpanding(d == Direction::Expanding);
}

bool Expander::isExpanding() const {
    return m_expanding;
}

void Expander::setExpanding(const int expanding)
{
    m_expanding = expanding;
    setToolTip(expanding ? tr("Click to see %1 more items").arg(expanding) : tr("Click to collapse"));
}

QSize Expander::sizeHint() const
{
    QSize s = QLabel::sizeHint();
    s.setWidth(m_expanding ? 2*s.height() : s.height());
    return s;
}

void Expander::mouseReleaseEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton)
        emit clicked();
}

void Expander::paintEvent(QPaintEvent *pe)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r(rect());
    r.setWidth(height());
    if (m_expanding) {
        // ellipsis
        p.setPen(palette().color(foregroundRole()));
        p.setBrush(Qt::NoBrush);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("\u2026"));
        r.moveRight(rect().right());
    }
    p.setBrush(palette().color(foregroundRole()));
    p.setPen(Qt::NoPen);
    p.drawEllipse(r);
    p.setBrush(palette().color(backgroundRole()));
    float d = r.height()/4.0f;
    r.adjust(d,d,-d,-d);
    // the unicode triangles are not necessarily centered - looks crap
    if (m_expanding) {
        r.translate(r.width()/8.0f,0); // fix gravity
        const QPointF points[3] = { r.topLeft(), r.bottomLeft(), QPointF(r.right(), r.y() + r.height()/2.0) };
        p.drawConvexPolygon(points, 3);
    } else {
        r.translate(-r.width()/8.0f,0); // fix gravity
        const QPointF points[3] = { r.topRight(), r.bottomRight(), QPointF(r.left(), r.y() + r.height()/2.0) };
        p.drawConvexPolygon(points, 3);
    }
    p.end();
}

}
