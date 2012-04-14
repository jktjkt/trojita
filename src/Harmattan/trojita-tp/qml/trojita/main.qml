import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

PageStackWindow {
    id: appWindow

    function showConnectionError(message) {
        connectionErrorBanner.text = message
        connectionErrorBanner.parent = pageStack.currentPage
        connectionErrorBanner.show()
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
        authFailedMessage.text = message
    }

    function connectModels() {
        imapAccess.imapModel.connectionError.connect(showConnectionError)
        imapAccess.imapModel.alertReceived.connect(showImapAlert)
        imapAccess.imapModel.authRequested.connect(requestingPassword)
        imapAccess.imapModel.authAttemptFailed.connect(authAttemptFailed)
        mailboxListPage.setMailboxModel(imapAccess.mailboxModel)
        messageListPage.model = imapAccess.msgListModel
    }

    Component.onCompleted: {
        serverSettings.open()
    }

    //initialPage: imapSettingsPage
    initialPage: mailboxListPage

    /*ImapSettingsPage {
        id: imapSettingsPage
    }*/

    MailboxListPage {
        id: mailboxListPage

        onMailboxSelected: {
            imapAccess.msgListModel.setMailbox(mailbox)
            pageStack.push(messageListPage)
        }
    }

    MessageListPage {
        id: messageListPage
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
        timerShowTime: 5000
    }

    InfoBanner {
        id: alertBanner
        timerShowTime: 5000
    }

    Sheet {
        id: serverSettings
        acceptButtonText: qsTr("OK")
        rejectButtonText: qsTr("Cancel")

        content: ImapSettings {
            id: imapSettings
        }

        onAccepted: {
            imapAccess.server = imapSettings.imapServer
            imapAccess.port = imapSettings.imapPort
            imapAccess.username = imapSettings.imapUserName
            imapAccess.password = imapSettings.imapPassword
            imapAccess.sslMode = imapSettings.imapSslMode
            connectModels()
        }

        onVisibleChanged: theme.inverted = visible
    }

    PasswordInputSheet {
        id: passwordDialog
        onAccepted: imapAccess.imapModel.imapPassword = password
        onRejected: imapAccess.imapModel.imapPassword = undefined
        onVisibleChanged: theme.inverted = visible
    }
}
