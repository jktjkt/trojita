import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    Component {
        id: messageItemDelegate

        Item {
            width: parent.width
            height: 120

            function formatMailAddress(items) {
                if (items[0] !== null) {
                    return items[0] + " <" + items[2] + "@" + items[3] + ">"
                } else {
                    return items[2] + "@" + items[3]
                }
            }

            Column {
                Text {
                    font: UiConstants.TitleFont
                    text: subject
                }
                Text {
                    font: UiConstants.SubtitleFont
                    // FIXME: multiple/no addresses...
                    text: formatMailAddress(from[0])
                }
                Text {
                    font: UiConstants.BodyTextFont
                    // if there's a better way to compare QDateTime::date with "today", well, please do tell me
                    text: Qt.formatDate(date, "YYYY-mm-dd") == Qt.formatDate(new Date(), "YYYY-mm-dd") ?
                              Qt.formatTime(date) : Qt.formatDate(date)
                }
            }
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        delegate: messageItemDelegate

        signal messageSelected(string mailbox)
    }

    anchors.margins: UiConstants.DefaultMargin

    property alias model: view.model
}
