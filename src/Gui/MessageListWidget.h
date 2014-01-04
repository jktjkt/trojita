/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QWidget>

class LineEdit;
class QTimer;
class QToolButton;

namespace Gui {

class MsgListView;

/** @short Widget containing a list of messages along with quick filtering toolbar */
class MessageListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent = 0);

    void setFuzzySearchSupported(bool supported);
    void setRawSearchEnabled(bool enabled);

    QStringList searchConditions() const;

    // FIXME: consider making this private and moving the logic from Window with it
    MsgListView *tree;

public slots:
    void focusSearch();
    void focusRawSearch();

signals:
    void requestingSearch(const QStringList &conditions);
    void rawSearchSettingChanged(bool enabled);

protected slots:
    void slotApplySearch();
    void slotAutoEnableDisableSearch();
    void slotSortingFailed();

private slots:
    void slotComplexSearchInput(QAction*);
    void slotConditionalSearchReset();
    void slotDeActivateSimpleSearch();
    void slotResetSortingFailed();
    void slotUpdateSearchCursor();

private:
    LineEdit *m_quickSearchText;
    QToolButton *m_searchOptions;
    QAction *m_searchInSubject;
    QAction *m_searchInBody;
    QAction *m_searchInSenders;
    QAction *m_searchInRecipients;
    QAction *m_searchFuzzy;
    QAction *m_rawSearch;
    bool m_supportsFuzzySearch;
    QTimer *m_searchResetTimer;
    QString m_queryPlaceholder;
};

}

#endif // MESSAGELISTWIDGET_H
