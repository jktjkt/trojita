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
#ifndef EMBEDDEDWEBVIEW_H
#define EMBEDDEDWEBVIEW_H

#include <QWebPluginFactory>
#include <QWebView>

namespace Gui
{


/** @short An embeddable QWebView with some safety checks and modified resizing

  This class configures the QWebView in such a way that it will prevent certain
  dangerous (or unexpected, in the context of a MUA) features from being invoked.

  Another function is to configure the QWebView in such a way that it resizes
  itself to show all required contents.

  Note that you still have to provide a proper eventFilter in the parent widget
  (and register it for use).

  @see Gui::MessageView

  */
class EmbeddedWebView: public QWebView
{
    Q_OBJECT
public:
    EmbeddedWebView(QWidget *parent, QNetworkAccessManager *networkManager);
    QSize sizeHint() const;
protected:
    void changeEvent(QEvent *e);
    bool eventFilter(QObject *o, QEvent *e);
    void showEvent(QShowEvent *se);
private:
    void constrainSize();
    void findScrollParent();
private slots:
    void slotLinkClicked(const QUrl &url);
    void handlePageLoadFinished();
private:
    QWidget *m_scrollParent;
    int m_scrollParentPadding;
    int m_resizeInProgress;
};

class ErrorCheckingPage: public QWebPage
{
    Q_OBJECT
public:
    explicit ErrorCheckingPage(QObject *parent);

    virtual bool extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output);
    virtual bool supportsExtension(Extension extension) const;
};

}

#endif // EMBEDDEDWEBVIEW_H
