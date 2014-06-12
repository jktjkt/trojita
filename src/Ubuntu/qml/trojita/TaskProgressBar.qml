/* Copyright (C) 2014 Boren Zhang <bobo1993324@gmail.com>

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

Panel {
    id: panel
    locked: true
    anchors {
        left: parent.left
        right: parent.right
    }
    clip: true
    Rectangle {
        anchors.fill: parent
        color: "white"
    }

    height: progressBar.height
    Behavior on height {
        UbuntuNumberAnimation {}
    }
    ListView {
        id: view
        model: imapAccess.visibleTasksModel ? imapAccess.visibleTasksModel : null
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: progressBar.left
            left: parent.left
        }
        //only displays the first task
        delegate: Item{
            height: view.height
            width: view.width
            Label {
                anchors.centerIn: parent
                text: compactName
            }
        }
        onCountChanged: {
            if (count > 0)
                panel.open();
            else
                panel.close();
        }
    }

    ProgressBar {
        id: progressBar
        width: units.gu(15)
        anchors.right: parent.right
        anchors.rightMargin: units.gu(1)
        indeterminate: true
    }
}
