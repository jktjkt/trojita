TEMPLATE = subdirs
SUBDIRS  = Imap MSA Streams qwwsmtpclient Common AppVersion Composer
CONFIG += ordered

lessThan(QT_MAJOR_VERSION, 5) {
    SUBDIRS += mimetypes-qt4
}

trojita_harmattan {
    SUBDIRS += QmlSupport Harmattan
} else {
    SUBDIRS += Gui be.contacts
    XtConnect:SUBDIRS += XtConnect
}

CODECFORTR = UTF-8
