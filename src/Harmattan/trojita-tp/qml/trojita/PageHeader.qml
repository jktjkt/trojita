import QtQuick 1.1
import com.nokia.meego 1.0

Rectangle {
    property alias text: lbl.text

    color: theme.selectionColor
    anchors {
        left: parent.left; right: parent.right
        bottomMargin: screen.currentOrientation == Screen.Landscape ? UiConstants.HeaderDefaultBottomSpacingLandscape :
                                                                      UiConstants.HeaderDefaultBottomSpacingPortrait
    }
    height: screen.currentOrientation == Screen.Landscape ? UiConstants.HeaderDefaultHeightLandscape :
                                                            UiConstants.HeaderDefaultHeightPortrait
    Label {
        id: lbl
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font: UiConstants.HeaderFont
    }

    BusyIndicator {
        id: busyIndicator
        visible: imapAccess.visibleTasksModel ? imapAccess.visibleTasksModel.hasVisibleTasks : false
        running: visible
        anchors { right: parent.right; verticalCenter: parent.verticalCenter; margins: UiConstants.DefaultMargin }
        MouseArea {
            anchors.fill: parent
            onClicked: activeTasks.state = activeTasks.state == "shown" ? "hidden" : "shown"
        }
    }

    Rectangle {
        id: activeTasks
        width: 0.8 * parent.width
        height: Math.max(Math.min(view.count * (view.itemHeight + view.spacing) + 2 * UiConstants.DefaultMargin, 350), 60)
        anchors.right: parent.right
        color: Qt.rgba(0.1, 0.1, 0.1, 0.9)
        z: -1 // below the title bar, but still above the real page contents

        states: [
            State {
                name: "shown"
                AnchorChanges { target: activeTasks; anchors.top: parent.bottom; anchors.bottom: undefined }
            },
            State {
                name: "hidden"
                AnchorChanges { target: activeTasks; anchors.top: undefined; anchors.bottom: parent.bottom }
            }
        ]
        state: "hidden"

        transitions: Transition {
            AnchorAnimation { duration: 150 }
        }

        Timer {
            interval: 3000
            running: view.count == 0
            onTriggered: activeTasks.state = "hidden"
        }

        ListView {
            property int itemHeight: 34
            id: view
            clip: true
            anchors.fill: parent
            anchors.margins: UiConstants.DefaultMargin
            spacing: UiConstants.ButtonSpacing
            visible: count > 0

            model: imapAccess ? imapAccess.visibleTasksModel : undefined

            delegate: Label {
                id: delegate
                text: compactName
                color: "white"
                height: view.itemHeight
            }
        }
        Label {
            anchors.centerIn: parent
            anchors.margins: UiConstants.DefaultMargin
            visible: !view.visible
            text: qsTr("No activity")
        }

        ScrollDecorator {
            flickableItem: view
        }
    }
}
