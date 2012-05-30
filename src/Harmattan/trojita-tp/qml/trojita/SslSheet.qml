import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1
import net.flaska.QNAMWebView 1.0

Sheet {
    property alias htmlText: htmlView.html
    property alias titleText: title.text

    acceptButtonText: qsTr("Connect")
    rejectButtonText: qsTr("Reject")

    content: Item {
        id: item
        anchors.fill: parent

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: htmlView.width
            contentHeight: htmlView.height + header.height

            Column {
                Item {
                    id: header
                    width: item.width
                    height: icon.height + icon.anchors.topMargin + icon.anchors.bottomMargin
                    Image {
                        id: icon
                        anchors {
                            margins: UiConstants.DefaultMargin
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                        }
                        source: "icon-m-settings-keychain.svg"
                        width: 100
                        height: 100
                    }

                    Label {
                        id: title
                        text: "Blésmrt Trojitá"
                        anchors {
                            margins: UiConstants.DefaultMargin
                            top: parent.top
                            bottom: parent.bottom
                            left: icon.right
                            right: parent.right
                        }
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font {
                            pixelSize: UiConstants.TitleFont.pixelSize
                            family: UiConstants.TitleFont.family
                            bold: UiConstants.TitleFont.bold
                        }
                    }
                }

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
        }

        ScrollDecorator {
            flickableItem: flick
        }
    }
}
