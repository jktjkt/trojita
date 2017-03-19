/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
   Copyright (C) 2014        Luke Dashjr <luke+trojita@dashjr.org>
   Copyright (C) 2012        Mohammed Nafees <nafees.technocool@gmail.com>
   Copyright (C) 2013        Pali Rohár <pali.rohar@gmail.com>

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
#include <QColorDialog>
#include <QComboBox>
#include <QDataWidgetMapper>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include "SettingsDialog.h"
#include "ColoredItemDelegate.h"
#include "Common/InvokeMethod.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Gui/Util.h"
#include "Gui/Window.h"
#include "Imap/Model/ImapAccess.h"
#include "MSA/Account.h"
#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/IconLoader.h"
#include "UiUtils/PasswordWatcher.h"
#include "ShortcutHandler/ShortcutHandler.h"

namespace Gui
{

QString SettingsDialog::warningStyleSheet = QStringLiteral("border: 2px solid red; background-color: #E7C575; color: black; "
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

SettingsDialog::SettingsDialog(MainWindow *parent, Composer::SenderIdentitiesModel *identitiesModel,
        Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel, QSettings *settings):
    QDialog(parent), mainWindow(parent), m_senderIdentities(identitiesModel), m_favoriteTags(favoriteTagsModel),
        m_settings(settings)
{
    setWindowTitle(tr("Settings"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    stack = new QTabWidget(this);
    layout->addWidget(stack);
    stack->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    addPage(new GeneralPage(this, *m_settings, m_senderIdentities), tr("&General"));
    addPage(new ImapPage(this, *m_settings), tr("I&MAP"));
    addPage(new CachePage(this, *m_settings), tr("&Offline"));
    addPage(new OutgoingPage(this, *m_settings), tr("&SMTP"));
#ifdef XTUPLE_CONNECT
    addPage(xtConnect = new XtConnectPage(this, *m_settings, imap), tr("&xTuple"));
#endif
    addPage(new FavoriteTagsPage(this, *m_settings, m_favoriteTags), tr("Favorite &tags"));

    buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    EMIT_LATER_NOARG(this, reloadPasswordsRequested);
}

void SettingsDialog::setOriginalPasswordPlugin(const QString &plugin)
{
    m_originalPasswordPlugin = plugin;
}

void SettingsDialog::adjustSizeToScrollAreas()
{
    QScrollArea *area = qobject_cast<QScrollArea*>(sender());
    Q_ASSERT(area && area->widget());

    // task #A: figure the "minimum" size for the tabwidget

    // #A.1: search scrollareas and align their size to their content
    // update size of the widget in the tabbed scrollarea
    area->widget()->adjustSize();

    // figure the size demand of this scroll area (content + margins)
    int l,t,r,b;
    area->getContentsMargins(&l,&r,&t,&b);
    QSize minSize(area->widget()->size() + QSize(l+r, t+b));

    // TODO: clamp this to 640x480 or QDesktopWidget::availableGeometry() dependent?

    // do not shrink (prevent nasty size jumps for no reason)
    minSize.setWidth(qMax(area->width(), minSize.width()));
    minSize.setHeight(qMax(area->height(), minSize.height()));

    // task #B: find the QStackedWidget inside the QTabWidget to determine its margins
    Q_FOREACH(const QObject *o, stack->children()) {
        if (const QStackedWidget *actualStack = qobject_cast<const QStackedWidget*>(o)) {
            minSize.setWidth(minSize.width() + stack->width() - actualStack->width());
            minSize.setHeight(minSize.height() + stack->height() - actualStack->height());
            break;
        }
    }

    // task #C: convince the dialog to the new size
    // #C.1: arrest the tabwidget
    stack->setMinimumSize(minSize);
    // #C.2: force a relayout of the dialog (do NOT use "adjustSize", which may still shrink)
    layout()->activate();
    // #C.3: release the tabwidget minimum size
    stack->setMinimumSize(QSize(0, 0));
}

Plugins::PluginManager *SettingsDialog::pluginManager()
{
    return mainWindow->pluginManager();
}

Imap::ImapAccess *SettingsDialog::imapAccess()
{
    return mainWindow->imapAccess();
}

void SettingsDialog::accept()
{
    m_saveSignalCount = 0;

    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        if (!page->checkValidity()) {
            stack->setCurrentWidget(page->asWidget());
            return;
        }
        connect(page->asWidget(), SIGNAL(saved()), this, SLOT(slotAccept())); // new-signal-slot: we're abusing the type system a bit here, cannot use the new syntax
        ++m_saveSignalCount;
    }

#ifndef Q_OS_WIN
    // Try to wour around QSettings' inability to set umask for its file access. We don't want to set umask globally.
    QFile settingsFile(m_settings->fileName());
    settingsFile.setPermissions(QFile::ReadUser | QFile::WriteUser);
#endif

    buttons->setEnabled(false);
    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        page->asWidget()->setEnabled(false);
    }

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
}

void SettingsDialog::slotAccept()
{
    disconnect(sender(), SIGNAL(saved()), this, SLOT(slotAccept())); // new-signal-slot: we're abusing the type system a bit here, cannot use the new syntax
    if (--m_saveSignalCount > 0) {
        return;
    }

    QStringList passwordFailures;
    Q_FOREACH(ConfigurationWidgetInterface *page, pages) {
        QString message;
        if (page->passwordFailures(message)) {
            passwordFailures << message;
        }
    }
    if (!passwordFailures.isEmpty()) {
        Gui::Util::messageBoxWarning(this, tr("Saving passwords failed"),
                                     tr("<p>Couldn't save passwords. These were the error messages:</p>\n<p>%1</p>")
                                     .arg(passwordFailures.join(QStringLiteral("<br/>"))));
    }

    buttons->setEnabled(true);
    QDialog::accept();
}

void SettingsDialog::reject()
{
    // The changes were performed on the live data, so we have to make sure they are discarded when user cancels
    if (!m_originalPasswordPlugin.isEmpty() && pluginManager()->passwordPlugin() != m_originalPasswordPlugin) {
        // Password plugin was changed, revert back to original one
        pluginManager()->setPasswordPlugin(m_originalPasswordPlugin);
    }
    m_senderIdentities->loadFromSettings(*m_settings);
    m_favoriteTags->loadFromSettings(*m_settings);
    QDialog::reject();
}

void SettingsDialog::addPage(ConfigurationWidgetInterface *page, const QString &title)
{
    stack->addTab(page->asWidget(), title);
    connect(page->asWidget(), SIGNAL(widgetsUpdated()), SLOT(adjustSizeToScrollAreas())); // new-signal-slot: we're abusing the type system a bit here, cannot use the new syntax
    QMetaObject::invokeMethod(page->asWidget(), "updateWidgets", Qt::QueuedConnection);
    pages << page;
}

FavoriteTagsPage::FavoriteTagsPage(SettingsDialog *parent, QSettings &s, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel):
    QScrollArea(parent), Ui_FavoriteTagsPage(), m_favoriteTagsModel(favoriteTagsModel), m_parent(parent)
{
    Ui_FavoriteTagsPage::setupUi(this);
    Q_ASSERT(m_favoriteTagsModel);
    moveUpButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-up")));
    moveDownButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-down")));
    tagTableView->setModel(m_favoriteTagsModel);
    tagTableView->setItemDelegate(new ColoredItemDelegate(this));
    tagTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tagTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tagTableView->setGridStyle(Qt::NoPen);
    tagTableView->resizeRowsToContents();
    tagTableView->horizontalHeader()->setStretchLastSection(true);
    // show tag name in color instead
    tagTableView->hideColumn(Imap::Mailbox::FavoriteTagsModel::COLUMN_COLOR);

    connect(tagTableView, &QAbstractItemView::clicked, this, &FavoriteTagsPage::updateWidgets);
    connect(tagTableView, &QAbstractItemView::doubleClicked, this, &FavoriteTagsPage::editButtonClicked);
    connect(m_favoriteTagsModel, &QAbstractItemModel::modelReset, this, &FavoriteTagsPage::updateWidgets);
    connect(m_favoriteTagsModel, &QAbstractItemModel::rowsInserted, this, &FavoriteTagsPage::updateWidgets);
    connect(m_favoriteTagsModel, &QAbstractItemModel::rowsRemoved, this, &FavoriteTagsPage::updateWidgets);
    connect(m_favoriteTagsModel, &QAbstractItemModel::dataChanged, this, &FavoriteTagsPage::updateWidgets);
    connect(moveUpButton, &QAbstractButton::clicked, this, [this](){ FavoriteTagsPage::moveTagBy(-1); });
    connect(moveDownButton, &QAbstractButton::clicked, this, [this](){ FavoriteTagsPage::moveTagBy(1); });
    connect(addButton, &QAbstractButton::clicked, this, &FavoriteTagsPage::addButtonClicked);
    connect(editButton, &QAbstractButton::clicked, this, &FavoriteTagsPage::editButtonClicked);
    connect(deleteButton, &QAbstractButton::clicked, this, &FavoriteTagsPage::deleteButtonClicked);

    updateWidgets();
}

void FavoriteTagsPage::updateWidgets()
{
    bool enabled = tagTableView->currentIndex().isValid();
    deleteButton->setEnabled(enabled);
    editButton->setEnabled(enabled);
    bool upEnabled = m_favoriteTagsModel->rowCount() > 0 && tagTableView->currentIndex().row() > 0;
    bool downEnabled = m_favoriteTagsModel->rowCount() > 0 && tagTableView->currentIndex().isValid() &&
            tagTableView->currentIndex().row() < m_favoriteTagsModel->rowCount() - 1;
    moveUpButton->setEnabled(upEnabled);
    moveDownButton->setEnabled(downEnabled);

    tagTableView->resizeColumnToContents(Imap::Mailbox::FavoriteTagsModel::COLUMN_INDEX);
    tagTableView->resizeColumnToContents(Imap::Mailbox::FavoriteTagsModel::COLUMN_NAME);

    emit widgetsUpdated();
}

void FavoriteTagsPage::moveTagBy(const int offset)
{
    int from = tagTableView->currentIndex().row();
    int to = tagTableView->currentIndex().row() + offset;

    m_favoriteTagsModel->moveTag(from, to);
    updateWidgets();
}

void FavoriteTagsPage::addButtonClicked()
{
    m_favoriteTagsModel->appendTag(Imap::Mailbox::ItemFavoriteTagItem());
    tagTableView->setCurrentIndex(m_favoriteTagsModel->index(m_favoriteTagsModel->rowCount() - 1, 0));
    EditFavoriteTag *dialog = new EditFavoriteTag(this, m_favoriteTagsModel, tagTableView->currentIndex());
    dialog->setDeleteOnReject();
    dialog->setWindowTitle(tr("Add New Tag"));
    dialog->show();
    updateWidgets();
}

void FavoriteTagsPage::editButtonClicked()
{
    EditFavoriteTag *dialog = new EditFavoriteTag(this, m_favoriteTagsModel, tagTableView->currentIndex());
    dialog->setWindowTitle(tr("Edit Tag"));
    dialog->show();
}

void FavoriteTagsPage::deleteButtonClicked()
{
    Q_ASSERT(tagTableView->currentIndex().isValid());
    m_favoriteTagsModel->removeTagAt(tagTableView->currentIndex().row());
    updateWidgets();
}

void FavoriteTagsPage::save(QSettings &s)
{
    m_favoriteTagsModel->saveToSettings(s);

    emit saved();
}

QWidget *FavoriteTagsPage::asWidget()
{
    return this;
}

bool FavoriteTagsPage::checkValidity() const
{
    return true;
}

bool FavoriteTagsPage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

GeneralPage::GeneralPage(SettingsDialog *parent, QSettings &s, Composer::SenderIdentitiesModel *identitiesModel):
    QScrollArea(parent), Ui_GeneralPage(), m_identitiesModel(identitiesModel), m_parent(parent)
{
    Ui_GeneralPage::setupUi(this);
    Q_ASSERT(m_identitiesModel);
    editButton->setEnabled(false);
    deleteButton->setEnabled(false);
    moveUpButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-up")));
    moveDownButton->setIcon(UiUtils::loadIcon(QStringLiteral("go-down")));
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

    Plugins::PluginManager *pluginManager = parent->pluginManager();
    QMap<QString, QString>::const_iterator it;
    int i;

    const QMap<QString, QString> &addressbookPlugins = pluginManager->availableAddressbookPlugins();
    const QString &addressbookPlugin = pluginManager->addressbookPlugin();
    int addressbookIndex = -1;

    for (it = addressbookPlugins.constBegin(), i = 0; it != addressbookPlugins.constEnd(); ++it, ++i) {
        addressbookBox->addItem(it.value(), it.key());
        if (addressbookIndex < 0 && addressbookPlugin == it.key())
            addressbookIndex = i;
    }

    addressbookBox->addItem(tr("Disable address book"));

    if (addressbookPlugin == QLatin1String("none"))
        addressbookIndex = addressbookBox->count()-1;

    if (addressbookIndex == -1) {
        if (!addressbookPlugin.isEmpty())
            addressbookBox->addItem(tr("Plugin not found (%1)").arg(addressbookPlugin), addressbookPlugin);
        addressbookIndex = addressbookBox->count()-1;
    }

    addressbookBox->setCurrentIndex(addressbookIndex);


    const QMap<QString, QString> &passwordPlugins = pluginManager->availablePasswordPlugins();
    const QString &passwordPlugin = pluginManager->passwordPlugin();
    int passwordIndex = -1;

    for (it = passwordPlugins.constBegin(), i = 0; it != passwordPlugins.constEnd(); ++it, ++i) {
        passwordBox->addItem(it.value(), it.key());
        if (passwordIndex < 0 && passwordPlugin == it.key())
            passwordIndex = i;
    }

    passwordBox->addItem(tr("Disable passwords"));

    if (passwordPlugin == QLatin1String("none"))
        passwordIndex = passwordBox->count()-1;

    if (passwordIndex == -1) {
        if (!passwordPlugin.isEmpty())
            passwordBox->addItem(tr("Plugin not found (%1)").arg(passwordPlugin), passwordPlugin);
        passwordIndex = passwordBox->count()-1;
    }

    passwordBox->setCurrentIndex(passwordIndex);

    m_parent->setOriginalPasswordPlugin(passwordPlugin);


    markReadCheckbox->setChecked(s.value(Common::SettingsNames::autoMarkReadEnabled, QVariant(true)).toBool());
    markReadSeconds->setValue(s.value(Common::SettingsNames::autoMarkReadSeconds, QVariant(0)).toUInt());
    connect(markReadCheckbox, &QAbstractButton::toggled, markReadSeconds, &QWidget::setEnabled);

    showHomepageCheckbox->setChecked(s.value(Common::SettingsNames::appLoadHomepage, QVariant(true)).toBool());
    showHomepageCheckbox->setToolTip(trUtf8("<p>If enabled, Trojitá will show its homepage upon startup.</p>"
                                        "<p>The remote server will receive the user's IP address and versions of Trojitá, the Qt library, "
                                        "and the underlying operating system. No private information, like account settings "
                                        "or IMAP server details, are collected.</p>"));

    guiSystrayCheckbox->setChecked(s.value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool());
    guiStartMinimizedCheckbox->setChecked(s.value(Common::SettingsNames::guiStartMinimized, QVariant(false)).toBool());

    preferPlaintextCheckbox->setChecked(s.value(Common::SettingsNames::guiPreferPlaintextRendering).toBool());
    revealTrojitaVersions->setChecked(s.value(Common::SettingsNames::interopRevealVersions, QVariant(true)).toBool());

    connect(identityTabelView, &QAbstractItemView::clicked, this, &GeneralPage::updateWidgets);
    connect(identityTabelView, &QAbstractItemView::doubleClicked, this, &GeneralPage::editButtonClicked);
    connect(m_identitiesModel, &QAbstractItemModel::layoutChanged, this, &GeneralPage::updateWidgets);
    connect(m_identitiesModel, &QAbstractItemModel::rowsInserted, this, &GeneralPage::updateWidgets);
    connect(m_identitiesModel, &QAbstractItemModel::rowsRemoved, this, &GeneralPage::updateWidgets);
    connect(m_identitiesModel, &QAbstractItemModel::dataChanged, this, &GeneralPage::updateWidgets);
    connect(moveUpButton, &QAbstractButton::clicked, this, &GeneralPage::moveIdentityUp);
    connect(moveDownButton, &QAbstractButton::clicked, this, &GeneralPage::moveIdentityDown);
    connect(addButton, &QAbstractButton::clicked, this, &GeneralPage::addButtonClicked);
    connect(editButton, &QAbstractButton::clicked, this, &GeneralPage::editButtonClicked);
    connect(deleteButton, &QAbstractButton::clicked, this, &GeneralPage::deleteButtonClicked);
    connect(passwordBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &GeneralPage::passwordPluginChanged);

    connect(this, &GeneralPage::reloadPasswords, m_parent, &SettingsDialog::reloadPasswordsRequested);

    updateWidgets();
}

void GeneralPage::passwordPluginChanged()
{
    const QString &passwordPlugin = m_parent->pluginManager()->passwordPlugin();
    const QString &selectedPasswordPlugin = passwordBox->itemData(passwordBox->currentIndex()).toString();

    if (selectedPasswordPlugin != passwordPlugin) {
        m_parent->pluginManager()->setPasswordPlugin(selectedPasswordPlugin);
        emit reloadPasswords();
    }
}

void GeneralPage::updateWidgets()
{
    bool enabled = identityTabelView->currentIndex().isValid();
    deleteButton->setEnabled(enabled);
    editButton->setEnabled(enabled);
    bool upEnabled = m_identitiesModel->rowCount() > 0 && identityTabelView->currentIndex().row() > 0;
    bool downEnabled = m_identitiesModel->rowCount() > 0 && identityTabelView->currentIndex().isValid() &&
            identityTabelView->currentIndex().row() < m_identitiesModel->rowCount() - 1;
    moveUpButton->setEnabled(upEnabled);
    moveDownButton->setEnabled(downEnabled);

    identityTabelView->resizeColumnToContents(Composer::SenderIdentitiesModel::COLUMN_NAME);

    emit widgetsUpdated();
}

void GeneralPage::moveIdentityUp()
{
    int from = identityTabelView->currentIndex().row();
    int to = identityTabelView->currentIndex().row() - 1;

    m_identitiesModel->moveIdentity(from, to);
    updateWidgets();
}

void GeneralPage::moveIdentityDown()
{
    int from = identityTabelView->currentIndex().row();
    int to = identityTabelView->currentIndex().row() + 1;

    m_identitiesModel->moveIdentity(from, to);
    updateWidgets();
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
    s.setValue(Common::SettingsNames::autoMarkReadEnabled, markReadCheckbox->isChecked());
    s.setValue(Common::SettingsNames::autoMarkReadSeconds, markReadSeconds->value());
    s.setValue(Common::SettingsNames::appLoadHomepage, showHomepageCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiPreferPlaintextRendering, preferPlaintextCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiShowSystray, guiSystrayCheckbox->isChecked());
    s.setValue(Common::SettingsNames::guiStartMinimized, guiStartMinimizedCheckbox->isChecked());
    s.setValue(Common::SettingsNames::interopRevealVersions, revealTrojitaVersions->isChecked());

    const QString &addressbookPlugin = m_parent->pluginManager()->addressbookPlugin();
    const QString &selectedAddressbookPlugin = addressbookBox->itemData(addressbookBox->currentIndex()).toString();

    if (selectedAddressbookPlugin != addressbookPlugin) {
        m_parent->pluginManager()->setAddressbookPlugin(selectedAddressbookPlugin);
    }

    const QString &passwordPlugin = m_parent->pluginManager()->passwordPlugin();
    const QString &selectedPasswordPlugin = passwordBox->itemData(passwordBox->currentIndex()).toString();

    if (selectedPasswordPlugin != passwordPlugin) {
        m_parent->pluginManager()->setPasswordPlugin(selectedPasswordPlugin);
    }

    emit saved();
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

bool GeneralPage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
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
    connect(realNameLineEdit, &QLineEdit::textChanged, this, &EditIdentity::enableButton);
    connect(emailLineEdit, &QLineEdit::textChanged, this, &EditIdentity::enableButton);
    connect(organisationLineEdit, &QLineEdit::textChanged, this, &EditIdentity::enableButton);
    connect(signaturePlainTextEdit, &QPlainTextEdit::textChanged, this, &EditIdentity::enableButton);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &QDialog::accept);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::accepted, m_mapper, &QDataWidgetMapper::submit);
    connect(this, &QDialog::rejected, this, &EditIdentity::onReject);
    setModal(true);
    signaturePlainTextEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
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

EditFavoriteTag::EditFavoriteTag(QWidget *parent, Imap::Mailbox::FavoriteTagsModel *favoriteTagsModel, const QModelIndex &currentIndex):
    QDialog(parent), Ui_EditFavoriteTag(), m_favoriteTagsModel(favoriteTagsModel), currentIndex(currentIndex), m_deleteOnReject(false)
{
    Ui_EditFavoriteTag::setupUi(this);

    nameLineEdit->setText(name());
    setColorButtonColor(color());
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(colorButton, &QAbstractButton::clicked, this, &EditFavoriteTag::colorButtonClick);
    connect(nameLineEdit, &QLineEdit::textChanged, this, &EditFavoriteTag::tryEnableButton);

    connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &QDialog::accept);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::accepted, this, &EditFavoriteTag::onAccept);
    connect(this, &QDialog::rejected, this, &EditFavoriteTag::onReject);
    setModal(true);
}

