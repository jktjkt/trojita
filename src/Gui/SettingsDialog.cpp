/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2012        Mohammed Nafees <nafees.technocool@gmail.com>

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
#include <QStandardItemModel>
#include <QToolTip>
#include <QMessageBox>
#include <QDataWidgetMapper>
#include "SettingsDialog.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Gui/Util.h"

namespace Gui
{

QString SettingsDialog::warningStyleSheet = QLatin1String("border: 2px solid red; background-color: #E7C575; color: black; "
        "font-weight: bold; padding: 5px; margin: 5px; "
        "text-align: center;");

/** @short Check a text field for being non empty. If it's empty, show an error to the user. */
template<typename T>
bool checkProblemWithEmptyTextField(T *field, const QString &message)
{
    if (field->text().isEmpty()) {
        QToolTip::showText(field->mapToGlobal(QPoint(10, field->height() / 2)), message, 0);
        return true;
    } else {
        return false;
    }
}

SettingsDialog::SettingsDialog(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel, QSettings *settings):
    QDialog(parent), m_senderIdentities(identitiesModel), m_settings(settings)
{
    setWindowTitle(tr("Settings"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    stack = new QTabWidget(this);
    layout->addWidget(stack);
    stack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    addPage(new GeneralPage(this, *m_settings, m_senderIdentities), tr("&General"));
    addPage(new ImapPage(stack, *m_settings), tr("I&MAP"));
    addPage(new CachePage(this, *m_settings), tr("&Offline"));
    addPage(new OutgoingPage(this, *m_settings), tr("&SMTP"));
#ifdef XTUPLE_CONNECT
    xtConnect = new XtConnectPage(this, *m_settings, imap);
    stack->addTab(xtConnect, tr("&xTuple"));
#endif

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    layout->addWidget(buttons);
}

void SettingsDialog::accept()
{
    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        if (!page->checkValidity()) {
            stack->setCurrentWidget(page->asWidget());
            return;
        }
    }

#ifndef Q_OS_WIN
    // Try to wour around QSettings' inability to set umask for its file access. We don't want to set umask globally.
    QFile settingsFile(m_settings->fileName());
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif
    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        page->save(*m_settings);
    }
#ifdef XTUPLE_CONNECT
    xtConnect->save(*m_settings);
#endif
    m_settings->sync();
#ifndef Q_OS_WIN
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif
    QDialog::accept();
}

void SettingsDialog::reject()
{
    // The changes were performed on the live data, so we have to make sure they are discarded when user cancels
    m_senderIdentities->loadFromSettings(*m_settings);
    QDialog::reject();
}

void SettingsDialog::addPage(ConfigurationWidgetInterface *page, const QString &title)
{
    stack->addTab(page->asWidget(), title);
    pages << page;
}

GeneralPage::GeneralPage(QWidget *parent, QSettings &s, Composer::SenderIdentitiesModel *identitiesModel):
    QScrollArea(parent), Ui_GeneralPage(), m_identitiesModel(identitiesModel)
{
    Ui_GeneralPage::setupUi(this);
    Q_ASSERT(m_identitiesModel);
    editButton->setEnabled(false);
    deleteButton->setEnabled(false);
    moveUpButton->setIcon(QIcon::fromTheme("go-up", QIcon(":/icons/arrow-up.png")));
    moveDownButton->setIcon(QIcon::fromTheme("go-down", QIcon(":/icons/arrow-down.png")));
    moveUpButton->setEnabled(false);
    moveDownButton->setEnabled(false);
    identityTabelView->setModel(m_identitiesModel);
    identityTabelView->setSelectionBehavior(QAbstractItemView::SelectRows);
    identityTabelView->setSelectionMode(QAbstractItemView::SingleSelection);
    identityTabelView->setGridStyle(Qt::NoPen);
    identityTabelView->hideColumn(Composer::SenderIdentitiesModel::COLUMN_ORGANIZATION);
    identityTabelView->setColumnHidden(Composer::SenderIdentitiesModel::COLUMN_SIGNATURE, true);
    identityTabelView->resizeColumnToContents(Composer::SenderIdentitiesModel::COLUMN_NAME);
    identityTabelView->resizeRowsToContents();
    identityTabelView->horizontalHeader()->setStretchLastSection(true);

    showHomepageCheckbox->setChecked(s.value(Common::SettingsNames::appLoadHomepage, QVariant(true)).toBool());
    showHomepageCheckbox->setToolTip(trUtf8("<p>If enabled, Trojitá will show its homepage upon startup.</p>"
                                        "<p>The remote server will receive the user's IP address and versions of Trojitá, the Qt library, "
                                        "and the underlying operating system. No private information, like account settings "
                                        "or IMAP server details, are collected.</p>"));

    guiSystrayCheckbox->setChecked(s.value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool());

    preferPlaintextCheckbox->setChecked(s.value(Common::SettingsNames::guiPreferPlaintextRendering).toBool());

    connect(identityTabelView, SIGNAL(clicked(QModelIndex)), SLOT(updateWidgets()));
    connect(identityTabelView, SIGNAL(doubleClicked(QModelIndex)), SLOT(editButtonClicked()));
    connect(m_identitiesModel, SIGNAL(layoutChanged()), SLOT(updateWidgets()));
    connect(m_identitiesModel, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(updateWidgets()));
    connect(m_identitiesModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(updateWidgets()));
    connect(m_identitiesModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updateWidgets()));
    connect(moveUpButton, SIGNAL(clicked()), SLOT(moveIdentityUp()));
    connect(moveDownButton, SIGNAL(clicked()), SLOT(moveIdentityDown()));
    connect(addButton, SIGNAL(clicked()), SLOT(addButtonClicked()));
    connect(editButton, SIGNAL(clicked()), SLOT(editButtonClicked()));
    connect(deleteButton, SIGNAL(clicked()), SLOT(deleteButtonClicked()));
}

