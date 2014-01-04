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
#ifndef GUI_COMPLETEMESSAGEWIDGET_H
#define GUI_COMPLETEMESSAGEWIDGET_H

#include <QWidget>

class QScrollArea;
class QSettings;
class QWebView;
class QPropertyAnimation;

namespace Gui
{

class FindBar;
class MessageView;

class CompleteMessageWidget: public QWidget
{
    Q_OBJECT
public:
    CompleteMessageWidget(QWidget *parent, QSettings *settings);

    MessageView *messageView;
    QScrollArea *area;
protected:
    void keyPressEvent(QKeyEvent *ke);

private slots:
    void searchRequestedBy(QWebView *webView);

private:
    CompleteMessageWidget(const CompleteMessageWidget &); // don't implement
    CompleteMessageWidget &operator=(const CompleteMessageWidget &); // don't implement

    FindBar *m_findBar;
    QPropertyAnimation *animator;
};

}

#endif // GUI_COMPLETEMESSAGEWIDGET_H
