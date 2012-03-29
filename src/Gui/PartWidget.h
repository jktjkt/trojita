/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef GUI_PARTWIDGET_H
#define GUI_PARTWIDGET_H

#include <QGroupBox>
#include <QTabWidget>

#include "AbstractPartWidget.h"

class QModelIndex;

namespace Gui
{

class PartWidgetFactory;

/** @short Message quoting support for multipart/alternative MIME type */
class MultipartAlternativeWidget: public QTabWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    MultipartAlternativeWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth);
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for multipart/signed MIME type */
class MultipartSignedWidget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    MultipartSignedWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth);
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for generic multipart/ * */
class GenericMultipartWidget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    GenericMultipartWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth);
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for generic multipart/ * */
class Message822Widget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    Message822Widget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth);
    virtual QString quoteMe() const;
    virtual void reloadContents();
};


}

#endif // GUI_PARTWIDGET_H
