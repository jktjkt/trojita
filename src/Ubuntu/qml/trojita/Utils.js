var contactIconColors = ["#B68B01", "#CA4B14", "#DB3131", "#D33781", "#6B72C3", "#298BD6", "#2BA098", "#859A01"]

function isMailAddressValid(address) {
    return ! (address === null || address === undefined || address.length === 0)
}

function formatMailAddress(items, nameOnly, addressOnly) {
    if (!isMailAddressValid(items)) {
        return undefined;
    } else if (items[0] !== null && items[0].length > 0) {
        var name = items[0];
        var address = "<a href=\"" + items[2] + "@" + items[3] + "\">"+ " " + items[2] + "@" + items[3] + "</a>";
        if (nameOnly) {
            return name;
        } else if (addressOnly) {
            return address;
        } else {
            return name + address;
        }
    } else {
        return items[2] + "@" + items[3]
    }
}

function formatMailAddresses(addresses, nameOnly, addressOnly) {
    if (!addresses) { return ""; }
    var res = Array()
    for (var i = 0; i < addresses.length; ++i) {
        res.push(formatMailAddress(addresses[i], nameOnly, addressOnly))
    }
    return (addresses.length == 1) ? res[0] : res.join(", ")
}

function formatDateDetailed(date) {
    // if there's a better way to compare
//    QDateTime::date with "today", well, please do tell me
    return Qt.formatDate(date, "YYYY-mm-dd") == Qt.formatDate(new Date(), "YYYY-mm-dd") ?
                Qt.formatTime(date) :
                Qt.formatDateTime(date)
}

function getIconColor(name) {
    var tmp = 0;
    for (var i = 0; i < name.length; i++) {
        tmp += name.charCodeAt(i);
    }
    return contactIconColors[tmp % contactIconColors.length];
}
