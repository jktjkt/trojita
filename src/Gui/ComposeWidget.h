/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <QWidget>

namespace Ui {
    class ComposeWidget;
}

class QAbstractListModel;
class QComboBox;
class QCompleter;
class QLineEdit;
class QPushButton;

namespace Gui {

/** @short A "Compose New Mail..." dialog

  Implements a widget which can act as a standalone window for composing e-mail messages
  */
class ComposeWidget : public QWidget
{
    Q_OBJECT
public:
    ComposeWidget(QWidget *parent, QAbstractListModel *autoCompleteModel);
    ~ComposeWidget();

    void setData( const QString& from, const QList<QPair<QString, QString> >& recipients,
                  const QString& subject, const QString& body );

protected:
    void changeEvent(QEvent *e);

private slots:
    void send();
    void gotError( const QString& error );
    void sent();
    void handleRecipientAddressChange();

private:
    static QByteArray encodeHeaderField( const QString& text );
    static QByteArray extractMailAddress( const QString& text, bool& ok );
    void addRecipient( int position, const QString& kind, const QString& address );
    QList<QPair<QString, QString> > _parseRecipients();

    Ui::ComposeWidget *ui;
    QPushButton *sendButton;
    QPushButton *cancelButton;
    QList<QComboBox*> _recipientsKind;
    QList<QLineEdit*> _recipientsAddress;

    ComposeWidget(const ComposeWidget&); // don't implement
    ComposeWidget& operator=(const ComposeWidget&); // don't implement

    QCompleter *_recipientCompleter;    //< completer for known / recently used recipients
    void maybeAddNewKnownRecipient( const QString& recipient );
};

}

#endif // COMPOSEWIDGET_H
