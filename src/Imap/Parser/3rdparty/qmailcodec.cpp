/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Qt Software Information (qt-info@nokia.com)
**
** This file is part of the Qt Messaging Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the either Technology Preview License Agreement or the
** Beta Release License Agreement.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this
** package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QTextCodec>
#include <QtDebug>

static void enumerateCodecs()
{
    static bool enumerated = false;

    if (!enumerated)
    {
        qWarning() << "Available codecs:";
        foreach (const QByteArray& codec, QTextCodec::availableCodecs())
            qWarning() << "  " << codec;

        enumerated = true;
    }
}

static QTextCodec* codecForName(const QByteArray& charset, bool translateAscii = true)
{
    QByteArray encoding(charset.toLower());

    if (!encoding.isEmpty())
    {
        int index;

        if (translateAscii && encoding.contains("ascii")) 
        {
            // We'll assume the text is plain ASCII, to be extracted to Latin-1
            encoding = "ISO-8859-1";
        }
        else if ((index = encoding.indexOf('*')) != -1)
        {
            // This charset specification includes a trailing language specifier
            encoding = encoding.left(index);
        }

        QTextCodec* codec = QTextCodec::codecForName(encoding);
        if (!codec)
        {
            qWarning() << "QMailCodec::codecForName - Unable to find codec for charset" << encoding;
            enumerateCodecs();
        }

        return codec;
    }

    return 0;
}

QString decodeByteArray(const QByteArray &encoded, const QString &charset)
{
    if (QTextCodec *codec = codecForName(charset.toLatin1())) {
        return codec->toUnicode(encoded);
    }
    return QString();
}

// ASCII character values used throughout
const unsigned char MaxPrintableRange = 0x7e;
const unsigned char Space = 0x20;
const unsigned char Equals = 0x3d;
const unsigned char QuestionMark = 0x3f;
const unsigned char Underscore = 0x5f;

bool rfc2047QPNeedsEscpaing(const int unicode)
{
    if (unicode <= Space)
        return true;
    if (unicode == Equals || unicode == QuestionMark || unicode == Underscore)
        return true;
    if (unicode > MaxPrintableRange)
        return true;
    return false;
}