QString EditFavoriteTag::name()
{
    return m_favoriteTagsModel->data(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_NAME)).toString();
}

QString EditFavoriteTag::color()
{
    return m_favoriteTagsModel->data(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_COLOR)).toString();
}

void EditFavoriteTag::setColorButtonColor(const QString color)
{
    colorButton->setProperty("colorName", color);
    QPalette pal = colorButton->palette();
    pal.setColor(QPalette::Button, QColor(color));
    colorButton->setAutoFillBackground(true);
    colorButton->setPalette(pal);
    colorButton->setFlat(true);
    colorButton->update();
}

void EditFavoriteTag::colorButtonClick()
{
    const QColor color = QColorDialog::getColor(QColor(colorButton->property("colorName").toString()), this, tr("Select tag color"));
    if (color.isValid()) {
        setColorButtonColor(color.name());
        tryEnableButton();
    }
}

void EditFavoriteTag::tryEnableButton()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
        !nameLineEdit->text().isEmpty() && QColor(colorButton->property("colorName").toString()).isValid()
    );
}

/** @short If enabled, make sure that the current row gets deleted when the dialog is rejected */
void EditFavoriteTag::setDeleteOnReject(const bool reject)
{
    m_deleteOnReject = reject;
}

void EditFavoriteTag::onAccept()
{
    m_favoriteTagsModel->setData(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_NAME),
            nameLineEdit->text());
    m_favoriteTagsModel->setData(m_favoriteTagsModel->index(currentIndex.row(), Imap::Mailbox::FavoriteTagsModel::COLUMN_COLOR),
            colorButton->property("colorName"));
}

