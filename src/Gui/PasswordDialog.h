/*
   Copyright (C) 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>

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

#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>
#include "ui_PasswordDialog.h"

class QAction;
class QKeySequence;

namespace Gui
{

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(QWidget *parent = 0);
    ~PasswordDialog();

    QString password() const;
    static QString getPassword(QWidget *parent, const QString &windowTitle, const QString &description,
                               const QString &password = 0, bool *ok = 0);

protected:
    void showEvent(QShowEvent *event);

    Ui::PasswordDialog ui;
};

} // namespace Gui

#endif // PASSWORDDIALOG_H
