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

import QtQuick 2.0
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItems
import "Utils.js" as Utils

Page {
    id: messageListPage
    property alias model: view.model
    property bool _pendingScroll: false
    property bool indexValid: model ? model.itemsValid : true
    title: qsTr("Messages")
    signal messageSelected(int uid)
    function scrollToBottom() {
        _pendingScroll = true
    }
    onIndexValidChanged: if (!indexValid) appWindow.showHome()

    Component {
        id: normalMessageItemDelegate
        Rectangle {
            property variant model

            height: units.gu(8)
            color: model.isMarkedRead ? "#CBCBCB": "transparent"
            width: view.width
            ListItems.Base {
                removable: true
                confirmRemoval: true
                height: parent.height
                onClicked: {
                    if (model.isFetched) {
                        messageSelected(model.messageUid)
                    }
                }
                UbuntuShape {
                    id: msgListContactImg
                    height: units.gu(6)
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
                        Image {
                            height: parent.height * 0.8
                            width: height
                            source: Qt.resolvedUrl("./contact_grey.svg");
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            id: fromLabel
                            text: Utils.formatMailAddresses(model.from, true)
                            color: "black"
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: units.gu(2.2)
                        }
                    }
                    Row {
                        height: units.gu(2)
                        spacing: units.gu(1)
                        Image {
                            height: parent.height * 0.8
                            width: height
                            source: Qt.resolvedUrl("./email.svg");
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: model.subject ? model.subject : qsTr('<No Subject>')
                            width: fromSubjectColumn.width - parent.height - units.gu(1)
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
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

                    Image {
                        height: units.gu(2)
                        width: height
                        source: model.isMarkedFlagged ? Qt.resolvedUrl("./favorite-selected.svg") : Qt.resolvedUrl("./favorite-unselected.svg")
                        anchors {
                            right: parent.right
                            rightMargin: units.gu(1)
                        }
                    }
                    Label{
                        id: timeLabel
                        text: Utils.formatDate(model.date)
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
        model: messageListPage.model
        width: parent.width
        height:  parent.height
        delegate: messageItemDelegate
    }

    flickable: view
}
