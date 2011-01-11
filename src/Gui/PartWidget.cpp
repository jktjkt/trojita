/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "PartWidget.h"

#include <QLabel>
#include <QVBoxLayout>

#include "PartWidgetFactory.h"
#include "Rfc822HeaderView.h"
#include "Imap/Model/MailboxTree.h"

namespace Gui {

QString quoteMeHelper( const QObjectList& children )
{
    QStringList res;
    for ( QObjectList::const_iterator it = children.begin(); it != children.end(); ++it ) {
        const AbstractPartWidget* w = dynamic_cast<const AbstractPartWidget*>( *it );
        if ( w )
            res += w->quoteMe();
    }
    return res.join("\n");
}

MultipartAlternativeWidget::MultipartAlternativeWidget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    const int recursionDepth ):
        QTabWidget( parent )
{
    for ( uint i = 0; i < part->childrenCount( factory->model() ); ++i ) {
        using namespace Imap::Mailbox;
        TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                part->child( i, factory->model() ) );
        Q_ASSERT( anotherPart );
        QWidget* item = factory->create( anotherPart, recursionDepth + 1 );
        addTab( item, anotherPart->mimeType() );
    }
    setCurrentIndex( part->childrenCount( factory->model() ) - 1 );
}

QString MultipartAlternativeWidget::quoteMe() const
{
    const AbstractPartWidget* w = dynamic_cast<const AbstractPartWidget*>( currentWidget() );
    return w ? w->quoteMe() : QString();
}

void MultipartAlternativeWidget::reloadContents()
{
    if ( count() ) {
        for ( int i = 0; i < count(); ++i ) {
            AbstractPartWidget* w = dynamic_cast<AbstractPartWidget*>( widget(i) );
            if ( w )
                w->reloadContents();
        }
    }
}

MultipartSignedWidget::MultipartSignedWidget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    const int recursionDepth ):
        QGroupBox( tr("Signed Message"), parent )
{
    using namespace Imap::Mailbox;
    QVBoxLayout* layout = new QVBoxLayout( this );
    uint childrenCount = part->childrenCount( factory->model() );
    if ( childrenCount != 2 ) {
        QLabel* lbl = new QLabel( tr("Mallformed multipart/signed message"), this );
        layout->addWidget( lbl );
        return;
    }
    TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
            part->child( 0, factory->model() ) );
    layout->addWidget( factory->create( anotherPart, recursionDepth + 1 ) );
}

QString MultipartSignedWidget::quoteMe() const
{
    return quoteMeHelper( children() );
}

GenericMultipartWidget::GenericMultipartWidget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    int recursionDepth):
        QGroupBox( tr("Multipart Message"), parent )
{
    // multipart/mixed or anything else, as mandated by RFC 2046, Section 5.1.3
    QVBoxLayout* layout = new QVBoxLayout( this );
    for ( uint i = 0; i < part->childrenCount( factory->model() ); ++i ) {
        using namespace Imap::Mailbox;
        TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                part->child( i, factory->model() ) );
        Q_ASSERT( anotherPart );
        QWidget* res = factory->create( anotherPart, recursionDepth + 1 );
        layout->addWidget( res );
    }
}

QString GenericMultipartWidget::quoteMe() const
{
    return quoteMeHelper( children() );
}

Message822Widget::Message822Widget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    int recursionDepth):
        QGroupBox( tr("Message"), parent )
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    QLabel* header = new Rfc822HeaderView( 0, factory->model(), part );
    layout->addWidget( header );
    for ( uint i = 0; i < part->childrenCount( factory->model() ); ++i ) {
        using namespace Imap::Mailbox;
        TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                part->child( i, factory->model() ) );
        Q_ASSERT( anotherPart );
        QWidget* res = factory->create( anotherPart, recursionDepth + 1 );
        layout->addWidget( res );
    }
}

QString Message822Widget::quoteMe() const
{
    return quoteMeHelper( children() );
}

#define IMPL_RELOAD(CLASS) void CLASS::reloadContents() \
{\
    /*qDebug() << metaObject()->className() << children().size();*/\
    Q_FOREACH( QObject* const obj, children() ) {\
        /*qDebug() << obj->metaObject()->className();*/\
        AbstractPartWidget* w = dynamic_cast<AbstractPartWidget*>( obj );\
        if ( w ) {\
            /*qDebug() << "reloadContents:" << w;*/\
            w->reloadContents();\
        }\
    }\
}

IMPL_RELOAD(MultipartSignedWidget);
IMPL_RELOAD(GenericMultipartWidget);
IMPL_RELOAD(Message822Widget);

#undef IMPL_RELOAD


}


