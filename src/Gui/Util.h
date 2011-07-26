#ifndef UTIL_H
#define UTIL_H

class QWidget;

namespace Gui {

namespace Util {

/** @short center widget on screen containing its parent widget of the mousepointer */
void centerWidgetOnScreen(QWidget* widget, bool centerOnCursorScreen = true) ;

}//namespace Util

}//namespace Gui

#endif // UTIL_H
