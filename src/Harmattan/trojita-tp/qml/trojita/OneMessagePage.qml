import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1
import net.flaska.QNAMWebView 1.0

Page {
    property string mailbox
    property alias url: messageView.url

    tools: commonTools

    Item {
        anchors {left: parent.left; right: parent.right; bottom: parent.bottom; top: header.bottom}

        Column {
            Label {
                text: imapAccess.oneMessageModel ? qsTr("Date: ") + imapAccess.oneMessageModel.date : ""
            }
            Label {
                text: imapAccess.oneMessageModel ? qsTr("Subject: ") + imapAccess.oneMessageModel.subject : ""
            }

            QNAMWebView {
                id: messageView
                width: parent.width
                height: 500
                networkAccessManager: imapAccess.msgQNAM
            }
        }

        /*ScrollDecorator {
            flickableItem: view
        }

        PercentageSectionScroller {
            listView: view
        }*/
    }

    PageHeader {
        id: header
        text: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.subject : ""
        anchors {left: parent.left; right: parent.right; top: parent.top}
    }
}
