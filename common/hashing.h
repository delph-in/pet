/* -*- Mode: C++ -*- */
/* \file hashing.h
 * Check if hash maps are available, set \c HASH_SPACE to the namespace
 * containing hash maps, or provide a them using maps.
 *
 * If hashing data structures are not available, \c HASH_SPACE is not defined
 */
#ifndef _HASHING_H
#define _HASHING_H

#include "pet-config.h"

#ifdef HAVE_HASH_MAP
#define HASH_SPACE std
#include <hash_map>
#include <hash_set>
#endif

#ifdef HAVE_EXT_HASH_MAP
#include <ext/hash_map>
#include <ext/hash_set>

// hopefully a gnu compiler with
#if ((__GNUC__ == 3) && ( __GNUC_MINOR__ > 1)) || (__GNUC__ > 3)
//g++ version 3.2.x or later
#define HASH_SPACE __gnu_cxx
using namespace HASH_SPACE;
#else
// g++ version 3.0.x or 3.1.x, not recommended
#define HASH_SPACE std
#endif
#endif 

#if ! defined(HAVE_HASH_MAP) && ! defined(HAVE_EXT_HASH_MAP)
// no hash_map available
#undef HASH_SPACE
#include <map>
#include <set>
#define hash_map map
#define hash_set set
#endif

/* function object: hash function for pointers */
struct pointer_hash {
  inline size_t operator() (void* p) const {
    return reinterpret_cast<size_t>(p);
  }
};

/* function object: simple hash function for std::string */
struct simple_string_hash {
  inline size_t operator()(const std::string &key) const {
    size_t v = 0;
    for(unsigned int i = 0; i < key.length(); i++)
      v += key[i];
    return v;
  }
};


#endif
