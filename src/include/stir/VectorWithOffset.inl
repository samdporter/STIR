//
//
/*
    Copyright (C) 2000 PARAPET partners
    Copyright (C) 2000 - 2010-07-01, Hammersmith Imanet Ltd
    Copyright (C) 2012-06-01 - 2012, Kris Thielemans
    Copyright (C) 2023 - 2024, University College London
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0 AND License-ref-PARAPET-license

    See STIR/LICENSE.txt for details
*/
/*!
  \file
  \ingroup Array
  \brief inline implementations of stir::VectorWithOffset

  \author Kris Thielemans
  \author PARAPET project


*/

#include "stir/IndexRange.h"
#include <algorithm>
#include <stdexcept>
#include "thresholding.h"
#include "stir/error.h"

START_NAMESPACE_STIR

template <class T>
void
VectorWithOffset<T>::init()
{
  length = 0;    // i.e. an empty row of zero length,
  start = 0;     // no offsets
  num = nullptr; // and no data.
  begin_allocated_memory = nullptr;
  end_allocated_memory = nullptr;
  allocated_memory_sptr = nullptr;
}

template <class T>
void
VectorWithOffset<T>::init(const int min_index, const int max_index, T* const data_ptr, bool copy_data)
{
  this->pointer_access = false;
  if (copy_data)
    {
      this->resize(min_index, max_index);
      std::copy(data_ptr, data_ptr + this->length, this->begin());
    }
  else
    {
      this->length = static_cast<unsigned>(max_index - min_index) + 1;
      this->start = min_index;
      this->begin_allocated_memory = data_ptr;
      this->end_allocated_memory = data_ptr + this->length;
      this->num = this->begin_allocated_memory - this->start;
      this->check_state();
    }
}

template <class T>
bool
VectorWithOffset<T>::owns_memory_for_data() const
{
  return this->allocated_memory_sptr ? true : false;
}

/*!
This function (only non-empty when debugging)
is used before and after any modification of the object
*/
template <class T>
void
VectorWithOffset<T>::check_state() const
{
  // disable for normal debugging
#if _DEBUG > 1
  assert(((length > 0) || (length == 0 && start == 0 && num == begin_allocated_memory)));

#endif
  assert(begin_allocated_memory <= num + start);
  assert(end_allocated_memory >= begin_allocated_memory);
  assert(static_cast<unsigned>(end_allocated_memory - begin_allocated_memory) >= length);
  assert(!allocated_memory_sptr || (allocated_memory_sptr.get() == begin_allocated_memory));
}

template <class T>
void
VectorWithOffset<T>::_destruct_and_deallocate()
{
  // check if data is being accessed via a pointer (see get_data_ptr())
  assert(pointer_access == false);
  // TODO when reserve() no longer initialises new elements,
  // we'll have to be careful to delete only initialised elements
  // and just de-allocate the rest

  this->allocated_memory_sptr = nullptr;
}

template <class T>
void
VectorWithOffset<T>::recycle()
{
  this->check_state();
  this->_destruct_and_deallocate();
  this->init();
}

template <class T>
int
VectorWithOffset<T>::get_min_index() const
{
  return start;
}

template <class T>
int
VectorWithOffset<T>::get_max_index() const
{
  return start + length - 1;
}

/*! Out of range errors are detected using assert() */
template <class T>
T&
VectorWithOffset<T>::operator[](int i)
{
  this->check_state();
  assert(i >= this->get_min_index());
  assert(i <= this->get_max_index());

  return num[i];
}

/*! Out of range errors are detected using assert() */
template <class T>
const T&
VectorWithOffset<T>::operator[](int i) const
{
  this->check_state();
  assert(i >= this->get_min_index());
  assert(i <= this->get_max_index());

  return num[i];
}

template <class T>
T&
VectorWithOffset<T>::at(int i)
{
  if (length == 0 || i < this->get_min_index() || i > this->get_max_index())
    throw std::out_of_range("index out of range");
  this->check_state();
  return num[i];
}

template <class T>
const T&
VectorWithOffset<T>::at(int i) const
{
  if (length == 0 || i < this->get_min_index() || i > this->get_max_index())
    throw std::out_of_range("index out of range");
  this->check_state();

  return num[i];
}

template <class T>
bool
VectorWithOffset<T>::empty() const
{
  return length == 0;
}

template <class T>
typename VectorWithOffset<T>::iterator
VectorWithOffset<T>::begin()
{
  this->check_state();
  return typename VectorWithOffset<T>::iterator(num + this->get_min_index());
}

