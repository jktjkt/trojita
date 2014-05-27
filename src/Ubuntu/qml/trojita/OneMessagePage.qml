/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>

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
import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItem
import "Utils.js" as Utils

Page {
    id: oneMessagePage
    title: imapAccess.oneMessageModel.subject
    property url url: messageView.url

    tools:  ToolbarItems {
        id: oneMailTools
    }

    flickable: view

    Flickable {
        id: view
        anchors.fill: parent
        contentHeight: plainTextView.height + header.height + attachmentsView.height + units.gu(5)
        clip: true
        HeaderComponent {
            id: header
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            onContactNameClicked: console.log(
                                      "Nothing implemented to handle clicking a contact yet, Patches welcome :-)"
                                      )
            onEmailAddressClicked: console.log(
                                       "Nothing implemented to handle clicking an email address yet, Patches welcome :-)"
                                       )
        }
        TrojitaWebView {
            id: plainTextView
            anchors {
                left: parent.left
                leftMargin: units.gu(2)
                top: header.bottom
                topMargin: units.gu(2)
                right: parent.right
                rightMargin: units.gu(2)
            }
            url: imapAccess.oneMessageModel.mainPartUrl
//          NOTE: disable url & uncomment to show loading html formatted plaintext sets correct height
//            Component.onCompleted: {
//                plainTextView.loadHtml("<html><head></head><body><p>Hello Trojitá</p></body></html>")
//            }

        }
        // FIXME: move this to a dedicated page...
        Component {
            id: attachmentItemDelegate
            ListItem.Standard {

                text: "Attachment " + (model.fileName.length ? model.fileName + " " : "") + "(" + model.mimeType +
                      (model.size ?
                           "): " + imapAccess.prettySize(model.size) :
                           ")")
                // TODO: use correct icon for mime type
                iconSource: Qt.resolvedUrl("./attach.svg")
                iconFrame: false
                showDivider: false
            }
        }

        ListView {
            id: attachmentsView
            anchors.top: plainTextView.bottom
            width: view.width
            interactive: false
            height: count * 40 + 30
            // FIXME: filter out the main part from the view (in C++, of course)
            model: imapAccess.oneMessageModel.attachmentsModel
            delegate: attachmentItemDelegate
        }
    }
}
