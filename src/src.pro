TEMPLATE = subdirs
SUBDIRS  = Imap MSA Streams qwwsmtpclient Common Composer
CONFIG += ordered

trojita_harmattan {
    SUBDIRS += Harmattan
} else {
    SUBDIRS += Gui
    XtConnect:SUBDIRS += XtConnect
}

TRANSLATIONS = trojita.ts
CODECFORTR = UTF-8
