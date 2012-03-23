/* Copyright (C) 2012 Mildred <mildred-pub@mildred.fr>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software, you can do what you want with it, including
   changing its license (which is this text right here).
*/

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>

#include "TagListWidget.h"
#include "FlowLayout.h"
#include "TagWidget.h"

namespace Gui {

TagListWidget::TagListWidget(QWidget *parent) :
    QWidget(parent)
{
    parentLayout = new FlowLayout( this );
    setLayout(parentLayout);

    addButton = new TagWidget("+");
    connect(addButton, SIGNAL(clicked()), this, SLOT(newTagRequested()));

    parentLayout->addWidget( new QLabel(tr("<b>Tags:</b>")) );
    parentLayout->addWidget(addButton);

    m_ignoredFlags.insert(QLatin1String("\\seen"));
    m_ignoredFlags.insert(QLatin1String("\\recent"));
    m_ignoredFlags.insert(QLatin1String("\\deleted"));
    m_ignoredFlags.insert(QLatin1String("\\answered"));
    // We do want to show \Flagged, though
    m_ignoredFlags.insert(QLatin1String("\\draft"));
    m_ignoredFlags.insert(QLatin1String("$mdnsent"));
    m_ignoredFlags.insert(QLatin1String("$forwarded"));
    m_ignoredFlags.insert(QLatin1String("$submitpending"));
    m_ignoredFlags.insert(QLatin1String("$submitted"));
}

void TagListWidget::setTagList(QStringList list)
{
    empty();
    parentLayout->removeWidget(addButton);

    foreach(const QString &tagName, list) {
        if (m_ignoredFlags.contains(tagName.toLower()))
            continue;
        TagWidget* lbl = new TagWidget(tagName, "x");
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
    QString tag = QInputDialog::getText( this, tr("New Label"), tr("Label name:") );
    if (!tag.isEmpty())
        emit tagAdded(tag);
}

}
