#include <QApplication>
#include <QCursor> //for Util::centerWidgetOnScreen
#include <QDesktopWidget> //for Util::centerWidgetOnScreen

#include "Util.h"

namespace Gui {

namespace Util {

void centerWidgetOnScreen(QWidget* widget, bool centerOnCursorScreen) {
    widget->adjustSize();
    //regarding the option to center widget on screen containing mousepointer:
    //(only relevant for dual-screen-setups)
    //If some day we'll have (configurable) key shortcuts there might be
    //situations when the mousepointer (and therefore most likely the user's
    //attention) is not on the screen containg widget's parentWidget
    //So by centerning the widget on the screen containing the mousepointer
    //we assure to be as close to the user's focus as possible.
    //For single screen setups this doesn't make any difference at all.
    //If the widget to be centered is shown as a result of a mouseClick
    //this makes no difference, too, since the mouseClick most probably happened
    //on the widget's parentWidget so the widget will be centered on the screen
    //containing it.
    //Still for the sake of completeness the option to pass false is kept open
    //for any case where it might be needed.
    if (centerOnCursorScreen) {
        widget->move( QApplication::desktop()->screenGeometry(
                            QCursor::pos()
                          ).center()
                      - widget->rect().center() );
    } else {
        widget->move( QApplication::desktop()->screenGeometry(
                            widget->parentWidget()
                          ).center()
                      - widget->rect().center() );
    }
}

}//namespace Util

}//namespace Gui


