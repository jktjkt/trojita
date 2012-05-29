import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1
import net.flaska.QNAMWebView 1.0

Sheet {
    property alias htmlText: htmlView.html

    id: sheet
    acceptButtonText: qsTr("Connect")
    rejectButtonText: qsTr("Reject")

    content: Item {
        anchors.fill: parent

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: UiConstants.DefaultMargin
            contentWidth: htmlView.width
            contentHeight: htmlView.height

            QNAMWebView {
                id: htmlView

                preferredWidth: sheet.width
                width: sheet.width
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
