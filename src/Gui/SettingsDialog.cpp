/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QDebug>
#include "SettingsDialog.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"

namespace Gui
{

QString SettingsDialog::warningStyleSheet = QLatin1String("border: 2px solid red; background-color: #E7C575; "
        "font-weight: bold; padding: 5px; margin: 5px; "
        "text-align: center;");

SettingsDialog::SettingsDialog(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel):
    QDialog(parent), m_senderIdentities(identitiesModel)
{
    setWindowTitle(tr("Settings"));
    QSettings s;

    QVBoxLayout *layout = new QVBoxLayout(this);
    stack = new QTabWidget(this);
    layout->addWidget(stack);
    stack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    general = new GeneralPage(this, s, m_senderIdentities);
    stack->addTab(general, tr("General"));
    imap = new ImapPage(stack, s);
    stack->addTab(imap, tr("IMAP"));
    cache = new CachePage(this, s);
    stack->addTab(cache, tr("Offline"));
    outgoing = new OutgoingPage(this, s);
    stack->addTab(outgoing, tr("SMTP"));
#ifdef XTUPLE_CONNECT
    xtConnect = new XtConnectPage(this, s, imap);
    stack->addTab(xtConnect, tr("xTuple"));
#endif

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    layout->addWidget(buttons);
}

void SettingsDialog::accept()
{
    QSettings s;
#ifndef Q_OS_WIN
    // Try to wour around QSettings' inability to set umask for its file access. We don't want to set umask globally.
    QFile settingsFile(s.fileName());
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif
    general->save(s);
    imap->save(s);
    cache->save(s);
    outgoing->save(s);
#ifdef XTUPLE_CONNECT
    xtConnect->save(s);
#endif
    s.sync();
#ifndef Q_OS_WIN
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif
    QDialog::accept();
}

GeneralPage::GeneralPage(QWidget *parent, QSettings &s, Composer::SenderIdentitiesModel *identitiesModel):
    QWidget(parent), m_identitiesModel(identitiesModel)
{
    QFormLayout *layout = new QFormLayout(this);
    Q_ASSERT(m_identitiesModel);
    QModelIndex nameIndex = m_identitiesModel->index(0, Composer::SenderIdentitiesModel::COLUMN_NAME);
    QModelIndex mailIndex = m_identitiesModel->index(0, Composer::SenderIdentitiesModel::COLUMN_EMAIL);
    realName = new QLineEdit(nameIndex.data().toString(), this);
    layout->addRow(tr("Real Name"), realName);
    address = new QLineEdit(mailIndex.data().toString(), this);
    layout->addRow(tr("E-mail"), address);
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    layout->addRow(separator);
    showHomepage = new QCheckBox(trUtf8("Show Trojitá's homepage on startup"), this);
    showHomepage->setChecked(s.value(Common::SettingsNames::appLoadHomepage, QVariant(true)).toBool());
    showHomepage->setToolTip(trUtf8("<p>If enabled, Trojitá will show its homepage upon startup.</p>"
                                    "<p>The remote server will receive the user's IP address and versions of Trojitá, the Qt library, "
                                    "and the underlying operating system. No private information, like account settings "
                                    "or IMAP server details, are collected.</p>"));
    layout->addRow(showHomepage);
}

void GeneralPage::save(QSettings &s)
{
    // FIXME: replace this block with support for multiple identities
    while (m_identitiesModel->rowCount()) {
        m_identitiesModel->removeIdentityAt(0);
    }
    m_identitiesModel->appendIdentity(Composer::ItemSenderIdentity(realName->text(), address->text()));
    m_identitiesModel->saveToSettings(s);

    s.setValue(Common::SettingsNames::appLoadHomepage, showHomepage->isChecked());
}

ImapPage::ImapPage(QWidget *parent, QSettings &s): QScrollArea(parent), Ui_ImapPage()
{
    Ui_ImapPage::setupUi(this);
    method->insertItem(0, tr("TCP"), QVariant(TCP));
    method->insertItem(1, tr("SSL"), QVariant(SSL));
    method->insertItem(2, tr("Local Process"), QVariant(PROCESS));
    using Common::SettingsNames;
    if (QSettings().value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodTCP) {
        method->setCurrentIndex(0);
    } else if (QSettings().value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodSSL) {
        method->setCurrentIndex(1);
    } else {
        method->setCurrentIndex(2);
    }

    imapHost->setText(s.value(SettingsNames::imapHostKey).toString());
    imapPort->setText(s.value(SettingsNames::imapPortKey, QString::number(Common::PORT_IMAP)).toString());
    imapPort->setValidator(new QIntValidator(1, 65535, this));
    passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    connect(imapPass, SIGNAL(textChanged(QString)), this, SLOT(maybeShowPasswordWarning()));
    startTls->setChecked(s.value(SettingsNames::imapStartTlsKey, true).toBool());
    imapUser->setText(s.value(SettingsNames::imapUserKey).toString());
    imapPass->setText(s.value(SettingsNames::imapPassKey).toString());
    processPath->setText(s.value(SettingsNames::imapProcessKey).toString());
    startOffline->setChecked(s.value(SettingsNames::imapStartOffline).toBool());
    imapEnableId->setChecked(s.value(SettingsNames::imapEnableId, true).toBool());
    imapCapabilitiesBlacklist->setText(s.value(SettingsNames::imapBlacklistedCapabilities).toStringList().join(QLatin1String(" ")));

    connect(method, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
    updateWidgets();
    maybeShowPasswordWarning();
}

void ImapPage::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    scrollAreaWidgetContents->setMinimumSize(event->size());
    scrollAreaWidgetContents->adjustSize();
}

void ImapPage::updateWidgets()
{
    QFormLayout *lay = formLayout;
    Q_ASSERT(lay);

    switch (method->itemData(method->currentIndex()).toInt()) {
    case TCP:
        imapHost->setEnabled(true);
        lay->labelForField(imapHost)->setEnabled(true);
        imapPort->setEnabled(true);
        if (imapPort->text().isEmpty() || imapPort->text() == QString::number(Common::PORT_IMAPS))
            imapPort->setText(QString::number(Common::PORT_IMAP));
        else if (QSettings().contains(Common::SettingsNames::imapPortKey))
            imapPort->setText(QSettings().value(Common::SettingsNames::imapPortKey).toString());
        lay->labelForField(imapPort)->setEnabled(true);
        startTls->setEnabled(true);
        startTls->setChecked(QSettings().value(Common::SettingsNames::imapStartTlsKey, true).toBool());
        lay->labelForField(startTls)->setEnabled(true);
        processPath->setEnabled(false);
        lay->labelForField(processPath)->setEnabled(false);
        break;
    case SSL:
        imapHost->setEnabled(true);
        lay->labelForField(imapHost)->setEnabled(true);
        imapPort->setEnabled(true);
        if (imapPort->text().isEmpty() || imapPort->text() == QString::number(Common::PORT_IMAP))
            imapPort->setText(QString::number(Common::PORT_IMAPS));
        else if (QSettings().contains(Common::SettingsNames::imapPortKey))
            imapPort->setText(QSettings().value(Common::SettingsNames::imapPortKey).toString());
        lay->labelForField(imapPort)->setEnabled(true);
        startTls->setEnabled(false);
        startTls->setChecked(true);
        lay->labelForField(startTls)->setEnabled(false);
        processPath->setEnabled(false);
        lay->labelForField(processPath)->setEnabled(false);
        break;
    default:
        imapHost->setEnabled(false);
        lay->labelForField(imapHost)->setEnabled(false);
        imapPort->setEnabled(false);
        lay->labelForField(imapPort)->setEnabled(false);
        startTls->setEnabled(false);
        lay->labelForField(startTls)->setEnabled(false);
        processPath->setEnabled(true);
        lay->labelForField(processPath)->setEnabled(true);
    }
}

void ImapPage::save(QSettings &s)
{
    using Common::SettingsNames;
    if (s.value(SettingsNames::imapHostKey) != imapHost->text()) {
        s.remove(Common::SettingsNames::imapSslPemCertificate);
    }
    switch (method->currentIndex()) {
    case TCP:
        s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
        s.setValue(SettingsNames::imapHostKey, imapHost->text());
        s.setValue(SettingsNames::imapPortKey, imapPort->text());
        s.setValue(SettingsNames::imapStartTlsKey, startTls->isChecked());
        break;
    case SSL:
        s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodSSL);
        s.setValue(SettingsNames::imapHostKey, imapHost->text());
        s.setValue(SettingsNames::imapPortKey, imapPort->text());
        s.setValue(SettingsNames::imapStartTlsKey, startTls->isChecked());
        break;
    default:
        s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodProcess);
        s.setValue(SettingsNames::imapProcessKey, processPath->text());
    }
    s.setValue(SettingsNames::imapUserKey, imapUser->text());
    s.setValue(SettingsNames::imapPassKey, imapPass->text());
    s.setValue(SettingsNames::imapStartOffline, startOffline->isChecked());
    s.setValue(SettingsNames::imapEnableId, imapEnableId->isChecked());
    s.setValue(SettingsNames::imapBlacklistedCapabilities, imapCapabilitiesBlacklist->text().split(QChar(' ')));
}

