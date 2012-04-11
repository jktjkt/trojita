import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    //tools: imapSettingsTools
    Component.onCompleted: {theme.inverted = true}
    //anchors.fill: parent

    /*ToolBarLayout {
        id: imapSettingsTools
        ToolIcon { iconId: "toolbar-back"; onClicked: { myMenu.close(); pageStack.pop(); }  }
        ToolButtonRow {
            ToolButton { text: "ToolButton 1" }
            ToolButton { text: "ToolButton 2" }
        }
        ToolIcon { iconId: "toolbar-view-menu" ; onClicked: myMenu.open(); }
    }*/

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
                text: retrieveText()

                function retrieveText() {
                    return encryptionMethodDialog.titleText + ": "
                            + encryptionMethodDialog.model.get(encryptionMethodDialog.selectedIndex).name
                }

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
                    encryptionMethodBtn.text = encryptionMethodBtn.retrieveText()
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
            }
        }
    }





}
