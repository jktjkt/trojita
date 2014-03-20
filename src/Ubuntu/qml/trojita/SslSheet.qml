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

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.Popups 0.1
import QtWebKit 3.0 as Web
Item {
    id: sslItem
    anchors.fill: parent
    property alias htmlText: htmlView.url
    //    property alias titleText: title.text
    signal confirmClicked()
    signal cancelClicked()
    width: parent.width
    height: parent.height 
    Web.WebView{
        id: htmlView
        width: sslItem.width
        height: sslItem.height
    }
    Row{
        anchors.bottom: parent.bottom
        spacing: 1
        Button{
            id: okButton
            text: "Confirm"
            onClicked:{
                sslItem.confirmClicked()
            }
        }
        Button{
            id: cancelButton
            text: "cancel"
            onClicked:{
                sslItem.cancelClicked()
            }
        }
    }
}