void ImapPage::maybeShowPasswordWarning()
{
    passwordWarning->setVisible(!imapPass->text().isEmpty());
}

CachePage::CachePage(QWidget *parent, QSettings &s): QScrollArea(parent), Ui_CachePage()
{
    Ui_CachePage::setupUi(this);

    using Common::SettingsNames;

    QString val = s.value(SettingsNames::cacheOfflineKey).toString();
    if (val == SettingsNames::cacheOfflineAll) {
        offlineEverything->setChecked(true);
    } else if (val == SettingsNames::cacheOfflineNone) {
        offlineNope->setChecked(true);
    } else {
        offlineXDays->setChecked(true);
    }

    offlineNumberOfDays->setValue(s.value(SettingsNames::cacheOfflineNumberDaysKey, QVariant(30)).toInt());

    updateWidgets();
    connect(offlineNope, SIGNAL(clicked()), this, SLOT(updateWidgets()));
    connect(offlineXDays, SIGNAL(clicked()), this, SLOT(updateWidgets()));
    connect(offlineEverything, SIGNAL(clicked()), this, SLOT(updateWidgets()));
}

void CachePage::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    scrollAreaWidgetContents->setMinimumSize(event->size());
    scrollAreaWidgetContents->adjustSize();
}

void CachePage::updateWidgets()
{
    offlineNumberOfDays->setEnabled(offlineXDays->isChecked());
}

