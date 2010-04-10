#ifndef EXTERNALELEMENTSWIDGET_H
#define EXTERNALELEMENTSWIDGET_H

#include <QWidget>

class QPushButton;

namespace Gui {

class ExternalElementsWidget : public QWidget
{
    Q_OBJECT
public:
    ExternalElementsWidget( QWidget* parent );
signals:
    void loadingEnabled();
private:
    QPushButton* loadStuffButton;
};

}

#endif // EXTERNALELEMENTSWIDGET_H
