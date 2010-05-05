lessThan(QT_VERSION, 4.6) {
    error("Trojita requires Qt 4.6 or greater")
}

TEMPLATE = subdirs
SUBDIRS  = src
