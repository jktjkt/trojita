
function formatMailAddress(items) {
    if (items[0] !== null) {
        return items[0] + " <" + items[2] + "@" + items[3] + ">"
    } else {
        return items[2] + "@" + items[3]
    }
}