void GeneralPage::updateWidgets()
{
    bool enabled = identityTabelView->currentIndex().isValid();
    deleteButton->setEnabled(enabled);
    editButton->setEnabled(enabled);
    bool upEnabled = m_identitiesModel->rowCount() > 0 && identityTabelView->currentIndex().row() > 0;
    bool downEnabled = m_identitiesModel->rowCount() > 0 && identityTabelView->currentIndex().row() < m_identitiesModel->rowCount() - 1;
    moveUpButton->setEnabled(upEnabled);
    moveDownButton->setEnabled(downEnabled);

    identityTabelView->resizeColumnToContents(Composer::SenderIdentitiesModel::COLUMN_NAME);
}

void GeneralPage::moveIdentityUp()
{
    int from = identityTabelView->currentIndex().row();
    int to = identityTabelView->currentIndex().row() - 1;

    m_identitiesModel->moveIdentity(from, to);
}

void GeneralPage::moveIdentityDown()
{
    int from = identityTabelView->currentIndex().row();
    int to = identityTabelView->currentIndex().row() + 1;

    m_identitiesModel->moveIdentity(from, to);
}

void GeneralPage::addButtonClicked()
{
    m_identitiesModel->appendIdentity(Composer::ItemSenderIdentity());
    identityTabelView->setCurrentIndex(m_identitiesModel->index(m_identitiesModel->rowCount() - 1, 0));
    EditIdentity *dialog = new EditIdentity(this, m_identitiesModel, identityTabelView->currentIndex());
    dialog->setDeleteOnReject();
    dialog->setWindowTitle(tr("Add New Identity"));
    dialog->show();
    updateWidgets();
}

void GeneralPage::editButtonClicked()
{
    EditIdentity *dialog = new EditIdentity(this,  m_identitiesModel, identityTabelView->currentIndex());
    dialog->setWindowTitle(tr("Edit Identity"));
    dialog->show();
}

void GeneralPage::deleteButtonClicked()
{
    Q_ASSERT(identityTabelView->currentIndex().isValid());
    QMessageBox::StandardButton answer =
            QMessageBox::question(this, tr("Delete Identity?"),
                                  tr("Are you sure you want to delete identity %1 <%2>?").arg(
                                      m_identitiesModel->index(identityTabelView->currentIndex().row(),
                                                               Composer::SenderIdentitiesModel::COLUMN_NAME).data().toString(),
                                      m_identitiesModel->index(identityTabelView->currentIndex().row(),
                                                               Composer::SenderIdentitiesModel::COLUMN_EMAIL).data().toString()),
                                  QMessageBox::Yes | QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        m_identitiesModel->removeIdentityAt(identityTabelView->currentIndex().row());
        updateWidgets();
    }
}

void GeneralPage::save(QSettings &s)
{
    m_identitiesModel->saveToSettings(s);
    s.setValue(Common::SettingsNames::appLoadHomepage, showHomepageCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiPreferPlaintextRendering, preferPlaintextCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiShowSystray, guiSystrayCheckbox->isChecked());
}

QWidget *GeneralPage::asWidget()
{
    return this;
}

