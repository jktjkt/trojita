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

Rectangle {
    property alias text: lbl.text

    color: theme.selectionColor
    anchors {
        left: parent.left; right: parent.right
        bottomMargin: screen.currentOrientation == Screen.Landscape ? UiConstants.HeaderDefaultBottomSpacingLandscape :
                                                                      UiConstants.HeaderDefaultBottomSpacingPortrait
    }
    height: screen.currentOrientation == Screen.Landscape ? UiConstants.HeaderDefaultHeightLandscape :
                                                            UiConstants.HeaderDefaultHeightPortrait
    Label {
        id: lbl
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font: UiConstants.HeaderFont
        maximumLineCount: 1
        elide: Text.ElideRight
        width: parent.width - busyIndicator.width
    }

    BusyIndicator {
        id: busyIndicator
        visible: imapAccess.visibleTasksModel ? imapAccess.visibleTasksModel.hasVisibleTasks : false
        running: visible
        anchors { right: parent.right; verticalCenter: parent.verticalCenter; margins: UiConstants.DefaultMargin }
        MouseArea {
            anchors.fill: parent
            onClicked: activeTasks.state = activeTasks.state == "shown" ? "hidden" : "shown"
        }
    }

    Rectangle {
        id: activeTasks
        width: 0.8 * parent.width
        height: state == "shown" ?
                    Math.max(Math.min(view.count * (view.itemHeight + view.spacing) + 2 * UiConstants.DefaultMargin, 350), 60) :
                    1
        anchors.right: parent.right
        color: Qt.rgba(0.1, 0.1, 0.1, 0.9)
        z: -1 // below the title bar, but still above the real page contents

        states: [
            State {
                name: "shown"
                AnchorChanges { target: activeTasks; anchors.top: parent.bottom }
            },
            State {
                name: "hidden"
                AnchorChanges { target: activeTasks; anchors.top: parent.top }
            }
        ]
        state: "hidden"

        transitions: Transition {
            AnchorAnimation { duration: 200 }
        }

        Behavior on height {
            NumberAnimation { duration: 200 }
        }

        Timer {
            interval: 3000
            running: view.count == 0
            onTriggered: activeTasks.state = "hidden"
        }

        ListView {
            property int itemHeight: 34
            id: view
            clip: true
            anchors.fill: parent
            anchors.margins: UiConstants.DefaultMargin
            spacing: UiConstants.ButtonSpacing
            visible: count > 0

            model: imapAccess ? imapAccess.visibleTasksModel : undefined

            delegate: Label {
                id: delegate
                text: compactName
                color: "white"
                height: view.itemHeight
            }
        }
        Label {
            anchors.centerIn: parent
            anchors.margins: UiConstants.DefaultMargin
            visible: !view.visible
            text: qsTr("No activity")
        }

        ScrollDecorator {
            flickableItem: view
        }
    }
}
