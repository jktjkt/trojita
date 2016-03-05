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

#ifndef GUI_TAGWIDGET_H
#define GUI_TAGWIDGET_H

#include <QLabel>

namespace Gui
{

class TagWidget : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString tagName READ tagName USER true)
public:
    static TagWidget *addingWidget();
    static TagWidget *userKeyword(const QString &tagName);
    static TagWidget *systemFlag(const QString &flagName);

    QString tagName() const;

    bool event(QEvent *e);

signals:
    void removeClicked(QString);
    void addingClicked();

private:
    enum class Mode {
        SystemFlag,
        UserKeyword,
        AddingWidget,
    };

    QString m_tagName;
    const Mode m_mode;
    QColor m_tint;
    int m_splitPos;

    TagWidget(const Mode mode, const QString &tagName, const QColor &backgroundColor);
};

} // namespace Gui

#endif // GUI_TAGWIDGET_H
