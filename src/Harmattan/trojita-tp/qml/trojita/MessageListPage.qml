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
                id: col
                visible: ! (model === undefined || model.subject === undefined || !model.isFetched)
                Label {
                    font: UiConstants.TitleFont
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: view.width
                    text: !col.visible ? "" : model.subject
                }
                Label {
                    font: UiConstants.SubtitleFont
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: view.width
                    // FIXME: multiple/no addresses...
                    text: !col.visible ? "" : formatMailAddress(model.from[0])
                }
                Label {
                    font: UiConstants.BodyTextFont
                    // if there's a better way to compare QDateTime::date with "today", well, please do tell me
                    text: !col.visible ? "" : Qt.formatDate(model.date, "YYYY-mm-dd") == Qt.formatDate(new Date(), "YYYY-mm-dd") ?
                              Qt.formatTime(model.date) : Qt.formatDate(model.date)
                }
            }
            Label {
                id: loadingIndicator
                text: qsTr("Message is loading...")
                visible: !col.visible
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
            sourceComponent: view.verticalVelocity > 2000 ? scrollingMessageDelegate: normalMessageItemDelegate
            Binding { target: item; property: "model"; value: model; when: status == Loader.Ready }
        }
    }

    ListView {
        signal messageSelected(string mailbox)

        id: view
        anchors.fill: parent
        delegate: messageItemDelegate
        visible: count > 0

        section.property: "fuzzyDate"
        section.delegate: Item {
            width: parent.width
            height: 40
            Label {
                id: headerLabel
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 8
                anchors.bottomMargin: 2
                text: section
                font.bold: true
                font.pixelSize: 18
                color: theme.inverted ? "#4D4D4D" : "#3C3C3C";
            }
            Image {
                anchors.right: headerLabel.left
                anchors.left: parent.left
                anchors.verticalCenter: headerLabel.verticalCenter
                anchors.rightMargin: 24
                source: "image://theme/meegotouch-groupheader" + (theme.inverted ? "-inverted" : "") + "-background"
            }
        }

        onVisibleChanged: if (visible) positionViewAtEnd()
    }

    Label {
        visible: !view.visible
        anchors.fill: parent
        text: qsTr("Empty Mailbox")
        color: "#606060"
        font {bold: true; pixelSize: 90}
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    ScrollDecorator {
        flickableItem: view
    }

    PercentageSectionScroller {
        listView: view
    }
}
