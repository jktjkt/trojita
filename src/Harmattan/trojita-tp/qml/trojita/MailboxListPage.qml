import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    signal mailboxSelected(string mailbox)
    property int nestingDepth: 0
    property string viewTitle: isNestedSomewhere() ? currentMailbox : imapAccess.server
    property string currentMailbox

    function setMailboxModel(model) {
        proxyModel.model = model
        view.model = proxyModel
    }

    function openParentMailbox() {
        moveListViewRight.start()
        view.model.rootIndex = view.model.parentModelIndex()
        --nestingDepth
        currentMailbox = ""
    }

    function isNestedSomewhere() {
        return nestingDepth > 0
    }

    tools: commonTools

    //Component.onCompleted: {theme.inverted = true}

    Component {
        id: mailboxItemDelegate

        Item {
            width: parent.width
            height: UiConstants.ListItemHeightDefault
            anchors.margins: UiConstants.DefaultMargin

            Item {
                anchors {
                    top: parent.top; bottom: parent.bottom; left: parent.left;
                    right: moreIndicator.visible ? moreIndicator.left : parent.right
                    leftMargin: 6
                    rightMargin: 16
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
                Label {
                    id: titleText
                    text: shortMailboxName
                    font: UiConstants.TitleFont
                }
                Label {
                    id: messageCountsText
                    anchors.top: titleText.bottom
                    font: UiConstants.SubtitleFont
                    visible: mailboxIsSelectable && totalMessageCount !== undefined
                    text: totalMessageCount === 0 ?
                              "empty" :
                              (totalMessageCount + " total, " + unreadMessageCount + " unread")
                }
                Label {
                    anchors.top: titleText.bottom
                    font: UiConstants.SubtitleFont
                    visible: mailboxIsSelectable && totalMessageCount === undefined
                    text: qsTr("Loading...")
                }
            }

            MoreIndicator {
                id: moreIndicator
                visible: mailboxHasChildMailboxes
                anchors {verticalCenter: parent.verticalCenter; right: parent.right}

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        view.positionViewAtIndex(model.index, ListView.Visible);
                        currentMailbox = shortMailboxName
                        view.currentIndex = model.index
                        moveListViewLeft.start()
                        view.model.rootIndex = view.model.modelIndex(index)
                        ++nestingDepth
                    }
                }
            }
        }
    }

    VisualDataModel {
        id: proxyModel
        delegate: mailboxItemDelegate
    }

    ListView {
        id: view
        anchors.fill: parent
        focus: true
    }

    ScrollDecorator {
        flickableItem: view
    }

    // FIXME: can we use shaders for these?
    SequentialAnimation {
        id: moveListViewLeft
        property int oneStepDuration: 100
        ScriptAction { script: view.anchors.fill = undefined }
        PropertyAnimation { target: view; properties: "x"; to: -view.width; duration: moveListViewLeft.oneStepDuration }
        PropertyAction { target: view; property: "x"; value: view.width }
        PropertyAnimation { target: view; properties: "x"; to: 0; duration: moveListViewLeft.oneStepDuration }
        ScriptAction { script: view.anchors.fill = view.parent }
    }

    SequentialAnimation {
        id: moveListViewRight
        property alias oneStepDuration: moveListViewLeft.oneStepDuration
        ScriptAction { script: view.anchors.fill = undefined }
        PropertyAnimation { target: view; properties: "x"; to: view.width; duration: moveListViewRight.oneStepDuration }
        PropertyAction { target: view; property: "x"; value: -view.width }
        PropertyAnimation { target: view; properties: "x"; to: 0; duration: moveListViewRight.oneStepDuration }
        ScriptAction { script: view.anchors.fill = view.parent }
    }
}
