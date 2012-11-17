/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
 *
 * This file is part of the Trojita Qt IMAP e-mail client,
 * http://trojita.flaska.net/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or the version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

class QTimer;

#include <QList>
#include <QUrl>
#include <QTextEdit>

class ComposerTextEdit : public QTextEdit {
    Q_OBJECT
public:
    ComposerTextEdit(QWidget *parent = 0);
    /**
     * use the view to display a notification for @p timeout ms
     * using an empty or null string will clear the notification at once
     * a @p timeout of 0ms shows the notification until it's replaced or reset
     */
    void notify(const QString &n, uint timeout = 0);
signals:
    void sendRequest();
    void urlsAdded(QList<QUrl> urls);
protected:
    /** DND reimplementation **/
    bool canInsertFromMimeData( const QMimeData * source ) const;
    void insertFromMimeData(const QMimeData *source);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    /** painter reimplementation for notification **/
    void paintEvent(QPaintEvent *pe);
private slots:
    void resetNotification();
private:
    QString m_notification;
    QTimer *m_notificationTimer;
    bool m_couldBeSendRequest;
};
