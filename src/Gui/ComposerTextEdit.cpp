/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

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

#include "ComposerTextEdit.h"
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QUrl>

ComposerTextEdit::ComposerTextEdit(QWidget *parent) : QTextEdit(parent)
, m_couldBeSendRequest(false)
{
    setAcceptRichText(false);
    setLineWrapMode(QTextEdit::FixedColumnWidth);
    setWordWrapMode(QTextOption::WordWrap);
    setLineWrapColumnOrWidth(78);
    m_notificationTimer = new QTimer(this);
    m_notificationTimer->setSingleShot(true);
    connect (m_notificationTimer, SIGNAL(timeout()), SLOT(resetNotification()));
}

void ComposerTextEdit::notify(const QString &n, uint timeout)
{
    m_notification = n;
    if (m_notification.isEmpty() || !timeout) {
        m_notificationTimer->stop();
    } else {
        m_notificationTimer->start(timeout);
    }
    viewport()->update();
}

void ComposerTextEdit::resetNotification()
{
    notify(QString());
}

bool ComposerTextEdit::canInsertFromMimeData( const QMimeData * source ) const
{
    QList<QUrl> urls = source->urls();
    foreach (const QUrl &url, urls) {
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
        if (url.isLocalFile())
#else
        if (url.scheme() == QLatin1String("file"))
#endif
            return true;
    }
    return QTextEdit::canInsertFromMimeData(source);
}

void ComposerTextEdit::insertFromMimeData(const QMimeData *source)
{
    QList<QUrl> urls = source->urls();
    foreach (const QUrl &url, urls) {
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
        if (url.isLocalFile()) {
#else
        if (url.scheme() == QLatin1String("file")) {
#endif
            emit urlsAdded(urls);
            return;
        }
    }
    QTextEdit::insertFromMimeData(source);
}


static inline bool isSendCombo(QKeyEvent *ke) {
    return (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && ke->modifiers() == Qt::ControlModifier;
}

void ComposerTextEdit::keyPressEvent(QKeyEvent *ke) {
    m_couldBeSendRequest = false;
    if (isSendCombo(ke)) {
        m_couldBeSendRequest = true;
    }
    QTextEdit::keyPressEvent(ke);
}

void ComposerTextEdit::keyReleaseEvent(QKeyEvent *ke)
{
    if (m_couldBeSendRequest && isSendCombo(ke)) {
        m_couldBeSendRequest = false;
        emit sendRequest();
        return;
    }
    m_couldBeSendRequest = false;
    QTextEdit::keyReleaseEvent(ke);
}

void ComposerTextEdit::paintEvent(QPaintEvent *pe)
{
    QTextEdit::paintEvent(pe);
    if ( !m_notification.isEmpty() )
    {
        const int s = width()/5;
        QRect r(s, 0, width()-2*s, height());
        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing);
        p.setClipRegion(pe->region());
        QFont fnt = font();
        fnt.setBold(true);
        fnt.setPointSize( fnt.pointSize()*2*r.width()/(3*QFontMetrics(fnt).width(m_notification)) );
        r.setHeight( QFontMetrics(fnt).height() + 16 );
        r.moveCenter( rect().center() );

        QColor c = palette().color(viewport()->foregroundRole());
        c.setAlpha( 2 * c.alpha() / 3 );
        p.setBrush( c );
        p.setPen( Qt::NoPen );
        p.drawRoundedRect( r, 8,8 );

        p.setPen( palette().color(viewport()->backgroundRole()) );
        p.setFont( fnt );
        p.drawText(r, Qt::AlignCenter|Qt::TextDontClip, m_notification );
        p.end();
    }
}
