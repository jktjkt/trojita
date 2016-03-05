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
#ifndef GUI_ADDRESSROWWIDGET_H
#define GUI_ADDRESSROWWIDGET_H

#include <QLabel>

namespace Imap {
namespace Message {
class MailAddress;
}
}

namespace Gui {

class MessageView;

/** @short A label with expansion indicating text and "clicked" signal */
class Expander : public QLabel
{
    Q_OBJECT
public:
    explicit Expander(QWidget *parent, int count = 0);
    int expanding() const;
    void setExpanding(const int expanding);
    QSize sizeHint() const;
signals:
    void clicked();
protected:
    void mouseReleaseEvent(QMouseEvent *me);
    void paintEvent(QPaintEvent *pe);
private:
    int m_expanding;
};

/** @short Widget displaying the message envelope */
class AddressRowWidget : public QWidget
{
    Q_OBJECT
public:
    AddressRowWidget(QWidget *parent, const QString &description, const QList<Imap::Message::MailAddress> &addresses, MessageView *messageView);
    void addAddresses(const QString &description, const QList<Imap::Message::MailAddress> &addresses, MessageView *messageView);

private slots:
    void toggle();
private:
    AddressRowWidget(const AddressRowWidget &) = delete;
    AddressRowWidget &operator=(const AddressRowWidget &) = delete;
    Expander *m_expander;
    uint m_expandedLength;
};

}

#endif
