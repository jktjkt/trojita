function isMailAddressValid(address) {
    return ! (address === null || address === undefined || address.length === 0)
}

function formatMailAddress(items) {
    if (!isMailAddressValid(items)) {
        return undefined
    } else if (items[0] !== null && items[0].length > 0) {
        return items[0] + " <" + items[2] + "@" + items[3] + ">"
    } else {
        return items[2] + "@" + items[3]
    }
}

function formatMailAddresses(addresses) {
    var res = Array()
    for (var i = 0; i < addresses.length; ++i) {
        res.push(formatMailAddress(addresses[i]))
    }
    return (addresses.length == 1) ? res[0] : res.join(", ")
}

function formatDate(date) {
    // if there's a better way to compare QDateTime::date with "today", well, please do tell me
    return Qt.formatDate(date, "YYYY-mm-dd") == Qt.formatDate(new Date(), "YYYY-mm-dd") ?
                Qt.formatTime(date) :
                Qt.formatDate(date)
}

function formatDateDetailed(date) {
    // if there's a better way to compare QDateTime::date with "today", well, please do tell me
    return Qt.formatDate(date, "YYYY-mm-dd") == Qt.formatDate(new Date(), "YYYY-mm-dd") ?
                Qt.formatTime(date) :
                Qt.formatDateTime(date)
}
