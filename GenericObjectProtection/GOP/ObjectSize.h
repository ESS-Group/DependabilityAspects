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

#ifndef __OBJECTSIZE_H__
#define __OBJECTSIZE_H__

#include "GOP_GlobalConfig.h"
#include "TypeTraits.h"
#include "JPTL.h"

namespace CoolChecksum {

// compare two pointers at compile time
// if this comparison cannot be made at compile time, assume pointers differ
// Note: the __attribute__((always_inline)) is needed to clone this function for each particular instance
__attribute__((always_inline)) inline const bool EqualPointers(const void* const a, const void* const b){
  return a == b;
}

// compare two types
template<typename T, typename U>
struct TypeTest { enum { EQUAL=0 }; };
template<typename T>
struct TypeTest<T,T> { enum { EQUAL=1 }; };

// static check, whether <T> has an attribute '__hasChecksumFunctions' (SFINAE)
template<typename T> int ttest(...); // overload resolution matches always
template<typename T> char ttest(typename T::__hasChecksumFunctions const volatile *); // preferred by overload resolution
template<typename T> struct __hasChecksumFunctions {
  enum { RET=((sizeof(ttest<T>(0))==1)?1:0) };
};

// static check, whether <T> has an attribute '__hasLocker' (SFINAE)
template<typename T> int lockTest(...); // overload resolution matches always
template<typename T> char lockTest(typename T::__hasLocker const volatile *); // preferred by overload resolution
template<typename T> struct __hasLockerFunctions {
  enum { RET=((sizeof(lockTest<T>(0))==1)?1:0) };
};

//----------------------------------------

template<typename T, bool ABORT>
struct UnsizedArrayAssertion {
  enum { ABORT_ = sizeof(T) }; // do not remove unless AspectC++ is fixed to handle unsized arrays correctly!
  // AspectC++ weaves slices after the last member of a target class,
  // which is incorrect when the last member is an unsized array.
  // Thus, we cannot protect classes with unsized arrays at the moment.
  // The following sizeof evaluation will force the compiler to stop in such a case.
};
template<typename T>
struct UnsizedArrayAssertion<T, true> {};

template<typename MemberInfo, bool STATIC>
struct MemberDetails {
  enum { IS_PUBLIC  = (MemberInfo::prot == AC::PROT_PUBLIC),
         IS_STATIC  = ((MemberInfo::spec & AC::SPEC_STATIC) != 0),
         IS_MUTABLE = ((MemberInfo::spec & AC::SPEC_MUTABLE) != 0),
         IS_CONST   = (is_const<typename MemberInfo::Type>::IS_CONST == 1),
         IS_STATIC_CONST = ((IS_STATIC == true) && (IS_CONST == true)),
         IS_PLAIN   = (FoldArray<typename MemberInfo::Type>::isClass == 0),
         IS_UNSIZED_ARRAY = (__isUnsizedArray<typename MemberInfo::Type>::value == true),
//         IS_POINTER = (TypeTraits::pointerType<typename MemberInfo::Type>::result != TypeTraits::NO_POINTER_TYPE),
         IS_CHECKSUMMED = (
#if GOP_USE_GET_SET_ADVICE
#else
                            (IS_PUBLIC==false) &&
#endif
                            (IS_PLAIN==true)
                         && (IS_UNSIZED_ARRAY==false)
                         && (IS_STATIC_CONST==false)
                         && (IS_STATIC == STATIC)
//                         && (IS_POINTER==false)
                          ) };

