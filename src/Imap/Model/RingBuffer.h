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
#ifndef TROJITA_RUNGBUFFER_H
#define TROJITA_RINGBUFFER_H

#include <QVector>

namespace Imap {

/** @short Circular buffer holding a number of items

This class holds a fixed number of items. Use the append() function to put stuff into it. When you want to retrieve them,
obtain an iterator by calling begin(). The returned iterator will cease to be valid immediately after modifying the ring
buffer. Iteration has to be performed by comparing the current value of an iterator for equivalence against the
container's end(), and incrementing the iterator. Any other form is not supported.

Invalidated iterators will likely not get caught .
*/
template<typename T>
class RingBuffer
{
public:
    class const_iterator
    {
        const RingBuffer<T> *container_;
        int myPos_;
    public:
        /** @short Dereference the iterator */
        const T& operator()() const
        {
            Q_ASSERT(myPos_ >= 0 && myPos_ < container_->size());
            return container_->buf_[myPos_];
        }

        /** @short Increment the iterator, wrapping around past end if neccessary */
        void operator++()
        {
            if (myPos_ == container_->buf_.size() - 1)
                myPos_ = 0;
            else
                ++myPos_;
        }

        /** @short Compare two iterators from the same container for equality */
        bool operator==(const const_iterator& other)
        {
            Q_ASSERT(container_ == other.container_);
            return myPos_ == other.myPos_;
        }

        /** @short Compare two iterators from the same container for inqeuality */
        bool operator!=(const const_iterator& other)
        {
            return !(*this == other);
        }
    private:
        friend class RingBuffer<T>;
        const_iterator(const RingBuffer<T>* container, int myPos): container_(container), myPos_(myPos)
        {
        }
    };

    /** @short Instantiate a ring buffer holding size elements */
    RingBuffer(const int size): buf_(size), appendPos_(0), wrapped_(false)
    {
        Q_ASSERT(size >= 1);
    }

    /** @short Return an interator pointing to the oldest item in the container */
    const_iterator begin() const
    {
        if (!wrapped_) {
            return const_iterator(this, 0);
        } else {
            return const_iterator(this, appendPos_ == 0 ? 1 : appendPos_ + 1);
        }
    }

    /** @short Return an interator pointing to one item past the recent addition */
    const_iterator end() const
    {
        return const_iterator(this, appendPos_);
    }

    /** @short Append an item to the container. Oldest item could get overwritten. */
    void append(const T &what)
    {
        if (appendPos_ == buf_.size()) {
            wrapped_ = true;
            appendPos_ = 0;
        }
        buf_[appendPos_] = what;
        ++appendPos_;
    }

    /** @short Remove all items from the container */
    void clear()
    {
        buf_ = QVector<T>(buf_.size());
        wrapped_ = false;
    }

private:
    QVector<T> buf_;
    int appendPos_;
    bool wrapped_;
};

}

#endif // TROJITA_RINGBUFFER_H
