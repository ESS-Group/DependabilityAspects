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

#ifndef __CHECKSUM_LOCKER_H__
#define __CHECKSUM_LOCKER_H__

namespace CoolChecksum {

//add -march=i486 to gcc flags for __sync_fetch_and_add builtins

//TODO: Rename this class to "Counter"

// explicit join point for uncorrectable-error handling
__attribute__((always_inline)) inline void synchronizerLockError() {}

template<bool NOT_EMPTY>
class ChksumLocker {
  private:
  mutable unsigned int lock; // ANB-Encoded (A = 127, B = 5) //TODO: find optimal values
  enum { A_CONSTANT = 127,
         B_CONSTANT = 5 };

/*
  //FIXME: for error repair: uint32!!!
  enum { A_LOW      = 127U,
         A_HIGH     = (A_LOW << 16),
         A_CONSTANT = (A_HIGH | A_LOW),
         B_LOW      = 5U,
         B_HIGH     = (B_LOW << 16),
         B_CONSTANT = (B_HIGH | B_LOW) };

  void __repair() const {
    // check lower half:
    if( ((static_cast<unsigned short>(this->lock)) % A_LOW) == B_LOW ) {
      // lower half correct
      this->lock = (static_cast<unsigned short>(this->lock) << 16) | static_cast<unsigned short>(this->lock);
    }
    else if( (((this->lock) & 0xFFFF0000) % A_HIGH) == B_HIGH ) {
      // upper half correct
      this->lock = (this->lock & 0xFFFF0000) | (static_cast<unsigned short>( (this->lock) >> 16 )
    }
  }
*/

  // check the ANB code
  __attribute__((always_inline)) inline void __check() const {
    if( ((this->lock) % A_CONSTANT) != B_CONSTANT ) {
      synchronizerLockError();
    }
  }

  public:
  inline ChksumLocker() : lock(B_CONSTANT) {}
  inline ChksumLocker(const ChksumLocker&) : lock(B_CONSTANT) {} // do not copy (see below); init instead

  // do not copy on assignment (each object has its own locker, with possibly different states)
  __attribute__((always_inline)) inline ChksumLocker& operator=(const ChksumLocker&) { return *this; }

  // initialize in 'locked' state (needed for usage before static members had been constructed)
  __attribute__((always_inline)) inline void __init_and_lock() const {
    this->lock = (A_CONSTANT+B_CONSTANT);
  }

  // just increment the lock value
  __attribute__((always_inline)) inline void __lock() const {
    __sync_fetch_and_add(&(this->lock), A_CONSTANT); // must be atomic
  }

  // just decrement the lock value
  __attribute__((always_inline)) inline void __unlock() const {
    /*
    if(lock == B_CONSTANT) { // check for underflow (DEBUG only)
      while(1);
    }
    */
    __sync_fetch_and_sub(&(this->lock), A_CONSTANT); // must be atomic
  }

  // return 'false' when locked *only* by a single thread: we need to compute a new checksum before __unlock()'ing
  __attribute__((always_inline)) inline bool __is_locked() const {
    /*
    if(this->lock != (A_CONSTANT+B_CONSTANT)) {
      __check(); // clever: bit errors in the locker can only be present if its value is not A_CONSTANT+B_CONSTANT
      return true;
    }
    else { return false; }
    */
    return (this->lock != (A_CONSTANT+B_CONSTANT));
  }

  // return 'true' when the locker is in its initial state
  __attribute__((always_inline)) inline bool __is_unlocked() const {
    if(this->lock == B_CONSTANT) {
      // since B_CONSTANT is a valid value, there can't be any bit errors in the locker => no __check()'ing
      return true;
    }
    else {
      __check(); // check for (possible) bit errors in the locker
      return false;
    }
  }
};

template<>
class ChksumLocker<false> {
public:
  __attribute__((always_inline)) inline void __lock() const {}
  __attribute__((always_inline)) inline void __unlock() const {}
  __attribute__((always_inline)) inline bool __is_locked() const { return true; }
  __attribute__((always_inline)) inline bool __is_unlocked() const { return false; }
  __attribute__((always_inline)) inline void __init_and_lock() const {}
};

} //CoolChecksum

#endif /* __CHECKSUM_LOCKER_H__ */
