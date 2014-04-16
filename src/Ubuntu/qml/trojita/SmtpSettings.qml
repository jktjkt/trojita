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
import Ubuntu.Components.ListItems 0.1 as ListItems
import Ubuntu.Components.Popups 0.1
import trojita.MSA.Account 0.1

Page {
    id: smtpAccountSettingsPage

    property alias smtpUserName: smtpUserNameInput.text
    property alias smtpServer: smtpServerInput.text
    property alias smtpPort: smtpPortInput.text
    property alias smtpPassword: smtpPasswordInput.text
    property alias smtpAuthenticate: authCheckBox.checked
    property alias smtpBurl: burlCheckbox.checked
    property alias smtpSaveToImap: outgoingCheckbox.checked
    property alias smtpSentLocation: smtpSentLocationInput.text
    property alias smtpPathToSendmail: smtpSendMailInput.text
    property alias smtpConnectionModelIndex: connectionSelector.selectedIndex
    // FIXME: default to encryption, similar to how the DesktopGui does that
    property alias smtpEncryptionModelIndex: encryptionSelector.selectedIndex
    //TODO: what about sendmail and imapsendmail required values??
    property bool settingsValid: {
        if (smtpConnectionModelIndex === 0) {
            if (smtpAuthenticate) {
                return smtpServerLabel.valid && smtpUsername.valid
            } else {
                return smtpServerLabel.valid
            }
        } else {
            return true
        }
    }


    function connectSignals() {
        smtpAccountSettings.showPortWarning.connect(function(message) { smtpPortWarnLabel.text = message })
    }

    onSmtpConnectionModelIndexChanged: {

        switch (connectionSelector.model.get(connectionSelector.selectedIndex).name) {
        case "NETWORK":
            smtpAccountSettingsPage.state = "NETWORK_SETTINGS"
            smtpAccountSettings.submissionMethod = encryptionSelector.model.get(encryptionSelector.selectedIndex).method
            break
        case "SENDMAIL":
            smtpAccountSettingsPage.state = "SENDMAIL_SETTINGS"
            smtpAccountSettings.submissionMethod = MSAAccount.SENDMAIL
            break
        case "IMAP_SENDMAIL":
            smtpAccountSettingsPage.state = "IMAP_SENDMAIL_SETTINGS"
            smtpAccountSettings.submissionMethod = MSAAccount.IMAP_SENDMAIL
            break
        }
    }

    onSmtpEncryptionModelIndexChanged: {
        smtpAccountSettings.submissionMethod = encryptionSelector.model.get(encryptionSelector.selectedIndex).method

    }

    states: [
        State {
            name: "NETWORK_SETTINGS"
            PropertyChanges { target: smtpAccountSettingsPage; smtpConnectionModelIndex: 0 }
            PropertyChanges { target: smtpServerLabel; visible: true }
            PropertyChanges { target: smtpPortLabel; visible: !smtpPortWarnLabel.visible }
            PropertyChanges { target: authItem; visible: true }
        },
        State {
            name: "SENDMAIL_SETTINGS"
            PropertyChanges { target: smtpAccountSettingsPage; smtpConnectionModelIndex: 1 }
            PropertyChanges { target: smtpUsername; visible: false }
            PropertyChanges { target: smtpServerLabel; visible: false }
            PropertyChanges { target: smtpPortLabel; visible: false }
            PropertyChanges { target: smtpPortWarnLabel; visible: false }
            PropertyChanges { target: smtpSendMail; visible: true }
            PropertyChanges { target: authItem; visible: false }
            PropertyChanges { target: authItem; checked: false }
            PropertyChanges { target: burlItem; visible: false }
        },
        State {
            name: "IMAP_SENDMAIL_SETTINGS"
            PropertyChanges { target: smtpAccountSettingsPage; smtpConnectionModelIndex: 2 }
            PropertyChanges { target: smtpUsername; visible: false }
            PropertyChanges { target: smtpServerLabel; visible: false }
            PropertyChanges { target: smtpPortLabel; visible: false }
            PropertyChanges { target: smtpPortWarnLabel; visible: false }
            PropertyChanges { target: smtpSendMail; visible: false }
            PropertyChanges { target: authItem; visible: false }
            PropertyChanges { target: authItem; checked: false }
            PropertyChanges { target: burlItem; visible: false }
        }
    ]

    flickable: null
    Flickable {
        anchors.fill: parent
        clip: true
        contentHeight: col.height + units.gu(5)

        Column {
            id: col
            spacing: units.gu(1)
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: units.gu(2)
            }

            OptionSelector {
                id: connectionSelector
                text: qsTr("Connection Method")
                model: connectionMethodModel
                showDivider: false
                expanded: false
                colourImage: true
                delegate: connectionMethodDelegate
            }
            Component {
                id: connectionMethodDelegate
                OptionSelectorDelegate { text: qsTr(description) }
            }
            ListModel {
                id: connectionMethodModel
                ListElement { name: "NETWORK"; description: QT_TR_NOOP("Network") }
                ListElement { name: "SENDMAIL"; description: QT_TR_NOOP("Local sendmail-compatible") }
                ListElement { name: "IMAP_SENDMAIL"; description: QT_TR_NOOP("IMAP SENDMAIL Extension") }
            }

            OptionSelector {
                id: encryptionSelector
                text: qsTr("Encryption")
                visible: connectionSelector.selectedIndex === 0
                model: encryptionMethodModel
                showDivider: false
                expanded: false
                colourImage: true
                delegate: encryptionMethodDelegate
            }
            Component {
                id: encryptionMethodDelegate
                OptionSelectorDelegate { text: qsTr(description) }
            }
            ListModel {
                id: encryptionMethodModel
                ListElement { description: QT_TR_NOOP("No encryption"); method: MSAAccount.SMTP }
                ListElement { description: QT_TR_NOOP("Use encryption (STARTTLS)"); method: MSAAccount.SMTP_STARTTLS }
                ListElement { description: QT_TR_NOOP("Force encryption (TLS)"); method: MSAAccount.SSMTP }
            }

            Label {
                id: smtpServerLabel
                property bool valid: !!smtpServerInput.text
                text: valid ? qsTr("Server address") : qsTr("Server address required")
                color: valid ? "#888888" : "red"
            }
            TextField {
                id: smtpServerInput
                visible: smtpServerLabel.visible
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase | Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                anchors {
                    left: col.left
                    right: col.right
                }
                Binding { target: smtpAccountSettings; property: "server"; value: smtpServerInput.text }
            }

            Label {
                id: smtpPortLabel
                visible: true
                text: qsTr("Port")
            }
            Label {
                id: smtpPortWarnLabel
                color: "red"
                visible: smtpPortWarnLabel.text
            }
            TextField {
                id: smtpPortInput
                visible: smtpPortLabel.visible || smtpPortWarnLabel.visible
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
                anchors {
                    left: col.left
                    right: col.right
                }
                Binding { target: smtpAccountSettings; property: "port"; value: smtpPortInput.text }
            }

            Item {
                id: authItem
                width: parent.width
                height: smtpPortInput.height
                property alias checked: authCheckBox.checked
                Label {
                    text: qsTr("Authenticate")
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
                CheckBox {
                    id: authCheckBox
                    anchors.right: parent.right
                }
                Binding { target: smtpAccountSettings; property: "authenticateEnabled"; value: authCheckBox.checked }
            }

            Label {
                id: smtpUsername
                visible: authCheckBox.checked
                property bool valid: !!smtpUserNameInput.text
                text: valid ? qsTr("Username") : qsTr("Username required")
                color: valid ? "#888888" : "red"
            }
            TextField {
                id: smtpUserNameInput
                visible: smtpUsername.visible
                anchors {
                    left: col.left;
                    right: col.right;
                }
                Binding { target: smtpAccountSettings; property: "username"; value: smtpUserNameInput.text }
            }

            Label {
                id: smtpPassword
                visible: smtpUsername.visible
                text: qsTr("Password")
            }
            TextField {
                id: smtpPasswordInput
                visible: smtpPassword.visible
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                echoMode: TextInput.Password
                anchors {
                    left: parent.left;
                    right: parent.right;
                }
                Binding {
                    target: smtpAccountSettings
                    property: "password"
                    value: smtpPasswordInput.text
                    when: smtpPasswordInput.text.length > 0
                }
            }

            Label {
                id: smtpSendMail
                visible: false
                text: qsTr("Sendmail executable")
            }
            TextField {
                id: smtpSendMailInput
                visible: smtpSendMail.visible
                anchors {
                    left: col.left;
                    right: col.right;
                }
                Binding { target: smtpAccountSettings; property: "pathToSendmail"; value: smtpSendMailInput.text }

            }

            Item {
                id: saveOutgoingItem
                width: parent.width
                height: smtpPortInput.height
                property alias checked: outgoingCheckbox.checked
                Label {
                    text: qsTr("Save outgoing")
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
                CheckBox {
                    id: outgoingCheckbox
                    anchors.right: parent.right
                }
                Binding { target: smtpAccountSettings; property: "saveToImap"; value: outgoingCheckbox.checked }
            }

            Label {
                id: smtpSentLocation
                visible: saveOutgoingItem.checked
                text: qsTr("Sent folder name")
            }
            TextField {
                id: smtpSentLocationInput
                visible: smtpSentLocation.visible
                anchors {
                    left: parent.left;
                    right: parent.right;
                }
                Binding { target: smtpAccountSettings; property: "sentMailboxName"; value: smtpSentLocationInput.text }
            }

            Item {
                id: burlItem
                width: parent.width
                height: smtpPortInput.height
                visible: saveOutgoingItem.checked
                Label {
                    text: qsTr("Send with BURL")
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
                CheckBox {
                    id: burlCheckbox
                    anchors.right: parent.right
                }
                Binding { target: smtpAccountSettings; property: "useBurl"; value: burlCheckbox.checked }
            }
        }
    }

    Component.onCompleted: {
        connectSignals()
        if (smtpAccountSettings.submissionMethod) {
            switch (smtpAccountSettings.submissionMethod){
            case MSAAccount.SMTP:
            case MSAAccount.SMTP_STARTTLS:
            case MSAAccount.SSMTP:
                state = "NETWORK_SETTINGS"
                break
            case MSAAccount.SENDMAIL:
                state = "SENDMAIL_SETTINGS"
                break
            case MSAAccount.IMAP_SENDMAIL:
                state = "IMAP_SENDMAIL_SETTINGS"
                break
            }
        }
    }
}