void EditFavoriteTag::onReject()
{
    if (m_deleteOnReject)
        m_favoriteTagsModel->removeTagAt(currentIndex.row());
}

ImapPage::ImapPage(SettingsDialog *parent, QSettings &s): QScrollArea(parent), Ui_ImapPage(), m_parent(parent)
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
    connect(imapPort, &QLineEdit::textChanged, this, &ImapPage::maybeShowPortWarning);
    connect(encryption, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::maybeShowPortWarning);
    connect(method, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::maybeShowPortWarning);
    connect(encryption, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::changePort);
    portWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
    connect(imapPass, &QLineEdit::textChanged, this, &ImapPage::updateWidgets);
    imapUser->setText(s.value(SettingsNames::imapUserKey).toString());
    processPath->setText(s.value(SettingsNames::imapProcessKey).toString());

    imapCapabilitiesBlacklist->setText(s.value(SettingsNames::imapBlacklistedCapabilities).toStringList().join(QStringLiteral(" ")));
    imapUseSystemProxy->setChecked(s.value(SettingsNames::imapUseSystemProxy, true).toBool());
    imapNeedsNetwork->setChecked(s.value(SettingsNames::imapNeedsNetwork, true).toBool());
    imapIdleRenewal->setValue(s.value(SettingsNames::imapIdleRenewal, QVariant(29)).toInt());
    imapNumberRefreshInterval->setValue(m_parent->imapAccess()->numberRefreshInterval());
    accountIcon->setText(s.value(SettingsNames::imapAccountIcon).toString());
    archiveFolderName->setText(s.value(SettingsNames::imapArchiveFolderName).toString().isEmpty() ?
        SettingsNames::imapDefaultArchiveFolderName : s.value(SettingsNames::imapArchiveFolderName).toString());

    m_imapPort = s.value(SettingsNames::imapPortKey, QString::number(defaultImapPort)).value<quint16>();

    connect(method, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ImapPage::updateWidgets);

    // FIXME: use another account-id
    m_pwWatcher = m_parent->imapAccess()->passwordWatcher();
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::stateChanged, this, &ImapPage::updateWidgets);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingFailed, this, &ImapPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingDone, this, &ImapPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::readingDone, this, &ImapPage::slotSetPassword);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, imapPass, &QLineEdit::clear);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, m_pwWatcher, &UiUtils::PasswordWatcher::reloadPassword);

    updateWidgets();
    maybeShowPortWarning();
}

