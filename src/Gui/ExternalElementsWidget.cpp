#include "ExternalElementsWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace Gui {

ExternalElementsWidget::ExternalElementsWidget( QWidget* parent ):
        QWidget(parent)
{
    QHBoxLayout* layout = new QHBoxLayout( this );
    loadStuffButton = new QPushButton( tr("Load"), this );
    QLabel* lbl = new QLabel( tr("This e-mail wants to load external entities form the Internet"), this );
    layout->addWidget( lbl );
    layout->addWidget( loadStuffButton );
    connect(loadStuffButton, SIGNAL(clicked()), this, SIGNAL(loadingEnabled()));

/*
FIXME: would be cool to have more clear error messages

Also perhaps provide a list of URLs of external entities? But note that
the current implementation is an "all or nothing" -- once the user clicks
the "allow external entities" button, they will be enabled for the current
message until it is closed. Therefore, it is possible that user allows requests
to a presumably "safe site", but something evil subsequently asks for something
from a malicious, third-party site. I'm not sure how they'd do that, though, given
that we already do disable JavaScript...

*/

}

}
