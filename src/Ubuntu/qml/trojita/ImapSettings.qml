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
import Ubuntu.Components.ListItems 0.1 as ListItems

Page {
    id: flickable
    visible: false
    title: qsTr("Server Settings")

    property alias imapServer: imapServerInput.text
    property alias imapPort: imapPortInput.text
    property alias imapUserName: imapUserNameInput.text
    property alias imapPassword: imapPasswordInput.text
    property string imapSslMode: useSSLSwitch.checked === true ?  "SSL" : "No"
    property int  imapSslModelIndex: dialogView.currentIndex
    property bool usingSSL

    Column {
        id: col
        spacing: 10
        width: Math.round(parent.width / 1.3)
        height: parent.height - parent.header.height
        y: parent.header.height + units.gu(2)
        anchors.horizontalCenter: parent.horizontalCenter
        Label {
            text: qsTr("Username")
        }
        TextField {
            id: imapUserNameInput
            anchors {
                left: col.left;
                right: col.right;
            }
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: imapPasswordInput
        }

        Label {
            text: qsTr("Password")
        }
        TextField {
            id: imapPasswordInput
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            echoMode: TextInput.Password
            anchors {
                left: parent.left;
                right: parent.right;
            }
            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: imapServerInput
            KeyNavigation.backtab: imapUserNameInput
        }
        Label {
            text: qsTr("Server address")
        }
        TextField {
            id: imapServerInput
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase | Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
            anchors {
                left: col.left
                right: col.right
            }
            KeyNavigation.priority: KeyNavigation.BeforeItem
            KeyNavigation.tab: imapPortInput
            KeyNavigation.backtab: imapPasswordInput
        }

        Label {
            text: qsTr("Port")
        }
        TextField {
            id: imapPortInput
            text: "143"
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 1; top: 65535 }
            anchors {
                left: col.left
                right: col.right
            }
        }
        Row{
            spacing: units.gu(14)
           Label{
                id: sslText
            anchors.verticalCenter: parent.verticalCenter
               text: qsTr("Use SSL")
           }
           Switch{
               id: useSSLSwitch
           }
        }

        Button{
            id: accessButton
            text: qsTr("OK")
            width:  parent.width
            anchors{
                horizontalCenter: parent.horizontalCenter
            }
            onClicked: {
                if (imapAccess.imapModel) {
                // prevent assert failure in
                // ImapAccess::doConnect due to duplicate calls
               return ;
                }
                if (imapSettings.imapServer !== imapAccess.server || imapSettings.imapUserName !== imapAccess.username )
                    imapAccess.nukeCache()
                if (imapSettings.imapServer != imapAccess.server)
                    imapAccess.forgetSslCertificate()
                imapAccess.server = imapSettings.imapServer
                imapAccess.port = imapSettings.imapPort
                imapAccess.username = imapSettings.imapUserName
                if (imapSettings.imapPassword.length)
                    imapAccess.password = imapSettings.imapPassword
                imapAccess.sslMode = imapSettings.imapSslMode
                imapAccess.doConnect()
                appWindow.connectModels()
            }
        }
    }

    Component.onCompleted: {
        imapSettings.imapServer = imapAccess.server
        if (imapAccess.port > 0){
            imapSettings.imapPort = imapAccess.port
            imapSettings.imapUserName = imapAccess.username
        }
        // That's right, we do not load the password

        // FIXME this is not working correctly we can not set the ssl ?
        if (imapAccess.sslMode === "StartTLS"){
            imapSettings.imapSslModeIndex = 2
        }else if (imapAccess.sslMode === "SSL"){
            imapSettings.imapSslModelIndex = 1
            usingSSL = true
        }else{
            imapSettings.imapSslModelIndex = 0
            usingSSL = false
        }
        if(usingSSL === true ){
            useSSLSwitch.checked = true
        }
        if (usingSSL === false ){
            useSSLSwitch.checked = false
        }
    }
}
