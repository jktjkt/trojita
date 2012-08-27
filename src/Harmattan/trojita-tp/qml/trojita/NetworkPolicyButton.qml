import QtQuick 1.1
import com.nokia.meego 1.0
import com.nokia.extras 1.1

ToolIcon {
    id: networkPolicyButton
    iconSource: "image://theme/icon-s-common-presence-" + (state.length ? state : "unknown")

    states: [
        State {
            name: "offline"
            when: imapAccess.imapModel && !imapAccess.imapModel.isNetworkAvailable
        },

        State {
            name: "away"
            when: imapAccess.imapModel && imapAccess.imapModel.isNetworkAvailable && !imapAccess.imapModel.isNetworkOnline
        },

        State {
            name: "online"
            when: imapAccess.imapModel && imapAccess.imapModel.isNetworkAvailable && imapAccess.imapModel.isNetworkOnline
        }
    ]

    onClicked: {
        if (state == "offline") {
            imapAccess.imapModel.setNetworkOnline()
        } else if (state == "online") {
            imapAccess.imapModel.setNetworkExpensive()
        } else if (state == "away") {
            imapAccess.imapModel.setNetworkOffline()
        } else {
            // The model is not set up yet -> show settings
            serverSettings.open()
        }
    }
}