template <class T>
typename VectorWithOffset<T>::const_iterator
VectorWithOffset<T>::begin() const
{
  this->check_state();
  return typename VectorWithOffset<T>::const_iterator(num + this->get_min_index());
}

template <class T>
typename VectorWithOffset<T>::iterator
VectorWithOffset<T>::end()
{
  this->check_state();
  return typename VectorWithOffset<T>::iterator(num + (this->get_max_index() + 1));
}

template <class T>
typename VectorWithOffset<T>::const_iterator
VectorWithOffset<T>::end() const
{
  this->check_state();
  return typename VectorWithOffset<T>::const_iterator(num + (this->get_max_index() + 1));
}

template <class T>
typename VectorWithOffset<T>::reverse_iterator
VectorWithOffset<T>::rbegin()
{
  this->check_state();
  return std::make_reverse_iterator(end());
}

template <class T>
typename VectorWithOffset<T>::const_reverse_iterator
VectorWithOffset<T>::rbegin() const
{
  this->check_state();
  return std::make_reverse_iterator(end());
}

template <class T>
typename VectorWithOffset<T>::reverse_iterator
VectorWithOffset<T>::rend()
{
  this->check_state();
  return std::make_reverse_iterator(begin());
}

template <class T>
typename VectorWithOffset<T>::const_reverse_iterator
VectorWithOffset<T>::rend() const
{
  this->check_state();
  return std::make_reverse_iterator(begin());
}

template <class T>
VectorWithOffset<T>::VectorWithOffset()
    : pointer_access(false)
{
  this->init();
}

template <class T>
VectorWithOffset<T>::VectorWithOffset(const int hsz)
    : VectorWithOffset(0, hsz - 1)
{}

template <class T>
VectorWithOffset<T>::VectorWithOffset(const int min_index, const int max_index)
    : length(static_cast<unsigned>(max_index - min_index) + 1),
      start(min_index),
      pointer_access(false)
{
  if (max_index >= min_index)
    {
      allocated_memory_sptr = shared_ptr<T[]>(new T[length]);
      begin_allocated_memory = allocated_memory_sptr.get();
      end_allocated_memory = begin_allocated_memory + length;
      num = begin_allocated_memory - min_index;
    }
  else
    this->init();
  this->check_state();
}

#if STIR_VERSION < 070000
template <class T>
VectorWithOffset<T>::VectorWithOffset(const int min_index, const int max_index, T* const data_ptr, T* const end_of_data_ptr)
    : length(static_cast<unsigned>(max_index - min_index) + 1),
      start(min_index),
      allocated_memory_sptr(nullptr), // we don't own the data
      pointer_access(false)
{
  this->begin_allocated_memory = data_ptr;
  this->end_allocated_memory = end_of_data_ptr;
  this->num = this->begin_allocated_memory - this->start;
  this->check_state();
}

template <class T>
VectorWithOffset<T>::VectorWithOffset(const int sz, T* const data_ptr, T* const end_of_data_ptr)
    : VectorWithOffset(0, sz - 1, data_ptr, end_of_data_ptr)
{}
#endif // STIR_VERSION < 070000

template <class T>
VectorWithOffset<T>::VectorWithOffset(const int min_index, const int max_index, const T* const data_ptr)
{
  // first set empty, such that resize() will work ok
  this->init();
  // note: need a const_cast, but it's safe because we will copy the data
  this->init(min_index, max_index, const_cast<T*>(data_ptr), /* copy_data = */ true);
}

template <class T>
VectorWithOffset<T>::VectorWithOffset(const int sz, const T* const data_ptr)
    : VectorWithOffset(0, sz - 1, data_ptr)
{}

template <class T>
VectorWithOffset<T>::VectorWithOffset(const int min_index, const int max_index, shared_ptr<T[]> data_sptr)
{
  this->allocated_memory_sptr = data_sptr;
  this->init(min_index, max_index, data_sptr.get(), /* copy_data = */ false);
}

template <class T>
VectorWithOffset<T>::VectorWithOffset(VectorWithOffset<T>&& other) noexcept
    : VectorWithOffset()
{
  swap(*this, other);
}

template <class T>
VectorWithOffset<T>::~VectorWithOffset()
{
  // check if data is being accessed via a pointer (see get_data_ptr())
  assert(pointer_access == false);
  _destruct_and_deallocate();
}

template <class T>
void
VectorWithOffset<T>::set_offset(const int min_index)
{
  this->check_state();
  //  only do something when non-zero length
  if (length == 0)
    return;
  num += start - min_index;
  start = min_index;
}

template <class T>
void
VectorWithOffset<T>::set_min_index(const int min_index)
{
  this->set_offset(min_index);
}

