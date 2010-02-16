#ifndef GUI_ABSTRACTPARTWIDGET_H
#define GUI_ABSTRACTPARTWIDGET_H

namespace Gui {

/** @short An abstract glue interface providing support for message quoting */
class AbstractPartWidget
{
public:
    virtual QString quoteMe() const = 0;
};

}

#endif // GUI_ABSTRACTPARTWIDGET_H
