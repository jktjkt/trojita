import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    property string mailbox
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
        }

        /*ScrollDecorator {
            flickableItem: view
        }

        PercentageSectionScroller {
            listView: view
        }*/
    }

    PageHeader {
        // FIXME: this needs work -- text eliding,...
        id: header
        text: imapAccess.oneMessageModel ? imapAccess.oneMessageModel.subject : ""
        anchors {left: parent.left; right: parent.right; top: parent.top}
    }
}
