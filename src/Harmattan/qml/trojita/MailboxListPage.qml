/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

Page {
    signal mailboxSelected(string mailbox)
    property int nestingDepth: 0
    property string viewTitle: isNestedSomewhere() ? currentMailbox : imapAccess.server
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

    id: root
    tools: commonTools

    Component {
        id: mailboxItemDelegate

        Item {
            width: parent.width
            height: UiConstants.ListItemHeightDefault
            anchors.margins: UiConstants.DefaultMargin

            Item {
                anchors {
                    top: parent.top; bottom: parent.bottom; left: parent.left;
                    right: moreIndicator.visible ? moreIndicator.left : parent.right
                    leftMargin: 6
                    rightMargin: 16
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        view.positionViewAtIndex(model.index, ListView.Visible);
                        if (mailboxIsSelectable) {
                            currentMailbox = shortMailboxName
                            currentMailboxLong = mailboxName
                            mailboxSelected(mailboxName)
                        }
                    }
                }
                Label {
                    id: titleText
                    text: shortMailboxName
                    font: UiConstants.TitleFont
                }
                Label {
                    id: messageCountsText
                    anchors.top: titleText.bottom
                    font: UiConstants.SubtitleFont
                    visible: mailboxIsSelectable && totalMessageCount !== undefined
                    text: totalMessageCount === 0 ?
                              "No messages" :
                              (totalMessageCount + " total, " + unreadMessageCount + " unread")
                }
                Label {
                    anchors.top: titleText.bottom
                    font: UiConstants.SubtitleFont
                    visible: mailboxIsSelectable && totalMessageCount === undefined
                    text: qsTr("Loading...")
                }
            }

            MoreIndicator {
                id: moreIndicator
                visible: mailboxHasChildMailboxes
                anchors {verticalCenter: parent.verticalCenter; right: parent.right}

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        view.positionViewAtIndex(model.index, ListView.Visible);
                        currentMailbox = shortMailboxName
                        currentMailboxLong = mailboxName
                        moveListViewLeft.start()
                        root.model.setRootItemByOffset(model.index)
                        ++nestingDepth
                    }
                }
            }
        }
    }

    Item {
        id: contentView
        anchors.fill: parent

        ListView {
            id: view
            anchors {
                top: header.bottom; left: parent.left; right: parent.right; bottom: parent.bottom
            }
            focus: true
            delegate: mailboxItemDelegate
            model: root.model
        }

        ScrollDecorator {
            flickableItem: view
        }

        PageHeader {
            id: header
            text: viewTitle
            anchors {left: parent.left; right: parent.right; top: parent.top}
        }
    }

    // FIXME: can we use shaders for these?
    SequentialAnimation {
        id: moveListViewLeft
        property int oneStepDuration: 100
        ScriptAction { script: contentView.anchors.fill = undefined }
        PropertyAnimation { target: contentView; properties: "x"; to: -contentView.width; duration: moveListViewLeft.oneStepDuration }
        PropertyAction { target: contentView; property: "x"; value: contentView.width }
        PropertyAnimation { target: contentView; properties: "x"; to: 0; duration: moveListViewLeft.oneStepDuration }
        ScriptAction { script: contentView.anchors.fill = contentView.parent }
    }

    SequentialAnimation {
        id: moveListViewRight
        property alias oneStepDuration: moveListViewLeft.oneStepDuration
        ScriptAction { script: contentView.anchors.fill = undefined }
        PropertyAnimation { target: contentView; properties: "x"; to: contentView.width; duration: moveListViewRight.oneStepDuration }
        PropertyAction { target: contentView; property: "x"; value: -contentView.width }
        PropertyAnimation { target: contentView; properties: "x"; to: 0; duration: moveListViewRight.oneStepDuration }
        ScriptAction { script: contentView.anchors.fill = contentView.parent }
    }
}
