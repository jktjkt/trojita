#include <QDateTime>
#include <QDir>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QImageReader>
#include <QKeyEvent>
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

#include "be.contacts.h"
#include "ui_be.contacts.h"
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

static QList<Field> fields;
QList<Field>::const_iterator findField(const QString &key)
{
    for (QList<Field>::const_iterator it = fields.constBegin(), end = fields.constEnd(); it != end; ++it)
        if (it->key == key)
            return it;
    return fields.constEnd();
}

}

BE::Contacts::Contacts()
{
    m_abook = new Gui::AbookAddressbook();

    m_currentContact = 0;
    QImage img(QDir::homePath() + "/.abook/incognito.png");
    if (!img.isNull())
        m_incognitoPic = QPixmap::fromImage(img.scaled(160,160,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    m_ui = new Ui::Contacts;
    m_ui->setupUi(this);
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
    connect (qApp, SIGNAL(aboutToQuit()), SLOT(saveContacts()));

    // cheat to correct the focuspolicies ;-)
    updateFocusPolicy(m_ui2->name, m_ui->filter);
}

BE::Contacts::~Contacts()
{
    delete m_abook;
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
        if (url.isLocalFile() && QImageReader(url.path()).canRead()) {
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
    const QString photo = QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + QFileInfo(path).suffix();
    const QString file = QDir::homePath() + "/.abook/" + photo;
    if (QFile::copy(path, file)) {
        m_currentContact->setData(photo, Gui::AbookAddressbook::Photo);
        setPhoto(file);
    }
}

void BE::Contacts::addContact()
{
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
    if (m_currentContact) {
        QModelIndex idx = m_sortFilterProxy->mapFromSource(m_currentContact->index());
        delete m_currentContact;
        m_currentContact = 0;
        m_sortFilterProxy->removeRow(idx.row());
    }
}

void BE::Contacts::setContact(const QModelIndex &index)
{
    if (m_currentContact) {
        if (m_abook->model()->itemFromIndex(m_sortFilterProxy->mapToSource(index)) == m_currentContact)
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
                }
            }
        }
    }

    m_currentContact = m_abook->model()->itemFromIndex(m_sortFilterProxy->mapToSource(index));
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

