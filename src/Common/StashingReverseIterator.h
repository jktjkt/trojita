/* Copyright (C) 2015 J. C. Moyer, http://stackoverflow.com/a/33851951/2245623

   Licensed under CC-BY-SA 3.0 as per stackoverflow.com's footer. This is GPLv3 compatible.
*/

#pragma once

#include <iterator>

namespace Common {

template <class Iterator>
class stashing_reverse_iterator :
  public std::iterator<
    typename std::iterator_traits<Iterator>::iterator_category,
    typename std::iterator_traits<Iterator>::value_type,
    typename std::iterator_traits<Iterator>::difference_type,
    typename std::iterator_traits<Iterator>::pointer,
    typename std::iterator_traits<Iterator>::reference
  > {
  typedef std::iterator_traits<Iterator> traits_type;
public:
  typedef Iterator iterator_type;
  typedef typename traits_type::difference_type difference_type;
  typedef typename traits_type::reference       reference;
  typedef typename traits_type::pointer         pointer;

  stashing_reverse_iterator() : current() {}

  explicit stashing_reverse_iterator(Iterator x) : current(x) {}

  template <class U>
  stashing_reverse_iterator(const stashing_reverse_iterator<U>& u) : current(u.current) {}

  template <class U>
  stashing_reverse_iterator& operator=(const stashing_reverse_iterator<U>& u) {
    current = u.base();
    return *this;
  }

  Iterator base() const {
    return current;
  }

  // Differs from reverse_iterator::operator*:
  // 1. const qualifier removed
  // 2. current iterator is stored in a member field to ensure references are
  //    always valid after this function returns
  reference operator*() {
    deref_tmp = current;
    --deref_tmp;
    return *deref_tmp;
  }

  pointer operator->() {
    return std::addressof(operator*());
  }

  stashing_reverse_iterator& operator++() {
    --current;
    return *this;
  }

 stashing_reverse_iterator operator++(int) {
    stashing_reverse_iterator tmp = *this;
    --current;
    return tmp;
  }

  stashing_reverse_iterator& operator--() {
    ++current;
    return *this;
  }

  stashing_reverse_iterator operator--(int) {
    stashing_reverse_iterator tmp = *this;
    ++current;
    return tmp;
  }

  stashing_reverse_iterator  operator+ (difference_type n) const {
    return stashing_reverse_iterator(current - n);
  }

  stashing_reverse_iterator& operator+=(difference_type n) {
    current -= n;
    return *this;
  }

  stashing_reverse_iterator  operator- (difference_type n) const {
    return stashing_reverse_iterator(current + n);
  }

  stashing_reverse_iterator& operator-=(difference_type n) {
    current += n;
    return *this;
  }

  // Differs from reverse_iterator::operator[]:
  // 1. const qualifier removed because this function makes use of operator*
  reference operator[](difference_type n) {
    return *(*this + n);
  }

protected:
  Iterator current;
private:
  Iterator deref_tmp;
};

template <class Iterator1, class Iterator2>
bool operator==(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y)
{ return x.base() == y.base(); }

template <class Iterator1, class Iterator2>
bool operator<(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y)
{ return x.base() > y.base(); }

template <class Iterator1, class Iterator2>
bool operator!=(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y)
{ return !(x.base() == y.base()); }

template <class Iterator1, class Iterator2>
bool operator>(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y)
{ return x.base() < y.base(); }

template <class Iterator1, class Iterator2>
bool operator>=(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y)
{ return x.base() <= y.base(); }

template <class Iterator1, class Iterator2>
bool operator<=(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y)
{ return x.base() >= y.base(); }

template <class Iterator1, class Iterator2>
auto operator-(
  const stashing_reverse_iterator<Iterator1>& x,
  const stashing_reverse_iterator<Iterator2>& y) -> decltype(y.base() - x.base())
{ return y.base() - x.base(); }

template <class Iterator>
stashing_reverse_iterator<Iterator> operator+(
  typename stashing_reverse_iterator<Iterator>::difference_type n,
  const stashing_reverse_iterator<Iterator>& x)
{ return stashing_reverse_iterator<Iterator>(x.base() - n); }

template <class Iterator>
stashing_reverse_iterator<Iterator> make_stashing_reverse_iterator(Iterator i)
{ return stashing_reverse_iterator<Iterator>(i); }

}
