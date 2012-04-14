import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Flickable {
    property alias imapServer: imapServerInput.text
    property alias imapPort: imapPortInput.text
    property alias imapUserName: imapUserNameInput.text
    property alias imapPassword: imapPasswordInput.text
    property string imapSslMode: encryptionMethodDialog.model.get(encryptionMethodDialog.selectedIndex).name

    id: flickable
    anchors.fill: parent
    flickableDirection: Flickable.VerticalFlick

    //Component.onCompleted: {theme.inverted = true}

    Column {
        id: col
        spacing: 10
        anchors.fill: parent
        anchors.margins: UiConstants.DefaultMargin

        /*Label {
                text: qsTr("Name")
            }
            TextField {
                id: realName
                anchors {left: col.left; right: col.right;}
            }

            Label {
                text: qsTr("E-mail address")
            }
            TextField {
                id: email
                anchors {left: col.left; right: col.right;}
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
            }*/

        Label {
            text: qsTr("Username")
        }
        TextField {
            id: imapUserNameInput
            anchors {left: col.left; right: col.right;}
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhEmailCharactersOnly | Qt.ImhNoPredictiveText
        }

        Label {
            text: qsTr("Password")
        }
        TextField {
            id: imapPasswordInput
            anchors {left: parent.left; right: parent.right;}
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            echoMode: TextInput.Password
        }

        Label {
            text: qsTr("Server address")
        }
        TextField {
            id: imapServerInput
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

        Label {
            text: qsTr("Port")
        }
        TextField {
            id: imapPortInput
            text: "143"
            anchors {left: col.left; right: col.right;}
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 1; top: 65535 }
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
            imapPortInput.text = encryptionMethodDialog.model.get(encryptionMethodDialog.selectedIndex).port
        }
    }

}
