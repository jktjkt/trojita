/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2014 Stephan Platz <trojita@paalsteek.de>

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
#ifndef TEST_IMAP_DATA
#define TEST_IMAP_DATA

#include <QByteArray>

const QByteArray bsPlaintext("\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL");
const QByteArray bsMultipartSignedTextPlain(
        "(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"quoted-printable\" 539 16)"
        "(\"application\" \"pgp-signature\" (\"name\" \"signature.asc\") NIL \"Digital signature\" \"7bit\" 205)"
        " \"signed\"");
const QByteArray bsTortureTest("(\"TEXT\" \"PLAIN\" (\"charset\" \"us-ascii\") NIL \"Explanation\" \"7bit\" 190 3 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Rich Text demo\" \"7bit\" 4906 "
        "(\"Tue, 24 Dec 1991 08:14:27 -0500 (EST)\" \"Re: a MIME-Version misfeature\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((NIL NIL \"ietf-822\" \"dimacs.rutgers.edu\")) NIL NIL NIL \"<sdJn_nq0M2YtNKaFtO@thumper.bellcore.com>\") "
        "((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 767 16 NIL NIL NIL NIL)"
        "((\"text\" \"richtext\" (\"charset\" \"us-ascii\") NIL NIL \"quoted-printable\" 887 13 NIL NIL NIL NIL) "
        "\"mixed\" (\"boundary\" \"Alternative_Boundary_8dJn:mu0M2Yt5KaFZ8AdJn:mu0M2Yt1KaFdA\") NIL NIL NIL)"
        "(\"application\" \"andrew-inset\" NIL NIL NIL \"7bit\" 917 NIL NIL NIL NIL) \"alternative\" "
        "(\"boundary\" \"Interpart_Boundary_AdJn:mu0M2YtJKaFh9AdJn:mu0M2YtJKaFk=\") NIL NIL NIL) 106 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Voice Mail demo\" \"7bit\" 562270 (\"Tue, 8 Oct 91 10:25:36 EDT\" \"Re: multipart mail\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((NIL NIL \"mrc\" \"panda.com\")) "
        "NIL NIL NIL \"<9110081425.AA00616@greenbush.bellcore.com>\") (\"audio\" \"basic\" NIL NIL \"Hi Mark\" \"base64\" 561308 NIL NIL NIL NIL) 7605 NIL NIL NIL NIL)"
        "(\"audio\" \"basic\" NIL NIL \"Flint phone\" \"base64\" 36234 NIL NIL NIL NIL)(\"image\" \"pbm\" NIL NIL \"MTR's photo\" \"base64\" 1814 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Star Trek Party\" \"7bit\" 182880 (\"Thu, 19 Sep 91 12:41:43 EDT\" NIL "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((NIL NIL \"abel\" \"thumper.bellcore.com\")(NIL NIL \"bianchi\" \"thumper.bellcore.com\")(NIL NIL \"braun\" \"thumper.bellcore.com\")(NIL NIL \"cameron\" \"thumper.bellcore.com\")"
        "(NIL NIL \"carmen\" \"thumper.bellcore.com\")(NIL NIL \"jfp\" \"thumper.bellcore.com\")(NIL NIL \"jxr\" \"thumper.bellcore.com\")(NIL NIL \"kraut\" \"thumper.bellcore.com\")"
        "(NIL NIL \"lamb\" \"thumper.bellcore.com\")(NIL NIL \"lowery\" \"thumper.bellcore.com\")(NIL NIL \"lynn\" \"thumper.bellcore.com\")(NIL NIL \"mlittman\" \"thumper.bellcore.com\")"
        "(NIL NIL \"nancyg\" \"thumper.bellcore.com\")(NIL NIL \"sau\" \"thumper.bellcore.com\")(NIL NIL \"shoshi\" \"thumper.bellcore.com\")(NIL NIL \"slr\" \"thumper.bellcore.com\")"
        "(NIL NIL \"stornett\" \"flash.bellcore.com\")(NIL NIL \"tkl\" \"thumper.bellcore.com\")) ((NIL NIL \"nsb\" \"thumper.bellcore.com\")(NIL NIL \"trina\" \"flash.bellcore.com\")) "
        "NIL NIL \"<9109191641.AA12840@greenbush.bellcore.com>\") (((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 731 16 NIL NIL NIL NIL)"
        "(\"audio\" \"x-sun\" NIL NIL \"He's dead, Jim\" \"base64\" 31472 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"Where_No_One_Has_Gone_Before\") NIL NIL NIL)"
        "((\"image\" \"gif\" NIL NIL \"Kirk/Spock/McCoy\" \"base64\" 26000 NIL NIL NIL NIL)(\"image\" \"gif\" NIL NIL \"Star Trek Next Generation\" \"base64\" 18666 NIL NIL NIL NIL)"
        "(\"APPLICATION\" \"X-BE2\" (\"version\" \"12\") NIL NIL \"7bit\" 46125 NIL NIL NIL NIL)"
        "(\"application\" \"atomicmail\" (\"version\" \"1.12\") NIL NIL \"7bit\" 9203 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"Where_No_Man_Has_Gone_Before\") NIL NIL NIL)"
        "(\"audio\" \"x-sun\" NIL NIL \"Distress calls\" \"base64\" 47822 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"Outermost_Trek\") NIL NIL NIL) 4565 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Digitizer test\" \"7bit\" 86113 (\"Fri, 24 May 91 10:40:25 EDT\" \"A cheap digitizer test\" "
        "((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) "
        "((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((NIL NIL \"nsb\" \"sleepy.bellcore.com\")) ((NIL NIL \"sau\" \"sleepy.bellcore.com\")) "
        "NIL NIL \"<9105241440.AA08935@sleepy.bellcore.com>\") ((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 21 0 NIL NIL NIL NIL)"
        "(\"image\" \"pgm\" NIL NIL \"Bellcore mug\" \"base64\" 84174 NIL NIL NIL NIL)(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 267 8 NIL NIL NIL NIL) \"MIXED\" "
        "(\"boundary\" \"mail.sleepy.sau.144.8891\") NIL NIL NIL) 483 NIL NIL NIL NIL)(\"message\" \"rfc822\" NIL NIL \"More Imagery\" \"7bit\" 74470 "
        "(\"Fri, 7 Jun 91 09:09:05 EDT\" \"meta-mail\" ((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) "
        "((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((NIL NIL \"nsb\" \"sleepy.bellcore.com\")) NIL NIL NIL \"<9106071309.AA00574@sleepy.bellcore.com>\") "
        "((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 1246 26 NIL NIL NIL NIL)(\"image\" \"pbm\" NIL NIL \"Mail architecture slide\" \"base64\" 71686 NIL NIL NIL NIL) "
        "\"MIXED\" (\"boundary\" \"mail.sleepy.sau.158.532\") NIL NIL NIL) 431 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"PostScript demo\" \"7bit\" 398008 (\"Mon, 7 Oct 91 12:13:55 EDT\" \"An image that went gif->ppm->ps\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((NIL NIL \"mrc\" \"cac.washington.edu\")) NIL NIL NIL \"<9110071613.AA10867@greenbush.bellcore.com>\") "
        "(\"application\" \"postscript\" NIL NIL \"Captain Picard\" \"7bit\" 397154 NIL NIL NIL NIL) 6438 NIL NIL NIL NIL)"
        "(\"image\" \"gif\" NIL NIL \"Quoted-Printable test\" \"quoted-printable\" 78302 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"q-p vs. base64 test\" \"7bit\" 103685 (\"Sat, 26 Oct 91 09:35:10 EDT\" \"Audio in q-p\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((NIL NIL \"mrc\" \"akbar.cac.washington.edu\")) "
        "NIL NIL NIL \"<9110261335.AA01130@greenbush.bellcore.com>\") ((\"AUDIO\" \"BASIC\" NIL NIL \"I'm sorry, Dave (q-p)\" \"QUOTED-PRINTABLE\" 62094 NIL NIL NIL NIL)"
        "(\"AUDIO\" \"BASIC\" NIL NIL \"I'm sorry, Dave (BASE64)\" \"BASE64\" 40634 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"hal_9000\") NIL NIL NIL) 1382 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Multiple encapsulation\" \"7bit\" 314835 (\"Thu, 24 Oct 1991 17:32:56 -0700 (PDT)\" \"Here's some more\" "
        "((\"Mark Crispin\" NIL \"MRC\" \"CAC.Washington.EDU\")) ((\"Mark Crispin\" NIL \"mrc\" \"Tomobiki-Cho.CAC.Washington.EDU\")) "
        "((\"Mark Crispin\" NIL \"MRC\" \"CAC.Washington.EDU\")) ((\"Mark Crispin\" NIL \"MRC\" \"CAC.Washington.EDU\")) "
        "NIL NIL NIL \"<MailManager.688350776.11603.mrc@Tomobiki-Cho.CAC.Washington.EDU>\") "
        "((\"APPLICATION\" \"POSTSCRIPT\" NIL NIL \"The Simpsons!!\" \"BASE64\" 53346 NIL NIL NIL NIL)"
        "(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL \"Alice's PDP-10 w/ TECO & DDT\" \"BASE64\" 18530 299 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Going deeper\" \"7bit\" 241934 (\"Thu, 24 Oct 1991 17:08:20 -0700 (PDT)\" \"A Multipart message\" "
        "((\"Nathaniel S. Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel S. Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel S. Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((NIL NIL \"nsb\" \"thumper.bellcore.com\")) NIL NIL NIL NIL) "
        "((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 319 7 NIL NIL NIL NIL)((\"image\" \"gif\" NIL NIL \"Bunny\" \"base64\" 3276 NIL NIL NIL NIL)"
        "(\"audio\" \"basic\" NIL NIL \"TV Theme songs\" \"base64\" 156706 NIL NIL NIL NIL) \"parallel\" (\"boundary\" \"seconddivider\") NIL NIL NIL)"
        "(\"application\" \"atomicmail\" NIL NIL NIL \"7bit\" 4924 NIL NIL NIL NIL)(\"message\" \"rfc822\" NIL NIL \"Yet another level deeper...\" \"7bit\" 75920 "
        "(\"Thu, 24 Oct 1991 17:09:10 -0700 (PDT)\" \"Monster!\" ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) NIL NIL NIL NIL NIL) "
        "(\"AUDIO\" \"X-SUN\" NIL NIL \"I'm Twying...\" \"base64\" 75682 NIL NIL NIL NIL) 1031 NIL NIL NIL NIL) \"mixed\" (\"boundary\" \"foobarbazola\") "
        "NIL NIL NIL) 2094 NIL NIL NIL NIL) \"MIXED\" (\"BOUNDARY\" \"16819560-2078917053-688350843:#11603\") NIL NIL NIL) 3282 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"owatagusiam\") NIL NIL NIL");
const QByteArray bsSignedInsideMessageInsideMessage("\"message\" \"rfc822\" NIL NIL NIL \"7bit\" 1511 (\"Thu, 8 Aug 2013 09:02:50 +0200\" \"Re: Your GSoC status\" "
        "((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) ((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) ((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) "
        "((\"Jan\" NIL \"jkt\" \"flaska.net\")) NIL NIL NIL \"<201308080902.51071@pali>\") "
        "((\"Text\" \"Plain\" (\"charset\" \"utf-8\") NIL NIL \"quoted-printable\" 632 20)"
        "(\"application\" \"pgp-signature\" (\"name\" \"signature.asc\") NIL \"This is a digitally signed message part.\" \"7bit\" 205) \"signed\") 51");
const QByteArray bsManyPlaintexts("(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
        "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
        "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
        "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
        "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
        " \"mixed\"");
const QByteArray bsEvernote("((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 125 0 NIL NIL NIL NIL)"
        "(\"text\" \"html\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 284 11 NIL NIL NIL NIL) "
        "\"alternative\" (\"boundary\" \"----------6C67657367910D\") NIL NIL NIL)"
        "(\"application\" \"octet-stream\" (\"name\" \"CAN0000009221(1)\") "
        "\"<008101cf443f$421f4e10$02bfc0bf@99H45HF>\" NIL \"base64\" 288 NIL NIL NIL NIL) "
        "\"mixed\" (\"boundary\" \"----------6B22D2B9A32361\") NIL NIL NIL");
const QByteArray bsMultipartRelated("((\"text\" \"plain\" (\"charset\" \"iso-8859-1\") NIL NIL \"quoted-printable\" 827 47 NIL NIL NIL NIL)"
        "(\"text\" \"html\" (\"charset\" \"iso-8859-1\") NIL NIL \"quoted-printable\" 8406 184 NIL NIL NIL NIL) "
        "\"alternative\" (\"boundary\" \"----=_NextPart_001_04C4_01CEFBEC.1304AC10\") NIL NIL NIL)"
        "(\"image\" \"jpeg\" (\"name\" \"image001.jpg\") \"<image001.jpg@01CEFBE3.062418E0>\" NIL \"base64\" 39990 NIL NIL NIL NIL)"
        "(\"image\" \"jpeg\" (\"name\" \"image002.jpg\") \"<image002.jpg@01CEFBE5.40406E50>\" NIL \"7bit\" 63866 NIL NIL NIL NIL) "
        "\"related\" (\"boundary\" \"----=_NextPart_000_04C3_01CEFBEC.1304AC10\") NIL (\"de\") NIL");
const QByteArray bsPlaintextWithFilenameAsName("\"text\" \"plain\" (\"namE\" \"pwn.txt\") NIL NIL NIL 19 2 NIL NIL NIL NIL");
const QByteArray bsPlaintextWithFilenameAsFilename("\"text\" \"plain\" NIL NIL NIL NIL 19 2 NIL (\"inline\" (\"filenaMe\" \"pwn.txt\")) NIL NIL NIL");
const QByteArray bsPlaintextWithFilenameAsBoth("\"text\" \"plain\" (\"name\" \"canary\") NIL NIL NIL 19 2 NIL (\"this is not recognized\" (\"filename\" \"pwn.txt\")) NIL NIL NIL");
const QByteArray bsPlaintextEmptyFilename("\"text\" \"plain\" (\"name\" \"actual\") NIL NIL NIL 19 2 NIL (\"attachment\" (\"filename\" \"\")) NIL NIL NIL");

const QByteArray bsEncrypted = QByteArrayLiteral(
                "(\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 12 NIL NIL NIL NIL)"
                "(\"application\" \"octet-stream\" (\"name\" \"encrypted.asc\") NIL \"OpenPGP encrypted message\" \"7bit\" "
                "4127 NIL (\"inline\" (\"filename\" \"encrypted.asc\")) NIL NIL) \"encrypted\" "
                "(\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL");

#endif /*TEST_IMAP_DATA*/
