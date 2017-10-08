/* 
 * This file is part of the library of dependability aspects.
 * See: http://dx.doi.org/10.17877/DE290R-17995
 * Copyright (c) 2017 Christoph Borchert.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CHECKSUMMING_BASE_H__
#define __CHECKSUMMING_BASE_H__

#include "MemoryBarriers.h"


namespace CoolChecksum {

// Base class for all "Checksumming_*" classes that provides flags for
// soft (non-blocking) synchronization

template<typename TypeInfo, bool STATIC, unsigned SYNCHRONIZED=
  (TypeInfo::That::SYNCHRONIZED == 1) ? 1 : ((STATIC == true) ? TypeInfo::That::INHERITANCE : 0)> // 1 or 0
  // classes with inheritance relations get always a static locker for efficiency (see LockAdviceInvoker.ah)
class ChecksummingBase {
private:
  // (flags) for soft synchronization
  mutable void* dirty; // the thread's id while __generating a new checksum
  unsigned int version; // a value identifying the checksum/replica instance

  // all functions are guarded by compiler-only memory barriers: barrier()
  // this prevents the compiler from speculating those functions around,
  // thus, enforcing strictly in-order execution.
  // moreover, the compiler cannot cache values in registers and
  // will always reload the accessed variables.
protected:
  __attribute__((always_inline)) inline void reset_dirty() {
    __sync_bool_compare_and_swap(&dirty, __builtin_frame_address(0), 0); // compare to my thread's id
    // the __sync routines provide a memory barrier (implicitly)
  }

  __attribute__((always_inline)) inline unsigned int get_version() const {
    barrier();
    unsigned int reg_version = version;
    barrier();
    return reg_version;
  }

  __attribute__((always_inline)) inline void inc_version() {
    barrier();
    version++;
    barrier();
  }

public:
  // used in LockAdviceInvoker
  __attribute__((always_inline)) inline void __dirty() const {
    barrier();
    dirty = __builtin_frame_address(0); // set to: my thread's id
    barrier();
  }

  __attribute__((always_inline)) inline const void* const get_dirty() const {
    barrier();
    const void* const reg_dirty = dirty;
    barrier();
    return reg_dirty;
  }
};


template<typename TypeInfo, bool STATIC>
class ChecksummingBase<TypeInfo, STATIC, 0> { // Empty class, providing no flags at all
protected:
  __attribute__((always_inline)) inline void reset_dirty() const {}

  __attribute__((always_inline)) inline const unsigned int get_version() const { return 0; }
  __attribute__((always_inline)) inline void inc_version() const {}
public:
  __attribute__((always_inline)) inline void __dirty() const {}
  __attribute__((always_inline)) inline const void* const get_dirty() const { return 0; }
};


//-------------------------------------------------------------------------------------------------------------------


// obtain Checksumming sub-object for a given object pointer/type
template<typename T, bool STATIC>
struct Get {
  __attribute__((always_inline)) inline static typename T::__chksum_t& self(T* obj) { return obj->__chksum; }
};
template<typename T>
struct Get<T, true> {
  __attribute__((always_inline)) inline static typename T::__static_chksum_t& self(T* obj) { return T::__static_chksum; }
};

// explicit join point, signaling that an error was corrected
__attribute__((always_inline)) inline void errorCorrected() {}

} //CoolChecksum

#endif /* __CHECKSUMMING_BASE_H__ */
