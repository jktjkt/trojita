import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    property alias model: view.model

    anchors.margins: UiConstants.DefaultMargin
    tools: commonTools

    Component {
        id: normalMessageItemDelegate

        Item {
            function formatMailAddress(items) {
                if (items[0] !== null) {
                    return items[0] + " <" + items[2] + "@" + items[3] + ">"
                } else {
                    return items[2] + "@" + items[3]
                }
            }

            property variant model

            Column {
                visible: model.isFetched
                Label {
                    font: UiConstants.TitleFont
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: view.width
                    text: model.subject
                }
                Label {
                    font: UiConstants.SubtitleFont
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: view.width
                    // FIXME: multiple/no addresses...
                    text: formatMailAddress(model.from[0])
                }
                Text {
                    font: UiConstants.BodyTextFont
                    // if there's a better way to compare QDateTime::date with "today", well, please do tell me
                    text: Qt.formatDate(model.date, "YYYY-mm-dd") == Qt.formatDate(new Date(), "YYYY-mm-dd") ?
                              Qt.formatTime(model.date) : Qt.formatDate(model.date)
                }
            }
            Label {
                text: qsTr("Message is loading...")
                visible: !model.isFetched
                anchors.centerIn: parent
                platformStyle: LabelStyle {
                    fontFamily: "Nokia Pure Text Light"
                    fontPixelSize: 40
                    textColor: "#a0a0a0"
                }
            }
        }
    }

    Component {
        id: scrollingMessageDelegate

        Item {
            Label {
                text: qsTr("Scrolling...")
                anchors.centerIn: parent
                platformStyle: LabelStyle {
                    fontFamily: "Nokia Pure Text Light"
                    fontPixelSize: 40
                    textColor: "#a0a0a0"
                }
            }
        }
    }

    Component {
        id: messageItemDelegate

        Loader {
            width: parent.width
            height: 120
            sourceComponent: view.verticalVelocity > 6000 ? scrollingMessageDelegate: normalMessageItemDelegate
            Binding { target: item; property: "model"; value: model; when: status == Loader.Ready }
        }
    }

    ListView {
        signal messageSelected(string mailbox)

        id: view
        anchors.fill: parent
        delegate: messageItemDelegate

        maximumFlickVelocity: 999999
    }

    ScrollDecorator {
        flickableItem: view
    }
}
