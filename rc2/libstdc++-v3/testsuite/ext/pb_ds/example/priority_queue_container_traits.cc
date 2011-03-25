// -*- C++ -*-

// Copyright (C) 2005, 2006 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the terms
// of the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later
// version.

// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
// MA 02111-1307, USA.

// As a special exception, you may use this file as part of a free
// software library without restriction.  Specifically, if other files
// instantiate templates or use macros or inline functions from this
// file, or you compile this file and link it with other files to
// produce an executable, this file does not by itself cause the
// resulting executable to be covered by the GNU General Public
// License.  This exception does not however invalidate any other
// reasons why the executable file might be covered by the GNU General
// Public License.

// Copyright (C) 2004 Ami Tavory and Vladimir Dreizin, IBM-HRL.

// Permission to use, copy, modify, sell, and distribute this software
// is hereby granted without fee, provided that the above copyright
// notice appears in all copies, and that both that copyright notice
// and this permission notice appear in supporting documentation. None
// of the above authors, nor IBM Haifa Research Laboratories, make any
// representation about the suitability of this software for any
// purpose. It is provided "as is" without express or implied
// warranty.

/**
 * @file priority_queue_container_traits_example.cpp
 * A basic example showing how to use container_traits for querying container types
 *    for their behavior.
 */

/**
 * The following example shows how to use container_traits in order to print
 * out information on an priority queue's behavior, e.g., its underlying
 * data structure.
 */

#include <iostream>
#include <ext/pb_ds/priority_queue.hpp>
#include <ext/pb_ds/tag_and_trait.hpp>

using namespace std;
using namespace pb_ds;
using namespace pb_ds;

template<class DS_Category>
void
print_container_category(DS_Category);

template<>
void
print_container_category(binary_heap_tag)
{ cout << "Binary heap:" << endl; }

template<>
void
print_container_category(binomial_heap_tag)
{ cout << "Binomial heap:" << endl; }

template<>
void
print_container_category(rc_binomial_heap_tag)
{ cout << "Redundant-counter binomial heap:" << endl; }

template<>
void
print_container_category(pairing_heap_tag)
{ cout << "Pairing heap:" << endl; }

template<>
void
print_container_category(thin_heap_tag)
{ cout << "Thin heap:" << endl; }

template<class Invalidation_Guarantee>
void
print_invalidation_guarantee(Invalidation_Guarantee);

template<>
void
print_invalidation_guarantee(basic_invalidation_guarantee)
{
  cout << "Guarantees only that found references, pointers, and "
    "iterators are valid as long as the container object is not "
    "modified" << endl;
}

template<>
void
print_invalidation_guarantee(point_invalidation_guarantee)
{
  cout << "Guarantees that found references, pointers, and "
    "point_iterators are valid even if the container object "
    "is modified" << endl;
}

template<>
void
print_invalidation_guarantee(range_invalidation_guarantee)
{
  cout << "Guarantees that iterators remain valid even if the "
    "container object is modified" << endl;
}

void
print_split_join_can_throw(bool can)
{
  if (can)
    {
      cout << "Can throw exceptions if split or joined" << endl;
      return;
    }
  cout << "Cannot throw exceptions if split or joined" << endl;
}

template<class DS_Traits>
void
print_container_attributes()
{
  // First print out the data structure category.
  print_container_category(typename DS_Traits::container_category());

  // Now print the attributes of the container.
  print_invalidation_guarantee(typename DS_Traits::invalidation_guarantee());
  print_split_join_can_throw(DS_Traits::split_join_can_throw);
  cout << endl << endl;
}

int main()
{
  {
    // Print the attributes of a binary heap.
    typedef
      pb_ds::priority_queue<
      int,
      std::less<int>,
      binary_heap_tag>
      t;

    print_container_attributes<container_traits<t> >();
  }

  {
    // Print the attributes of a binomial heap.
    typedef
      pb_ds::priority_queue<
      int,
      std::less<int>,
      binomial_heap_tag>
      t;

    print_container_attributes<container_traits<t> >();
  }

  {
    // Print the attributes of a redundant-counter binomial heap.
    typedef
      pb_ds::priority_queue<
      int,
      std::less<int>,
      rc_binomial_heap_tag>
      t;

    print_container_attributes<container_traits<t> >();
  }

  {
    // Print the attributes of a pairing heap.
    typedef
      pb_ds::priority_queue<
      int,
      std::less<int>,
      pairing_heap_tag>
      t;

    print_container_attributes<container_traits<t> >();
  }

  {
    /**
     *  Print the attributes of a thin heap.
     */

    typedef
      pb_ds::priority_queue<
      int,
      std::less<int>,
      thin_heap_tag>
      t;

    print_container_attributes<container_traits<t> >();
  }

  return 0;
}