bool GeneralPage::checkValidity() const
{
    if (m_identitiesModel->rowCount() < 1) {
        QToolTip::showText(identityTabelView->mapToGlobal(QPoint(10, identityTabelView->height() / 2)),
                           tr("Please define some identities here"), 0);
        return false;
    }
    return true;
}

EditIdentity::EditIdentity(QWidget *parent, Composer::SenderIdentitiesModel *identitiesModel, const QModelIndex &currentIndex):
    QDialog(parent), Ui_EditIdentity(), m_identitiesModel(identitiesModel), m_deleteOnReject(false)
{
    Ui_EditIdentity::setupUi(this);
    m_mapper = new QDataWidgetMapper(this);
    m_mapper->setModel(m_identitiesModel);
    m_mapper->addMapping(realNameLineEdit, Composer::SenderIdentitiesModel::COLUMN_NAME);
    m_mapper->addMapping(emailLineEdit, Composer::SenderIdentitiesModel::COLUMN_EMAIL);
    m_mapper->addMapping(organisationLineEdit, Composer::SenderIdentitiesModel::COLUMN_ORGANIZATION);
    m_mapper->addMapping(signaturePlainTextEdit, Composer::SenderIdentitiesModel::COLUMN_SIGNATURE);
    m_mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    m_mapper->setCurrentIndex(currentIndex.row());
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(realNameLineEdit, SIGNAL(textChanged(QString)), this, SLOT(enableButton()));
    connect(emailLineEdit, SIGNAL(textChanged(QString)), this, SLOT(enableButton()));
    connect(organisationLineEdit, SIGNAL(textChanged(QString)), this, SLOT(enableButton()));
    connect(signaturePlainTextEdit, SIGNAL(textChanged()), this, SLOT(enableButton()));
    connect(buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(reject()));
    connect(this, SIGNAL(accepted()), m_mapper, SLOT(submit()));
    connect(this, SIGNAL(rejected()), this, SLOT(onReject()));
    setModal(true);
    signaturePlainTextEdit->setFont(Gui::Util::systemMonospaceFont());
}

void EditIdentity::enableButton()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
        !realNameLineEdit->text().isEmpty() && !emailLineEdit->text().isEmpty());
}

/** @short If enabled, make sure that the current row gets deleted when the dialog is rejected */
void EditIdentity::setDeleteOnReject(const bool reject)
{
    m_deleteOnReject = reject;
}

void EditIdentity::onReject()
{
    if (m_deleteOnReject)
        m_identitiesModel->removeIdentityAt(m_mapper->currentIndex());
}

ImapPage::ImapPage(QWidget *parent, QSettings &s): QScrollArea(parent), Ui_ImapPage()
{
    Ui_ImapPage::setupUi(this);
    method->insertItem(NETWORK, tr("Network Connection"));
    method->insertItem(PROCESS, tr("Local Process"));

    encryption->insertItem(NONE, tr("No encryption"));
    encryption->insertItem(STARTTLS, tr("Use encryption (STARTTLS)"));
    encryption->insertItem(SSL, tr("Force encryption (TLS)"));
    using Common::SettingsNames;
    int defaultImapPort = Common::PORT_IMAPS;

    if (s.value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodTCP) {
        method->setCurrentIndex(NETWORK);

        if (s.value(SettingsNames::imapStartTlsKey,true).toBool())
            encryption->setCurrentIndex(STARTTLS);
        else
            encryption->setCurrentIndex(NONE);

        defaultImapPort = Common::PORT_IMAP;
    } else if (s.value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodSSL) {
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SSL);
    } else if (s.value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodProcess) {
        method->setCurrentIndex(PROCESS);
    } else {
        // Default settings -- let's assume SSL and hope that users who just press Cancel will configure when they see
        // the network error...
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SSL);
    }

    imapHost->setText(s.value(SettingsNames::imapHostKey).toString());
    imapPort->setText(s.value(SettingsNames::imapPortKey, QString::number(defaultImapPort)).toString());
    imapPort->setValidator(new QIntValidator(1, 65535, this));
    connect(imapPort, SIGNAL(textChanged(QString)), this, SLOT(maybeShowPortWarning()));
    connect(encryption, SIGNAL(currentIndexChanged(int)), this, SLOT(maybeShowPortWarning()));
    connect(method, SIGNAL(currentIndexChanged(int)), this, SLOT(maybeShowPortWarning()));
    connect(encryption, SIGNAL(currentIndexChanged(int)), this, SLOT(changePort()));
    portWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    connect(imapPass, SIGNAL(textChanged(QString)), this, SLOT(maybeShowPasswordWarning()));
    imapUser->setText(s.value(SettingsNames::imapUserKey).toString());
    imapPass->setText(s.value(SettingsNames::imapPassKey).toString());
    processPath->setText(s.value(SettingsNames::imapProcessKey).toString());
    startOffline->setChecked(s.value(SettingsNames::imapStartOffline).toBool());
    imapEnableId->setChecked(s.value(SettingsNames::imapEnableId, true).toBool());
    imapCapabilitiesBlacklist->setText(s.value(SettingsNames::imapBlacklistedCapabilities).toStringList().join(QLatin1String(" ")));

    m_imapPort = s.value(SettingsNames::imapPortKey, QString::number(defaultImapPort)).value<quint16>();

    connect(method, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
    updateWidgets();
    maybeShowPasswordWarning();
    maybeShowPortWarning();
}

