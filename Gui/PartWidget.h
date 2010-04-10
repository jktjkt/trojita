#ifndef GUI_PARTWIDGET_H
#define GUI_PARTWIDGET_H

#include <QGroupBox>
#include <QTabWidget>

#include "AbstractPartWidget.h"

namespace Imap {
namespace Mailbox {
    class TreeItemPart;
}
}

namespace Gui {

class PartWidgetFactory;

/** @short Message quoting support for multipart/alternative MIME type */
class MultipartAlternativeWidget: public QTabWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    MultipartAlternativeWidget( QWidget* parent, PartWidgetFactory* factory,
                                Imap::Mailbox::TreeItemPart* part,
                                const int recursionDepth );
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for multipart/signed MIME type */
class MultipartSignedWidget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    MultipartSignedWidget( QWidget* parent, PartWidgetFactory* factory,
                                Imap::Mailbox::TreeItemPart* part,
                                const int recursionDepth );
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for generic multipart/ * */
class GenericMultipartWidget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    GenericMultipartWidget( QWidget* parent, PartWidgetFactory* factory,
                                Imap::Mailbox::TreeItemPart* part,
                                const int recursionDepth );
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for generic multipart/ * */
class Message822Widget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    Message822Widget( QWidget* parent, PartWidgetFactory* factory,
                                Imap::Mailbox::TreeItemPart* part,
                                const int recursionDepth );
    virtual QString quoteMe() const;
    virtual void reloadContents();
};


}

#endif // GUI_PARTWIDGET_H
