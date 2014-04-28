/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItems

Page {
    id: mailboxPage
    title: isNestedSomewhere() ? currentMailbox : qsTr("Mail Box")
    signal mailboxSelected(string mailbox)
    property int nestingDepth: 0
    property string currentMailbox
    property string currentMailboxLong
    property QtObject model

    function openParentMailbox() {
        moveListViewRight.start()
        model.setRootOneLevelUp()
        --nestingDepth
        currentMailbox = imapAccess.mailboxListShortMailboxName()
        currentMailboxLong = imapAccess.mailboxListMailboxName()

    }

    function isNestedSomewhere() {
        return nestingDepth > 0
    }

    ListView {
        id: view
        model: mailboxPage.model
        width: parent.width
        height: parent.height
        delegate: ListItems.Empty{

            ListItems.Subtitled {
                id: titleText
                anchors {
                    left: parent.left
                    right: progression.visible ? progression.left : parent.right
                }
                showDivider: false
                text: shortMailboxName
                subText: qsTr("%L1 total, %L2 unread").arg(model.totalMessageCount).arg(model.unreadMessageCount)
                onClicked: {
                    view.positionViewAtIndex(model.index, ListView.Visible);
                    currentMailbox = shortMailboxName
                    currentMailboxLong = mailboxName
                    mailboxSelected(mailboxName)
                }
            }

            ListItems.Standard {
                id: progression
                width: units.gu(5)
                visible: model.mailboxHasChildMailboxes
                progression: true
                anchors.right: parent.right
                showDivider: false
                onClicked: {
                    view.positionViewAtIndex(model.index, ListView.Visible);
                    currentMailbox = shortMailboxName
                    currentMailboxLong = mailboxName
                    moveListViewLeft.start()
                    ++nestingDepth
                    mailboxPage.model.setRootItemByOffset(index)
                }
            }
        }
    }
    tools: ToolbarItems {
        back: ToolbarButton {
            // only show back button if nested
            visible: mailboxPage.isNestedSomewhere()
            action: Action {
                iconSource: Qt.resolvedUrl("./back.svg")
                text: qsTr("Back")
                onTriggered: mailboxPage.openParentMailbox()
            }
        }
        ToolbarButton {
            action: Action {
                iconSource: Qt.resolvedUrl("./settings.svg")
                text: qsTr("Settings")
                onTriggered: {
                    pageStack.push(Qt.resolvedUrl("SettingsTabs.qml"))
                }
            }
        }
    }

    flickable: view
    Scrollbar {
        flickableItem: view
    }

    SequentialAnimation {
        id: moveListViewLeft
        property int oneStepDuration: 100
        ScriptAction { script: mailboxPage.anchors.fill = undefined }
        PropertyAnimation { target: mailboxPage; properties: "x"; to: -mailboxPage.width; duration: moveListViewLeft.oneStepDuration }
        PropertyAction { target: mailboxPage; property: "x"; value: mailboxPage.width }
        PropertyAnimation { target: mailboxPage; properties: "x"; to: 0; duration: moveListViewLeft.oneStepDuration }
        ScriptAction { script: mailboxPage.anchors.fill = mailboxPage.parent }
    }

    SequentialAnimation {
        id: moveListViewRight
        property alias oneStepDuration: moveListViewLeft.oneStepDuration
        ScriptAction { script: mailboxPage.anchors.fill = undefined }
        PropertyAnimation { target: mailboxPage; properties: "x"; to: mailboxPage.width; duration: moveListViewRight.oneStepDuration }
        PropertyAction { target: mailboxPage; property: "x"; value: -mailboxPage.width }
        PropertyAnimation { target: mailboxPage; properties: "x"; to: 0; duration: moveListViewRight.oneStepDuration }
        ScriptAction { script: mailboxPage.anchors.fill = mailboxPage.parent }
    }
}
