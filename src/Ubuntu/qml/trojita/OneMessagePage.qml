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
import Ubuntu.Components 1.1
import QtWebKit 3.0
import QtWebKit.experimental 1.0
//import Qt5NAMWebView 1.0
import "Utils.js" as Utils

Page {
    id: oneMessagePage
    title: imapAccess.oneMessageModel.subject
    property alias url: messageView.url

    tools:  ToolbarItems {
        id: oneMailTools
        visible: true
        ToolbarButton {
            id: messageReadButton
            //            toggled: imapAccess.oneMessageModel.isMarkedRead
            //            iconSource: "image://theme/icon-m-toolbar-done-white" + (toggled ? "-selected" : "")
            onTriggered:  imapAccess.oneMessageModel.isMarkedRead = !imapAccess.oneMessageModel.isMarkedRead
        }

        ToolbarButton {
            id: messageDeleteButton
            //            toggled: imapAccess.oneMessageModel.isMarkedDeleted
            //            iconSource: "image://theme/icon-m-toolbar-delete-white" + (toggled ? "-selected" : "")
            onTriggered:  imapAccess.oneMessageModel.isMarkedDeleted = !imapAccess.oneMessageModel.isMarkedDeleted
        }
    }

    property string mailbox
    //    property alias url: messageView.url
    function handleChangedEnvelope() {
        if (status === PageStatus.Active && !imapAccess.oneMessageModel.hasValidIndex)
            appWindow.showHome()
    }

    Item {
        width: parent.width
        height: parent.height
        Flickable {
            id: view
            anchors.fill: parent
            contentHeight: col.height
            contentWidth: col.width
            Column {
                id: col
                AddressWidget {
                    caption: qsTr("From")
                    address: imapAccess.oneMessageModel.from
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("To")
                    address: imapAccess.oneMessageModel.to
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("Cc")
                    address: imapAccess.oneMessageModel.cc
                    width: view.width
                }
                AddressWidget {
                    caption: qsTr("Bcc")
                    address: imapAccess.oneMessageModel.bcc
                    width: view.width
                }

                Label {
                    id: dateLabel
                    width: view.width
                    text: qsTr("<b>Date:</b> ") + Utils.formatDateDetailed(imapAccess.oneMessageModel.date)
                }

                Label {
                    id: subjectLabel
                    width: view.width
                    wrapMode: Text.WordWrap
                    text: qsTr("<b>Subject:</b> ") + imapAccess.oneMessageModel.subject
                }

                Label {
                    id: messageLabel
                    width: view.width
                    wrapMode: Text.WordWrap
                    text: qsTr("<b>Message URL:</b> ")
                }

                WebView {
                    id: messageView
                    width: view.width
                    height: view.width
                    experimental {
                        urlSchemeDelegates: [
                            UrlSchemeDelegate {
                                scheme: "trojita-imap"
                                onReceivedRequest: {
                                    console.log('trojita-imap scheme called')
                                    messageLabel.text = qsTr("<b>Message URL:</b> ") + imapAccess.oneMessageModel.messageId
                                    imapAccess.msgQNAM.wrapQmlWebViewRequest(request, reply)
                                }
                            }
                        ]
                    }
                }
                // FIXME: move this to a dedicated page...
                Component {
                    id: attachmentItemDelegate
                    Label {
                        id: lbl
                        text: "Attachment " + (model.fileName.length ? model.fileName + " " : "") + "(" + model.mimeType +
                              (model.size ?
                                   "): " + imapAccess.prettySize(model.size) :
                                   ")")
                        width: attachmentsView.width
                        height: 40
                    }
                }

                ListView {
                    id: attachmentsView
                    interactive: false
                    width: view.width
                    // FIXME: magic constants...
                    height: count * 40 + 30
                    // FIXME: filter out the main part from the view (in C++, of course)
                    model: imapAccess.oneMessageModel.attachmentsModel
                    delegate: attachmentItemDelegate
                }
            }
        }

        Scrollbar{
            flickableItem: view
        }
    }
    flickable: view

    Component.onCompleted: imapAccess.oneMessageModel.envelopeChanged.connect(handleChangedEnvelope)
    Component.onDestruction: imapAccess.oneMessageModel.envelopeChanged.disconnect(handleChangedEnvelope)
}
