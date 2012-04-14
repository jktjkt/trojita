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
        mailboxListPage.model = imapAccess.mailboxModel
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
            iconId: theme.inverted ?
                        (enabled ? "toolbar-back-white" : "toolbar-back-dimmed-white") :
                        (enabled ? "toolbar-back" : "toolbar-back-dimmed")
            onClicked: pageStack.pop()
            enabled: pageStack.depth > 1
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

        content: Column {
            id: col
            spacing: 10
            anchors.fill: parent
            anchors.margins: UiConstants.DefaultMargin

            Label {
                text: qsTr("Username")
            }
            TextField {
                id: userName
                anchors {left: col.left; right: col.right;}
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
            }

            Label {
                text: qsTr("Password")
            }
            TextField {
                id: imapPassword
                anchors {left: parent.left; right: parent.right;}
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                echoMode: TextInput.Password
            }

            Label {
                text: qsTr("Server address")
            }
            TextField {
                id: imapServer
                text: "192.168.2.14"
                anchors {left: col.left; right: col.right;}
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase | Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
            }

            Button {
                id: encryptionMethodBtn
                anchors {left: col.left; right: col.right;}
                text: encryptionMethodDialog.titleText + ": " + encryptionMethodDialog.model.get(encryptionMethodDialog.selectedIndex).name

                onClicked: {
                    encryptionMethodDialog.open()
                }
            }

            SelectionDialog {
                id: encryptionMethodDialog
                titleText: qsTr("Secure connection")
                selectedIndex: 0
                model: ListModel {
                    ListElement {
                        name: QT_TR_NOOP("No")
                        port: 143
                    }
                    ListElement {
                        name: QT_TR_NOOP("SSL")
                        port: 993
                    }
                    ListElement {
                        name: QT_TR_NOOP("StartTLS")
                        port: 143
                    }
                }
                onAccepted: {
                    imapPort.text = encryptionMethodDialog.model.get(encryptionMethodDialog.selectedIndex).port
                }
            }

            Label {
                text: qsTr("Port")
            }
            TextField {
                id: imapPort
                text: "143"
                anchors {left: col.left; right: col.right;}
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
            }
        }

        onAccepted: {
            imapAccess.server = imapServer.text
            imapAccess.port = imapPort.text
            imapAccess.username = userName.text
            imapAccess.password = imapPassword.text
            imapAccess.sslMode = encryptionMethodDialog.model.get(encryptionMethodDialog.selectedIndex).name
            connectModels()
        }
    }

    Sheet {
        id: passwordDialog
        acceptButtonText: qsTr("Login")
        rejectButtonText: qsTr("Cancel")

        content: Column {
            anchors.fill: parent
            anchors.margins: UiConstants.DefaultMargin
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

        onAccepted: imapModel.imapPassword = password.text
        onRejected: imapModel.imapPassword = undefined
    }
}