void ImapPage::slotSetPassword()
{
    imapPass->setText(m_pwWatcher->password());
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
        imapUseSystemProxy->setVisible(true);
        lay->labelForField(imapUseSystemProxy)->setVisible(true);
        // the "needs network" can very well apply to accounts using "local process" via SSH, so it is not disabled here
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
        imapUseSystemProxy->setVisible(false);
        lay->labelForField(imapUseSystemProxy)->setVisible(false);
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

    if (!m_pwWatcher->isPluginAvailable())
        imapPass->setText(QString());

    passwordWarning->setVisible(!imapPass->text().isEmpty());
    if (m_pwWatcher->isStorageEncrypted()) {
        passwordWarning->setStyleSheet(QString());
        passwordWarning->setText(trUtf8("This password will be saved in encrypted storage. "
            "If you do not enter password here, Trojitá will prompt for one when needed."));
    } else {
        passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
        passwordWarning->setText(trUtf8("This password will be saved in clear text. "
            "If you do not enter password here, Trojitá will prompt for one when needed."));
    }

    passwordPluginStatus->setVisible(!m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || !m_pwWatcher->didReadOk() || !m_pwWatcher->didWriteOk());
    passwordPluginStatus->setText(m_pwWatcher->progressMessage());

    imapPass->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());
    imapPassLabel->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());

    emit widgetsUpdated();
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
        s.setValue(SettingsNames::imapUseSystemProxy, imapUseSystemProxy->isChecked());
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
    s.setValue(SettingsNames::imapBlacklistedCapabilities, imapCapabilitiesBlacklist->text().split(QStringLiteral(" ")));
    s.setValue(SettingsNames::imapNeedsNetwork, imapNeedsNetwork->isChecked());
    s.setValue(SettingsNames::imapIdleRenewal, imapIdleRenewal->value());
    m_parent->imapAccess()->setNumberRefreshInterval(imapNumberRefreshInterval->value());

    s.setValue(SettingsNames::imapAccountIcon, accountIcon->text().isEmpty() ? QVariant() : QVariant(accountIcon->text()));
    s.setValue(SettingsNames::imapArchiveFolderName, archiveFolderName->text());

    if (m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin()) {
        m_pwWatcher->setPassword(imapPass->text());
    } else {
        emit saved();
    }
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

