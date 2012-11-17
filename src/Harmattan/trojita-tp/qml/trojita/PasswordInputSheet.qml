/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Sheet {
    property alias password: password.text
    property alias authErrorMessage: authFailedMessage.text

    acceptButtonText: qsTr("Login")
    rejectButtonText: qsTr("Cancel")

    content: Column {
        anchors.fill: parent
        anchors.margins: UiConstants.DefaultMargin
        spacing: 10
        Label {
            id: authFailureReason
            visible: false
        }
        Label {
            text: qsTr("Password")
        }
        TextField {
            id: password
            anchors {left: parent.left; right: parent.right;}
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            echoMode: TextInput.Password
        }
        Label {
            id: authFailedMessage
            anchors { left: parent.left; right: parent.right; topMargin: 40; leftMargin: 16; rightMargin: 16 }
            wrapMode: Text.Wrap
        }
    }
}
