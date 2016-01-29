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
#include <QPair>

#include "TagWidget.h"

namespace Gui
{

TagWidget::TagWidget(const Mode mode, const QString &tagName, const QString &backgroundColor):
    QLabel(),
    m_tagName(tagName),
    m_mode(mode)
{
    setStyleSheet(QString::fromLatin1("border: 1px solid black; border-radius: 4px; background-color: %1;").arg(backgroundColor));
}

TagWidget *TagWidget::addingWidget()
{
    auto res = new TagWidget(Mode::AddingWidget, QString(), QStringLiteral("lightgreen"));
    res->setText(QStringLiteral("+"));
    return res;
}

TagWidget *TagWidget::userKeyword(const QString &tagName)
{
    auto res = new TagWidget(Mode::UserKeyword, tagName, QStringLiteral("lightyellow"));
    res->setText(tagName + QLatin1String(" | x"));
    return res;
}

TagWidget *TagWidget::systemFlag(const QString &flagName)
{
    auto res = new TagWidget(Mode::SystemFlag, flagName, QStringLiteral("lightgrey"));
    res->setText(flagName);
    return res;
}

bool TagWidget::event(QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress) {
        switch (m_mode) {
        case Mode::AddingWidget:
            emit addingClicked();
            return true;
        case Mode::UserKeyword:
            Q_ASSERT(!m_tagName.isEmpty());
            emit removeClicked(m_tagName);
            return true;
        case Mode::SystemFlag:
            // do nothing -- this is just an indicator
            break;
        }
    }

    return QLabel::event(e);
}

QString TagWidget::tagName() const
{
    return m_tagName;
}

} // namespace Gui
