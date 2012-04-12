import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    property alias model: view.model
    signal mailboxSelected(string mailbox)

    anchors.margins: UiConstants.DefaultMargin

    //Component.onCompleted: {theme.inverted = true}

    Component {
        id: mailboxItemDelegate

        Item {
            width: parent.width
            height: UiConstants.ListItemHeightDefault
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    view.positionViewAtIndex(model.index, ListView.Visible);
                    view.currentIndex = model.index
                    mailboxSelected(mailboxName)
                }
            }
            Text {
                id: titleText
                text: shortMailboxName
                font: UiConstants.TitleFont
            }
            Text {
                id: messageCountsText
                anchors.top: titleText.bottom
                font: UiConstants.SubtitleFont
                text: mailboxIsSelectable ?
                          (totalMessageCount === 0 ?
                               "empty" :
                               (totalMessageCount + " total, " + unreadMessageCount + " unread"))
                        :
                          "This mailbox does not contain any messages."
            }
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        delegate: mailboxItemDelegate
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
        highlightMoveDuration: 600
        focus: true
    }

    ScrollDecorator {
        flickableItem: view
    }
}
