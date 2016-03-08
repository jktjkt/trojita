/* Copyright (C) 2012 Mildred <mildred-pub@mildred.fr>
   Copyright (C) 2015 Erik Quaeghebeur <trojita@equaeghe.nospammail.net>
   Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

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

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPair>

#include "UiUtils/Color.h"

#include "TagWidget.h"

namespace Gui
{

static const QLatin1String closeIndicator(" | x");

TagWidget::TagWidget(const Mode mode, const QString &tagName, const QColor &backgroundColor):
    QLabel(),
    m_tagName(tagName),
    m_mode(mode),
    m_tint(backgroundColor),
    m_splitPos(0)
{
    m_tint.setAlpha(m_tint.alpha()/3);
    setAlignment(Qt::AlignCenter);
    int l,t,r,b;
    getContentsMargins(&l, &t, &r, &b);
    setContentsMargins(l + 4, t, r + 4, b);
    if (m_mode == Mode::AddingWidget)
        setCursor(Qt::PointingHandCursor);
}

TagWidget *TagWidget::addingWidget()
{
    auto res = new TagWidget(Mode::AddingWidget, QString(), Qt::green);
    res->setText(tr("Tag", "This is a verb!"));
    return res;
}

TagWidget *TagWidget::userKeyword(const QString &tagName)
{
    auto res = new TagWidget(Mode::UserKeyword, tagName, Qt::yellow);
    res->setText(tagName + closeIndicator);
    res->setMouseTracking(true);
    return res;
}

TagWidget *TagWidget::systemFlag(const QString &flagName)
{
    auto res = new TagWidget(Mode::SystemFlag, flagName, Qt::gray);
    res->setText(flagName);
    return res;
}

bool TagWidget::event(QEvent *e)
{
    if (e->type() == QEvent::MouseMove) {
        if (m_mode == Mode::UserKeyword)
            setCursor(static_cast<QMouseEvent*>(e)->pos().x() > m_splitPos ? Qt::PointingHandCursor : Qt::ArrowCursor);
    } else if (e->type() == QEvent::Resize) {
        if (m_mode == Mode::UserKeyword)
            m_splitPos = contentsRect().right() - fontMetrics().width(closeIndicator);
    } else if (e->type() == QEvent::MouseButtonPress) {
        switch (m_mode) {
        case Mode::AddingWidget:
            emit addingClicked();
            return true;
        case Mode::UserKeyword: {
            Q_ASSERT(!m_tagName.isEmpty());
            if (static_cast<QMouseEvent*>(e)->pos().x() > m_splitPos) {
                setDisabled(true);
                setText(m_tagName);
                setCursor(Qt::ArrowCursor);
                setMouseTracking(false);
                emit removeClicked(m_tagName);
            }
            return true;
        }
        case Mode::SystemFlag:
            // do nothing -- this is just an indicator
            break;
        }
    } else if (e->type() == QEvent::Paint) {
        if (isEnabled()) {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(UiUtils::tintColor(palette().color(QPalette::Active, backgroundRole()), m_tint));
            p.setPen(Qt::NoPen);
            const int rnd = qMin(width()/2, height()/4);
            p.drawRoundedRect(rect(), rnd, rnd);
            p.end();
        }
    }

    return QLabel::event(e);
}

QString TagWidget::tagName() const
{
    return m_tagName;
}

} // namespace Gui