template <class T>
size_t
VectorWithOffset<T>::capacity() const
{
  return size_t(end_allocated_memory - begin_allocated_memory);
}

template <class T>
int
VectorWithOffset<T>::get_capacity_min_index() const
{
  // the behaviour for length==0 depends on num==begin_allocated_memory
  assert(length > 0 || num == begin_allocated_memory);
  return static_cast<int>(begin_allocated_memory - num);
}

template <class T>
int
VectorWithOffset<T>::get_capacity_max_index() const
{
  // the behaviour for length==0 depends on num==begin_allocated_memory
  assert(length > 0 || num == begin_allocated_memory);
  return static_cast<int>(end_allocated_memory - num - 1);
}

// the new members will be initialised with the default constructor for T
//  but this should change in the future
template <class T>
void
VectorWithOffset<T>::reserve(const int new_capacity_min_index, const int new_capacity_max_index)
{
  this->check_state();
  const int actual_capacity_min_index
      = length == 0 ? new_capacity_min_index : std::min(this->get_capacity_min_index(), new_capacity_min_index);
  const int actual_capacity_max_index
      = length == 0 ? new_capacity_max_index : std::max(this->get_capacity_max_index(), new_capacity_max_index);
  if (actual_capacity_min_index > actual_capacity_max_index)
    return;

  const unsigned int new_capacity = static_cast<unsigned>(actual_capacity_max_index - actual_capacity_min_index) + 1;
  if (new_capacity <= this->capacity())
    return;

  // check if data is being accessed via a pointer (see get_data_ptr())
  assert(pointer_access == false);
  // TODO use allocator here instead of new
  shared_ptr<T[]> new_allocated_memory_sptr(new T[new_capacity]);
  const unsigned extra_at_the_left = length == 0 ? 0U : std::max(0, this->get_min_index() - actual_capacity_min_index);
  std::copy(this->begin(), this->end(), new_allocated_memory_sptr.get() + extra_at_the_left);
  this->_destruct_and_deallocate();
  allocated_memory_sptr = std::move(new_allocated_memory_sptr);
  begin_allocated_memory = allocated_memory_sptr.get();
  end_allocated_memory = begin_allocated_memory + new_capacity;
  num = begin_allocated_memory + extra_at_the_left - (length > 0 ? start : 0);
  this->check_state();
}

template <class T>
void
VectorWithOffset<T>::reserve(const unsigned int new_size)
{
  // note: for 0 new_size, we avoid a wrap-around
  // otherwise we would be reserving quite a lot of memory!
  if (new_size != 0)
    reserve(0, static_cast<int>(new_size - 1));
}

// the new members will be initialised with the default constructor for T
template <class T>
void
VectorWithOffset<T>::resize(const int min_index, const int max_index)
{
  this->check_state();
  if (min_index > max_index)
    {
      length = 0;
      start = 0;
      num = begin_allocated_memory;
      return;
    }
  const unsigned old_length = length;
  if (old_length > 0)
    {
      if (min_index == this->get_min_index() && max_index == this->get_max_index())
        return;
      // determine overlapping range to avoid copying too much data when calling reserve()
      const int overlap_min_index = std::max(this->get_min_index(), min_index);
      const int overlap_max_index = std::min(this->get_max_index(), max_index);
      // TODO when using non-initialised memory, call delete here on elements that go out of range
      length = overlap_max_index - overlap_min_index < 0 ? 0 : static_cast<unsigned>(overlap_max_index - overlap_min_index) + 1;
      if (length == 0)
        {
          start = 0;
          num = begin_allocated_memory;
        }
      else
        {
          // do not change num as num[0] should remain the same
          start = overlap_min_index;
        }
    } // end if (length>0)
  const unsigned overlapping_length = length;
  this->reserve(min_index, max_index);
  // TODO when using allocator, call default constructor for new elements here
  // (and delete the ones that go out of range!)
  length = static_cast<unsigned>(max_index - min_index) + 1;
  start = min_index;
  if (overlapping_length > 0)
    {
      // do not change num as num[0] should remain the same
    }
  else
    {
      // we have reallocated the whole array, so set num correctly
      num = begin_allocated_memory - min_index;
    }
  this->check_state();
}

template <class T>
void
VectorWithOffset<T>::resize(const unsigned new_size)
{
  if (new_size == 0)
    {
      length = 0;
      start = 0;
      num = begin_allocated_memory;
    }
  else
    this->resize(0, static_cast<int>(new_size - 1));
}

template <class T>
void
VectorWithOffset<T>::grow(const int min_index, const int max_index)
{
  this->resize(min_index, max_index);
}

