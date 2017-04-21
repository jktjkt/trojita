/* Copyright (C) Roland Pallai <dap78@magex.hu>

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

#include "TagAddDialog.h"
#include "ui_TagAddDialog.h"

namespace Gui
{

TagAddDialog::TagAddDialog(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTags) :
    QDialog(parent),
    ui(new Ui::TagAddDialog)
{
    ui->setupUi(this);

    auto tagsLabel = ui->tagsLabel;
    tagsLabel->setTextFormat(Qt::RichText);
    tagsLabel->setOpenExternalLinks(false);
    tagsLabel->setWordWrap(true);

    QStringList tagCloud;
    for (int i = 0; i < m_favoriteTags->rowCount(); i++) {
        auto tagName = m_favoriteTags->tagNameByIndex(i);
        tagCloud.append(QStringLiteral("<a href=\"%1\" style=\"color: %2; text-decoration: none;\">%1</a>")
            .arg(tagName, m_favoriteTags->findBestColorForTags({tagName}).name()));
    }

    tagsLabel->setText(tagCloud.join(QStringLiteral(", ")));

    connect(tagsLabel, &QLabel::linkActivated, this, [=](const QString &link) {
        if (!ui->lineEdit->text().isEmpty())
            ui->lineEdit->setText(ui->lineEdit->text() + QStringLiteral(" "));
        ui->lineEdit->setText(ui->lineEdit->text() + link);
    });
}

TagAddDialog::~TagAddDialog()
{
    delete ui;
}

QStringList TagAddDialog::getTags(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *m_favoriteTags)
{
    return TagAddDialog(parent, m_favoriteTags).getTags();
}

QStringList TagAddDialog::getTags()
{
    QStringList tagList;
    if (exec()) {
        tagList += ui->lineEdit->text().split(QStringLiteral(" "), QString::SkipEmptyParts);
        tagList.removeDuplicates();
    }
    return tagList;
}

}
