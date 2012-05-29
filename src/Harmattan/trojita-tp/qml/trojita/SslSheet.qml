import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1
import net.flaska.QNAMWebView 1.0

Sheet {
    property alias htmlText: htmlView.html

    acceptButtonText: qsTr("Connect")
    rejectButtonText: qsTr("Reject")

    content: Item {
        id: item
        anchors.fill: parent

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: htmlView.width
            contentHeight: htmlView.height

            QNAMWebView {
                id: htmlView

                preferredWidth: item.width
                width: item.width
                preferredHeight: item.height
                settings.userStyleSheetUrl: "data:text/css;charset=utf-8;base64," +
                                            Qt.btoa("* {color: white; background: black; font-size: " +
                                                    UiConstants.BodyTextFont.pixelSize + "px;};")
            }
        }

        ScrollDecorator {
            flickableItem: flick
        }
    }
}
