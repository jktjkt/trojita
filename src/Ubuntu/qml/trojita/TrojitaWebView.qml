/* Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>

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
import QtQuick 2.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0
import "Utils.js" as Utils
import trojita.UiFormatting 0.1
import Ubuntu.Components 1.1

WebView {
    id: trojitaWebView

    property color linkColor: "#0000FF";
    property color linkClickedColor: "#FF00FF";

    //Set the height to document height
    // FIXME: changing the window size will cause this to duplicate
    //        obviously this isn't an issue on phablet devices
    //        which is our target device at this time.
    height: experimental.page.height

    experimental {
        preferences {
            navigatorQtObjectEnabled: false
            developerExtrasEnabled: true
            javascriptEnabled: false
        }
        preferredMinimumContentsWidth: 0

        // By setting this to false we disable the webviews flickable
        // and we have to bind contentWidth/Height accordingly.
        useDefaultContentItemSize: false


        urlSchemeDelegates: [
            UrlSchemeDelegate {
                scheme: "trojita-imap"
                onReceivedRequest: {
                    console.log('trojita-imap scheme called')
                    imapAccess.msgQNAM.wrapQmlWebViewRequest(request, reply)
                }
            }
        ]
    }

    Label {
        id: labelForFont
    }

    Component.onCompleted: loadingChanged.connect(htmlizeText)
    function htmlizeText() {
        if (!loading) {
            loadHtml(UiFormatting.htmlizedTextPart(
                         imapAccess.oneMessageModel.mainPartModelIndex,
                         labelForFont.font,
                         Theme.palette.normal.background,
                         Theme.palette.normal.fieldText,
                         linkColor,
                         linkClickedColor));
            loadingChanged.disconnect(htmlizeText);
        }
    }
}
