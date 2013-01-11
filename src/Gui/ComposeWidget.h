/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <QList>
#include <QUrl>
#include <QWidget>

#include "Imap/Model/MessageComposer.h"

namespace Ui
{
class ComposeWidget;
}

class QAbstractListModel;
class QComboBox;
class QLineEdit;
class QMenu;
class QPushButton;

namespace Gui
{

class MainWindow;

/** @short A "Compose New Mail..." dialog

  Implements a widget which can act as a standalone window for composing e-mail messages
  */
class ComposeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ComposeWidget(MainWindow *parent);
    ~ComposeWidget();
    void setData(const QList<QPair<Composer::RecipientKind, QString> > &recipients,
                 const QString &subject,
                 const QString &body,
                 const QList<QByteArray> &inReplyTo, const QList<QByteArray> &references,
                 const QModelIndex &replyingToMessage);

protected:
    void changeEvent(QEvent *e);
    bool eventFilter(QObject *o, QEvent *e);

public slots:
    bool setReplyMode(const Composer::ReplyMode mode);

private slots:
    void collapseRecipients();
    void completeRecipient(QAction *act);
    void completeRecipients(const QString &text);
    void send();
    void gotError(const QString &error);
    void sent();
    void updateRecipientList();

    void slotAskForFileAttachment();
    void slotRemoveAttachment();
    void slotAttachFiles(QList<QUrl> urls);

    void slotAppendUidKnown(const uint uidValidity, const uint uid);
    void slotGenUrlAuthReceived(const QString &url);

    void slotUpdateSignature();

private:
    static QByteArray extractMailAddress(const QString &text, bool &ok);
    static Composer::RecipientKind recipientKindForNextRow(const Composer::RecipientKind kind);
    void addRecipient(int position, Composer::RecipientKind kind, const QString &address);
    bool parseRecipients(QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress> > &results, QString &errorMessage);
    void removeRecipient(int position);

    bool buildMessageData();
    bool shouldBuildMessageLocally() const;

    static QString killDomainPartFromString(const QString &s);

    Ui::ComposeWidget *ui;
    QPushButton *sendButton;
    QPushButton *cancelButton;
    typedef QPair<QComboBox*, QLineEdit*> Recipient;
    QList<Recipient> m_recipients;
    QTimer *m_recipientListUpdateTimer;

    bool m_appendUidReceived;
    uint m_appendUidValidity;
    uint m_appendUid;
    bool m_genUrlAuthReceived;
    QString m_urlauth;

    MainWindow *m_mainWindow;

    Imap::Mailbox::MessageComposer *m_composer;
    QAction *m_actionRemoveAttachment;

    QMenu *m_completionPopup;
    QLineEdit *m_completionReceiver;
    int m_completionCount;
    QPersistentModelIndex m_replyingTo;


    ComposeWidget(const ComposeWidget &); // don't implement
    ComposeWidget &operator=(const ComposeWidget &); // don't implement
};

}

#endif // COMPOSEWIDGET_H
