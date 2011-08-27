import QtQuick 1.1
import com.meego 1.0

Page {
    id: mainPage
    tools: commonTools
    Label {
        id: label
        anchors.centerIn: parent
        text: qsTr("Hello world!")
        visible: false
    }
    Button{
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: label.bottom
        anchors.topMargin: 10
        text: qsTr("Click here!")
        onClicked: label.visible=true
    }
}
