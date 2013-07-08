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

#include "PasswordDialog.h"

#include "IconLoader.h"

namespace Gui
{

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setModal(true);
    ui.iconLabel->setPixmap(loadIcon("dialog-password").pixmap(64, 64));
}

PasswordDialog::~PasswordDialog()
{
}

void PasswordDialog::showEvent(QShowEvent *event)
{
    ui.passwordLineEdit->setFocus();
    QDialog::showEvent(event);
}

/***************************************************************************/

QString PasswordDialog::password() const
{
    return ui.passwordLineEdit->text();
}

QString PasswordDialog::getPassword(QWidget *parent, const QString &windowTitle, const QString &description,
                                    const QString &password, bool *ok)
{
    PasswordDialog dialog(parent);
    dialog.setWindowTitle(windowTitle);
    dialog.ui.descriptionLabel->setText(description);
    dialog.ui.passwordLineEdit->setEchoMode(QLineEdit::Password);
    dialog.ui.passwordLineEdit->setText(password);

    int ret = dialog.exec();
    if (ok)
        *ok = !!ret;
    if (ret)
        return dialog.ui.passwordLineEdit->text();
    return QString();
}

} // namespace Gui