template <class T>
void
VectorWithOffset<T>::grow(const unsigned new_size)
{
  this->grow(0, static_cast<int>(new_size - 1));
}

template <class T>
VectorWithOffset<T>&
VectorWithOffset<T>::operator=(const VectorWithOffset& il)
{
  this->check_state();
  if (this == &il)
    return *this; // in case of x=x
  {
    if (this->capacity() < il.size())
      {
        // first truncate current and then reserve space
        length = 0;
        start = 0;
        num = begin_allocated_memory;
        this->reserve(il.get_min_index(), il.get_max_index());
      }
    length = il.length;
    this->set_offset(il.get_min_index());
    std::copy(il.begin(), il.end(), this->begin());
  }

  this->check_state();
  return *this;
}

template <class T>
VectorWithOffset<T>::VectorWithOffset(const VectorWithOffset& il)
    : pointer_access(false)
{
  this->init();
  *this = il; // Uses assignment operator (above)
}

template <class T>
int
VectorWithOffset<T>::get_length() const
{
  this->check_state();
  return static_cast<int>(length);
}

template <class T>
size_t
VectorWithOffset<T>::size() const
{
  this->check_state();
  return size_t(length);
}

template <class T>
bool
VectorWithOffset<T>::operator==(const VectorWithOffset& iv) const
{
  this->check_state();
  if (length != iv.length || start != iv.start)
    return false;
  return std::equal(this->begin(), this->end(), iv.begin());
}

template <class T>
bool
VectorWithOffset<T>::operator!=(const VectorWithOffset& iv) const
{
  return !(*this == iv);
}

template <class T>
void
VectorWithOffset<T>::fill(const T& n)
{
  this->check_state();
  std::fill(this->begin(), this->end(), n);
  this->check_state();
}

template <class T>
inline void
VectorWithOffset<T>::apply_lower_threshold(const T& lower)
{
  this->check_state();
  threshold_lower(this->begin(), this->end(), lower);
  this->check_state();
}

template <class T>
inline void
VectorWithOffset<T>::apply_upper_threshold(const T& upper)
{
  this->check_state();
  threshold_upper(this->begin(), this->end(), upper);
  this->check_state();
}

/*!
  This returns a \c T* to the first element of a,
  members are guaranteed to be stored contiguously in memory.

  Use only in emergency cases...

  To prevent invalidating the safety checks (and making
  reimplementation more difficult), NO manipulation with
  the vector is allowed between the pairs
      get_data_ptr() and release_data_ptr()
  and
      get_const_data_ptr() and release_data_ptr().
  (This is checked with assert() in DEBUG mode.)
*/
template <class T>
T*
VectorWithOffset<T>::get_data_ptr()
{
  assert(!pointer_access);

  pointer_access = true;
  return (num + start);

  // if implementation changes, this would need to keep track
  // if which pointer it returns.
};

/*!
  This returns a \c const \c T* to the first element of a,
  members are guaranteed to be stored contiguously in memory.

  Use get_const_data_ptr() when you are not going to modify
  the data.

  \see get_data_ptr()
*/
template <class T>
const T*
VectorWithOffset<T>::get_const_data_ptr() const
{
  assert(!pointer_access);

  pointer_access = true;
  return (num + start);

  // if implementation changes, this would need to keep track
  // if which pointer it returns.
};

/*!
  This has to be used when access to the T* returned by get_data_ptr() is
  finished. It updates
  the vector with any changes you made, and allows access to
  the other member functions again.

  \see get_data_ptr()
*/
template <class T>
void
VectorWithOffset<T>::release_data_ptr()
{
  assert(pointer_access);

  pointer_access = false;
}

/*!
  This has to be used when access to the const T* returned by get_const_data_ptr() is
  finished. It allows access to
  the other member functions again.

  \see get_const_data_ptr()
*/

template <class T>
void
VectorWithOffset<T>::release_const_data_ptr() const
{
  assert(pointer_access);

  pointer_access = false;
}

/********************** arithmetic operators ****************/
template <class T>
inline VectorWithOffset<T>&
VectorWithOffset<T>::operator+=(const VectorWithOffset& v)
{
  this->check_state();
#if 1
  if (this->get_min_index() != v.get_min_index() && this->get_max_index() != v.get_max_index())
    error("VectorWithOffset::+= with non-matching range");
#else
  // first check if *this is empty
  if (this->get_length() == 0)
    {
      return *this = v;
    }
  this->grow(std::min(get_min_index(), v.get_min_index()), std::max(get_max_index(), v.get_max_index()));
#endif
  for (int i = v.get_min_index(); i <= v.get_max_index(); i++)
    num[i] += v.num[i];
  this->check_state();
  return *this;
}

