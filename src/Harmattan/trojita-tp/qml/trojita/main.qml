import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

PageStackWindow {
    id: appWindow
    property bool networkOffline: true

    function showConnectionError(message) {
        passwordDialog.close()
        connectionErrorBanner.text = message
        connectionErrorBanner.parent = pageStack.currentPage
        connectionErrorBanner.show()
        networkOffline = true
    }

    function showImapAlert(message) {
        alertBanner.text = message
        alertBanner.parent = pageStack.currentPage
        alertBanner.show()
    }

    function requestingPassword() {
        passwordDialog.open()
    }

    function authAttemptFailed(message) {
        passwordDialog.authErrorMessage = message
    }

    function connectModels() {
        imapAccess.imapModel.connectionError.connect(showConnectionError)
        imapAccess.imapModel.alertReceived.connect(showImapAlert)
        imapAccess.imapModel.authRequested.connect(requestingPassword)
        imapAccess.imapModel.authAttemptFailed.connect(authAttemptFailed)
        imapAccess.imapModel.networkPolicyOffline.connect(function() {networkOffline = true})
        imapAccess.imapModel.networkPolicyOnline.connect(function() {networkOffline = false})
        imapAccess.imapModel.networkPolicyExpensive.connect(function() {networkOffline = false})
    }

    Component.onCompleted: {
        theme.inverted = true
        theme.colorScheme = 7
        serverSettings.open()
    }

    initialPage: mailboxListPage

    MailboxListPage {
        id: mailboxListPage
        model: imapAccess.mailboxModel ? imapAccess.mailboxModel : undefined

        onMailboxSelected: {
            imapAccess.msgListModel.setMailbox(mailbox)
            pageStack.push(messageListPage)
            oneMessagePage.mailbox = mailbox
        }
    }

    MessageListPage {
        id: messageListPage
        model: imapAccess.msgListModel ? imapAccess.msgListModel : undefined

        onMessageSelected: {
            imapAccess.oneMessageModel.setMessage(oneMessagePage.mailbox, uid)
            pageStack.push(oneMessagePage)
        }
    }

    OneMessagePage {
        id: oneMessagePage
    }

    ToolBarLayout {
        id: commonTools
        visible: true
        ToolIcon {
            iconId: enabled ? "toolbar-back" : "toolbar-back-dimmed"
            enabled: (pageStack.currentPage === mailboxListPage && mailboxListPage.isNestedSomewhere()) || pageStack.depth > 1
            onClicked: {
                if (pageStack.currentPage === mailboxListPage && mailboxListPage.isNestedSomewhere()) {
                    mailboxListPage.openParentMailbox()
                } else {
                    pageStack.pop()
                }
            }
        }
    }

    InfoBanner {
        id: connectionErrorBanner
        timerShowTime: 0
    }

    InfoBanner {
        id: alertBanner
        timerShowTime: 0
    }

    Sheet {
        id: serverSettings
        acceptButtonText: qsTr("OK")
        rejectButtonText: qsTr("Cancel")

        content: ImapSettings {
            id: imapSettings
        }

        Component.onCompleted: {
            imapSettings.imapServer = imapAccess.server
            if (imapAccess.port > 0)
                imapSettings.imapPort = imapAccess.port
            imapSettings.imapUserName = imapAccess.username
            // That's right, we do not load the password
            if (imapAccess.sslMode == "StartTLS")
                imapSettings.imapSslModeIndex = 2
            else if (imapAccess.sslMode == "SSL")
                imapSettings.imapSslModeIndex = 1
            else
                imapSettings.imapSslModeIndex = 0
        }

        onAccepted: {
            imapAccess.server = imapSettings.imapServer
            imapAccess.port = imapSettings.imapPort
            imapAccess.username = imapSettings.imapUserName
            if (imapSettings.imapPassword.length)
                imapAccess.password = imapSettings.imapPassword
            imapAccess.sslMode = imapSettings.imapSslMode
            connectModels()
        }
    }

    PasswordInputSheet {
        id: passwordDialog
        onAccepted: imapAccess.imapModel.imapPassword = password
        onRejected: imapAccess.imapModel.imapPassword = undefined
    }
}
