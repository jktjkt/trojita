#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QTextEdit;

namespace Gui {

class RecipientsWidget;

class ComposeWidget : public QWidget
{
    Q_OBJECT
public:
    ComposeWidget( QWidget* parent, const QString& from, const QList<QPair<QString, QString> >& recipients, const QString& subject );
private slots:
    void send();
    void gotError( const QString& error );
    void sent();
private:
    void setupWidgets( const QString& from, const QList<QPair<QString, QString> >& recipients, const QString& subject );
    static QByteArray encodeHeaderField( const QString& text );
    static QByteArray extractMailAddress( const QString& text, bool& ok );

    QLineEdit* fromField;
    RecipientsWidget* recipientsField;
    QLineEdit* subjectField;
    QPushButton* sendButton;
    QTextEdit* bodyField;
};

}

#endif // COMPOSEWIDGET_H
