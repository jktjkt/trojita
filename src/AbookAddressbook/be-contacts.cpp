/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2013 Caspar Schutijser <caspar@schutijser.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QDateTime>
#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QImageReader>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMetaProperty>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

#include <QtDebug>

#include "be-contacts.h"
#include "ui_be-contacts.h"
#include "ui_onecontact.h"
#include "AbookAddressbook/AbookAddressbook.h"

namespace BE {

class Field {
public:
    typedef Gui::AbookAddressbook::Type Type;
    Field(Type type, QLabel *label, QString key)
    {
        this->type = type;
        this->label = label;
        this->key = key;
    }
    Field(const Field &other)
    {
        this->type = other.type;
        this->label = other.label;
        this->key = other.key;
    }
    QLabel *label;
    Type type;
    QString key;
};
}

BE::Contacts::Contacts(Gui::AbookAddressbook *abook): m_abook(abook), m_dirty(false)
{
    m_currentContact = 0;
    QImage img(QDir::homePath() + "/.abook/incognito.png");
    if (!img.isNull())
        m_incognitoPic = QPixmap::fromImage(img.scaled(160,160,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    m_ui = new Ui::Contacts;
    m_ui->setupUi(this);
#if QT_VERSION >= 0x040700
    m_ui->filter->setPlaceholderText(tr("Filter"));
#endif
    m_ui2 = new Ui::OneContact;
    m_ui2->setupUi(m_ui->oneContact);

    fields <<   Field(Gui::AbookAddressbook::Name, m_ui2->name, "name") << Field(Gui::AbookAddressbook::Mail, m_ui2->mail, "email") <<
                Field(Gui::AbookAddressbook::Address, m_ui2->address, "address") << Field(Gui::AbookAddressbook::City, m_ui2->city, "city") <<
                Field(Gui::AbookAddressbook::State, m_ui2->state, "state") << Field(Gui::AbookAddressbook::ZIP, m_ui2->zip, "zip") <<
                Field(Gui::AbookAddressbook::Country, m_ui2->country, "country") << Field(Gui::AbookAddressbook::Phone, m_ui2->phone, "phone") <<
                Field(Gui::AbookAddressbook::Workphone, m_ui2->workphone, "workphone") << Field(Gui::AbookAddressbook::Fax, m_ui2->fax, "fax") <<
                Field(Gui::AbookAddressbook::Mobile, m_ui2->mobile, "mobile") << Field(Gui::AbookAddressbook::Nick, m_ui2->nick, "nick") <<
                Field(Gui::AbookAddressbook::URL, m_ui2->url, "url") << Field(Gui::AbookAddressbook::Notes, m_ui2->notes, "notes") <<
                Field(Gui::AbookAddressbook::Anniversary, m_ui2->anniversary, "anniversary") << Field(Gui::AbookAddressbook::Photo, m_ui2->photo, "photo");

    m_sortFilterProxy = new QSortFilterProxyModel(this);
    m_sortFilterProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortFilterProxy->setFilterKeyColumn(-1);
    m_sortFilterProxy->setSourceModel(m_abook->model());
    connect (m_ui->filter, SIGNAL(textChanged(QString)), m_sortFilterProxy, SLOT(setFilterWildcard(QString)));
    m_ui->filter->installEventFilter(this);

    QFont fnt = m_ui2->name->font();
    fnt.setPointSize(fnt.pointSize()*2);
    m_ui2->name->setFont(fnt);
    for (QList<Field>::const_iterator   it = fields.constBegin(),
                                        end = fields.constEnd(); it != end; ++it) {
        it->label->installEventFilter(this);
    }

    m_ui->contacts->setModel(m_sortFilterProxy);
    m_ui->contacts->setSelectionMode(QAbstractItemView::SingleSelection);
    connect (m_ui->contacts->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(setContact(QModelIndex)));
    QModelIndex idx = m_sortFilterProxy->index(0,0);
    if (idx.isValid())
        m_ui->contacts->setCurrentIndex(idx);
    m_ui->contacts->installEventFilter(this);

    connect (m_ui->add, SIGNAL(clicked()), SLOT(addContact()));
    connect (m_ui->remove, SIGNAL(clicked()), SLOT(removeCurrentContact()));
    connect (qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), SLOT(updateFocusPolicy(QWidget*, QWidget*)));

    // cheat to correct the focuspolicies ;-)
    updateFocusPolicy(m_ui2->name, m_ui->filter);
}

BE::Contacts::~Contacts()
{
}

void BE::Contacts::closeEvent(QCloseEvent *)
{
    if (m_dirty) {
        if (QMessageBox::question(this, tr("Contacts"), tr("Save changes?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            saveContacts();
        } else {
            m_abook->model()->removeRows(0, m_abook->model()->rowCount());
            m_abook->readAbook(false);
        }
    }
}

void BE::Contacts::saveContacts()
{
    setContact(QModelIndex()); // store current contact changes
    m_abook->saveContacts();
}

static QString lifeText(const QLabel *l)
{
    if (QTextDocument *t = l->findChild<QTextDocument*>())
        return t->toPlainText();
    return l->text();
}

static void setText(QLabel *l, const QString &text)
{
    if (QTextDocument *t = l->findChild<QTextDocument*>())
        t->setPlainText(text);
    l->setText("");
    l->setText(text);
}

bool BE::Contacts::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_ui->filter) {
        if (e->type() == QEvent::KeyPress)
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Down)
            m_ui->contacts->setFocus();
        return false;
    }

    if (o == m_ui->contacts) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_Up && !m_ui->contacts->currentIndex().row()) {
            m_ui->filter->setFocus();
            return true;
            } else if (ke->key() == Qt::Key_Delete) {
                removeCurrentContact();
                return true;
            }
        }
        return false;
    }

    if (!m_currentContact)
        return false;
    switch (e->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::Drop: {
        if (o != m_ui2->photo)
            return false;
        QDropEvent *de = static_cast<QDropEvent*>(e);
        if (!de->mimeData())
            return false;
        QList<QUrl> urls = de->mimeData()->urls();
        if (urls.isEmpty())
            return false;
        QUrl url = urls.first();
        if (
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
                url.isLocalFile()
#else
                (url.scheme() == QLatin1String("file"))
#endif
                && QImageReader(url.path()).canRead()) {
            if (e->type() == QEvent::Drop)
                importPhoto(url.path());
            else
                de->acceptProposedAction();
        }
        return false;
    }
    case QEvent::KeyPress: {
        const int key = static_cast<QKeyEvent*>(e)->key();
        if (key == Qt::Key_Delete && o == m_ui2->photo) { // reset photo
            if (m_currentContact)
                m_currentContact->setData(QString(), Gui::AbookAddressbook::Photo);
            m_ui2->photo->setPixmap(m_incognitoPic);
        }
        else if (key == Qt::Key_Escape && o != m_ui2->photo)
        if (QLabel *l = qobject_cast<QLabel*>(o)) {
            setText(l, l->text());
            return true; // prevent closing the dialog!
        }
    }
    default:
        return false;
    }
    return false;
}

