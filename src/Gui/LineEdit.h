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
    bool isHistoryEnabled();
    void setHistoryEnabled(bool enabled = true);

signals:
    void textEditingFinished(const QString &text);

protected:
    bool eventFilter(QObject *o, QEvent *e);
    void keyReleaseEvent(QKeyEvent *ke);

private Q_SLOTS:
    void learnEntry();
    void restoreInlineCompletion();

private:
    bool m_historyEnabled;
    int m_historyPosition;
    QString m_currentText;
};

#endif // LINEEDIT_H
