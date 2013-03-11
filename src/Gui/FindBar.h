/* ============================================================
*
* This file is a part of the rekonq project
*
* Copyright (C) 2008-2012 by Andrea Diamantini <adjam7 at gmail dot com>
* Copyright (C) 2009-2011 by Lionel Chauvin <megabigbug@yahoo.fr>
*
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License or (at your option) version 3 or any later version
* accepted by the membership of KDE e.V. (or its successor approved
* by the membership of KDE e.V.), which shall act as a proxy
* defined in Section 14 of version 3 of the license.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* ============================================================ */


#ifndef GUI_FINDBAR_H
#define GUI_FINDBAR_H

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QString;
class QWebView;

namespace Gui {

class FindBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindBar(QWidget *parent);

    bool matchCase() const;
    void notifyMatch(bool match);
    bool highlightAllState() const;

    void setVisible(bool visible);
    void setAssociatedWebView(QWebView *webView);

protected:
    void keyPressEvent(QKeyEvent *event);

private slots:
    void find(const QString &);
    void matchCaseUpdate();
    void findNext();
    void findPrevious();
    void updateHighlight();

signals:
    void searchString(const QString &);

private:
    QLineEdit *m_lineEdit;
    QCheckBox *m_matchCase;
    QCheckBox *m_highlightAll;

    QString _lastStringSearched;
    QPointer<QWebView> m_associatedWebView;
};

}

#endif
