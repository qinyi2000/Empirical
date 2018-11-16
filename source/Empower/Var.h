/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2018
 *
 *  @file  Var.h
 *  @brief A collection of information about a single, instantiated variable in Empower
 */

#ifndef EMP_EMPOWER_VAR_H
#define EMP_EMPOWER_VAR_H

#include "../base/assert.h"
#include "../base/Ptr.h"

#include "MemoryImage.h"
#include "Type.h"

namespace emp {

  class Var {
  private:
    emp::Ptr<Type> type_ptr;        ///< What type is this variable?
    emp::Ptr<MemoryImage> mem_ptr;  ///< Which memory image is variable using (by default)
    size_t mem_pos;                 ///< Where is this variable in a memory image?

  public:
    Var(Type & _type, MemoryImage & _mem, size_t _pos)
      : type_ptr(&_type), mem_ptr(&_mem), mem_pos(_pos) { ; }
    Var(const Var &) = default;

    template <typename T>
    T & Restore() {
      // Make sure function is restoring the correct type.
      emp_assert( type_ptr->GetID() == GetTypeID<T>(), "Trying to restore Var to incorrect type." );

      // Convert this memory to a reference that can be returned.
      return mem_ptr->GetRef<T>(mem_pos);
    }
  };


}

#endif