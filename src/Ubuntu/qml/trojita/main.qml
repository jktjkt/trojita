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
import Ubuntu.Components.ListItems 0.1 as ListItems

MainView{
    id: appWindow
    objectName: "appWindow"
    applicationName: "net.flaska.trojita"
    automaticOrientation: true
    anchorToKeyboard: true
    // resize for target device
    Component.onCompleted: {
        if (tablet) {
            width = units.gu(100);
            height = units.gu(75);
        } else if (phone) {
            width = units.gu(40);
            height = units.gu(75);
        }
    }

    property bool networkOffline: true
    property Item fwdOnePage: null

    function showImapError(message) {
        PopupUtils.open(Qt.resolvedUrl("InfoDialog.qml"), appWindow, {
                            title: qsTr("Error"),
                            text: message
                        })
    }

    function showNetworkError(message) {
        PopupUtils.open(Qt.resolvedUrl("InfoDialog.qml"), appWindow, {
                            title: qsTr("Network Error"),
                            text: message
                        })
    }

    function showImapAlert(message) {
        PopupUtils.open(Qt.resolvedUrl("InfoDialog.qml"), appWindow, {
                            title: qsTr("Server Message"),
                            text: message
                        })
    }

    function requestingPassword() {
        PopupUtils.open(passwordDialogComponent)
    }

    function authAttemptFailed(message) {
        passwordInput.title = qsTr("<font color='#333333'>Authentication Error</font>")
        passwordInput.authMessage = message
        passwordInput.settingsMessage = qsTr("Try re-entering account password or click cancel to go to Settings")
    }

    function connectModels() {
        imapAccess.imapModel.imapError.connect(showImapError)
        imapAccess.imapModel.networkError.connect(showNetworkError)
        imapAccess.imapModel.alertReceived.connect(showImapAlert)
        imapAccess.imapModel.authRequested.connect(requestingPassword)
        imapAccess.imapModel.authAttemptFailed.connect(authAttemptFailed)
        imapAccess.imapModel.networkPolicyOffline.connect(function() {networkOffline = true})
        imapAccess.imapModel.networkPolicyOnline.connect(function() {networkOffline = false})
        imapAccess.imapModel.networkPolicyExpensive.connect(function() {networkOffline = false})
        imapAccess.checkSslPolicy.connect(function() {PopupUtils.open(sslSheetPage)})
    }

    function showHome() {
        pageStack.push(mailboxList)
        mailboxList.nestingDepth = 0
        mailboxList.currentMailbox = ""
        mailboxList.currentMailboxLong = ""
        if (mailboxList.model)
            mailboxList.model.setOriginalRoot()
    }

    Component{
        id: sslSheetPage
        SslSheet {
            id: sslSheet
            title:  imapAccess.sslInfoTitle
            htmlText: imapAccess.sslInfoMessage
            onConfirmClicked: imapAccess.setSslPolicy(true)
            onCancelClicked: imapAccess.setSslPolicy(false)
        }
    }

    Item {
        id: passwordInput
        // AWESOME!!, a bit of html tagging to the rescue
        property string title: qsTr("<font color='#333333'>Authentication Required</font>")
        property string authMessage
        property string settingsMessage
        Component {
            id: passwordDialogComponent
            PasswordInputSheet {
                id: passwordInputSheet
                title: passwordInput.title
                message: qsTr("Please provide IMAP password for user <b>%1</b> on <b>%2</b>:").arg(
                                     imapAccess.username).arg(imapAccess.server)
                authErrorMessage: passwordInput.authMessage
                settingsMessage: passwordInput.settingsMessage
            }
        }
    }

    PageStack{
        id:pageStack
        Component.onCompleted: pageStack.push(imapSettings)

        // Get the Users name and details
        ImapSettings {
            id: imapSettings
            anchors.fill: parent
            visible: false
        }

        // Access Granted show MailBox Lists
        MailboxListPage {
            id: mailboxList
            visible: false
            model: imapAccess.mailboxModel ? imapAccess.mailboxModel : null
            onMailboxSelected: {
                imapAccess.msgListModel.setMailbox(mailbox)
                messageList.title = mailbox
                messageList.scrollToBottom()
                pageStack.push(messageList)
            }
            // Looks like this gotta be in this file. If moved to the MailboxListPage.qml,
            // QML engine complains about a binding loop.
            // WTF?

            //???? we should add a item to a loader here with some binding ?
            property bool indexValid: mailboxList.model ? mailboxList.model.itemsValid : true
            onIndexValidChanged:{
                if (indexValid !== true)
                    return appWindow.showHome()
            }
        }

        MessageListPage {
            id: messageList
            visible: false
            model: imapAccess.threadingMsgListModel ? imapAccess.threadingMsgListModel : undefined
            onMessageSelected: {
                imapAccess.openMessage(mailboxList.currentMailboxLong, uid)
                pageStack.push(Qt.resolvedUrl("OneMessagePage.qml"),
                               {
                                   mailbox: mailboxList.currentMailboxLong,
                                   url: imapAccess.oneMessageModel.mainPartUrl
                               }
                               )
            }
        }

    }
}
