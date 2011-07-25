// FIXME: license

#ifndef GUI_UTIL_H
#define GUI_UTIL_H

class QWidget;

namespace Gui {

namespace Util {

/** @short Center widget on screen containing its parent widget of the mousepointer */
void centerWidgetOnScreen(QWidget *widget, bool centerOnCursorScreen=true);

} // namespace Util

} // namespace Gui

#endif // GUI_UTIL_H
