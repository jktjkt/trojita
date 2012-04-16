import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    property alias model: view.model

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

            width: view.width
            height: 120

            Column {
                id: col
                anchors.margins: UiConstants.DefaultMargin
                anchors.fill: parent
                visible: ! (model === undefined || model.subject === undefined || !model.isFetched)
                Label {
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: parent.width
                    text: !col.visible ? "" : model.subject
                    color: !col.visible ? platformStyle.textColor :
                                          model.isMarkedRead ?
                                              (model.isMarkedDeleted ? Qt.darker(platformStyle.textColor) : platformStyle.textColor) :
                                              theme.selectionColor
                    font { pixelSize: UiConstants.TitleFont.pixelSize; family: UiConstants.TitleFont.family; bold: UiConstants.TitleFont.bold }
                    font.strikeout: !col.visible ? false : model.isMarkedDeleted
                }
                Label {
                    font: UiConstants.SubtitleFont
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: parent.width
                    // FIXME: multiple/no addresses...
                    text: !col.visible ? "" : formatMailAddress(model.from[0])
                }
                Label {
                    width: parent.width
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
            width: view.width
            height: 120
            anchors.margins: UiConstants.DefaultMargin
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
            sourceComponent: view.count > 1000 && (view.massiveScrolling || view.verticalVelocity > 2000) ? scrollingMessageDelegate: normalMessageItemDelegate
            Binding { target: item; property: "model"; value: model; when: status == Loader.Ready }
        }
    }

    Item {
        anchors {left: parent.left; right: parent.right; bottom: parent.bottom; top: header.bottom}

        ListView {
            signal messageSelected(string mailbox)
            property bool massiveScrolling

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

    PageHeader {
        id: header
        text: "pwned"
        anchors {left: parent.left; right: parent.right; top: parent.top}
    }
}