void CachePage::save(QSettings &s)
{
    using Common::SettingsNames;

    if (offlineEverything->isChecked())
        s.setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineAll);
    else if (offlineXDays->isChecked())
        s.setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineXDays);
    else
        s.setValue(SettingsNames::cacheOfflineKey, SettingsNames::cacheOfflineNone);

    s.setValue(SettingsNames::cacheOfflineNumberDaysKey, offlineNumberOfDays->value());
}

OutgoingPage::OutgoingPage(QWidget *parent, QSettings &s): QScrollArea(parent), Ui_OutgoingPage()
{
    using Common::SettingsNames;
    Ui_OutgoingPage::setupUi(this);
    method->insertItem(0, tr("SMTP"), QVariant(SMTP));
    method->insertItem(1, tr("Secure SMTP"), QVariant(SSMTP));
    method->insertItem(2, tr("Local sendmail-compatible"), QVariant(SENDMAIL));
    method->insertItem(3, tr("IMAP SENDMAIL Extension"), QVariant(IMAP_SENDMAIL));
    QString selectedMethod = s.value(SettingsNames::msaMethodKey).toString();
    if (selectedMethod == SettingsNames::methodSMTP) {
        method->setCurrentIndex(0);
    } else if (selectedMethod == SettingsNames::methodSSMTP) {
        method->setCurrentIndex(1);
    } else if (selectedMethod == SettingsNames::methodSENDMAIL) {
        method->setCurrentIndex(2);
    } else if (selectedMethod == SettingsNames::methodImapSendmail) {
        method->setCurrentIndex(3);
    }

    smtpHost->setText(s.value(SettingsNames::smtpHostKey).toString());
    smtpPort->setText(s.value(SettingsNames::smtpPortKey, Common::PORT_SMTP_SUBMISSION).toString());
    smtpPort->setValidator(new QIntValidator(1, 65535, this));
    smtpStartTls->setChecked(s.value(SettingsNames::smtpStartTlsKey).toBool());
    smtpAuth->setChecked(s.value(SettingsNames::smtpAuthKey, false).toBool());
    smtpUser->setText(s.value(SettingsNames::smtpUserKey).toString());
    smtpPass->setText(s.value(SettingsNames::smtpPassKey).toString());
    sendmail->setText(s.value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString());
    saveToImap->setChecked(s.value(SettingsNames::composerSaveToImapKey, true).toBool());
    // Would be cool to support the special-use mailboxes
    saveFolderName->setText(s.value(SettingsNames::composerImapSentKey, QLatin1String("Sent")).toString());
    smtpBurl->setChecked(s.value(SettingsNames::smtpUseBurlKey, false).toBool());

    connect(method, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
    connect(smtpAuth, SIGNAL(toggled(bool)), this, SLOT(updateWidgets()));
    connect(saveToImap, SIGNAL(toggled(bool)), this, SLOT(updateWidgets()));
    updateWidgets();
}

void OutgoingPage::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    scrollAreaWidgetContents->setMinimumSize(event->size());
    scrollAreaWidgetContents->adjustSize();
}

