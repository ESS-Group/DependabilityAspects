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

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include "ObjectSize.h"

namespace CoolChecksum {


// CONDITIONAL FUNCTIONS
template<typename T, int hasCheckFct=__hasChecksumFunctions<T>::RET>
struct __ConditionalCall { // only used if <T> __hasChecksumFunctions (sliced)
  __attribute__((always_inline)) static inline bool __check(T *c) {
    return T::__chksum_t::__check(c);
  }
  __attribute__((always_inline)) static inline void __generate(T *c) {
    T::__chksum_t::__generate(c);
  }
  __attribute__((always_inline)) static inline void __generate_mutable(T *c) {
    if(T::MEMBERS_MUTABLE != 0) {
      T::__chksum_t::__generate(c);
    }
  }
  __attribute__((always_inline)) static inline void __generate_non_mutable(T *c) {
    if(T::MEMBERS_MUTABLE == 0) {
      T::__chksum_t::__generate(c);
    }
  }
  __attribute__((always_inline)) static inline void __dirty(T *c) {
    c->__chksum.__dirty();
  }
  __attribute__((always_inline)) static inline void __static_dirty() {
    T::__static_chksum.__dirty();
  }
  __attribute__((always_inline)) static inline void __static_check() { T::__static_check(); }
  __attribute__((always_inline)) static inline void __static_check_get() { T::__static_check_get(); }
  __attribute__((always_inline)) static inline void __static_generate() { T::__static_generate(); }
  __attribute__((always_inline)) static inline void __lock(T *c) {
    c->__locker.__lock();
  }
  __attribute__((always_inline)) static inline void __unlock(T *c) {
    c->__locker.__unlock();
  }
  __attribute__((always_inline)) static inline bool __is_locked(T *c) {
    return c->__locker.__is_locked();
  }
  __attribute__((always_inline)) static inline bool __is_unlocked(T *c) {
    return c->__locker.__is_unlocked();
  }
  __attribute__((always_inline)) static inline void __static_lock() { T::__static_lock(); }
  __attribute__((always_inline)) static inline void __static_construction_lock() {
    T::__static_lock();
    __ConditionalCall<T, T::STATIC_CHECKSUM_SIZE>::__static_dirty();
  }
  __attribute__((always_inline)) static inline void __static_init_and_lock() { T::__static_init_and_lock(); }
};
template<typename T>
struct __ConditionalCall<T,0> { // used if <T> has no ChecksumFunctions
  __attribute__((always_inline)) static inline bool __check(T *c) { return true; }
  __attribute__((always_inline)) static inline void __generate(T *c) {}
  __attribute__((always_inline)) static inline void __generate_mutable(T *c) {}
  __attribute__((always_inline)) static inline void __generate_non_mutable(T *c) {}
  __attribute__((always_inline)) static inline void __dirty(T *c) {}
  __attribute__((always_inline)) static inline void __static_dirty() {}
  __attribute__((always_inline)) static inline void __static_check() {}
  __attribute__((always_inline)) static inline void __static_check_get() {}
  __attribute__((always_inline)) static inline void __static_generate() {}
  __attribute__((always_inline)) static inline void __lock(T *c) {}
  __attribute__((always_inline)) static inline void __unlock(T *c) {}
  __attribute__((always_inline)) static inline bool __is_locked(T *c) { return true; }
  __attribute__((always_inline)) static inline bool __is_unlocked(T *c) { return false; }
  __attribute__((always_inline)) static inline void __static_lock() {}
  __attribute__((always_inline)) static inline void __static_construction_lock() {}
  __attribute__((always_inline)) static inline void __static_init_and_lock() {}
};


// ACTION TYPES
template<typename TypeInfo, typename>
struct Check {
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj, bool* proceed) {
    if(*proceed) {
      *proceed = __ConditionalCall<typename TypeInfo::That>::__check(obj);
    }
  }
};

template<typename TypeInfo, typename>
struct Generate {
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj) {
    __ConditionalCall<typename TypeInfo::That>::__generate(obj);
  }
};

template<typename TypeInfo, typename>
struct GenerateMutable {
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj) {
    __ConditionalCall<typename TypeInfo::That>::__generate_mutable(obj);
  }
};

template<typename TypeInfo, typename>
struct GenerateNonMutable {
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj) {
    __ConditionalCall<typename TypeInfo::That>::__generate_non_mutable(obj);
  }
};

template<typename TypeInfo, typename>
struct Dirty {
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj) {
    __ConditionalCall<typename TypeInfo::That>::__dirty(obj);
  }
};


