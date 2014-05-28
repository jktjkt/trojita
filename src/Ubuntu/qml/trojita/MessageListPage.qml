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
import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItems
import "Utils.js" as Utils
import trojita.UiFormatting 0.1
import trojita.models.ThreadingMsgListModel 0.1

Page {
    id: messageListPage
    property alias model: msgListDelegateModel.model
    property bool _pendingScroll: false
    property bool indexValid: imapAccess.msgListModel ? imapAccess.msgListModel.itemsValid : true
    title: qsTr("Messages")
    signal messageSelected(int uid)
    function scrollToBottom() {
        _pendingScroll = true
    }
    onIndexValidChanged: if (!indexValid) appWindow.showHome()

    onMessageSelected: {
        imapAccess.openMessage(mailboxList.currentMailboxLong, uid)
        pageStack.push(Qt.resolvedUrl("OneMessagePage.qml"),
                       {
                           mailbox: mailboxList.currentMailboxLong,
                           url: imapAccess.oneMessageModel.mainPartUrl
                       }
                       )
    }

    Component.onCompleted: {
        sortTimerSingleshot.start()
    }

    // We only need to fire this once.... well actually this gets
    // fired twice 1) onTriggeredStart 2) again at interval.
    Timer {
        id: sortTimerSingleshot
        repeat: false
        interval: 1
        onTriggered: sortUserPrefs()
    }

    function sortUserPrefs() {
        // We have to force the sort order each time the view is created
        // but force it using the current sort order already in the model
        imapAccess.threadingMsgListModel.setUserSearchingSortingPreference(
                    [],
                    ThreadingMsgListModel.SORT_NONE,
                    imapAccess.threadingMsgListModel.currentSortOrder()
                    )
    }

    VisualDataModel {
        id: msgListDelegateModel
        model: imapAccess.threadingMsgListModel ? imapAccess.threadingMsgListModel : undefined
        delegate: messageItemDelegate
    }

    Component {
        id: normalMessageItemDelegate
        Rectangle {
            property variant model
            height: baseItem.height
            color: (model && model.isMarkedRead) ? "#CBCBCB": "transparent"
            width: view.width
            ListItems.Base {
                id: baseItem
                removable: true
                onClicked: {
                    if (model.isFetched) {
                        messageSelected(model.messageUid)
                    }
                }
                onItemRemoved: {
                    imapAccess.markMessageDeleted(imapAccess.deproxifiedIndex(view.model.modelIndex(model.index)),
                                                  !model.isMarkedDeleted)
                    // set height to the original to show again
                    // the listitem should not fire onItemRemoved after hiding the item
                    // bug filed at https://bugs.launchpad.net/ubuntu-ui-toolkit/+bug/1313134
                    height = __height;
                }
                UbuntuShape {
                    id: msgListContactImg
                    height: units.gu(4.5)
                    width: height
                    color: Utils.getIconColor(fromLabel.text)
                    anchors {
                        top: parent.top
                        margins: units.gu(1)
                        left: parent.left
                    }
                    Image {
                        anchors.fill: parent
                        anchors.margins: units.gu(0.5)
                        source: Qt.resolvedUrl("./contact.svg")
                    }
                }
                Column {
                    id: fromSubjectColumn
                    spacing: units.gu(0.5)
                    clip: true
                    anchors {
                        left: msgListContactImg.right
                        top: parent.top
                        right: favTimColumn.left
                        margins: units.gu(1)
                    }

                    Row {
                        height: units.gu(2)
                        spacing: units.gu(1)

                        Label {
                            id: fromLabel
                            text: model ? Utils.formatMailAddresses(model.from, true) : ""
                            color: "black"
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: FontUtils.sizeToPixels("medium")
                            font.strikeout: model ? model.isMarkedDeleted : false
                        }
                    }
                    Row {
                        height: units.gu(2)
                        spacing: units.gu(1)

                        Label {
                            text: (model && model.subject) ? model.subject : qsTr('<No Subject>')
                            width: fromSubjectColumn.width - parent.height - units.gu(1)
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: FontUtils.sizeToPixels("small")
                            font.strikeout: model ? model.isMarkedDeleted : false
                        }
                    }
                }
                Column {
                    id: favTimColumn
                    spacing: units.gu(0.5)
                    width: timeLabel.width
                    anchors {
                        right: parent.right
                        top: parent.top
                        margins: units.gu(1)
                    }

                    Row{
                        height: units.gu(2)
                        spacing: units.gu(1)
                        Image {
                            visible: model && model.hasAttachments === true
                            height: units.gu(2)
                            width: height
                            source: Qt.resolvedUrl("./attach.svg")
                        }
                        Image {
                            height: units.gu(2)
                            width: height
                            source: (model && model.isMarkedFlagged) ? Qt.resolvedUrl("./favorite-selected.svg") : Qt.resolvedUrl("./favorite-unselected.svg")
                        }
                        anchors.right: parent.right
                    }
                    Label{
                        id: timeLabel
                        text: model ? UiFormatting.prettyDate(model.date) : ""
                        font.pixelSize: FontUtils.sizeToPixels("small")
                    }
                }
            }
        }
    }

    Component {
        id: scrollingMessageDelegate

        Item {
            width: view.width
            height: units.gu(8)
            Label {
                text: qsTr("Scrolling...")
                anchors.centerIn: parent
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

    ListView {
        property bool massiveScrolling
        id: view
        model: msgListDelegateModel
        width: parent.width
        height:  parent.height
    }

    flickable: view
    tools: MessageListToolbar{}
}
