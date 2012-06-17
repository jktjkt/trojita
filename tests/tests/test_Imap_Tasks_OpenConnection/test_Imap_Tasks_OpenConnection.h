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

#ifndef TEST_IMAP_MODEL_OPENCONNECTIONTASK
#define TEST_IMAP_MODEL_OPENCONNECTIONTASK

#include "Imap/Tasks/OpenConnectionTask.h"
#include "Streams/SocketFactory.h"

class QSignalSpy;

class ImapModelOpenConnectionTest : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void init( bool startTlsRequired );
    void cleanup();
    void initTestCase();

    void testPreauth();
    void testPreauthWithCapability();

    void testOk();
    void testOkWithCapability();
    void testOkMissingImap4rev1();

    void testOkLogindisabled();
    void testOkLogindisabledWithoutStarttls();
    void testOkLogindisabledLater();

    void testOkStartTls();
    void testOkStartTlsForbidden();
    void testOkStartTlsDiscardCaps();
    void testCapabilityAfterLogin();

    void testCompressDeflateOk();
    void testCompressDeflateNo();

    void testOpenConnectionShallBlock();

    void provideAuthDetails();
    void acceptSsl(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &sslErrors);

private:
    Imap::Mailbox::Model* model;
    Imap::Mailbox::FakeSocketFactory* factory;
    Imap::Mailbox::OpenConnectionTask* task;
    QSignalSpy* completedSpy;
    QSignalSpy* failedSpy;
    QSignalSpy* authSpy;
};

#endif
