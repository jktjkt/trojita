#ifndef COMPOSEWIDGET_H
#define COMPOSEWIDGET_H

#include <QWidget>

class QLineEdit;
class QTextEdit;

namespace Gui {

class RecipientsWidget;

class ComposeWidget : public QWidget
{
    Q_OBJECT
public:
    ComposeWidget( QWidget* parent, const QString& from, const QList<QPair<QString, QString> >& recipients, const QString& subject );
private:
    void setupWidgets( const QString& from, const QList<QPair<QString, QString> >& recipients, const QString& subject );

    QLineEdit* fromField;
    RecipientsWidget* recipientsField;
    QLineEdit* subjectField;
    QTextEdit* bodyField;
};

}

#endif // COMPOSEWIDGET_H
