/*
   Copyright (C) 2012, 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>

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

#include "ShortcutConfigDialog.h"

#ifndef QT_NO_SHORTCUT

#include <QDialogButtonBox>

#include "ShortcutConfigWidget.h"
#include "ShortcutHandler.h"

namespace Gui
{

ShortcutConfigDialog::ShortcutConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setModal(true);
    setWindowTitle(tr("Configure Shortcuts") + QLatin1String(" - ") + trUtf8("Trojitá"));

    m_shortcutConfigWidget = new ShortcutConfigWidget(this);
    connect(m_shortcutConfigWidget, SIGNAL(shortcutsChanged(QHash<QString,ActionDescription>)), this, SIGNAL(shortcutsChanged(QHash<QString,ActionDescription>)));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(QDialogButtonBox::Ok);
    buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QWidget *buttonWidget = new QWidget(this);
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->addWidget(m_shortcutConfigWidget->clearButton());
    buttonLayout->addWidget(m_shortcutConfigWidget->useDefaultButton());
    buttonLayout->addStretch();
    buttonLayout->addWidget(buttonBox);
    buttonLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_shortcutConfigWidget);
    mainLayout->addWidget(buttonWidget);
    setLayout(mainLayout);
}

ShortcutConfigDialog::~ShortcutConfigDialog()
{
}

/***************************************************************************/

void ShortcutConfigDialog::setExclusivityGroups(const QList<QStringList> &groups)
{
    m_shortcutConfigWidget->setExclusivityGroups(groups);
}

void ShortcutConfigDialog::setActionDescriptions(const QHash<QString, ActionDescription> &actionDescriptions)
{
    m_shortcutConfigWidget->setActionDescriptions(actionDescriptions);
}

/***************************************************************************/

void ShortcutConfigDialog::accept()
{
    m_shortcutConfigWidget->accept();
    QDialog::accept();
}

void ShortcutConfigDialog::reject()
{
    m_shortcutConfigWidget->reject();
    QDialog::reject();
}

} // namespace Gui

#endif // QT_NO_SHORTCUT
