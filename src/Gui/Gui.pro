# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core \
    gui \
    network \
    sql
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets webkitwidgets
} else {
    QT += webkit
}
include(../../install.pri)
include(../../configh.pri)
include(../Streams/ZlibLinking.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
TARGET = trojita
TEMPLATE = app
SOURCES += \
    ../Imap/Model/ModelTest/modeltest.cpp \
    main.cpp \
    MessageView.cpp \
    EmbeddedWebView.cpp \
    PartWidgetFactory.cpp \
    PartWidget.cpp \
    SimplePartWidget.cpp \
    Rfc822HeaderView.cpp \
    AttachmentView.cpp \
    LoadablePartWidget.cpp \
    Window.cpp \
    SettingsDialog.cpp \
    ComposeWidget.cpp \
    MsgListView.cpp \
    ExternalElementsWidget.cpp \
    ProtocolLoggerWidget.cpp \
    Util.cpp \
    AutoCompletion.cpp \
    TaskProgressIndicator.cpp \
    TagListWidget.cpp \
    FlowLayout.cpp \
    TagWidget.cpp \
    UserAgentWebPage.cpp \
    MessageListWidget.cpp \
    MailBoxTreeView.cpp \
    LocalAddressbook.cpp \
    AbookAddressbook.cpp \
    ComposerTextEdit.cpp \
    ComposerAttachmentsList.cpp \
    FromAddressProxyModel.cpp \
    MessageSourceWidget.cpp \
    FindBar.cpp \
    CompleteMessageWidget.cpp \
    ShortcutHandler/ShortcutConfigDialog.cpp \
    ShortcutHandler/ShortcutConfigWidget.cpp \
    ShortcutHandler/ShortcutHandler.cpp \
    LineEdit.cpp \
    PasswordDialog.cpp
HEADERS += \
    ../Imap/Model/ModelTest/modeltest.h \
    ComposeWidget.h \
    MessageView.h \
    SimplePartWidget.h \
    AbstractPartWidget.h \
    EmbeddedWebView.h \
    Window.h \
    Rfc822HeaderView.h \
    LoadablePartWidget.h \
    PartWidget.h \
    SettingsDialog.h \
    PartWidgetFactory.h \
    MsgListView.h \
    AttachmentView.h \
    ExternalElementsWidget.h \
    ProtocolLoggerWidget.h \
    IconLoader.h \
    Util.h \
    AutoCompletion.h \
    TaskProgressIndicator.h \
    TagListWidget.h \
    FlowLayout.h \
    TagWidget.h \
    UserAgentWebPage.h \
    MessageListWidget.h \
    MailBoxTreeView.h \
    AbstractAddressbook.h \
    LocalAddressbook.h \
    AbookAddressbook.h \
    ComposerTextEdit.h \
    ComposerAttachmentsList.h \
    FromAddressProxyModel.h \
    MessageSourceWidget.h \
    FindBar.h \
    CompleteMessageWidget.h \
    ShortcutHandler/ShortcutConfigDialog.h \
    ShortcutHandler/ShortcutConfigWidget.h \
    ShortcutHandler/ShortcutHandler.h \
    LineEdit.h \
    PasswordDialog.h
FORMS += CreateMailboxDialog.ui \
    ComposeWidget.ui \
    SettingsImapPage.ui \
    SettingsCachePage.ui \
    SettingsOutgoingPage.ui \
    SettingsGeneralPage.ui \
    EditIdentity.ui \
    ShortcutHandler/ShortcutConfigWidget.ui \
    PasswordDialog.ui

RESOURCES += ../icons.qrc

trojita_libs = AppVersion Imap MSA Streams qwwsmtpclient Common Composer
lessThan(QT_MAJOR_VERSION, 5) {
    trojita_libs += mimetypes-qt4
}

myprefix = ../
include(../linking.pri)

XtConnect:DEFINES += XTUPLE_CONNECT
XtConnect:RESOURCES += ../xtconnect-icons.qrc

include(../po2qm.prf)
PO_FILES = $$files(../../po/trojita_common_*.po)

unix {
    INSTALLS += desktop iconsvg icon32

    desktop.path = $$DATADIR/applications
    desktop.files += trojita.desktop

    iconsvg.path = $$DATADIR/icons/hicolor/scalable/apps
    iconsvg.files += ../icons/trojita.svg

    icon32.path = $$DATADIR/icons/hicolor/32x32/apps
    icon32.files += ../icons/trojita.png

    isEmpty(PO_FILES) {
        # The localization works by letting KDE run the top-level Messages.sh to produce a po/trojita_common.pot file. This
        # translation template is then used by the translator teams to produce po/trojita_common_${LANG}.po files which are
        # picked by qmake.
        # Please be careful, qmake only picks up changes in the glob when it is explicitly invoked. This means that the .po
        # files have to be in place before one runs qmake. Updates to the number of installed .po files are not picked up on
        # each call to `make`.
        message(No .po files found at po/trojita_common_*.po.)
    } else {
        INSTALLS += translations
        translations.path = $$PKGDATADIR
        translations.files += locale
    }
}
