import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    Component.onCompleted: {theme.inverted = true}

    Flickable {
        id: flickable
        anchors.fill: parent
        flickableDirection: Flickable.VerticalFlick

        Column {
            id: col
            spacing: 10
            anchors.fill: parent

            Label {
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
            }


            Label {
                text: qsTr("Server address")
            }
            TextField {
                id: imapServer
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
    }





}
