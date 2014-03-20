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

Item {
    id: passRoot
    width: parent.width
    height: parent.height
    property string password: password.text
    property alias authErrorMessage: authFailedMessage.text
    signal confirmClicked()
    signal cancelClicked()
    Column {
        width: parent.width
        height: parent.height
        anchors.centerIn: parent
        spacing: 10
        Label {
            id: authFailureReason
            visible: false
        }
        Label {
            text: qsTr("Password")
            fontSize: "large"
        }
        TextField {
            id: password
            anchors {left: parent.left; right: parent.right;}
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            echoMode: TextInput.Password
        }
        Label {
            id: authFailedMessage
            onTextChanged: PopupUtils.open(authErrorPopup)
            anchors {
                left: parent.left;
                right: parent.right;
                topMargin: 40;
                leftMargin: 16;
                rightMargin: 16
            }
        }
        Row {
            spacing: 12
            anchors.horizontalCenter: parent.horizontalCenter
            Button{
                id: okButton
                text: "Confirm"
                onClicked:{
                    passRoot.confirmClicked()
                }
            }
            Button{
                id: cancelButton
                text: "Cancel"
                onClicked:{
                    passRoot.cancelClicked()
                }
            }
        }
    }
    Component{
        id: authErrorPopup
        Dialog{
            id: authErrorDialog
            title: authFailureReason
            text:  authErrorMessage


            Column{
                spacing: units.gu(1)
                Button{
                    id:closePopUp
                    width: parent.width
                    text: "try again"
                    onClicked: {
                        // TODO FLUSH cache and what not and try to login again
                        PopupUtils.close(authErrorDialog)
                    }
                }
                Button{
                    id: filebugButton
                    width: parent.width
                    text: qsTr("File bug")
                    onClicked: {
                        PopupUtils.close(authErrorDialog)
                        // TODO MAKE A pagestack that returns the bug tracker
                        Qt.openUrlExternally("https://bugs.kde.org/enter_bug.cgi?product=trojita&format=guided")
                    }
                }
            }
        }
    }
}
