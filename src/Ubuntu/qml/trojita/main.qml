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
import Ubuntu.Components 1.1
import Ubuntu.Components.Popups 1.0
import Ubuntu.Components.ListItems 1.0 as ListItems
import trojita.models.ThreadingMsgListModel 0.1

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

    function continueAuthentication() {
        if (imapAccess.passwordWatcher.didReadOk && imapAccess.passwordWatcher.password) {
            imapAccess.imapModel.imapPassword = imapAccess.passwordWatcher.password
        } else {
            PopupUtils.open(passwordDialogComponent)
        }
    }

    function passwordReadingFailed(message) {
        authAttemptFailed(message)
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
        imapAccess.imapModel.authRequested.connect(imapAccess.passwordWatcher.reloadPassword)
        imapAccess.imapModel.authAttemptFailed.connect(authAttemptFailed)
        imapAccess.imapModel.networkPolicyOffline.connect(function() {networkOffline = true})
        imapAccess.imapModel.networkPolicyOnline.connect(function() {networkOffline = false})
        imapAccess.imapModel.networkPolicyExpensive.connect(function() {networkOffline = false})
    }

    function setupConnections() {
        // connect these before calling imapAccess.doConnect()
        imapAccess.checkSslPolicy.connect(function() {PopupUtils.open(sslSheetPage)})
        imapAccess.modelsChanged.connect(modelsChanged)
        imapAccess.passwordWatcher.readingDone.connect(continueAuthentication)
        imapAccess.passwordWatcher.readingFailed.connect(passwordReadingFailed)
    }

    function settingsChanged() {
        // when settings are changed we want to unload the mailboxview model
        mailboxList.model = null
    }

    function modelsChanged() {
        mailboxList.model = imapAccess.mailboxModel
        pageStack.clear()
        // Initially set the sort order to descending, if the sort order
        // then gets changed from message list page toolbar, we respect that
        // change and use the specified sort order from then onwards
        imapAccess.threadingMsgListModel.setUserSearchingSortingPreference(
                    [],
                    ThreadingMsgListModel.SORT_NONE,
                    Qt.DescendingOrder
                    )
        showHome()
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
            onConfirmClicked: {
                imapAccess.setSslPolicy(true)
                showHome()
            }
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
        Component.onCompleted: {
            setupConnections()
            if (imapAccess.sslMode) {
                imapAccess.doConnect()
                connectModels()
            } else {
                pageStack.push(Qt.resolvedUrl("SettingsTabs.qml"))
            }
        }

        // Access Granted show MailBox Lists
        MailboxListPage {
            id: mailboxList
            visible: false
            onMailboxSelected: {
                imapAccess.msgListModel.setMailbox(mailbox)
                pageStack.push(Qt.resolvedUrl("MessageListPage.qml"), { title: mailbox })
            }
        }
    }
}
