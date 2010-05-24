/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_IMAPTASK_H
#define IMAP_IMAPTASK_H

#include <QObject>
#include "../Parser/Parser.h"

namespace Imap {

class Parser;

namespace Mailbox {

class Model;

class ImapTask : public QObject
{
Q_OBJECT
public:
    ImapTask( Model* _model, Imap::Parser* _parser, const CommandHandle& _parentTask );
    virtual void perform() = 0;
    virtual ~ImapTask() {};

    bool dependsOn( const CommandHandle& what ) const;

signals:
    void failed();
    void completed();

public slots:
    virtual void abort() = 0;
    virtual void upstreamFailed() = 0;

protected:
    Model* model;
    Parser* parser;
    CommandHandle parentTask;

};

/*class FetchMessagePart: public ImapTask
{
Q_OBJECT
public:
    FetchMessagePart( Model* _model, Imap::Parser* _parser, const CommandHandle& _parentTask );
    virtual void perform();
private:
};*/


}
}

#endif // IMAP_IMAPTASK_H
