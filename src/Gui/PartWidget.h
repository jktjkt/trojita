/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef GUI_PARTWIDGET_H
#define GUI_PARTWIDGET_H

#include <QFrame>
#include <QTabWidget>

#include "Gui/AbstractPartWidget.h"
#include "Gui/MessageView.h"
#include "Gui/PartWalker.h"
#include "Gui/Spinner.h"
#include "UiUtils/PartLoadingOptions.h"

class QModelIndex;
class QPushButton;

namespace Gui
{

/** @short Message quoting support for multipart/alternative MIME type */
class MultipartAlternativeWidget: public QTabWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    MultipartAlternativeWidget(QWidget *parent, PartWidgetFactory *factory,
                               const QModelIndex &partIndex,
                               const int recursionDepth, const UiUtils::PartLoadingOptions options);
    virtual QString quoteMe() const override;
    virtual void reloadContents() override;
    virtual void zoomIn() override;
    virtual void zoomOut() override;
    virtual void zoomOriginal() override;
protected:
    bool eventFilter(QObject *o, QEvent *e) override;
};

/** @short Widget to display status information when processing message parts */
class PartStatusWidget: public QFrame
{
    Q_OBJECT
public:
    explicit PartStatusWidget(QWidget *parent);

    void showStatus(const QString &icon, const QString &status, const QString &details = QString());

protected slots:
    void showDetails();

private:
    QLabel *m_icon, *m_text, *m_details;
    QFrame *m_seperator;
    QPushButton *m_detailButton;
};

/** @short Base class for handling parts where the structure of the chilren is not known yet */
class AsynchronousPartWidget: public QFrame, public AbstractPartWidget
{
    Q_OBJECT
public:
    AsynchronousPartWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                           const UiUtils::PartLoadingOptions loadingOptions);

protected slots:
    void handleRowsInserted(const QModelIndex &parent, int row, int column);
    void handleLayoutChanged(const QList<QPersistentModelIndex> &parents);
    void handleError(const QModelIndex &parent, const QString &status, const QString &details);
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    virtual void updateStatusIndicator() = 0;

private:
    void buildWidgets();

protected:
    virtual QWidget *addingOneWidget(const QModelIndex &index, UiUtils::PartLoadingOptions options);

    PartStatusWidget *m_statusWidget;
    PartWidgetFactory *m_factory;
    QPersistentModelIndex m_partIndex;
    const int m_recursionDepth;
    const UiUtils::PartLoadingOptions m_options;
};

/** @short Widget for multipart/signed or multipart/encrypted MIME types */
class MultipartSignedEncryptedWidget: public AsynchronousPartWidget
{
    Q_OBJECT
public:
    MultipartSignedEncryptedWidget(QWidget *parent, PartWidgetFactory *factory,
                                   const QModelIndex &partIndex, const int recursionDepth,
                                   const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const override;
    virtual void reloadContents() override;
    virtual void zoomIn() override;
    virtual void zoomOut() override;
    virtual void zoomOriginal() override;
protected slots:
    virtual void updateStatusIndicator() override;
protected:
    virtual QWidget *addingOneWidget(const QModelIndex &index, UiUtils::PartLoadingOptions options) override;
};

/** @short Message quoting support for generic multipart/ * */
class GenericMultipartWidget: public QWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    GenericMultipartWidget(QWidget *parent, PartWidgetFactory *factory,
                           const QModelIndex &partIndex, const int recursionDepth,
                           const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const override;
    virtual void reloadContents() override;
    virtual void zoomIn() override;
    virtual void zoomOut() override;
    virtual void zoomOriginal() override;
};

/** @short Message quoting support for generic multipart/ * */
class Message822Widget: public QWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    Message822Widget(QWidget *parent, PartWidgetFactory *factory,
                     const QModelIndex &partIndex, const int recursionDepth,
                     const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const override;
    virtual void reloadContents() override;
    virtual void zoomIn() override;
    virtual void zoomOut() override;
    virtual void zoomOriginal() override;
};


}

#endif // GUI_PARTWIDGET_H
