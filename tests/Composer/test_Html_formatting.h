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

#ifndef TEST_HTML_FORMATTING
#define TEST_HTML_FORMATTING

#include <QTest>
#include "Composer/PlainTextFormatter.h"

class QWebView;

/** @short Tests for HTML prettification of plaintext content */
class HtmlFormattingTest : public QObject
{
    Q_OBJECT
private slots:
    void testPlainTextFormattingFlowed();
    void testPlainTextFormattingFlowed_data();
    void testPlainTextFormattingFlowedDelSp();
    void testPlainTextFormattingFlowedDelSp_data();

    void testPlainTextFormattingViaHtml();
    void testPlainTextFormattingViaHtml_data();
    void testPlainTextFormattingViaPaste();
    void testPlainTextFormattingViaPaste_data();

    void testLinkRecognition();
    void testLinkRecognition_data();

    void testUnrecognizedLinks();
    void testUnrecognizedLinks_data();

    void testSignatures();
    void testSignatures_data();
};

class WebRenderingTester: public QObject
{
    Q_OBJECT
public:

    typedef enum {
        RenderDefaultCollapsing,
        RenderExpandEverythingCollapsed
    } CollapsingFlags;

    WebRenderingTester();
    virtual ~WebRenderingTester();
    QString asPlainText(const QString &input, const Composer::Util::FlowedFormat format,
                        const CollapsingFlags collapsing=RenderDefaultCollapsing);
public slots:
    void doDelayedLoad();
private:
    QWebView *m_web;
    QEventLoop *m_loop;
    QString sourceData;
};

#endif
