/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2013 Caspar Schutijser <caspar@schutijser.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#include <QDateTime>

#ifndef BE_CONTACTED_H
#define BE_CONTACTED_H

class QFileSystemWatcher;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;
class QSortFilterProxyModel;

namespace Ui {
    class Contacts;
    class OneContact;
}

namespace Gui {
class AbookAddressbook;
}

#include <QDialog>
#include <QPixmap>

namespace BE {
    class Field;
    class Contacts : public QDialog {
        Q_OBJECT
    public:
        explicit Contacts(Gui::AbookAddressbook *abook);
        virtual ~Contacts();

        void manageContact(const QString &mail, const QString &prettyName);

    protected:
        bool eventFilter(QObject *o, QEvent *e);
        virtual void closeEvent(QCloseEvent *);
    private slots:
        void addContact();
        void updateFocusPolicy(QWidget *oldFocus, QWidget *newFocus);
        void removeCurrentContact();
        void saveContacts();
        void setContact(const QModelIndex &index);
    private:
        void importPhoto(const QString &path);
        bool setPhoto(const QString &path);
    private:
        QStandardItem *m_currentContact;
        QSortFilterProxyModel *m_sortFilterProxy;
        Ui::Contacts *m_ui;
        Ui::OneContact *m_ui2;
        QPixmap m_incognitoPic;
        Gui::AbookAddressbook *m_abook;
        QList<Field> fields;
        bool m_dirty;
    };
} // namepsace

#endif // BE_CONTACTED_H
