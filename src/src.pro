TEMPLATE = subdirs
SUBDIRS  = Imap MSA Streams qwwsmtpclient Common
CONFIG += ordered

trojita_harmattan {
    SUBDIRS += Harmattan
} else {
    SUBDIRS += Gui
    XtConnect:SUBDIRS += XtConnect
}
