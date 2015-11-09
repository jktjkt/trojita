/* Copyright (C) 2012 Mildred <mildred-pub@mildred.fr>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software, you can do what you want with it, including
   changing its license (which is this text right here).
*/

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

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

    addButton = new TagWidget(QLatin1String("+"));
    connect(addButton, SIGNAL(clicked()), this, SLOT(newTagRequested()));

    parentLayout->addWidget(new QLabel(tr("<b>Tags:</b>")));
    parentLayout->addWidget(addButton);
}

void TagListWidget::setTagList(QStringList list)
{
    empty();
    parentLayout->removeWidget(addButton);

    foreach(const QString &tagName, list) {
        if (Imap::Mailbox::FlagNames::toCanonical.contains(tagName.toLower()))
            continue;
        TagWidget *lbl = new TagWidget(tagName, QLatin1String("x"));
        parentLayout->addWidget(lbl);
        connect(lbl, SIGNAL(removeClicked(QString)), this, SIGNAL(tagRemoved(QString)));

        children << lbl;
    }

    parentLayout->addWidget(addButton);
}

void TagListWidget::empty()
{
    qDeleteAll(children.begin(), children.end());
    children.clear();
}

void TagListWidget::newTagRequested()
{
    QString tag = QInputDialog::getText(this, tr("New Tag"), tr("Tag name:"));
    if (tag.isEmpty()) {
        return;
    }
    if (Imap::Mailbox::FlagNames::toCanonical.contains(tag.toLower())) {
        QMessageBox::warning(this, tr("Invalid tag value"),
                             tr("Tag name %1 is a reserved name which cannot be manipulated this way.").arg(tag));
        return;
    }

    emit tagAdded(tag);
}

}