bool ImapPage::passwordFailures(QString &message) const
{
    if (!m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || m_pwWatcher->didWriteOk()) {
        return false;
    } else {
        message = m_pwWatcher->progressMessage();
        return true;
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

    val = s.value(SettingsNames::watchedFoldersKey).toString();
    if (val == Common::SettingsNames::watchAll) {
        watchAll->setChecked(true);
    } else if (val == Common::SettingsNames::watchSubscribed) {
        watchSubscribed->setChecked(true);
    } else {
        watchInbox->setChecked(true);
    }

    updateWidgets();

    connect(offlineNope, &QAbstractButton::clicked, this, &CachePage::updateWidgets);
    connect(offlineXDays, &QAbstractButton::clicked, this, &CachePage::updateWidgets);
    connect(offlineEverything, &QAbstractButton::clicked, this, &CachePage::updateWidgets);
}

void CachePage::updateWidgets()
{
    offlineNumberOfDays->setEnabled(offlineXDays->isChecked());
    emit widgetsUpdated();
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

    if (watchAll->isChecked()) {
        s.setValue(SettingsNames::watchedFoldersKey, SettingsNames::watchAll);
    } else if (watchSubscribed->isChecked()) {
        s.setValue(SettingsNames::watchedFoldersKey, SettingsNames::watchSubscribed);
    } else {
        s.setValue(SettingsNames::watchedFoldersKey, SettingsNames::watchOnlyInbox);
    }

    emit saved();
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

bool CachePage::passwordFailures(QString &message) const
{
    Q_UNUSED(message);
    return false;
}

OutgoingPage::OutgoingPage(SettingsDialog *parent, QSettings &s): QScrollArea(parent), Ui_OutgoingPage(), m_parent(parent)
{
    using Common::SettingsNames;
    Ui_OutgoingPage::setupUi(this);
    // FIXME: use another account-id at some point in future
    //        we are now using the profile to avoid overwriting passwords of
    //        other profiles in secure storage
    QString profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
    m_smtpAccountSettings = new MSA::Account(this, &s, profileName);

    method->insertItem(NETWORK, tr("Network"));
    method->insertItem(SENDMAIL, tr("Local sendmail-compatible"));
    method->insertItem(IMAP_SENDMAIL, tr("IMAP SENDMAIL Extension"));;

    encryption->insertItem(SMTP, tr("No encryption"));
    encryption->insertItem(SMTP_STARTTLS, tr("Use encryption (STARTTLS)"));
    encryption->insertItem(SSMTP, tr("Force encryption (TLS)"));
    encryption->setCurrentIndex(SSMTP);

    connect(method, static_cast<void (QComboBox::*)(const int)>(&QComboBox::currentIndexChanged), this, &OutgoingPage::slotSetSubmissionMethod);
    connect(encryption, static_cast<void (QComboBox::*)(const int)>(&QComboBox::currentIndexChanged), this, &OutgoingPage::slotSetSubmissionMethod);

    connect(m_smtpAccountSettings, &MSA::Account::submissionMethodChanged, this, &OutgoingPage::updateWidgets);
    connect(m_smtpAccountSettings, &MSA::Account::saveToImapChanged, this, &OutgoingPage::updateWidgets);
    connect(m_smtpAccountSettings, &MSA::Account::authenticateEnabledChanged, this, &OutgoingPage::updateWidgets);
    connect(m_smtpAccountSettings, &MSA::Account::reuseImapAuthenticationChanged, this, &OutgoingPage::updateWidgets);
    connect(smtpPass, &QLineEdit::textChanged, this, &OutgoingPage::updateWidgets);
    connect(smtpHost, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setServer);
    connect(smtpUser, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setUsername);
    connect(smtpPort, &LineEdit::textEditingFinished, this, &OutgoingPage::setPortByText);
    connect(m_smtpAccountSettings, &MSA::Account::showPortWarning, this, &OutgoingPage::showPortWarning);
    connect(smtpAuth, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setAuthenticateEnabled);
    connect(smtpAuthReuseImapCreds, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setReuseImapAuthentication);
    connect(saveToImap, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setSaveToImap);
    connect(saveFolderName, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setSentMailboxName);
    connect(smtpBurl, &QAbstractButton::toggled, m_smtpAccountSettings, &MSA::Account::setUseBurl);
    connect(sendmail, &LineEdit::textEditingFinished, m_smtpAccountSettings, &MSA::Account::setPathToSendmail);

    m_pwWatcher = new UiUtils::PasswordWatcher(this, m_parent->pluginManager(), QStringLiteral("account-0"), QStringLiteral("smtp"));
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::stateChanged, this, &OutgoingPage::updateWidgets);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingFailed, this, &OutgoingPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::savingDone, this, &OutgoingPage::saved);
    connect(m_pwWatcher, &UiUtils::PasswordWatcher::readingDone, this, &OutgoingPage::slotSetPassword);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, smtpPass, &QLineEdit::clear);
    connect(m_parent, &SettingsDialog::reloadPasswordsRequested, m_pwWatcher, &UiUtils::PasswordWatcher::reloadPassword);

    updateWidgets();
}