  // compile-time assertion to detect objects with non-static unsized arrays (which are not checksummable!)
  UnsizedArrayAssertion<typename MemberInfo::Type, !((IS_UNSIZED_ARRAY==true) && (IS_STATIC==false))> unsized_assert;
};

//----------------------------------------

template<typename MemberInfo, bool STATIC, bool CHECKSUMMED=(bool)MemberDetails<MemberInfo, STATIC>::IS_CHECKSUMMED>
struct SizeOfChecksummed {
  enum { SIZE = sizeof(typename MemberInfo::Type) };
};
template<typename T, bool STATIC>
struct SizeOfChecksummed<T, STATIC, false> {
  // do not try to evaluate sizeof non-checksummed types, cause they might be incomplete
  enum { SIZE = 0 };
};

template<int OFFSET, int MEMBER_SIZE>
struct Alignment {
  /*
     Notes for alignment: http://www.catb.org/esr/structure-packing/
     ===============================================================
     Storage for the basic C datatypes on an x86 or ARM processor
     doesn't normally start at arbitrary byte addresses in memory.
     Rather, each type except char has an alignment requirement;
     chars can start on any byte address, but 2-byte shorts must
     start on an even address, 4-byte ints or floats must start on
     an address divisible by 4, and 8-byte longs or doubles must
     start on an address divisible by 8.
     The jargon for this is that basic C types on x86 and ARM are
     self-aligned.
     Pointers, whether 32-bit (4-byte) or 64-bit (8-byte) are
     self-aligned too. Pointer alignment is the strictest possible.
  */
  enum {
    POINTER_ALIGNMENT = sizeof(void*), // determine maximal alignment requirement
    ALIGNMENT_TYPE = ( (MEMBER_SIZE > POINTER_ALIGNMENT) ? POINTER_ALIGNMENT : MEMBER_SIZE ),
    PADDING = (ALIGNMENT_TYPE - (OFFSET % ALIGNMENT_TYPE)) % ALIGNMENT_TYPE // self alignment
  };
};
template<int OFFSET>
struct Alignment<OFFSET, 0> {
  enum { PADDING = 0 }; // no alignment
};

template<typename MemberInfo, typename LAST>
struct SizeOfNonPublic {
  struct EXEC {
    enum { STATIC = LAST::STATIC,
           OFFSET = LAST::SIZE,  // current position in the data-member stream
           MEMBER_SIZE = SizeOfChecksummed<MemberInfo, STATIC>::SIZE, // current member's size
           PADDING = Alignment<OFFSET, MEMBER_SIZE>::PADDING,
           SIZE = OFFSET + PADDING + MEMBER_SIZE };
  };
};

template<bool tSTATIC>
struct SizeOfNonPublicInit {
  // initial EXEC
  enum { SIZE = 0, STATIC = tSTATIC };
};

//----------------------------------------

template<typename MemberInfo, typename LAST>
struct MemberCount {
  struct EXEC {
    enum { MUTABLE = LAST::MUTABLE + (((MemberInfo::spec & AC::SPEC_MUTABLE) != 0) ? 1 : 0) };
  };
};
template<typename MemberInfo>
struct MemberCount<MemberInfo, void> {
  // initial EXEC
  struct EXEC {
    enum { MUTABLE = 0 };
  };
};

//----------------------------------------

template<typename TypeInfo, typename LAST>
struct ClassCount {
  struct EXEC {
    enum { WITH_STATIC_MEMBERS = LAST::WITH_STATIC_MEMBERS +
            ((JPTL::MemberIterator<TypeInfo, SizeOfNonPublic, SizeOfNonPublicInit<true> >::EXEC::SIZE) ? 1 : 0) };
  };
};
template<typename TypeInfo>
struct ClassCount<TypeInfo, void> {
  // initial EXEC
  struct EXEC {
    enum { WITH_STATIC_MEMBERS = 0 };
  };
};

//----------------------------------------

// Determine whether a type has a non-static unsized-array member (UNSIZED_ARRAY_COUNT) and
// the index of the last non-static unsized-array member (UNSIZED_ARRAY_INDEX)
template<typename MemberInfo, typename LAST>
struct UnsizedArrayInfo {
  static const bool IS_UNSIZED_ARRAY = (MemberDetails<MemberInfo, false>::IS_UNSIZED_ARRAY == true);
  struct EXEC {
    enum { UNSIZED_ARRAY_COUNT = LAST::UNSIZED_ARRAY_COUNT + (IS_UNSIZED_ARRAY ? 1 : 0), // how many unsigned arrays does this type have?
           UNSIZED_ARRAY_INDEX = IS_UNSIZED_ARRAY ? LAST::I : LAST::UNSIZED_ARRAY_INDEX, // index of the last unsigned array
           I = LAST::I + 1 };
  };
};
template<typename MemberInfo>
struct UnsizedArrayInfo<MemberInfo, void> {
  // initial EXEC
  struct EXEC {
    enum { UNSIZED_ARRAY_COUNT = 0,
           UNSIZED_ARRAY_INDEX = 0,
                             I = 0 };
  };
};

//----------------------------------------

template<typename T, unsigned int SIZE, unsigned int UNROLL_FACTOR, unsigned int I=UNROLL_FACTOR>
struct UnrollFactor {
  typedef UnrollFactor<T, SIZE, UNROLL_FACTOR+1, I-1> Next; // recursion
  static const unsigned int REMAINDER = SIZE % (UNROLL_FACTOR * sizeof(T));
  static const unsigned int MIN_REMAINDER = SIZE % (Next::BEST_FIT * sizeof(T));
  static const unsigned int BEST_FIT = (REMAINDER < MIN_REMAINDER) ? UNROLL_FACTOR : Next::BEST_FIT;
};

template<typename T, unsigned int SIZE, unsigned int UNROLL_FACTOR>
struct UnrollFactor<T, SIZE, UNROLL_FACTOR, 1> {
  static const unsigned int BEST_FIT = UNROLL_FACTOR;
};

} //CoolChecksum
  
#endif /* __OBJECTSIZE_H__ */

