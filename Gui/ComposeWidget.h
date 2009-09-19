#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <QWidget>

namespace Ui {
    class ComposeWidget;
}

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

signals:
    void fakeMail(const QString& body);

private:
    static QByteArray encodeHeaderField( const QString& text );
    static QByteArray extractMailAddress( const QString& text, bool& ok );

    Ui::ComposeWidget *ui;
};

}

#endif // COMPOSEWIDGET_H
