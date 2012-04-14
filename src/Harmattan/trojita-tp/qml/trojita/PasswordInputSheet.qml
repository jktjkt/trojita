import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Sheet {
    property alias password: password.text
    property alias authErrorMessage: authFailedMessage.text

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
}
