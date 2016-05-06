/*
   Copyright (C) 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#include "UiUtils/IconLoader.h"

namespace Gui
{

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    ui.setupUi(this);
    setModal(true);
    ui.iconLabel->setPixmap(UiUtils::loadIcon(QStringLiteral("dialog-password")).pixmap(64, 64));
}

PasswordDialog::~PasswordDialog()
{
}

void PasswordDialog::showEvent(QShowEvent *event)
{
    ui.passwordLineEdit->setFocus();
    QDialog::showEvent(event);
}

PasswordDialog *PasswordDialog::getPassword(QWidget *parent, const QString &windowTitle, const QString &description,
                                            const QString &errorMessage)
{
    auto d = new PasswordDialog(parent);
    d->setWindowTitle(windowTitle);
    d->ui.descriptionLabel->setText(description);
    d->ui.passwordLineEdit->setEchoMode(QLineEdit::Password);

    // fight the word wrapping beast, also see below
    int l,r,t,b; // we're gonna need the horizontal margins
    d->ui.verticalLayout->getContentsMargins(&l,&t,&r,&b);
    QList<QLabel*> fixedLabels; // and the labels we adjusted

    // 1. fix the dialog width, assuming to be wanted.
    d->setMinimumWidth(d->width());
    // 2. fix the label width
    fixedLabels << d->ui.descriptionLabel;
    // NOTICE: d->ui.descriptionLabel is inside a grid layout, which however has 0 margins
    d->ui.descriptionLabel->setMinimumWidth(d->width() - (l+r));
    // 3. have QLabel figure the size for that width and the content
    d->ui.descriptionLabel->adjustSize();
    // 4. make the label a fixed size element
    d->ui.descriptionLabel->setFixedSize(d->ui.descriptionLabel->size());
    d->adjustSize();

    if (!errorMessage.isEmpty()) {
        QLabel *errorLabel = new QLabel(d);
        d->ui.verticalLayout->insertWidget(0, errorLabel);
        errorLabel->setWordWrap(true);
        errorLabel->setText(errorMessage + QLatin1String("\n<hr>"));
        errorLabel->setTextFormat(Qt::RichText);

        // wordwrapping labels are a problem of its own
        fixedLabels << errorLabel;
        errorLabel->setMinimumWidth(d->width() - (l+r));
        errorLabel->adjustSize();
        errorLabel->setFixedSize(errorLabel->size());
    }

    d->adjustSize();
    d->setMinimumWidth(0);
    foreach(QLabel *label, fixedLabels) {
        label->setMinimumSize(0, 0);
        label->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    }

    connect(d, &PasswordDialog::accepted, d, [d]() { emit d->gotPassword(d->ui.passwordLineEdit->text()); });

    d->show();

    return d;
}

} // namespace Gui
