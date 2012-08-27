import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

ToolIcon {
    iconId: enabled ? "toolbar-back" : "toolbar-back-dimmed"
    enabled: appWindow.backButtonEnabled()
    onClicked: appWindow.goBack()
}
