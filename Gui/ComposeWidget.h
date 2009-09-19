#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <QWidget>

namespace Ui {
    class ComposeWidget;
}

class QComboBox;
class QLineEdit;

namespace Gui {

/** @short A "Compose New Mail..." dialog

  Implements a widget which can act as a standalone window for composing e-mail messages
  */
class ComposeWidget : public QWidget
{
    Q_OBJECT
public:
    ComposeWidget(QWidget *parent = 0);
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
    QList<QComboBox*> _recipientsKind;
    QList<QLineEdit*> _recipientsAddress;
};

}

#endif // COMPOSEWIDGET_H
