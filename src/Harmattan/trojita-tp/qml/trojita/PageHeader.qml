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
    }

    BusyIndicator {
        id: busyIndicator
        visible: imapAccess.visibleTasksModel ? imapAccess.visibleTasksModel.hasVisibleTasks : false
        running: visible
        anchors { right: parent.right; verticalCenter: parent.verticalCenter; margins: UiConstants.DefaultMargin }
    }
}
