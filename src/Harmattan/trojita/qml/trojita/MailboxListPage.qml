import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    //Component.onCompleted: {theme.inverted = true}

    Component {
        id: mailboxItemDelegate

        Item {
            width: parent.width
            height: 150
            Text {
                text: "<b>" + shortMailboxName + "</b><br/>" + totalMessageCount + " total, " + unreadMessageCount + " unread"
            }
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        delegate: mailboxItemDelegate
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
    }

    property alias model: view.model
}