void OutgoingPage::slotSetPassword()
{
    smtpPass->setText(m_pwWatcher->password());
}

void OutgoingPage::slotSetSubmissionMethod()
{
    switch (method->currentIndex()) {
    case SENDMAIL:
        m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SENDMAIL);
        break;
    case IMAP_SENDMAIL:
        m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::IMAP_SENDMAIL);
        break;
    case NETWORK:
        switch (encryption->currentIndex()) {
        case SMTP:
            m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SMTP);
            break;
        case SMTP_STARTTLS:
            m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SMTP_STARTTLS);
            break;
        case SSMTP:
            m_smtpAccountSettings->setSubmissionMethod(MSA::Account::Method::SSMTP);
            break;
        }
        break;
    default:
        Q_ASSERT(false);
    }
    // Toggle the default ports upon changing the delivery method
    smtpPort->setText(QString::number(m_smtpAccountSettings->port()));
}

void OutgoingPage::setPortByText(const QString &text)
{
    m_smtpAccountSettings->setPort(text.toUShort());
}

void OutgoingPage::updateWidgets()
{
    QFormLayout *lay = formLayout;
    Q_ASSERT(lay);

    switch (m_smtpAccountSettings->submissionMethod()) {
    case MSA::Account::Method::SMTP:
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SMTP);
        break;
    case MSA::Account::Method::SMTP_STARTTLS:
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SMTP_STARTTLS);
        break;
    case MSA::Account::Method::SSMTP:
        method->setCurrentIndex(NETWORK);
        encryption->setCurrentIndex(SSMTP);
        break;
    case MSA::Account::Method::SENDMAIL:
        method->setCurrentIndex(SENDMAIL);
        encryption->setVisible(false);
        encryptionLabel->setVisible(false);
        break;
    case MSA::Account::Method::IMAP_SENDMAIL:
        method->setCurrentIndex(IMAP_SENDMAIL);
        encryption->setVisible(false);
        encryptionLabel->setVisible(false);
        break;
    }

    switch (m_smtpAccountSettings->submissionMethod()) {
    case MSA::Account::Method::SMTP:
    case MSA::Account::Method::SMTP_STARTTLS:
    case MSA::Account::Method::SSMTP:
    {
        encryption->setVisible(true);
        encryptionLabel->setVisible(true);
        smtpHost->setVisible(true);
        lay->labelForField(smtpHost)->setVisible(true);
        smtpHost->setText(m_smtpAccountSettings->server());
        smtpPort->setVisible(true);
        lay->labelForField(smtpPort)->setVisible(true);
        smtpPort->setText(QString::number(m_smtpAccountSettings->port()));
        smtpPort->setValidator(new QIntValidator(1, 65535, this));
        smtpAuth->setVisible(true);
        lay->labelForField(smtpAuth)->setVisible(true);
        bool authEnabled = m_smtpAccountSettings->authenticateEnabled();
        smtpAuth->setChecked(authEnabled);
        smtpAuthReuseImapCreds->setVisible(authEnabled);
        lay->labelForField(smtpAuthReuseImapCreds)->setVisible(authEnabled);
        bool reuseImapCreds = m_smtpAccountSettings->reuseImapAuthentication();
        smtpAuthReuseImapCreds->setChecked(reuseImapCreds);
        smtpUser->setVisible(authEnabled && !reuseImapCreds);
        lay->labelForField(smtpUser)->setVisible(authEnabled && !reuseImapCreds);
        smtpUser->setText(m_smtpAccountSettings->username());
        sendmail->setVisible(false);
        lay->labelForField(sendmail)->setVisible(false);
        saveToImap->setVisible(true);
        lay->labelForField(saveToImap)->setVisible(true);
        saveToImap->setChecked(m_smtpAccountSettings->saveToImap());
        smtpBurl->setVisible(saveToImap->isChecked());
        lay->labelForField(smtpBurl)->setVisible(saveToImap->isChecked());
        smtpBurl->setChecked(m_smtpAccountSettings->useBurl());

        if (!m_pwWatcher->isPluginAvailable())
            smtpPass->setText(QString());

        passwordWarning->setVisible(authEnabled && !reuseImapCreds && !smtpPass->text().isEmpty());
        if (m_pwWatcher->isStorageEncrypted()) {
            passwordWarning->setStyleSheet(QString());
            passwordWarning->setText(trUtf8("This password will be saved in encrypted storage. "
                "If you do not enter password here, Trojitá will prompt for one when needed."));
        } else {
            passwordWarning->setStyleSheet(SettingsDialog::warningStyleSheet);
            passwordWarning->setText(trUtf8("This password will be saved in clear text. "
                "If you do not enter password here, Trojitá will prompt for one when needed."));
        }

        passwordPluginStatus->setVisible(authEnabled && !reuseImapCreds &&
                                         (!m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || !m_pwWatcher->didReadOk() || !m_pwWatcher->didWriteOk()));
        passwordPluginStatus->setText(m_pwWatcher->progressMessage());

        smtpPass->setVisible(authEnabled && !reuseImapCreds);
        smtpPass->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());
        lay->labelForField(smtpPass)->setVisible(authEnabled && !reuseImapCreds);
        lay->labelForField(smtpPass)->setEnabled(m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin());

        break;
    }
    case MSA::Account::Method::SENDMAIL:
    case MSA::Account::Method::IMAP_SENDMAIL:
        encryption->setVisible(false);
        encryptionLabel->setVisible(false);
        smtpHost->setVisible(false);
        lay->labelForField(smtpHost)->setVisible(false);
        smtpPort->setVisible(false);
        lay->labelForField(smtpPort)->setVisible(false);
        showPortWarning(QString());
        smtpAuth->setVisible(false);
        lay->labelForField(smtpAuth)->setVisible(false);
        smtpUser->setVisible(false);
        lay->labelForField(smtpUser)->setVisible(false);
        smtpPass->setVisible(false);
        lay->labelForField(smtpPass)->setVisible(false);
        passwordWarning->setVisible(false);
        passwordPluginStatus->setVisible(false);
        if (m_smtpAccountSettings->submissionMethod() == MSA::Account::Method::SENDMAIL) {
            sendmail->setVisible(true);
            lay->labelForField(sendmail)->setVisible(true);
            sendmail->setText(m_smtpAccountSettings->pathToSendmail());
            if (sendmail->text().isEmpty())
                sendmail->setText(Common::SettingsNames::sendmailDefaultCmd);
            saveToImap->setVisible(true);
            saveToImap->setChecked(m_smtpAccountSettings->saveToImap());
            lay->labelForField(saveToImap)->setVisible(true);
        } else {
            sendmail->setVisible(false);
            lay->labelForField(sendmail)->setVisible(false);
            saveToImap->setChecked(true);
            saveToImap->setVisible(false);
            lay->labelForField(saveToImap)->setVisible(false);
        }
        smtpBurl->setVisible(false);
        lay->labelForField(smtpBurl)->setVisible(false);
        smtpBurl->setChecked(m_smtpAccountSettings->useBurl());
        passwordPluginStatus->setVisible(false);
    }
    saveFolderName->setVisible(saveToImap->isChecked());
    lay->labelForField(saveFolderName)->setVisible(saveToImap->isChecked());
    saveFolderName->setText(m_smtpAccountSettings->sentMailboxName());

    emit widgetsUpdated();

}

