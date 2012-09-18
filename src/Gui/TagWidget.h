/* Copyright (C) 2012 Mildred <mildred-pub@mildred.fr>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software, you can do what you want with it, including
   changing its license (which is this text right here).
*/

#ifndef GUI_TAGWIDGET_H
#define GUI_TAGWIDGET_H

#include <QLabel>

namespace Gui
{

class TagWidget : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(QString tagName READ tagName USER true)
public:
    explicit TagWidget(const QString &buttonText, QWidget *parent=0, Qt::WindowFlags f=0);
    explicit TagWidget(const QString &tagName, const QString &buttonText, QWidget *parent=0, Qt::WindowFlags f=0);

    QString tagName() const;

    bool event(QEvent *e);

signals:
    void removeClicked(QString);
    void clicked();

private:
    QString m_tagName;
    void commonInit();
};

} // namespace Gui

#endif // GUI_TAGWIDGET_H
