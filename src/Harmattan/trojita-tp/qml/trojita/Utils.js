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
