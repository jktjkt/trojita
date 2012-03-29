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
private slots:
    void slotLinkClicked(const QUrl &url);
    void handlePageLoadFinished(bool ok);
};

}

#endif // EMBEDDEDWEBVIEW_H
