import QtQuick 2.0
import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItem
import "./Utils.js" as Utils


ListItem.Expandable {
    id: headerExpandable

    // Properties
    height: expanded ? undefined : units.gu(9)
    anchors {
        top: parent.top
        left: parent.left
        right: parent.right
        leftMargin: units.gu(1)
        rightMargin: units.gu(1)
        topMargin: units.gu(1.5)
    }
//            showDivider: false
    expandedHeight: infoColumn.height + units.gu(2)

    // Signals
    // this should emit the contact name and address as a JSObject
    // so we can query QtContacts api for existence or addtion
    signal contactNameClicked(var contactInfo)
    // This should emit the email address and open the compose page
    // with the "To" field populatedwith the clicked email address. This is seperate to a reply action
    // as the user may just wish for example to email "Foo@Bar.com" who is in the Cc list
    signal emailAddressClicked(string emailAddress)

    // Components
    UbuntuShape {
        id: msgListContactImg
        height: units.gu(8)
        width: height
        color: Utils.getIconColor(fromNameWidget.text)
        anchors {
            top: parent.top
            left: parent.left
        }
        Image {
            anchors.fill: parent
            anchors.margins: units.gu(0.5)
            source: Qt.resolvedUrl("./contact.svg")
        }
    }
    Image {
        id: conIcon
        anchors {
            leftMargin: units.gu(1)
            left: msgListContactImg.right
            top: msgListContactImg.top
        }
        height: units.gu(2)
        width: height
        source: Qt.resolvedUrl("./contact_grey.svg")
        MouseArea {
            anchors.fill: parent
            onClicked: contactNameClicked(imapAccess.oneMessageModel.from)
        }
    }
    Image {
        anchors {
            leftMargin: units.gu(1)
            topMargin: units.gu(1)
            left: msgListContactImg.right
            top: conIcon.bottom
        }
        height: units.gu(2)
        width: height
        source: Qt.resolvedUrl("./email.svg")
    }

    // FIXME: the caption labels dont quite (very marginally) line
    // up quite right with their info labels
    Column {
        anchors {
            left: parent.left
            top: msgListContactImg.bottom
            right: infoColumn.left
        }
        spacing: units.gu(0.5)
        Item {
            id: toCaptionLabel
            visible: headerExpandable.expanded ? toWidget.text : false
            height: toWidget.height
            anchors.left: parent.left
            anchors.right: parent.right
            Label {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: units.gu(0.5)
                text: qsTr("To:")
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
        Item {
            id: ccCaptionLabel
            visible: headerExpandable.expanded ? ccWidget.text : false
            height: ccWidget.height
            anchors.left: parent.left
            anchors.right: parent.right
            Label {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: units.gu(0.5)
                text: qsTr("Cc:")
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight

            }
        }
        Item {
            id: bccCaptionLabel
            visible: headerExpandable.expanded ? bccWidget.text : false
            height: bccWidget.height
            anchors.left: parent.left
            anchors.right: parent.right
            Label {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: units.gu(0.5)
                text: qsTr("Bcc:")
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight

            }
        }
        Item {
            id: subjectCaptionLabel
            visible: headerExpandable.expanded ? subjectLabel.text : false
            height: subjectLabel.height
            anchors.left: parent.left
            anchors.right: parent.right
            Label {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: units.gu(0.5)
                text: qsTr("Subject:")
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight

            }
        }
    }

    Column {
        id: infoColumn
        anchors.left: conIcon.right
        anchors.top: parent.top
        anchors.right: dropDownIcon.left
        anchors.leftMargin: units.gu(1)
        spacing: units.gu(0.5)

        AddressWidget {
            id: fromNameWidget
            anchors {
                left: parent.left
                right: parent.right
            }
            address: imapAccess.oneMessageModel.from
            nameOnly: true
            font.bold: true
            MouseArea {
                anchors.fill: parent
                onClicked: contactNameClicked(fromNameWidget.text)
            }
        }
        AddressWidget {
            id: fromAddressWidget
            anchors {
                left: parent.left
                right: parent.right
            }
            address: imapAccess.oneMessageModel.from
            addressOnly: true
        }
        Label {
            id: dateLabel
            anchors {
                left: parent.left
                right: parent.right
            }
            text: qsTr(Utils.formatDateDetailed(imapAccess.oneMessageModel.date))
        }
        AddressWidget {
            id: toWidget
            anchors {
                left: parent.left
                right: parent.right
            }
            address: imapAccess.oneMessageModel.to
            visible: headerExpandable.expanded
        }
        AddressWidget {
            id: ccWidget
            anchors {
                left: parent.left
                right: parent.right
            }
            address: imapAccess.oneMessageModel.cc
            visible: headerExpandable.expanded ? ccWidget.text : false
        }
        AddressWidget {
            id: bccWidget
            anchors {
                left: parent.left
                right: parent.right
            }
            address: imapAccess.oneMessageModel.bcc
            visible: headerExpandable.expanded ? bccWidget.text : false
        }
        Label {
            id: subjectLabel
            anchors {
                left: parent.left
                right: parent.right
            }
            text: imapAccess.oneMessageModel.subject
            visible: headerExpandable.expanded ? subjectLabel.text : false
            wrapMode: Text.WordWrap
        }
    }
    Image {
        id: dropDownIcon
        anchors {
            right: parent.right
            top: parent.top
            topMargin: units.gu(2)
            rightMargin: units.gu(3)
        }
        height: units.gu(3)
        width: height
        source: Qt.resolvedUrl("./dropdown-menu.svg")
        rotation: headerExpandable.expanded ? 180 : 0
        MouseArea {
            anchors.fill: parent
            onClicked: {
                headerExpandable.expanded = !headerExpandable.expanded
            }
        }
    }
}