void ImapPage::resizeEvent(QResizeEvent *event)
{
    QScrollArea::resizeEvent(event);
    scrollAreaWidgetContents->setMinimumSize(event->size());
    scrollAreaWidgetContents->adjustSize();
}

void ImapPage::changePort()
{
    imapPort->setText(QString::number(encryption->currentIndex() == SSL ? Common::PORT_IMAPS : Common::PORT_IMAP));
}

void ImapPage::updateWidgets()
{
    QFormLayout *lay = formLayout;
    Q_ASSERT(lay);

    switch (method->currentIndex()) {
    case NETWORK:
        imapHost->setVisible(true);
        imapPort->setVisible(true);
        encryption->setVisible(true);
        lay->labelForField(imapHost)->setVisible(true);
        lay->labelForField(imapPort)->setVisible(true);
        lay->labelForField(encryption)->setVisible(true);
        processPath->setVisible(false);
        lay->labelForField(processPath)->setVisible(false);
        break;
    default:
        imapHost->setVisible(false);
        imapPort->setVisible(false);
        encryption->setVisible(false);
        lay->labelForField(imapHost)->setVisible(false);
        lay->labelForField(imapPort)->setVisible(false);
        lay->labelForField(encryption)->setVisible(false);
        processPath->setVisible(true);
        lay->labelForField(processPath)->setVisible(true);
    }

    switch (encryption->currentIndex()) {
    case NONE:
    case STARTTLS:
        if (imapPort->text().isEmpty() || imapPort->text() == QString::number(Common::PORT_IMAPS))
            imapPort->setText(QString::number(Common::PORT_IMAP));
        break;
    default:
        if (imapPort->text().isEmpty() || imapPort->text() == QString::number(Common::PORT_IMAP))
            imapPort->setText(QString::number(Common::PORT_IMAPS));
    }
}

void ImapPage::save(QSettings &s)
{
    using Common::SettingsNames;
    if (s.value(SettingsNames::imapHostKey) != imapHost->text()) {
        s.remove(Common::SettingsNames::imapSslPemPubKey);
    }
    switch (method->currentIndex()) {
    case NETWORK:
        if (imapHost->text().isEmpty()) {
            s.remove(SettingsNames::imapMethodKey);
        } else if (encryption->currentIndex() == NONE){
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
            s.setValue(SettingsNames::imapStartTlsKey, false);
        } else if (encryption->currentIndex() == STARTTLS){
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodTCP);
            s.setValue(SettingsNames::imapStartTlsKey, true);
        } else {
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodSSL);
            s.setValue(SettingsNames::imapStartTlsKey, true);
        }
        s.setValue(SettingsNames::imapHostKey, imapHost->text());
        s.setValue(SettingsNames::imapPortKey, imapPort->text());
        break;
    default:
        if (processPath->text().isEmpty()) {
            s.remove(SettingsNames::imapMethodKey);
        } else {
            s.setValue(SettingsNames::imapMethodKey, SettingsNames::methodProcess);
        }
        s.setValue(SettingsNames::imapProcessKey, processPath->text());
    }
    s.setValue(SettingsNames::imapUserKey, imapUser->text());
    s.setValue(SettingsNames::imapPassKey, imapPass->text());
    s.setValue(SettingsNames::imapStartOffline, startOffline->isChecked());
    s.setValue(SettingsNames::imapEnableId, imapEnableId->isChecked());
    s.setValue(SettingsNames::imapBlacklistedCapabilities, imapCapabilitiesBlacklist->text().split(QChar(' ')));
}

QWidget *ImapPage::asWidget()
{
    return this;
}

