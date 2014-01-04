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

/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Components project.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 1.1
import com.nokia.meego 1.0

Item {
    id: root

    property ListView listView

    Rectangle {
        id: container
        color: "transparent"
        width: 80
        height: listView.height
        x: listView.x + listView.width - width
        property bool dragging: false
        property double offset: 0

        MouseArea {
            id: dragArea
            objectName: "dragArea"
            anchors.fill: parent
            drag.target: tooltip
            drag.axis: Drag.YAxis
            drag.minimumY: 0 - tooltip.height/2
            drag.maximumY: height - tooltip.height/2

            onPressed: {
                container.offset = mouse.y / dragArea.height
                mouseDownTimer.start()
            }

            onReleased: {
                container.dragging = false;
                mouseDownTimer.stop()
                delayedRenderingTimer.doIt()
                delayedRenderingTimer.stop()
            }

            onPositionChanged: {
                if (mouse.y < 0 || mouse.y > dragArea.height )
                    return;
                container.offset = mouse.y / dragArea.height
                delayedScrollingTimer.start()
            }

            Timer {
                id: mouseDownTimer
                interval: 150

                onTriggered: {
                    container.dragging = true;
                    tooltip.positionAtY(dragArea.mouseY)
                }
            }

            Timer {
                function doIt() {
                    listView.massiveScrolling = false
                }
                id: delayedRenderingTimer
                interval: 150
                onTriggered: doIt()
            }
            Timer {
                function doIt() {
                    listView.massiveScrolling = true
                    listView.positionViewAtIndex(Math.round(container.offset * (listView.count - 1)), ListView.Visible)
                    delayedRenderingTimer.start()
                }
                id: delayedScrollingTimer
                interval: 50
                onTriggered: doIt()
            }
            states: [
                State {
                    name: "dragging"; when: container.dragging
                    PropertyChanges { target: container; color: Qt.rgba(1, 1, 1, theme.inverted ? 0.3 : 0.5) }
                }
            ]
        }
        Item {
            id: tooltip
            objectName: "popup"
            opacity: container.dragging ? 1 : 0
            anchors.right: parent.right
            width: listView.width
            height: childrenRect.height

            function positionAtY(yCoord) {
                tooltip.y = Math.max(dragArea.drag.minimumY, Math.min(yCoord - tooltip.height/2, dragArea.drag.maximumY));
            }

            Rectangle {
                id: background
                width: parent.width
                height: childrenRect.height// + 20
                anchors.left: parent.left
                color: theme.inverted ? Qt.rgba(1, 1, 1, 0.8) : Qt.rgba(0, 0, 0, 0.5)

                Label {
                    text: Math.round(100 * container.offset) + " %"
                    color: theme.inverted ? "black" : "white"
                    anchors.left: parent.left
                    anchors.leftMargin: UiConstants.DefaultMargin / 2
                }
            }

            states: [
                State {
                    name: "visible"
                    when: container.dragging
                },

                State {
                    extend: "visible"
                    name: "atTop"
                    when: internal.curPos === "first"
                    PropertyChanges {
                        target: currentSectionLabel
                        text: internal.nextSection
                    }
                },

                State {
                    extend: "visible"
                    name: "atBottom"
                    when: internal.curPos === "last"
                    PropertyChanges {
                        target: currentSectionLabel
                        text: internal.prevSection
                    }
                }
            ]

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }
        }
    }
}
