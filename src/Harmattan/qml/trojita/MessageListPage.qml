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
import "Utils.js" as Utils

Page {
    id: root
    property alias model: view.model
    property bool _pendingScroll: false
    property bool indexValid: model ? model.itemsValid : true

    signal messageSelected(int uid)

    function scrollToBottom() {
        _pendingScroll = true
    }

    onIndexValidChanged: if (!indexValid) appWindow.showHome()

    tools: commonTools

    Component {
        id: normalMessageItemDelegate

        Item {
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
                    text: !col.visible ? "" : (model.subject.length ? model.subject : qsTr("(No subject)"))
                    color: !col.visible ? platformStyle.textColor :
                                          model.isMarkedRead ?
                                              (model.isMarkedDeleted ? Qt.darker(platformStyle.textColor) : platformStyle.textColor) :
                                              theme.selectionColor
                    font {
                        pixelSize: UiConstants.TitleFont.pixelSize
                        family: UiConstants.TitleFont.family
                        bold: UiConstants.TitleFont.bold
                        italic: col.visible ? !model.subject.length : false
                    }
                    font.strikeout: !col.visible ? false : model.isMarkedDeleted
                }
                Label {
                    font: UiConstants.SubtitleFont
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    width: parent.width
                    text: !col.visible ? "" : Utils.formatMailAddresses(model.from)
                }
                Label {
                    width: parent.width
                    font: UiConstants.BodyTextFont
                    text: !col.visible ? "" : Utils.formatDate(model.date)
                }
            }
            MouseArea {
                anchors.fill: col
                onClicked: {
                    // Do not try to open messages which are still loading, the viewer code doesn't like that and will assert angrily
                    if (model.isFetched) {
                        messageSelected(model.messageUid)
                    }
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

            onVisibleChanged: {
                if (root._pendingScroll && count > 0) {
                    root._pendingScroll = false
                    positionViewAtEnd()
                }
            }
        }

        Label {
            visible: !view.visible
            anchors.fill: parent
            text: (root.model && root.model.itemsValid) ? qsTr("Empty Mailbox") : qsTr("Invalid Mailbox")
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
        text: mailboxListPage.currentMailbox
        anchors {left: parent.left; right: parent.right; top: parent.top}
    }
}
