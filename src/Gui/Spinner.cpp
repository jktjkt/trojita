/* Copyright (C) 2013 Thomas Lübking <thomas.luebking@gmail.com>

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

#include "Spinner.h"
#include "Common/InvokeMethod.h"
#include "Util.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QTimer>
#include <QTimerEvent>
#include <qmath.h>
#include <QtDebug>

using namespace Gui;

Spinner::Spinner(QWidget *parent) : QWidget(parent), m_step(0), m_fadeStep(0), m_timer(0),
                                    m_startTimer(0), m_textCols(0), m_type(Sun), m_geometryDirty(false)
{
    updateAncestors();
    hide();
}

void Spinner::setText(const QString &text)
{
    static const QLatin1Char newLine('\n');
    m_text = text;
    // calculate the maximum glyphs per row
    // this is later on used in the painting code to determine the font size
    // size altering pointsizes of fonts does not scale dimensions in a linear way (depending on the
    // hinter) this is precise enough and by using the maximum glyph width has enough padding from
    // the circle
    int idx = text.indexOf(newLine);
    int lidx = 0;
    m_textCols = 0;
    while (idx > -1) {
        m_textCols = qMax(m_textCols, idx - lidx);
        lidx = idx + 1;
        idx = text.indexOf(newLine, lidx);
    }
    m_textCols = qMax(m_textCols, text.length() - lidx);
}

QString Spinner::text() const
{
    return m_text;
}

void Spinner::setType(const Type t)
{
    m_type = t;
}

Spinner::Type Spinner::type() const
{
    return m_type;
}

void Spinner::start(uint delay)
{
    if (m_timer) { // already running...
        m_fadeStep = qAbs(m_fadeStep);
        return;
    }

    if (delay) {
        if (!m_startTimer) {
            m_startTimer = new QTimer(this);
            m_startTimer->setSingleShot(true);
            connect(m_startTimer, SIGNAL(timeout()), SLOT(start()));
        }
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        if (m_startTimer->remainingTime() > -1) // preserve oldest request original delay
            delay = m_startTimer->remainingTime();
#endif
        m_startTimer->start(delay);
        return;
    }

    if (m_startTimer)
        m_startTimer->stop();
    m_step = 0;
    m_fadeStep = 0;
    show();
    raise();
    m_timer = startTimer(100);
}

void Spinner::stop()
{
    if (m_startTimer)
        m_startTimer->stop();
    m_fadeStep = qMax(-11, qMin(-1, -qAbs(m_fadeStep))); // [-11,-1]
}

bool Spinner::event(QEvent *e)
{
    if (e->type() == QEvent::Show && m_geometryDirty) {
        updateGeometry();
    } else if (e->type() == QEvent::ParentChange) {
        updateAncestors();
    }
    return QWidget::event(e);
}

bool Spinner::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Resize || e->type() == QEvent::Move) {
        if (!m_geometryDirty && isVisible()) {
            CALL_LATER_NOARG(this, updateGeometry);
        }
        m_geometryDirty = true;
    } else if (e->type() == QEvent::ChildAdded || e->type() == QEvent::ZOrderChange) {
        if (o == parentWidget())
            raise();
    } else if (e->type() == QEvent::ParentChange) {
        updateAncestors();
    }
    return false;
}

void Spinner::paintEvent(QPaintEvent *)
{
    QColor c1(palette().color(backgroundRole())),
           c2(palette().color(foregroundRole()));
    const int a = c1.alpha();
    c1.setAlpha(170); // 2/3
    c2 = Util::tintColor(c2, c1);
    c2.setAlpha(qAbs(m_fadeStep)*a/18);

    int startAngle(16*90), span(360*16); // full circle starting at 12 o'clock
    int strokeSize, segments(0); // segments need to match painting steps "12" -> "2,3,4,6,12,24 ..."
    qreal segmentRatio(0.5);
    QPen pen(Qt::SolidLine);
    pen.setCapStyle(Qt::RoundCap);
    pen.setColor(c2);

    switch (m_type) {
        case Aperture:
        default:
            pen.setCapStyle(Qt::FlatCap);
            strokeSize = qMax(1,width()/8);
            startAngle -= 5*16*m_step;
            segments = 6;
            break;
        case Scythe:
            strokeSize = qMax(1,width()/16);
            startAngle -= 10*16*m_step;
            segments = 3;
            break;
        case Sun: {
            pen.setCapStyle(Qt::FlatCap);
            strokeSize = qMax(1,width()/4);
            segments = 12;
            segmentRatio = 0.8;
            break;
        }
        case Elastic: {
            strokeSize = qMax(1,width()/16);
            int step = m_step;
            startAngle -= 40*step*step;
            step = (step+9)%12;
            const int endAngle = 16*90 - 40*step*step;
            span = (endAngle - startAngle);
            if (span < 0)
                span = 360*16+span; // fix direction.
            break;
        }
    }

    pen.setWidth(strokeSize);
    if (segments) {
        const int radius = width() - 2*(strokeSize/2 + 1);
        qreal d = (M_PI*radius)/(segments*strokeSize);
        pen.setDashPattern(QVector<qreal>() << d*segmentRatio << d*(1.0-segmentRatio));
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);

    QRect r(rect());
    r.adjust(strokeSize/2+1, strokeSize/2+1, -(strokeSize/2+1), -(strokeSize/2+1));
    p.drawArc(r, startAngle, span);

    if (m_type == Sun) {
        QColor c3(palette().color(foregroundRole()));
        c3.setAlpha(c2.alpha());

        startAngle -= 30*16*m_step;
        pen.setColor(c3);
        p.setPen(pen);
        p.drawArc(r, startAngle, 30*16);

        for (int i = 2; i > 0; --i) {
            startAngle += 30*16;
            const int a = c3.alpha();
            c2.setAlpha(255/(i+1));
            c3 = Util::tintColor(c3, c2);
            c3.setAlpha(a);
            pen.setColor(c3);
            p.setPen(pen);
            p.drawArc(r, startAngle, 30*16);
        }

    }

    if (!m_text.isEmpty()) {
        QFont fnt;
        if (fnt.pointSize() > -1) {
            fnt.setBold(true);
            fnt.setPointSizeF((fnt.pointSizeF() * r.width()) / (m_textCols*QFontMetricsF(fnt).maxWidth()));
            p.setFont(fnt);
        }
        // cheap "outline" for better readability
        // this works "good enough" on sublying distorsion (aka. text) but looks crap if the background
        // is really colored differently from backgroundRole() -> QPainterPath + stroke
        p.setPen(c1);
        r.translate(-1,-1);
        p.drawText(r, Qt::AlignCenter|Qt::TextDontClip, m_text);
        r.translate(2,2);
        p.drawText(r, Qt::AlignCenter|Qt::TextDontClip, m_text);
        r.translate(-1,-1);
        // actual text painting
        c2 = QColor(palette().color(foregroundRole()));
        c2.setAlpha(qAbs(m_fadeStep)*c2.alpha()/12);
        p.setPen(c2);
        p.drawText(r, Qt::AlignCenter|Qt::TextDontClip, m_text);
    }
    p.end();
}

void Spinner::timerEvent(QTimerEvent *e)
{
    // timerEvent being used for being more lightweight than QTimer - no particular other reason
    if (e->timerId() == m_timer) {
        if (++m_step > 11)
            m_step = 0;
        if (m_fadeStep == -1) { // stop
            hide();
            killTimer(m_timer);
            m_timer = 0;
            m_step = 0;
        }
        if (m_fadeStep < 12)
            ++m_fadeStep;
        repaint();
    } else {
        QWidget::timerEvent(e);
    }
}

void Spinner::updateAncestors()
{
    foreach (QWidget *w, m_ancestors)
        w->removeEventFilter(this);

    m_ancestors.clear();

    QWidget *w = this;
    while ((w = w->parentWidget())) {
        m_ancestors << w;
        w->installEventFilter(this);
        connect(w, SIGNAL(destroyed()), SLOT(updateAncestors()));
    }
    updateGeometry();
}

void Spinner::updateGeometry()
{
    if (!isVisible()) {
        m_geometryDirty = true;
        return;
    }
    QRect visibleRect(m_ancestors.last()->rect());
    QPoint offset;
    for (int i = m_ancestors.count() - 2; i > -1; --i) {
        visibleRect &= m_ancestors.at(i)->geometry().translated(offset);
        offset += m_ancestors.at(i)->geometry().topLeft();
    }
    visibleRect.translate(-offset);
    const int size = 2*qMin(visibleRect.width(), visibleRect.height())/3;
    QRect r(0, 0, size, size);
    r.moveCenter(visibleRect.center());
    setGeometry(r);
    m_geometryDirty = false;
}
