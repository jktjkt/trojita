import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    property alias model: view.model
    signal mailboxSelected(string mailbox)

    anchors.margins: UiConstants.DefaultMargin
    tools: commonTools

    //Component.onCompleted: {theme.inverted = true}

    Component {
        id: mailboxItemDelegate

        Item {
            width: parent.width
            height: UiConstants.ListItemHeightDefault

            Item {
                anchors {
                    top: parent.top; bottom: parent.bottom; left: parent.left;
                    right: moreIndicator.visible ? moreIndicator.left : parent.right
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        view.positionViewAtIndex(model.index, ListView.Visible);
                        view.currentIndex = model.index
                        if (mailboxIsSelectable) {
                            mailboxSelected(mailboxName)
                        }
                    }
                }
                Text {
                    id: titleText
                    text: shortMailboxName
                    font: UiConstants.TitleFont
                }
                ProgressBar {
                    visible: mailboxIsSelectable && totalMessageCount === undefined
                    anchors { left: parent.left; right: parent.right; top: titleText.bottom }
                    indeterminate: true
                }
                Text {
                    id: messageCountsText
                    anchors.top: titleText.bottom
                    font: UiConstants.SubtitleFont
                    visible: mailboxIsSelectable && totalMessageCount !== undefined
                    text: totalMessageCount === 0 ?
                              "empty" :
                              (totalMessageCount + " total, " + unreadMessageCount + " unread")
                }
            }

            MoreIndicator {
                id: moreIndicator
                visible: mailboxHasChildMailboxes
                anchors {verticalCenter: parent.verticalCenter; right: parent.right}

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("expanding mailbox")
                    }
                }
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
