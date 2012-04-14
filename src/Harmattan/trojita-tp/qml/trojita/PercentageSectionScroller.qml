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
                periodicUpdateTimer.start()
            }

            onReleased: {
                container.dragging = false;
                mouseDownTimer.stop()
                periodicUpdateTimer.doIt()
                periodicUpdateTimer.stop()
            }

            onPositionChanged: {
                if (mouse.y < 0 || mouse.y > dragArea.height )
                    return;
                container.offset = mouse.y / dragArea.height
            }

            Timer {
                id: mouseDownTimer
                interval: 150

                onTriggered: {
                    container.dragging = true;
                }
            }

            Timer {
                function doIt() {
                    listView.positionViewAtIndex(Math.round(container.offset * (listView.count - 1)), ListView.Visible)
                }
                id: periodicUpdateTimer
                interval: 300
                repeat: true
                onTriggered: doIt()
            }
            states: [
                State {
                    name: "dragging"; when: container.dragging
                    PropertyChanges { target: container; color: Qt.rgba(1, 1, 1, 0.5) }
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
                color: Qt.rgba(0, 0, 0, 0.5)

                Label {
                    text: Math.round(100 * container.offset) + " %"
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