void OutgoingPage::updateWidgets()
{
    QFormLayout *lay = formLayout;
    Q_ASSERT(lay);
    int smtpMethod = method->itemData(method->currentIndex()).toInt();
    switch (smtpMethod) {
    case SMTP:
    case SSMTP:
    {
        smtpHost->setEnabled(true);
        lay->labelForField(smtpHost)->setEnabled(true);
        smtpPort->setEnabled(true);
        lay->labelForField(smtpPort)->setEnabled(true);
        smtpStartTls->setEnabled(smtpMethod == SMTP);
        lay->labelForField(smtpStartTls)->setEnabled(smtpMethod == SMTP);
        smtpAuth->setEnabled(true);
        lay->labelForField(smtpAuth)->setEnabled(true);
        bool authEnabled = smtpAuth->isChecked();
        smtpUser->setEnabled(authEnabled);
        lay->labelForField(smtpUser)->setEnabled(authEnabled);
        smtpPass->setEnabled(authEnabled);
        lay->labelForField(smtpPass)->setEnabled(authEnabled);
        sendmail->setEnabled(false);
        lay->labelForField(sendmail)->setEnabled(false);
        smtpBurl->setEnabled(saveToImap->isChecked());
        lay->labelForField(smtpBurl)->setEnabled(saveToImap->isChecked());

        // Toggle the default ports upon changing the delivery method
        if (smtpMethod == SMTP && (smtpPort->text().isEmpty() ||
                                   smtpPort->text() == QString::number(Common::PORT_SMTP_SSL))) {
            smtpPort->setText(QString::number(Common::PORT_SMTP_SUBMISSION));
        } else if (smtpMethod == SSMTP && (smtpPort->text().isEmpty() ||
                                           smtpPort->text() == QString::number(Common::PORT_SMTP_OBSOLETE) ||
                                           smtpPort->text() == QString::number(Common::PORT_SMTP_SUBMISSION))) {
            smtpPort->setText(QString::number(Common::PORT_SMTP_SSL));
        }
        break;
    }
    case SENDMAIL:
    case IMAP_SENDMAIL:
        smtpHost->setEnabled(false);
        lay->labelForField(smtpHost)->setEnabled(false);
        smtpPort->setEnabled(false);
        lay->labelForField(smtpPort)->setEnabled(false);
        smtpStartTls->setEnabled(false);
        lay->labelForField(smtpStartTls)->setEnabled(false);
        smtpAuth->setEnabled(false);
        lay->labelForField(smtpAuth)->setEnabled(false);
        smtpUser->setEnabled(false);
        lay->labelForField(smtpUser)->setEnabled(false);
        smtpPass->setEnabled(false);
        lay->labelForField(smtpPass)->setEnabled(false);
        if (smtpMethod == SENDMAIL) {
            sendmail->setEnabled(true);
            lay->labelForField(sendmail)->setEnabled(true);
            if (sendmail->text().isEmpty())
                sendmail->setText(Common::SettingsNames::sendmailDefaultCmd);
            saveToImap->setEnabled(true);
            lay->labelForField(saveToImap)->setEnabled(true);
        } else {
            sendmail->setEnabled(false);
            lay->labelForField(sendmail)->setEnabled(false);
            saveToImap->setChecked(true);
            saveToImap->setEnabled(false);
            lay->labelForField(saveToImap)->setEnabled(false);
        }
        smtpBurl->setEnabled(false);
        lay->labelForField(smtpBurl)->setEnabled(false);
    }
    saveFolderName->setEnabled(saveToImap->isChecked());
}

