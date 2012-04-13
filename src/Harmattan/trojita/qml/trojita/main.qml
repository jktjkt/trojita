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
        authFailedBanner.text = message
        authFailedBanner.show()
    }

    Component.onCompleted: {
        imapModel.connectionError.connect(showConnectionError)
        imapModel.alertReceived.connect(showImapAlert)
        imapModel.imapUser = "FIXME"
        imapModel.authRequested.connect(requestingPassword)
        imapModel.authAttemptFailed.connect(authAttemptFailed)
    }

    //initialPage: imapSettingsPage
    initialPage: mailboxListPage

    /*ImapSettingsPage {
        id: imapSettingsPage
    }*/

    MailboxListPage {
        id: mailboxListPage
        model: mailboxModel

        onMailboxSelected: {
            msgListModel.setMailbox(mailbox)
            pageStack.push(messageListPage)
        }
    }

    MessageListPage {
        id: messageListPage
        model: msgListModel
    }

    ToolBarLayout {
        id: commonTools
        visible: true
        ToolIcon {
            iconId: "toolbar-back"
            onClicked: pageStack.pop()
        }
        ButtonRow {
            style: TabButtonStyle {}
            TabButton {
                text: "blah"
            }
        }
    }

    /*Menu {
        id: myMenu
        visualParent: pageStack
        MenuLayout {
            MenuItem { text: qsTr("Sample menu item") }
        }
    }*/


    InfoBanner {
        id: connectionErrorBanner
        timerShowTime: 5000
    }

    InfoBanner {
        id: alertBanner
        timerShowTime: 5000
    }

    InfoBanner {
        id: authFailedBanner
    }

    Sheet {
        id: passwordDialog
        acceptButtonText: qsTr("Login")
        rejectButtonText: qsTr("Cancel")

        content: Column {
            anchors.fill: parent
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
                echoMode: TextInput.PasswordEchoOnEdit
            }
        }

        onAccepted: imapModel.imapPassword = password.text
        onRejected: imapModel.imapPassword = undefined
        onVisibleChanged: {
            if (visible) {
                authFailedBanner.parent = passwordDialog
                authFailedBanner.anchors.bottom = passwordDialog.bottom
            } else {
                authFailedBanner.parent = appWindow.pageStack.currentPage
                authFailedBanner.anchors.bottom = undefined
            }
        }
    }
}
