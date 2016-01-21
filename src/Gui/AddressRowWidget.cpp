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

static const int nonExpandingLength = 3*33;

static int plainChars(const QLabel *l)
{
    static QTextDocument converter;
    converter.setHtml(l->text());
    return converter.toPlainText().length();
}

AddressRowWidget::AddressRowWidget(QWidget *parent, const QString &description,
                                   const QList<Imap::Message::MailAddress> &addresses, MessageView *messageView):
    QWidget(parent), m_expander(0), m_expandedLength(0)
{
    FlowLayout *lay = new FlowLayout(this, 0, 0, 0);
    setLayout(lay);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    addAddresses(description, addresses, messageView);
}

void AddressRowWidget::addAddresses(const QString &description, const QList<Imap::Message::MailAddress> &addresses, MessageView *messageView)
{
    if (m_expander)
        layout()->removeWidget(m_expander);
    if (!description.isEmpty()) {
        QLabel *title = new QLabel(description, this);
        title->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        title->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
        layout()->addWidget(title);
        if (m_expander)
            title->hide();
    }
    int collapse = m_expander ? m_expander->expanding() : 0;
    for (int i = 0; i < addresses.size(); ++i) {
        auto *w = new OneEnvelopeAddress(this, addresses[i], messageView,
                                         i == addresses.size() - 1 ?
                                         OneEnvelopeAddress::Position::Last :
                                         OneEnvelopeAddress::Position::Middle);
        m_expandedLength += plainChars(w);
        layout()->addWidget(w);
        if (collapse || (i > 1 && m_expandedLength > nonExpandingLength)) {
            w->hide();
            ++collapse;
        }
    }
    if (collapse && !m_expander) {
        m_expander = new Expander(this, collapse);
        connect(m_expander, &Expander::clicked, this, &AddressRowWidget::toggle);
    }
    if (m_expander)
        layout()->addWidget(m_expander);
}

void AddressRowWidget::toggle()
{
    Q_ASSERT(m_expander);
    if (m_expander->expanding()) {
        m_expander->setExpanding(false);
        for (int i = 0; i < layout()->count(); ++i) {
            if (QWidget *w = layout()->itemAt(i)->widget())
                w->show();
        }
    } else {
        int chars = 0, addresses = 0;
        int collapse = 0;
        for (int i = 0; i < layout()->count(); ++i) {
            QWidget *w = layout()->itemAt(i)->widget();
            if (collapse && w != m_expander) {
                ++collapse;
                w->hide();
                continue;
            }
            if (OneEnvelopeAddress *oea = qobject_cast<OneEnvelopeAddress*>(w)) {
                ++addresses;
                chars += plainChars(oea);
                if ((collapse = (addresses > 1 && chars > nonExpandingLength)))
                    w->hide();
            }
        }
        m_expander->setExpanding(collapse);
    }
}

Expander::Expander(QWidget *parent, int count) : QLabel(parent)
{
    setCursor(Qt::PointingHandCursor);
    setExpanding(count);
}

int Expander::expanding() const {
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
