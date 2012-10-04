/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

/** @short A naive helper suitable for achieving roughly the same effect as Qt4's QAbstractItemModel::reset() in both Qt4 and Qt5 */
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  define RESET_MODEL_2(MODEL) \
{ \
    MODEL->beginResetModel(); \
    MODEL->endResetModel();\
}
#else
#  define RESET_MODEL_2(MODEL) \
{ \
    MODEL->reset(); \
}
#endif

#define RESET_MODEL RESET_MODEL_2(this)
