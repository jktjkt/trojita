/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
** Modified (c) 2009, 2011, 2013 by Glad Deschrijver <glad.deschrijver@gmail.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QLineEdit>

class QToolButton;

class LineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit LineEdit(const QString &text, QWidget *parent = 0);
    explicit LineEdit(QWidget *parent = 0);
    QToolButton *clearButton();
    bool isHistoryEnabled();
    void setHistoryEnabled(bool enabled = true);
    virtual QSize sizeHint() const;

protected:
    bool eventFilter(QObject *o, QEvent *e);
    void keyReleaseEvent(QKeyEvent *ke);

private Q_SLOTS:
    void updateClearButton(const QString &text);
    void learnEntry();
    void restoreInlineCompletion();

private:
    void init();
    bool m_historyEnabled;
    int m_historyPosition;
    QString m_currentText;
    QToolButton *m_clearButton;
};

#endif // LINEEDIT_H
