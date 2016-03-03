/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#include "PartWidget.h"

#include <QFontDatabase>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabBar>

#include "EnvelopeView.h"
#include "LoadablePartWidget.h"
#include "MessageView.h"
#include "Common/InvokeMethod.h"
#include "Cryptography/MessageModel.h"
#include "Gui/Util.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "UiUtils/IconLoader.h"

namespace {

    /** @short Unset flags which only make sense for one level of nesting */
    UiUtils::PartLoadingOptions filteredForEmbedding(const UiUtils::PartLoadingOptions options)
    {
        return options & UiUtils::MASK_PROPAGATE_WHEN_EMBEDDING;
    }
}

namespace Gui
{

QString quoteMeHelper(const QObjectList &children)
{
    QStringList res;
    for (QObjectList::const_iterator it = children.begin(); it != children.end(); ++it) {
        const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(*it);
        if (w)
            res += w->quoteMe();
    }
    return res.join(QStringLiteral("\n"));
}

MultipartAlternativeWidget::MultipartAlternativeWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        const int recursionDepth, const UiUtils::PartLoadingOptions options):
    QTabWidget(parent)
{
    setContentsMargins(0,0,0,0);

    const bool plaintextIsPreferred = options & UiUtils::PART_PREFER_PLAINTEXT_OVER_HTML;

    // Which "textual, boring part" should be shown?
    int preferredTextIndex = -1;

    // Some part which is "strange", i.e. something to draw the user's attention to
    int someSuspiciousIndex = -1;

    // First loop iteration is used to find out what MIME type to show.
    // Two iterations are needed because we have to know about whether we're shown or hidden when creating child widgets.
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid());

        QString mimeType = anotherPart.data(Imap::Mailbox::RolePartMimeType).toString();

        const bool isPlainText = mimeType == QLatin1String("text/plain");
        const bool isHtml = mimeType == QLatin1String("text/html");

        // At first, check whether this is one of the textual parts which we like
        if (isPlainText && plaintextIsPreferred) {
            preferredTextIndex = i;
        } else if (isHtml && !plaintextIsPreferred) {
            preferredTextIndex = i;
        }

        if (!isPlainText && !isHtml && someSuspiciousIndex == -1) {
            someSuspiciousIndex = i;
        }
    }

    // Show that part which is "the most important". The choice is usually between text/plain and text/html, one of them will win,
    // depending on the user's preferences.  If there are additional parts, the user will be alerted about them later on, and some
    // of these suspicious parts wins the race.
    // As usual, the later parts win in general.
    int preferredIndex = someSuspiciousIndex == -1 ?
                (preferredTextIndex == -1 ? partIndex.model()->rowCount(partIndex) - 1 : preferredTextIndex) :
                someSuspiciousIndex;

    // The second loop actually creates the widgets
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid());
        // TODO: This is actually not perfect, the preferred part of a multipart/alternative
        // which is nested as a non-preferred part of another multipart/alternative actually gets loaded here.
        // I can live with that.
        QWidget *item = factory->walk(anotherPart, recursionDepth + 1,
                                        filteredForEmbedding(i == preferredIndex ?
                                            options :
                                            options | UiUtils::PART_IS_HIDDEN));
        QString mimeType = anotherPart.data(Imap::Mailbox::RolePartMimeType).toString();

        const bool isPlainText = mimeType == QLatin1String("text/plain");
        const bool isHtml = mimeType == QLatin1String("text/html");

        if (isPlainText) {
            //: Translators: use something very short, perhaps even "text". Don't describe this as "Clear text".
            //: This string is used as a caption of a tab showing the plaintext part of a mail which is e.g.
            //: sent in both plaintext and HTML formats.
            mimeType = tr("Plaintext");
        } else if (isHtml) {
            //: Translators: caption of the tab which shows a HTML version of the mail. Use some short, compact text here.
            mimeType = tr("HTML");
        }

        addTab(item, mimeType);

        // Bug 332950: some items nested within a multipart/alternative message are not exactly an alternative.
        // One such example is a text/calendar right next to a text/html and a text/plain.
        if (!isPlainText && !isHtml) {
            // Unfortunately we cannot change the tab background with current Qt (Q1 2014),
            // see https://bugreports.qt-project.org/browse/QTBUG-840 for details
            setTabIcon(i, QIcon::fromTheme(QStringLiteral("emblem-important")));
        }
    }

    setCurrentIndex(preferredIndex);

    tabBar()->installEventFilter(this);
}

QString MultipartAlternativeWidget::quoteMe() const
{
    const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(currentWidget());
    return w ? w->quoteMe() : QString();
}

void MultipartAlternativeWidget::reloadContents()
{
    if (count()) {
        for (int i = 0; i < count(); ++i) {
            AbstractPartWidget *w = dynamic_cast<AbstractPartWidget *>(widget(i));
            if (w) {
                w->reloadContents();
            }
        }
    }
}

bool MultipartAlternativeWidget::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Wheel && qobject_cast<QTabBar*>(o)) { // don't alter part while wheeling
        e->ignore();
        return true;
    }
    return false;
}

