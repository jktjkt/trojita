import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {

    Component {
        id: messageItemDelegate

        Item {
            width: parent.width
            height: 250
            Text {
                font.pointSize: 16
                text: "<b>" + subject + "</b><br/>" + from + date
            }
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        delegate: messageItemDelegate

        signal messageSelected(string mailbox)
    }

    property alias model: view.model
}