void BE::Contacts::importPhoto(const QString &path)
{
    if (!m_currentContact) {
        qWarning("CANNOT IMPORT PHOTO FOR INVALID CONTACT!");
        return;
    }
    const QString photo = QString::number(
#if QT_VERSION < QT_VERSION_CHECK(4, 7, 0)
                QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0)).secsTo(QDateTime::currentDateTime())
#else
                QDateTime::currentMSecsSinceEpoch()
#endif
                ) + "." + QFileInfo(path).suffix();

    const QString file = QDir::homePath() + "/.abook/" + photo;
    if (QFile::copy(path, file)) {
        m_currentContact->setData(photo, Gui::AbookAddressbook::Photo);
        setPhoto(file);
    }
}

void BE::Contacts::addContact()
{
    m_dirty = true;
    QStandardItem *item = new QStandardItem("[New Contact]");
    for (QList<Field>::const_iterator   it = fields.constBegin(),
                                        end = fields.constEnd(); it != end; ++it) {
        if (it->type == Gui::AbookAddressbook::Name || it->type == Gui::AbookAddressbook::Photo)
            continue;
        item->setData( "[" + it->key + "]", it->type );
    }
    if (m_currentContact)
        m_abook->model()->insertRow(m_currentContact->index().row() + 1, item);
    else
        m_abook->model()->appendRow(item);
    m_ui->contacts->setCurrentIndex(m_sortFilterProxy->mapFromSource(item->index()));
}

void BE::Contacts::removeCurrentContact()
{
    m_dirty = true;
    if (m_currentContact) {
        QModelIndex idx = m_sortFilterProxy->mapFromSource(m_currentContact->index());
        delete m_currentContact;
        m_currentContact = 0;
        m_sortFilterProxy->removeRow(idx.row());
    }
}