PartStatusWidget::PartStatusWidget(QWidget *parent)
    : QFrame(parent)
    , m_icon(new QLabel(this))
    , m_text(new QLabel(this))
    , m_details(new QLabel(this))
    , m_seperator(new QFrame(this))
    , m_detailButton(new QPushButton(tr("Show Details"), this))
{
    connect(m_detailButton, &QAbstractButton::clicked, this, &PartStatusWidget::showDetails);

    hide();
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::ToolTipBase);
    setForegroundRole(QPalette::ToolTipText);
    QGridLayout *layout = new QGridLayout(this);
    layout->addWidget(m_icon, 0, 0);
    layout->addWidget(m_text, 0, 1);
    layout->setColumnStretch(1, 1);
    layout->addWidget(m_detailButton, 0, 2);
    m_detailButton->hide();
    m_seperator->setFrameStyle(QFrame::HLine);
    layout->addWidget(m_seperator, 1, 0, 1, 3);
    m_seperator->hide();
    layout->addWidget(m_details, 2, 0, 1, 3);
    m_details->hide();

    m_text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_details->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_details->setWordWrap(true);
}

void PartStatusWidget::showStatus(const QString &icon, const QString &status, const QString &details)
{
    m_icon->setPixmap(UiUtils::loadIcon(icon).pixmap(height()));
    m_text->setText(status);
    m_details->setText(details);
    if (!details.isEmpty() && !m_detailButton->isVisible()) {
        m_detailButton->show();
    } else if (details.isEmpty() && m_detailButton->isVisible()) {
        m_detailButton->hide();
    }
    show();
}

void PartStatusWidget::showDetails()
{
    if (m_details->isVisible()) {
        m_details->hide();
        m_seperator->hide();
        m_detailButton->setText(tr("Show Details"));
    } else {
        m_details->show();
        m_seperator->show();
        m_detailButton->setText(tr("Hide Details"));
    }
}

AsynchronousPartWidget::AsynchronousPartWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        const int recursionDepth, const UiUtils::PartLoadingOptions options)
    : QGroupBox(parent)
    , m_factory(factory)
    , m_partIndex(partIndex)
    , m_recursionDepth(recursionDepth)
    , m_options(options)
{
    Q_ASSERT(partIndex.isValid());
    auto model = qobject_cast<const Cryptography::MessageModel *>(partIndex.model());
    Q_ASSERT(model);
    connect(model, &QAbstractItemModel::rowsInserted, this, &AsynchronousPartWidget::handleRowsInserted);
    connect(model, &QAbstractItemModel::layoutChanged, this, &AsynchronousPartWidget::handleLayoutChanged);
    connect(model, &Cryptography::MessageModel::error, this, &AsynchronousPartWidget::handleError);

    setContentsMargins(0, 0, 0, 0);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_statusWidget = new PartStatusWidget(this);
    layout->addWidget(m_statusWidget);

    m_loadingSpinner = new Spinner(this);
    m_loadingSpinner->setText(tr("Loading\nMessage"));
    m_loadingSpinner->setType(Spinner::Sun);
    // TODO: Replace hard coded size such that the text is readable
    m_loadingSpinner->setMinimumSize(300, 300);
    layout->addWidget(m_loadingSpinner);

    // We have to call buildWidgets from the derived classes here,
    // but dynamic binding does not work inside ctors.
    // The easiest way to make this work is a roundtrip through the event loop by using CALL_LATER
    CALL_LATER(this, handleRowsInserted, Q_ARG(QModelIndex, partIndex), Q_ARG(int, 0), Q_ARG(int, 0));
}

void AsynchronousPartWidget::handleRowsInserted(const QModelIndex &parent, int row, int column)
{
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (parent == m_partIndex) {
        buildWidgets();
        // Trigger recalculation of the layout after adding children
        adjustSize();
    }
}

void AsynchronousPartWidget::handleLayoutChanged(const QList<QPersistentModelIndex> &parents)
{
    if (parents.isEmpty() || parents.contains(m_partIndex.parent())) {
        buildWidgets();
        adjustSize();
    }
}

void AsynchronousPartWidget::handleError(const QModelIndex &parent, const QString &status, const QString &details)
{
    if (parent == m_partIndex) {
        disconnect(m_partIndex.model(), &QAbstractItemModel::rowsInserted, this, &AsynchronousPartWidget::handleRowsInserted);
        disconnect(m_partIndex.model(), &QAbstractItemModel::layoutChanged, this, &AsynchronousPartWidget::handleLayoutChanged);
        m_loadingSpinner->hide();
        m_loadingSpinner->stop();
        m_statusWidget->showStatus(QStringLiteral("dialog-error"), status, details);
    }
}

MultipartSignedWidget::MultipartSignedWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        const int recursionDepth, const UiUtils::PartLoadingOptions loadingOptions)
    : AsynchronousPartWidget(parent, factory, partIndex, recursionDepth, loadingOptions)
{
    setTitle(tr("Signed Message"));
    m_loadingSpinner->setText(tr("Checking\nSignature"));
}

