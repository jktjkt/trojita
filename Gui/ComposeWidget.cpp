#include "ComposeWidget.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include "RecipientsWidget.h"

namespace Gui {

ComposeWidget::ComposeWidget( QWidget* parent, const QString& from,
                              const QList<QPair<QString, QString> >& recipients,
                              const QString& subject ):
QWidget(parent, Qt::Window)
{
    setupWidgets( from, recipients, subject );
    setWindowTitle( tr("Compose Mail") );
}

void ComposeWidget::setupWidgets( const QString& from,
                                  const QList<QPair<QString, QString> >& recipients,
                                  const QString& subject )
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    QHBoxLayout* hLayout = new QHBoxLayout();
    QLabel* lbl = new QLabel( tr("From"), this );
    hLayout->addWidget( lbl );
    fromField = new QLineEdit( this );
    fromField->setText( from );
    hLayout->addWidget( fromField );
    layout->addLayout( hLayout );
    recipientsField = new RecipientsWidget( this, recipients );
    recipientsField->horizontalHeader()->hide();
    recipientsField->verticalHeader()->hide();
    layout->addWidget( recipientsField );
    hLayout = new QHBoxLayout();
    lbl = new QLabel( tr("Subject"), this );
    hLayout->addWidget( lbl );
    subjectField = new QLineEdit( this );
    subjectField->setText( subject );
    hLayout->addWidget( subjectField );
    layout->addLayout( hLayout );
    bodyField = new QTextEdit( this );
    layout->addWidget( bodyField );
}

}

#include "ComposeWidget.moc"