bool ImapPage::checkValidity() const
{
    switch (method->currentIndex()) {
    case NETWORK:
        // We don't require the username, and that's on purpose. Some servers *could* possibly support PREAUTH :)
        if (checkProblemWithEmptyTextField(imapHost, tr("The IMAP server hostname is missing here")))
            return false;
        break;
    default:
        // PREAUTH must definitely be supported here -- think imap-over-ssh-with-ssh-keys etc.
        if (checkProblemWithEmptyTextField(processPath,
                               tr("The command line to the IMAP server is missing here. Perhaps you need to use SSL or TCP?"))) {
            return false;
        }
        break;
    }
    return true;
}

void ImapPage::maybeShowPasswordWarning()
{
    passwordWarning->setVisible(!imapPass->text().isEmpty());
}

void ImapPage::maybeShowPortWarning()
{
    if (method->currentIndex() == PROCESS) {
        portWarning->setVisible(false);
        return;
    }

    if (encryption->currentIndex() == SSL) {
        portWarning->setVisible(imapPort->text() != QString::number(Common::PORT_IMAPS));
        portWarning->setText(tr("This port is nonstandard. The default port is 993."));
    } else {
        portWarning->setVisible(imapPort->text() != QString::number(Common::PORT_IMAP));
        portWarning->setText(tr("This port is nonstandard. The default port is 143."));
    }

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

QWidget *CachePage::asWidget()
{
    return this;
}

bool CachePage::checkValidity() const
{
    // Nothing really special for this class
    return true;
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
    passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    sendmail->setText(s.value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString());
    saveToImap->setChecked(s.value(SettingsNames::composerSaveToImapKey, true).toBool());
    // Would be cool to support the special-use mailboxes
    saveFolderName->setText(s.value(SettingsNames::composerImapSentKey, QLatin1String("Sent")).toString());
    smtpBurl->setChecked(s.value(SettingsNames::smtpUseBurlKey, false).toBool());

    connect(smtpPass, SIGNAL(textChanged(QString)), this, SLOT(maybeShowPasswordWarning()));
    connect(method, SIGNAL(currentIndexChanged(int)), this, SLOT(updateWidgets()));
    connect(smtpAuth, SIGNAL(toggled(bool)), this, SLOT(updateWidgets()));
    connect(saveToImap, SIGNAL(toggled(bool)), this, SLOT(updateWidgets()));
    updateWidgets();
    maybeShowPasswordWarning();
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
        saveToImap->setEnabled(true);
        lay->labelForField(saveToImap)->setEnabled(true);
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

QWidget *OutgoingPage::asWidget()
{
    return this;
}

bool OutgoingPage::checkValidity() const
{
    switch (method->currentIndex()) {
    case SMTP:
    case SSMTP:
        if (checkProblemWithEmptyTextField(smtpHost, tr("The SMTP server hostname is missing here")))
            return false;
        if (smtpAuth->isChecked() && checkProblemWithEmptyTextField(smtpUser, tr("The SMTP username is missing here")))
            return false;
        break;
    case SENDMAIL:
        if (checkProblemWithEmptyTextField(sendmail, tr("The SMTP server hostname is missing here")))
            return false;
        break;
    case IMAP_SENDMAIL:
        break;
    }

    if (saveToImap->isChecked() && checkProblemWithEmptyTextField(saveFolderName, tr("Please specify the folder name here")))
        return false;

    return true;
}

void OutgoingPage::maybeShowPasswordWarning()
{
    passwordWarning->setVisible(!smtpPass->text().isEmpty());
}

#ifdef XTUPLE_CONNECT
XtConnectPage::XtConnectPage(QWidget *parent, QSettings &s, ImapPage *imapPage): QWidget(parent), imap(imapPage)
{
    // Take care not to clash with the cache of the GUI
    QString cacheLocation = Common::writablePath(Common::LOCATION_CACHE) + QString::fromAscii("xtconnect-trojita");
    QFormLayout *layout = new QFormLayout(this);
    cacheDir = new QLineEdit(s.value(Common::SettingsNames::xtConnectCacheDirectory, cacheLocation).toString(), this);
    layout->addRow(tr("Cache Directory"), cacheDir);

    QGroupBox *box = new QGroupBox(tr("Mailboxes to synchronize"), this);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    QListWidget *mailboxes = new QListWidget(box);
    mailboxes->addItems(s.value(Common::SettingsNames::xtSyncMailboxList).toStringList());
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
                       Common::SettingsNames::imapSslPemPubKey;
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
    args << QLatin1String("-w") << password;

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
