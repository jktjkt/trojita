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

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

#include "TagListWidget.h"
#include "FlowLayout.h"
#include "TagWidget.h"
#include "Imap/Model/SpecialFlagNames.h"

namespace Gui
{

TagListWidget::TagListWidget(QWidget *parent) :
    QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    parentLayout = new FlowLayout(this, 0);
    setLayout(parentLayout);

    addButton = TagWidget::addingWidget();
    connect(addButton, &TagWidget::addingClicked, this, &TagListWidget::newTagsRequested);

    unsupportedReservedKeywords.insert(Imap::Mailbox::FlagNames::mdnsent.toLower());
    unsupportedReservedKeywords.insert(Imap::Mailbox::FlagNames::submitted.toLower());
    unsupportedReservedKeywords.insert(Imap::Mailbox::FlagNames::submitpending.toLower());
}

void TagListWidget::setTagList(QStringList list)
{
    empty();
    parentLayout->removeWidget(addButton);

    Q_FOREACH(const QString &tagName, list) {
        QString tagNameLowerCase = tagName.toLower();
        if (Imap::Mailbox::FlagNames::toCanonical.contains(tagNameLowerCase)) {
            if (unsupportedReservedKeywords.contains(tagNameLowerCase)) {
                TagWidget *lbl = TagWidget::systemFlag(tagName);
                parentLayout->addWidget(lbl);
                children << lbl;
            } else {
                continue;
            }
        } else {
            TagWidget *lbl = TagWidget::userKeyword(tagName);
            parentLayout->addWidget(lbl);
            connect(lbl, &TagWidget::removeClicked, this, &TagListWidget::tagRemoved);
            children << lbl;
        }
    }

    parentLayout->addWidget(addButton);
}

void TagListWidget::empty()
{
    qDeleteAll(children.begin(), children.end());
    children.clear();
}

void TagListWidget::newTagsRequested()
{
    QString tags = QInputDialog::getText(this, tr("New Tags"), tr("Tag names (space-separated):"));

    // Check whether any text has been entered
    if (tags.isEmpty()) {
        return;
    }

    // Check whether reserved keywords have been entered
    QStringList tagList = tags.split(QStringLiteral(" "), QString::SkipEmptyParts);
    tagList.removeDuplicates();
    QStringList reservedTagsList = QStringList();
    for (QStringList::const_iterator it = tagList.constBegin(); it != tagList.constEnd(); ++it) {
        if (Imap::Mailbox::FlagNames::toCanonical.contains(it->toLower())) {
            reservedTagsList << *it;
        }
    }
    if (!reservedTagsList.isEmpty()) {
        QMessageBox::warning(this, tr("Disallowed tag value"),
                             tr("No tags were set because the following given tag(s) are reserved and have been disallowed from being set in this way: %1.").arg(reservedTagsList.join(QStringLiteral(", "))));
        return;
    }

    emit tagAdded(tags);
}

}