void OutgoingPage::save(QSettings &s)
{
    using Common::SettingsNames;
    switch (method->currentIndex()) {
    case SMTP:
        s.setValue(SettingsNames::msaMethodKey, SettingsNames::methodSMTP);
        s.setValue(SettingsNames::smtpHostKey, smtpHost->text());
        s.setValue(SettingsNames::smtpPortKey, smtpPort->text());
        s.setValue(SettingsNames::smtpStartTlsKey, smtpStartTls->isChecked());
        s.setValue(SettingsNames::smtpAuthKey, smtpAuth->isChecked());
        s.setValue(SettingsNames::smtpUserKey, smtpUser->text());
        s.setValue(SettingsNames::smtpPassKey, smtpPass->text());
        break;
    case SSMTP:
        s.setValue(SettingsNames::msaMethodKey, SettingsNames::methodSSMTP);
        s.setValue(SettingsNames::smtpHostKey, smtpHost->text());
        s.setValue(SettingsNames::smtpPortKey, smtpPort->text());
        s.setValue(SettingsNames::smtpAuthKey, smtpAuth->isChecked());
        s.setValue(SettingsNames::smtpUserKey, smtpUser->text());
        s.setValue(SettingsNames::smtpPassKey, smtpPass->text());
        break;
    case SENDMAIL:
        s.setValue(SettingsNames::msaMethodKey, SettingsNames::methodSENDMAIL);
        s.setValue(SettingsNames::sendmailKey, sendmail->text());
        break;
    case IMAP_SENDMAIL:
        s.setValue(SettingsNames::msaMethodKey, SettingsNames::methodImapSendmail);
        break;
    }
    s.setValue(SettingsNames::composerSaveToImapKey, saveToImap->isChecked());
    if (saveToImap->isChecked()) {
        s.setValue(SettingsNames::composerImapSentKey, saveFolderName->text());
        s.setValue(SettingsNames::smtpUseBurlKey, smtpBurl->isChecked());
    } else {
        // BURL depends on having that message available on IMAP somewhere
        s.setValue(SettingsNames::smtpUseBurlKey, false);
    }
}

#ifdef XTUPLE_CONNECT
XtConnectPage::XtConnectPage(QWidget *parent, QSettings &s, ImapPage *imapPage): QWidget(parent), imap(imapPage)
{
    // Take care not to clash with the cache of the GUI
    QString cacheLocation = QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
    if (cacheLocation.isEmpty())
        cacheLocation = QDir::homePath() + QLatin1String("/.xtconnect-trojita");
    else
        cacheLocation += QString::fromAscii("/xtconnect-trojita");
    QFormLayout *layout = new QFormLayout(this);
    cacheDir = new QLineEdit(s.value(Common::SettingsNames::xtConnectCacheDirectory, cacheLocation).toString(), this);
    layout->addRow(tr("Cache Directory"), cacheDir);

    QGroupBox *box = new QGroupBox(tr("Mailboxes to synchronize"), this);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    QListWidget *mailboxes = new QListWidget(box);
    mailboxes->addItems(QSettings().value(Common::SettingsNames::xtSyncMailboxList).toStringList());
    for (int i = 0; i < mailboxes->count(); ++i) {
        mailboxes->item(i)->setFlags(Qt::ItemIsEnabled);
    }
    mailboxes->setToolTip(tr("Please use context menu inside the main application to select mailboxes to synchronize"));
    boxLayout->addWidget(mailboxes);
    layout->addRow(box);

    QString optionHost = s.value(Common::SettingsNames::xtDbHost).toString();
    int optionPort = s.value(Common::SettingsNames::xtDbPort, QVariant(5432)).toInt();
    QString optionDbname = s.value(Common::SettingsNames::xtDbDbName).toString();
    QString optionUsername = s.value(Common::SettingsNames::xtDbUser).toString();

    QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.length(); i++) {
        if (args.at(i) == "-h" && args.length() > i)
            optionHost = args.at(++i);
        else if (args.at(i) == "-d" && args.length() > i)
            optionDbname = args.at(++i);
        else if (args.at(i) == "-p" && args.length() > i)
            optionPort = args.at(++i).toInt();
        else if (args.at(i) == "-U" && args.length() > i)
            optionUsername = args.at(++i);
    }


    hostName = new QLineEdit(optionHost);
    layout->addRow(tr("DB Hostname"), hostName);
    port = new QSpinBox();
    port->setRange(1, 65535);
    port->setValue(optionPort);
    layout->addRow(tr("DB Port"), port);
    dbName = new QLineEdit(optionDbname);
    layout->addRow(tr("DB Name"), dbName);
    username = new QLineEdit(optionUsername);
    layout->addRow(tr("DB Username"), username);

    imapPasswordWarning = new QLabel(tr("Please fill in all IMAP options, including the password, at the IMAP page. "
                                        "If you do not save the password, background synchronization will not run."), this);
    imapPasswordWarning->setWordWrap(true);
    imapPasswordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    layout->addRow(imapPasswordWarning);
    debugLog = new QCheckBox();
    layout->addRow(tr("Debugging"), debugLog);

    QPushButton *btn = new QPushButton(tr("Run xTuple Synchronization"));
    connect(btn, SIGNAL(clicked()), this, SLOT(runXtConnect()));
    layout->addRow(btn);
}