void BE::Contacts::setContact(const QModelIndex &index)
{
    QModelIndex translated = index;
    if (index.model() == m_sortFilterProxy)
        translated = m_sortFilterProxy->mapToSource(index);

    if (m_currentContact) {
        if (m_abook->model()->itemFromIndex(translated) == m_currentContact)
            return; // same one
        for (QList<Field>::const_iterator   it = fields.constBegin(),
                                            end = fields.constEnd(); it != end; ++it) {
            if (it->type != Gui::AbookAddressbook::Photo) {
                QString s = lifeText(it->label);
                if (s.startsWith("["))
                    s.clear();
                if (m_currentContact->data(it->type).toString() != s) {
                    m_currentContact->setData( true, Gui::AbookAddressbook::Dirty );
                    if (s.isEmpty())
                        m_currentContact->setData( QVariant(), it->type ); // invalidate
                    else
                        m_currentContact->setData( s, it->type );
                    m_dirty = true;
                }
            }
        }
    }

    m_currentContact = m_abook->model()->itemFromIndex(translated);
    if (!m_currentContact)
        return;

    for (QList<Field>::const_iterator   it = fields.constBegin(),
        end = fields.constEnd(); it != end; ++it) {
        if (it->type != Gui::AbookAddressbook::Photo) {
            QString s;
            QVariant v = m_currentContact->data(it->type);
            if (v.isValid())
                s = v.toString();
            else
                s = "[" + it->key + "]";
            setText(it->label, s);
        }
    }
    QPixmap userPic = m_incognitoPic;
    QString photo = m_currentContact->data(Gui::AbookAddressbook::Photo).toString();
    if (!photo.isEmpty()) {
        if (QDir::isRelativePath(photo))
            photo = QDir::homePath() + "/.abook/" + photo;
        if (QFile::exists(photo) && setPhoto(photo))
            return;
    }
    m_ui2->photo->setPixmap(userPic);

    m_ui->contacts->setCurrentIndex(m_sortFilterProxy->mapFromSource(translated));
}

bool BE::Contacts::setPhoto(const QString &path)
{
    QRect r(0,0,160,160);
    QImage  img = QImage( path );
    if (img.isNull())
        return false;
    const float f = qMin( float(img.width())/float(r.width()),
                            float(img.height())/float(r.height()) );
    r.setSize( r.size()*f );
    r.moveTopRight( img.rect().topRight() );
    img = img.copy(r).scaled( QSize(160,160), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    img = img.convertToFormat(QImage::Format_ARGB32);

//     if ( !MPC::setting("rgbCover").toBool() )
    {
        int r,g,b;
        palette().color(foregroundRole()).getRgb(&r,&g,&b);

        int n = img.width() * img.height();
        const uchar *bits = img.bits();
        QRgb *pixel = (QRgb*)(const_cast<uchar*>(bits));

        // this creates a (slightly) translucent monochromactic version of the
        // image using the foreground color
        // the gray value is turned into the opacity
        #define ALPHA qAlpha(pixel[i])
        #define GRAY qGray(pixel[i])
        #define OPACITY 224
        if ( qMax( qMax(r,g), b ) > 128 ) // value > 50%, bright foreground
            for (int i = 0; i < n; ++i)
                pixel[i] = qRgba( r,g,b, ( ALPHA * ( (OPACITY*GRAY) / 255 ) ) / 255 );
            else // inverse
            for (int i = 0; i < n; ++i)
                pixel[i] = qRgba( r,g,b, ( ALPHA * ( (OPACITY*(255-GRAY)) / 255 ) ) / 255 );
    }
#if 1
    QPainterPath glasPath;
    glasPath.moveTo( img.rect().topLeft() );
    glasPath.lineTo( img.rect().topRight() );
    glasPath.quadTo( img.rect().center()/2, img.rect().bottomLeft() );

    QPainter p( &img );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( Qt::NoPen );
    p.setBrush( QColor(255,255,255,64) );
    p.drawPath(glasPath);
    p.end();
#endif
    m_ui2->photo->setPixmap( QPixmap::fromImage( img ) );
    return true;
}

void BE::Contacts::updateFocusPolicy(QWidget *oldFocus, QWidget *newFocus)
{
    bool wasEdit(false), isEdit(false);
    for (QList<Field>::const_iterator it = fields.constBegin(), end = fields.constEnd(); it != end; ++it) {
        if (oldFocus == it->label)
            wasEdit = true;
        else if (newFocus == it->label)
            isEdit = true;
        if (isEdit && wasEdit)
            break;
    }

    if (isEdit == wasEdit)
        return;

    Qt::FocusPolicy policy = isEdit ? Qt::StrongFocus : Qt::ClickFocus;

    for (QList<Field>::const_iterator it = fields.constBegin(), end = fields.constEnd(); it != end; ++it)
        it->label->setFocusPolicy(policy);

    m_ui2->photo->setFocusPolicy(Qt::ClickFocus);

    policy = isEdit ? Qt::ClickFocus : Qt::StrongFocus;
    m_ui->filter->setFocusPolicy(policy);
    m_ui->contacts->setFocusPolicy(policy);
    m_ui->add->setFocusPolicy(policy);
    m_ui->remove->setFocusPolicy(policy);
}

void BE::Contacts::manageContact(const QString &mail, const QString &prettyName)
{
    QStandardItemModel *model = m_abook->model();
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem *item = model->item(i);
        if (QString::compare(item->data(Gui::AbookAddressbook::Mail).toString(), mail, Qt::CaseInsensitive) == 0) {
            setContact(model->index(i, 0));
            return;
        }
    }

    // no match -> create one
    addContact();
    m_ui2->mail->setText(mail);
    if (!prettyName.isEmpty()) {
        m_ui2->name->setText(prettyName);
        m_currentContact->setText(prettyName);
    } else {
        m_ui2->name->setText("[name]");
    }
}