// Base class locking iteration. Stop when the first existing locker is found (per-object)
// SFINAE check, whether a class T has a ready-to-use locker
template<typename T, int hasLockFct=__hasLockerFunctions<T>::RET>
struct __hasRealLocker {
  static const bool RESULT = (T::BASE_CLASS_LOCKER == 0);
};
template<typename T>
struct __hasRealLocker<T, 0> {
  static const bool RESULT = false;
};

// static context (compile-time calculation), used for the action type "Lock", "Unlock", and "IsLocked"
template<typename TypeInfo, typename LAST>
struct LockCalculator {
  static const bool LOCKER_FOUND =
    (LAST::LOCKER_FOUND==true) || // we already traversed a class whose locker was used ... terminate here
    (__hasRealLocker<typename TypeInfo::That>::RESULT); // the current class has a ready-to-use locker

  // is this the first locker that we found during base-class traversal? if so, USE_THIS_LOCKER is true.
  static const bool USE_THIS_LOCKER = (LOCKER_FOUND==true) && (LAST::LOCKER_FOUND==false);
};

template<typename TypeInfo, typename LAST>
struct Lock {
  typedef LockCalculator<TypeInfo, LAST> EXEC; // static context (compile-time calculation)
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj) {
    __ConditionalCall<typename TypeInfo::That, (EXEC::USE_THIS_LOCKER ? 1 : 0)>::__lock(obj);
  }
};
template<typename TypeInfo>
struct Lock<TypeInfo, void> {
  struct EXEC { // initial EXEC
    static const bool LOCKER_FOUND = false;
  };
};

template<typename TypeInfo, typename LAST>
struct Unlock {
  typedef LockCalculator<TypeInfo, LAST> EXEC; // static context (compile-time calculation)
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj) {
    __ConditionalCall<typename TypeInfo::That, (EXEC::USE_THIS_LOCKER ? 1 : 0)>::__unlock(obj);
  }
};
template<typename TypeInfo>
struct Unlock<TypeInfo, void> {
  struct EXEC { // initial EXEC
    static const bool LOCKER_FOUND = false;
  };
};

template<typename TypeInfo, typename LAST>
struct IsLocked {
  typedef LockCalculator<TypeInfo, LAST> EXEC; // static context (compile-time calculation)
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj, bool* result) {
    if(EXEC::USE_THIS_LOCKER) {
      *result =
        __ConditionalCall<typename TypeInfo::That, (EXEC::USE_THIS_LOCKER ? 1 : 0)>::__is_locked(obj);
    }
  }
};
template<typename TypeInfo>
struct IsLocked<TypeInfo, void> {
  struct EXEC { // initial EXEC
    static const bool LOCKER_FOUND = false;
  };
};

template<typename TypeInfo, typename LAST>
struct IsUnLocked {
  typedef LockCalculator<TypeInfo, LAST> EXEC; // static context (compile-time calculation)
  __attribute__((always_inline)) static void exec(typename TypeInfo::That* obj, bool* result) {
    if(EXEC::USE_THIS_LOCKER) {
      *result =
        __ConditionalCall<typename TypeInfo::That, (EXEC::USE_THIS_LOCKER ? 1 : 0)>::__is_unlocked(obj);
    }
  }
};
template<typename TypeInfo>
struct IsUnLocked<TypeInfo, void> {
  struct EXEC { // initial EXEC
    static const bool LOCKER_FOUND = false;
  };
};


// action types concerning the static checksum

template<typename TypeInfo, typename>
struct StaticCheck {
  __attribute__((always_inline)) static void exec() {
    __ConditionalCall<typename TypeInfo::That>::__static_check();
  }
};

template<typename TypeInfo, typename>
struct StaticGenerate {
  __attribute__((always_inline)) static void exec() {
    __ConditionalCall<typename TypeInfo::That>::__static_generate();
  }
};

template<typename TypeInfo, typename>
struct StaticLock {
  __attribute__((always_inline)) static void exec() {
    __ConditionalCall<typename TypeInfo::That>::__static_lock();
  }
};

// this type is only used on object construction,
// to lock all *real* base class checksums,
// but only check the actual one (MOST_DERIVED),
// which therefore does not get locked explicitly.
template<typename TypeInfo, typename LAST>
struct StaticConstructionLock {
  struct EXEC {
    // do not invoke the locker on MOST_DERIVED
    typedef typename LAST::MOST_DERIVED MOST_DERIVED;
  };
  __attribute__((always_inline)) static void exec() {
    __ConditionalCall<typename TypeInfo::That,
                     ((bool)__hasLockerFunctions<typename TypeInfo::That>::RET) &&
                     (TypeTest<typename TypeInfo::That, typename EXEC::MOST_DERIVED>::EQUAL == 0)>::__static_construction_lock();
  }
};
template<typename T>
struct StaticConstructionLockInit {
  typedef T MOST_DERIVED;
};

} //CoolChecksum

#endif /* __ACTIONS_H__ */