template <class T>
inline VectorWithOffset<T>&
VectorWithOffset<T>::operator-=(const VectorWithOffset& v)
{
  this->check_state();
#if 1
  if (this->get_min_index() != v.get_min_index() && this->get_max_index() != v.get_max_index())
    error("VectorWithOffset::-= with non-matching range");
#else
  // first check if *this is empty
  if (get_length() == 0)
    {
      *this = v;
      return *this *= -1;
    }
  grow(std::min(get_min_index(), v.get_min_index()), std::max(get_max_index(), v.get_max_index()));
#endif
  for (int i = v.get_min_index(); i <= v.get_max_index(); i++)
    num[i] -= v.num[i];
  this->check_state();
  return *this;
}

template <class T>
inline VectorWithOffset<T>&
VectorWithOffset<T>::operator*=(const VectorWithOffset& v)
{
  this->check_state();
#if 1
  if (this->get_min_index() != v.get_min_index() && this->get_max_index() != v.get_max_index())
    error("VectorWithOffset::*= with non-matching range");
#else
  // first check if *this is empty
  if (get_length() == 0)
    {
      // we have to return an object of the same dimensions as v, but filled with 0.
      *this = v;
      return *this *= 0;
    }
  grow(std::min(get_min_index(), v.get_min_index()), std::max(get_max_index(), v.get_max_index()));
#endif
  for (int i = v.get_min_index(); i <= v.get_max_index(); i++)
    num[i] *= v.num[i];
  this->check_state();
  return *this;
}

template <class T>
inline VectorWithOffset<T>&
VectorWithOffset<T>::operator/=(const VectorWithOffset& v)
{
  this->check_state();
#if 1
  if (this->get_min_index() != v.get_min_index() && this->get_max_index() != v.get_max_index())
    error("VectorWithOffset::/= with non-matching range");
#else
  // first check if *this is empty
  if (get_length() == 0)
    {
      // we have to return an object of the same dimensions as v, but filled with 0.
      *this = v;
      return *this *= 0;
    }
  grow(std::min(get_min_index(), v.get_min_index()), std::max(get_max_index(), v.get_max_index()));
#endif
  for (int i = v.get_min_index(); i <= v.get_max_index(); i++)
    num[i] /= v.num[i];
  this->check_state();
  return *this;
}

/**** operator+=(T) etc *****/
#if 0
// disabled for now
// warning: not tested
template <class T>
inline VectorWithOffset<T>& 
VectorWithOffset<T>::operator+= (const T &t)
{
  typename iterator iter = this->begin();
  const typename iterator end_iter = this->end();
  while (iter != end_iter)
    *iter++ += t;
}

template <class T>
inline VectorWithOffset<T>& 
VectorWithOffset<T>::operator-= (const T &t)
{
  typename iterator iter = this->begin();
  const typename iterator end_iter = this->end();
  while (iter != end_iter)
    *iter++ -= t;
}
template <class T>
inline VectorWithOffset<T>& 
VectorWithOffset<T>::operator*= (const T &t)
{
  typename iterator iter = this->begin();
  const typename iterator end_iter = this->end();
  while (iter != end_iter)
    *iter++ *= t;
}
template <class T>
inline VectorWithOffset<T>& 
VectorWithOffset<T>::operator/= (const T &t)
{
  typename iterator iter = this->begin();
  const typename iterator end_iter = this->end();
  while (iter != end_iter)
    *iter++ /= t;
}

#endif

/**** operator* etc ********/

// addition
template <class T>
inline VectorWithOffset<T>
VectorWithOffset<T>::operator+(const VectorWithOffset& v) const
{
  this->check_state();
  VectorWithOffset retval(*this);
  return retval += v;
}

// subtraction
template <class T>
inline VectorWithOffset<T>
VectorWithOffset<T>::operator-(const VectorWithOffset& v) const
{
  this->check_state();
  VectorWithOffset retval(*this);
  return retval -= v;
}

// elem by elem multiplication
template <class T>
inline VectorWithOffset<T>
VectorWithOffset<T>::operator*(const VectorWithOffset& v) const
{
  this->check_state();
  VectorWithOffset retval(*this);
  return retval *= v;
}

// elem by elem division
template <class T>
inline VectorWithOffset<T>
VectorWithOffset<T>::operator/(const VectorWithOffset& v) const
{
  this->check_state();
  VectorWithOffset retval(*this);
  return retval /= v;
}

END_NAMESPACE_STIR
