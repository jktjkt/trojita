/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
** Modified (c) 2009, 2011, 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include "LineEdit.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QStyle>

LineEdit::LineEdit(const QString &text, QWidget *parent)
	: QLineEdit(parent)
{
    init();
    setText(text);
}

LineEdit::LineEdit(QWidget *parent)
	: QLineEdit(parent)
{
    init();
}

void LineEdit::init()
{
    m_clearButton = new QToolButton(this);
    const QPixmap pixmap(QLatin1String(":/icons/edit-clear-locationbar-rtl.png"));
    m_clearButton->setIcon(QIcon(pixmap));
    m_clearButton->setIconSize(pixmap.size());
    m_clearButton->setCursor(Qt::ArrowCursor);
    m_clearButton->setToolTip(tr("Clear input field"));
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->hide();
    connect(m_clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(QString)), this, SLOT(updateClearButton(QString)));

    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    m_clearButton->setStyleSheet(QString("QToolButton { border: none; padding-left: 1px; padding-top: %1px; padding-bottom: %1px; padding-right: %1px; }")
        .arg(frameWidth + 1));
    setStyleSheet(QString("QLineEdit { padding-right: %1px; }")
        .arg(m_clearButton->sizeHint().width()));

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    layout->addWidget(m_clearButton);
}

QToolButton *LineEdit::clearButton()
{
    return m_clearButton;
}

QSize LineEdit::sizeHint() const
{
    const QSize defaultSize = QLineEdit::sizeHint();
    const int minimumWidth = m_clearButton->sizeHint().width() + 2;
    return QSize(qMax(defaultSize.width(), minimumWidth), defaultSize.height());
}

void LineEdit::updateClearButton(const QString &text)
{
    m_clearButton->setVisible(!text.isEmpty());
}
