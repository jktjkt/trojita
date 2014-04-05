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
import Ubuntu.Components 0.1
import Ubuntu.Components.Popups 0.1

ComposerSheet {
    id: passRoot

    property alias message: message.text
    property alias authErrorMessage: authFailedMessage.text
    property alias settingsMessage: settingsMessage.text

    Column {
        id: rootColumn
        anchors.fill: parent
        anchors.margins: units.gu(3)
        spacing: units.gu(2)

        Item {
            id: pwdPrompt
            width: parent.width

            Image {
                id: keyIcon
                source: Qt.resolvedUrl("./key.svg")
                height: units.gu(10)
                width: height
            }

            Label {
                id: message
                visible: false
                anchors {
                    top: parent.top
                    left: keyIcon.right
                    right: parent.right
                    leftMargin: units.gu(3)
                }
                color: "#333333"
                onTextChanged: visible = true
                wrapMode: TextEdit.Wrap
            }

            TextField {
                id: password
                anchors {
                    left: keyIcon.right
                    top: message.bottom
                    topMargin: units.gu(2)
                    leftMargin: units.gu(3)
                    right: parent.right
                }
                placeholderText: qsTr("Enter password")
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                echoMode: TextInput.Password

            }

            Label {
                id: authFailedMessage
                anchors {
                    left: parent.left
                    right: parent.right
                    top: password.bottom
                    topMargin: units.gu(2)
                }
                color: "red"
                wrapMode: TextEdit.Wrap
            }

            Label {
                id: settingsMessage
                visible: false
                onTextChanged: visible = true
                width: parent.width
                anchors {
                    left: parent.left
                    right: parent.right
                    top: authFailedMessage.bottom
                    topMargin: units.gu(2)
                }
                color: "#333333"
                wrapMode: TextEdit.Wrap
            }
        }
    }

    onConfirmClicked: {
        imapAccess.imapModel.imapPassword = password.text
    }

    onCancelClicked: {
        imapAccess.imapModel.imapPassword = undefined
        pageStack.push(imapSettings)
    }
}
