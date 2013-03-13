TEMPLATE = subdirs
SUBDIRS  = Imap MSA Streams qwwsmtpclient Common Composer
CONFIG += ordered

trojita_harmattan {
    SUBDIRS += QmlSupport Harmattan
} else {
    SUBDIRS += Gui
    XtConnect:SUBDIRS += XtConnect
}

CODECFORTR = UTF-8