void OutgoingPage::save(QSettings &s)
{
    m_smtpAccountSettings->saveSettings();

    if (smtpAuth->isVisibleTo(this) && smtpAuth->isChecked() && m_pwWatcher->isPluginAvailable() && !m_pwWatcher->isWaitingForPlugin()) {
        m_pwWatcher->setPassword(smtpPass->text());
    } else {
        emit saved();
    }
}

void OutgoingPage::showPortWarning(const QString &warning)
{
    if (!warning.isEmpty()) {
        portWarningLabel->setStyleSheet(SettingsDialog::warningStyleSheet);
        portWarningLabel->setVisible(true);
        portWarningLabel->setText(warning);
    } else {
        portWarningLabel->setStyleSheet(QString());
        portWarningLabel->setVisible(false);
    }

}

QWidget *OutgoingPage::asWidget()
{
    return this;
}

bool OutgoingPage::checkValidity() const
{
    switch (m_smtpAccountSettings->submissionMethod()) {
    case MSA::Account::Method::SMTP:
    case MSA::Account::Method::SMTP_STARTTLS:
    case MSA::Account::Method::SSMTP:
        if (checkProblemWithEmptyTextField(smtpHost, tr("The SMTP server hostname is missing here")))
            return false;
        if (smtpAuth->isChecked() && !smtpAuthReuseImapCreds->isChecked() && checkProblemWithEmptyTextField(smtpUser, tr("The SMTP username is missing here")))
            return false;
        break;
    case MSA::Account::Method::SENDMAIL:
        if (checkProblemWithEmptyTextField(sendmail, tr("The SMTP server hostname is missing here")))
            return false;
        break;
    case MSA::Account::Method::IMAP_SENDMAIL:
        break;
    }

    if (saveToImap->isChecked() && checkProblemWithEmptyTextField(saveFolderName, tr("Please specify the folder name here")))
        return false;

    return true;
}

bool OutgoingPage::passwordFailures(QString &message) const
{
    // The const_cast is needed as Qt4 does not define the arguement of isVisibleTo as const
    if (!smtpAuth->isVisibleTo(const_cast<Gui::OutgoingPage*>(this)) || !smtpAuth->isChecked() || !m_pwWatcher->isPluginAvailable() || m_pwWatcher->isWaitingForPlugin() || m_pwWatcher->didWriteOk()) {
        return false;
    } else {
        message = m_pwWatcher->progressMessage();
        return true;
    }
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
    emit saved();
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
