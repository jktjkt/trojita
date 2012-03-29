/* Copyright (C) 2012 Mildred <mildred-pub@mildred.fr>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software, you can do what you want with it, including
   changing its license (which is this text right here).
*/

#ifndef TAGLISTWIDGET_H
#define TAGLISTWIDGET_H

#include <QList>
#include <QSet>
#include <QWidget>

class QPushButton;

namespace Gui
{

class FlowLayout;
class TagWidget;

class TagListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TagListWidget(QWidget *parent = 0);
    void setTagList(QStringList list);

signals:
    void tagAdded(QString tag);
    void tagRemoved(QString tag);

private slots:
    void newTagRequested();

private:
    FlowLayout *parentLayout;
    TagWidget *addButton;
    QList<QObject *> children;
    QSet<QString> m_ignoredFlags;

    void empty();
};

}

#endif // TAGLISTWIDGET_H