void XtConnectPage::save(QSettings &s)
{
    s.setValue(Common::SettingsNames::xtConnectCacheDirectory, cacheDir->text());
    s.setValue(Common::SettingsNames::xtDbHost, hostName->text());
    s.setValue(Common::SettingsNames::xtDbPort, port->value());
    s.setValue(Common::SettingsNames::xtDbDbName, dbName->text());
    s.setValue(Common::SettingsNames::xtDbUser, username->text());
    saveXtConfig();
}

void XtConnectPage::saveXtConfig()
{
    QSettings s(QSettings::UserScope, QString::fromAscii("xTuple.com"), QString::fromAscii("xTuple"));

    // Copy the IMAP settings
    Q_ASSERT(imap);
    imap->save(s);

    // XtConnect-specific stuff
    s.setValue(Common::SettingsNames::xtConnectCacheDirectory, cacheDir->text());
    QStringList keys = QStringList() <<
                       Common::SettingsNames::xtSyncMailboxList <<
                       Common::SettingsNames::xtDbHost <<
                       Common::SettingsNames::xtDbPort <<
                       Common::SettingsNames::xtDbDbName <<
                       Common::SettingsNames::xtDbUser <<
                       Common::SettingsNames::imapSslPemCertificate;
    Q_FOREACH(const QString &key, keys) {
        s.setValue(key, QSettings().value(key));
    }
}

void XtConnectPage::runXtConnect()
{
    // First of all, let's save the XTuple-specific configuration to save useless debugging
    saveXtConfig();

    QString path = QCoreApplication::applicationFilePath();
    QStringList args;

#ifdef Q_OS_WIN
    path = path.replace(
               QLatin1String("Gui/debug/trojita"),
               QLatin1String("XtConnect/debug/xtconnect-trojita")).replace(
               QLatin1String("Gui/release/trojita"),
               QLatin1String("XtConnect/release/xtconnect-trojita"));
    QString cmd = path;
#else
    path = path.replace(QLatin1String("src/Gui/trojita"),
                        QLatin1String("src/XtConnect/xtconnect-trojita"));
    args << QLatin1String("-e") << path;
    QString cmd = QLatin1String("xterm");
#endif

    if (! hostName->text().isEmpty())
        args << QLatin1String("-h") << hostName->text();

    if (port->value() != 5432)
        args << QLatin1String("-p") << QString::number(port->value());

    if (! dbName->text().isEmpty())
        args << QLatin1String("-d") << dbName->text();

    if (! username->text().isEmpty())
        args << QLatin1String("-U") << username->text();

    QString password = QInputDialog::getText(this, tr("Database Connection"), tr("Password"), QLineEdit::Password);
    args << QLatin1String("-W") << password;

    if (debugLog->isChecked())
        args << QLatin1String("--log") << cacheDir->text() + QLatin1String("/xt-trojita-log");

    QProcess::startDetached(cmd, args);
}

void XtConnectPage::showEvent(QShowEvent *event)
{
    if (imap) {
        imapPasswordWarning->setVisible(! imap->hasPassword());
    }
    QWidget::showEvent(event);
}

bool ImapPage::hasPassword() const
{
    return ! imapPass->text().isEmpty();
}

#endif

}