void MultipartSignedWidget::buildWidgets()
{
    Q_ASSERT(m_partIndex.isValid());
    disconnect(m_partIndex.model(), &QAbstractItemModel::rowsInserted, this, &MultipartSignedWidget::handleRowsInserted);
    disconnect(m_partIndex.model(), &QAbstractItemModel::layoutChanged, this, &MultipartSignedWidget::handleLayoutChanged);
    setFlat(true);
    uint childrenCount = m_partIndex.model()->rowCount(m_partIndex);
    if (childrenCount == 1) {
        setTitle(tr("Malformed multipart/signed message: only one nested part"));
        QModelIndex anotherPart = m_partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout()->addWidget(m_factory->walk(anotherPart, m_recursionDepth + 1, filteredForEmbedding(m_options)));
    } else if (childrenCount != 2) {
        QLabel *lbl = new QLabel(tr("Malformed multipart/signed message: %1 nested parts").arg(QString::number(childrenCount)), this);
        layout()->addWidget(lbl);
        return;
    } else {
        Q_ASSERT(childrenCount == 2); // from the if logic; FIXME: refactor
        QModelIndex anotherPart = m_partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout()->addWidget(m_factory->walk(anotherPart, m_recursionDepth + 1, filteredForEmbedding(m_options)));
    }
}

QString MultipartSignedWidget::quoteMe() const
{
    return quoteMeHelper(children());
}

bool MultipartSignedWidget::event(QEvent *e)
{
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        // Space is used by Qt for manipulating the checked/unchecked state, apparently.
        // We don't use that, but we need the Space key for scrolling.
        // In fact, this container should not accept any keyboard input anyway; it's just
        // a visual indicator saying "hey, this is a signed piece of data".
        return false;
    }
    return QGroupBox::event(e);
}

MultipartEncryptedWidget::MultipartEncryptedWidget(QWidget *parent, PartWidgetFactory *factory,
                                                   const QModelIndex &partIndex, const int recursionDepth,
                                                   const UiUtils::PartLoadingOptions loadingOptions)
    : AsynchronousPartWidget(parent, factory, partIndex, recursionDepth, loadingOptions)
{
    setTitle(tr("Encrypted Message"));
    m_loadingSpinner->setText(tr("Decrypting\nMessage"));
}

QString MultipartEncryptedWidget::quoteMe() const
{
    return quoteMeHelper(children());
}

void MultipartEncryptedWidget::buildWidgets()
{
    Q_ASSERT(m_partIndex.isValid());
    if (m_partIndex.model()->rowCount(m_partIndex) == 0 && !m_statusWidget->isVisibleTo(m_statusWidget->parentWidget())) {
        // We have to wait for the message structure to become available
        m_loadingSpinner->start(250);
        return;
    }

    m_loadingSpinner->hide();
    m_loadingSpinner->stop();

    disconnect(m_partIndex.model(), &QAbstractItemModel::rowsInserted, this, &MultipartEncryptedWidget::handleRowsInserted);
    disconnect(m_partIndex.model(), &QAbstractItemModel::layoutChanged, this, &MultipartEncryptedWidget::handleLayoutChanged);
    for (int i = 0; i < m_partIndex.model()->rowCount(m_partIndex); ++i) {
        QModelIndex anotherPart = m_partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        QWidget *res = m_factory->walk(anotherPart, m_recursionDepth + 1, filteredForEmbedding(m_options));
        layout()->addWidget(res);
    }
}

GenericMultipartWidget::GenericMultipartWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        int recursionDepth, const UiUtils::PartLoadingOptions options):
    QWidget(parent)
{
    setContentsMargins(0, 0, 0, 0);
    // multipart/mixed or anything else, as mandated by RFC 2046, Section 5.1.3
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        using namespace Imap::Mailbox;
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        QWidget *res = factory->walk(anotherPart, recursionDepth + 1, filteredForEmbedding(options));
        layout->addWidget(res);
    }
}

QString GenericMultipartWidget::quoteMe() const
{
    return quoteMeHelper(children());
}

Message822Widget::Message822Widget(QWidget *parent,
                                   PartWidgetFactory *factory, const QModelIndex &partIndex,
                                   int recursionDepth, const UiUtils::PartLoadingOptions options):
    QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    EnvelopeView *envelope = new EnvelopeView(0, factory->context());
    envelope->setMessage(partIndex);
    layout->addWidget(envelope);
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        using namespace Imap::Mailbox;
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        QWidget *res = factory->walk(anotherPart, recursionDepth + 1, filteredForEmbedding(options));
        layout->addWidget(res);
    }
}

QString Message822Widget::quoteMe() const
{
    return quoteMeHelper(children());
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
IMPL_RELOAD(MultipartEncryptedWidget);
IMPL_RELOAD(GenericMultipartWidget);
IMPL_RELOAD(Message822Widget);

#undef IMPL_RELOAD


}


