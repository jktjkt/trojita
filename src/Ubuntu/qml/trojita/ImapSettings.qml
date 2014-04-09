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
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItems

Page {
    id: imapSettings
    visible: false
    title: qsTr("Server Settings")

    signal imapSettingsChanged

    property alias imapUserName: imapUserNameInput.text
    property alias imapServer: imapServerInput.text
    property alias imapPort: imapPortInput.text
    property alias imapPassword: imapPasswordInput.text
    property alias imapSslModelIndex: connectionSelector.selectedIndex
    property bool inputValid: usernameLabel.valid && hostLabel.valid
    property bool settingsModified: false

    function setupConnections() {
        settingsModified = false
        imapAccess.serverChanged.connect(settingModified)
        imapAccess.usernameChanged.connect(settingModified)
        imapAccess.portChanged.connect(settingModified)
        imapAccess.connMethodChanged.connect(settingModified)
        imapSettingsChanged.connect(appWindow.settingsChanged)
    }

    function settingModified() { settingsModified = true }

    function saveImapSettings() {
        if (imapSettings.imapServer !== imapAccess.server || imapSettings.imapUserName !== imapAccess.username) {
            imapAccess.nukeCache()
        }
        if (imapSettings.imapServer !== imapAccess.server) {
            imapAccess.forgetSslCertificate()
            imapAccess.server = imapSettings.imapServer
        }
        if (imapSettings.port !== imapAccess.port) {
            imapAccess.port = imapSettings.imapPort
        }
        if (imapSettings.imapUserName !== imapAccess.username) {
            imapAccess.username = imapSettings.imapUserName
        }
        if (connectionSelector.model.get(connectionSelector.selectedIndex).name !== imapAccess.sslMode) {
            imapAccess.sslMode = connectionSelector.model.get(connectionSelector.selectedIndex).name
        }
        if (imapPasswordInput.text.length > 0) {
            imapAccess.password = imapPasswordInput.text
        }
    }

    states: [
        State {
            name: "NO_ACCOUNT"
            // Ensure the UI is clean
            PropertyChanges { target: imapSettings; imapServer: "" }
            PropertyChanges { target: imapServerInput; placeholderText: "" }
            PropertyChanges { target: imapSettings; imapUserName: "" }
            PropertyChanges { target: imapUserNameInput; placeholderText: "" }
            PropertyChanges { target: imapSettings; imapPassword: "" }
            PropertyChanges { target: imapPasswordInput; placeholderText: "" }
            PropertyChanges { target: imapSettings; imapSslModelIndex: 0 }
            PropertyChanges {
                target: imapSettings
                imapPort: connectionSelector.model.get(connectionSelector.selectedIndex).port
            }
        },
        State {
            name: "HAS_ACCOUNT"
            // Load user details
            PropertyChanges { target: imapSettings; imapServer: imapAccess.server }
            PropertyChanges { target: imapSettings; imapUserName: imapAccess.username }
            PropertyChanges { target: imapSettings; imapPassword: "" }
            PropertyChanges {
                target: imapSettings
                imapSslModelIndex: {
                    switch (imapAccess.sslMode) {
                    case "StartTLS":
                        1
                        break
                    case "SSL":
                        2
                        break
                    default:
                        0
                    }
                }
            }
            PropertyChanges {
                target: imapSettings
                imapPort: imapAccess.port ?
                              imapAccess.port :
                              connectionSelector.model.get(connectionSelector.selectedIndex).port
            }
        }
    ]

    Flickable {
        anchors.fill: parent

        Column {
            id: col
            spacing: units.gu(1)
            anchors.fill: parent
            anchors.margins: units.gu(3)
            anchors.horizontalCenter: parent.horizontalCenter

            Label {
                id: usernameLabel
                property bool valid: !!imapUserNameInput.text
                text: valid ? qsTr("Username") : qsTr("Username required")
                color: valid ? "#888888" : "red"
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
                id: hostLabel
                property bool valid: !!imapServerInput.text
                text: valid ? qsTr("Server address") : qsTr("Server address required")
                color: valid ? "#888888" : "red"
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

            OptionSelector {
                id: connectionSelector
                text: qsTr("Secure Connection")
                model: encryptionOptionsModel
                showDivider: false
                expanded: false
                colourImage: true
                delegate: connectionSelectorDelegate
                selectedIndex: imapSslModelIndex
                onSelectedIndexChanged: {
                    imapPortInput.text = model.get(connectionSelector.selectedIndex).port
                }
            }

            Component {
                id: connectionSelectorDelegate
                OptionSelectorDelegate { text: qsTr(description) }
            }

            ListModel {
                id: encryptionOptionsModel
                /*
                   It would appear that we cannot call qsTr() on a ListElement property

                   for ref see https://bugreports.qt-project.org/browse/QTBUG-11403
                      and also https://bugreports.qt-project.org/browse/qtbug-16289

                   It's to do with the way binding loops bla bla bla.....

                   Anyway we have to use QT_TR_NOOP which looks like a nasty hack with
                   warnings about using the 'new' keyword with a function that starts
                   with an Uppercase letter.

                   Surely they can do something better than this?
                */
                ListElement { name: "No"; description: QT_TR_NOOP("No Encryption"); port: 143 }
                ListElement { name: "StartTLS"; description: QT_TR_NOOP("Use encryption (STARTTLS)"); port: 143 }
                ListElement { name: "SSL"; description: QT_TR_NOOP("Force encryption (TLS)"); port: 993 }
            }

            Label {
                text: qsTr("Port")
            }
            TextField {
                id: imapPortInput
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
                anchors {
                    left: col.left
                    right: col.right
                }

            }
        }
    }

    Component.onCompleted: {
        imapAccess.sslMode ? state = "HAS_ACCOUNT" : state = "NO_ACCOUNT"
        setupConnections()
    }
}
