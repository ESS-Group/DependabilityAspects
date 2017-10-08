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

#ifndef __CHECKSUMMING_HAMMING_H__
#define __CHECKSUMMING_HAMMING_H__

#include "ChecksummingBase.h"
#include "ObjectSize.h"
#include "JPTL.h"
#include "StopPreemption.h"

#include <inttypes.h>

namespace CoolChecksum {

// Determine the maximum of instructions generated for a single member (during check)
enum { XOR_UNROLL_THRESHOLD = 32 };

#if defined(__x86_64__)
    typedef uint64_t machine_word_t;
#else
    typedef uint32_t machine_word_t;
#endif
} // CoolChecksum

//#include <cyg/infra/diag.h> // diag_printf
//#include <stdio.h>
//#include <iostream>
//using namespace std;

//-----------------------------------------------------------------------------------------------------------------
namespace HammingCodeParityMatrix {

// DIM := Anzahl Paritiy Bits ; value := Parity Matrix Spalte 'i' ; RESULT := Parity Matrix Spalte 'i+1'
template<unsigned int DIM, unsigned int value>
struct Rotate {
  static const unsigned int lsb = value & 0x1; // remember least-significant bit
  static const unsigned int v = value >> 1; // current permutation of bits (w/o lsb)

  //--------------------------------------------------------------------------------------------------
  // Compute the lexicographically next bit permutation, see:
  // https://graphics.stanford.edu/~seander/bithacks.html#NextBitPermutation

  //unsigned int t = v | (v - 1); // t gets v's least significant 0 bits set to 1
  // Next set to 1 the most significant bit to change,
  // set to 0 the least significant ones, and add the necessary 1 bits.
  //unsigned int w = (t + 1) | (((~t & -~t) - 1) >> (__builtin_ctz(v) + 1)); // next permutation of bits

  /*
   * The __builtin_ctz(v) GNU C compiler intrinsic for x86 CPUs returns the number of trailing zeros.
   * If you are using Microsoft compilers for x86, the intrinsic is _BitScanForward.
   * These both emit a bsf instruction, but equivalents may be available for other architectures.
   * If not, then consider using one of the methods for counting the consecutive zero bits mentioned earlier.
   * Here is another version that tends to be slower because of its division operator,
   * but it does not require counting the trailing zeros.
   */
  static const unsigned int t = (v | (v - 1)) + 1;
  static const unsigned int w = t | ((((t & -t) / (v & -v)) >> 1) - 1);
  //--------------------------------------------------------------------------------------------------

  static const unsigned int RESULT = (w >= (1U<<(DIM-1))) ? // overflow (next permutation greater than DIM)?
    ((((w << 2) | 0x3) << lsb) & ((1<<DIM)-1)) : // on overflow, determine next permuation manually
    ((w << 1) | lsb); // otherwise, restore old lsb
};

// runtime version
__attribute__((always_inline)) inline unsigned int rotate(const unsigned int DIM, unsigned int value) {
  unsigned int lsb = value & 0x1; // remember least-significant bit
  unsigned int v = value >> 1; // current permutation of bits (w/o lsb)

  //--------------------------------------------------------------------------------------------------
  // Compute the lexicographically next bit permutation, see:
  // https://graphics.stanford.edu/~seander/bithacks.html#NextBitPermutation

  unsigned int t = v | (v - 1); // t gets v's least significant 0 bits set to 1
  // Next set to 1 the most significant bit to change,
  // set to 0 the least significant ones, and add the necessary 1 bits.
  unsigned int w = (t + 1) | (((~t & -~t) - 1) >> (__builtin_ctz(v) + 1)); // next permutation of bits

  /*
   * The __builtin_ctz(v) GNU C compiler intrinsic for x86 CPUs returns the number of trailing zeros.
   * If you are using Microsoft compilers for x86, the intrinsic is _BitScanForward.
   * These both emit a bsf instruction, but equivalents may be available for other architectures.
   * If not, then consider using one of the methods for counting the consecutive zero bits mentioned earlier.
   * Here is another version that tends to be slower because of its division operator,
   * but it does not require counting the trailing zeros.
   */
  // unsigned int t = (v | (v - 1)) + 1;
  // unsigned int w = t | ((((t & -t) / (v & -v)) >> 1) - 1);
  //--------------------------------------------------------------------------------------------------

  unsigned int RESULT = (w >= (1U<<(DIM-1))) ? // overflow (next permutation greater than DIM)?
    ((((w << 2) | 0x3) << lsb) & ((1<<DIM)-1)) : // on overflow, determine next permuation manually
    ((w << 1) | lsb); // otherwise, restore old lsb

  return RESULT;
}

} // HammingCodeParityMatrix
//-----------------------------------------------------------------------------------------------------------------

namespace CoolChecksum {

// read and write a fixed-sized value from and to a (part of) a member's memory
template<typename T, unsigned int SIZE>
struct MemberAccess {
  typedef T Type;
  __attribute__((always_inline)) static inline T readValue(const void* member) {
    T value = 0;
    __builtin_memcpy(&value, member, SIZE); // read #SIZE bytes
    return value;
  }

  __attribute__((always_inline)) static inline void writeValue(void* member, const T value) {
    __builtin_memcpy(member, &value, SIZE); // set #SIZE bytes
  }
};
// read and write an integer-sized value from and to a (part of) a member's memory
template<typename T>
struct TypedMemberAccess {
  typedef T Type;
  __attribute__((always_inline)) static inline T readValue(const void* member) {
    return *static_cast<const T*>(member);
  }

  __attribute__((always_inline)) static inline void writeValue(void* member, const T value) {
    *static_cast<T*>(member) = value;
  }
};
// specializations for integer-sized memory accesses
template<typename T>
struct MemberAccess<T, sizeof(uint8_t)>  : public TypedMemberAccess<uint8_t> {};
template<typename T>
struct MemberAccess<T, sizeof(uint16_t)> : public TypedMemberAccess<uint16_t> {};
template<typename T>
struct MemberAccess<T, sizeof(uint32_t)> : public TypedMemberAccess<uint32_t> {};
template<typename T>
struct MemberAccess<T, sizeof(uint64_t)> : public TypedMemberAccess<uint64_t> {};


// add a word -- or smaller, determined by sizeof(T) -- to the hammingArray[]
template<unsigned int PARITY_MATRIX_COLUMN, unsigned int I=0, bool NEEDS_ADDITION=( (PARITY_MATRIX_COLUMN & 0x1) != 0 )>
struct WordAdder {
  template<typename T, typename U>
  __attribute__((always_inline)) static inline void add(T word, U* hammingArray) {
    hammingArray[I] ^= word; // xor 'addition'
    WordAdder< (PARITY_MATRIX_COLUMN >> 1), I+1 >::add(word, hammingArray); // step into recursion
  }
};
template<unsigned int PARITY_MATRIX_COLUMN, unsigned int I>
struct WordAdder<PARITY_MATRIX_COLUMN, I, false> {
  template<typename T, typename U>
  __attribute__((always_inline)) static inline void add(T word, U* hammingArray) {
    WordAdder< (PARITY_MATRIX_COLUMN >> 1), I+1 >::add(word, hammingArray); // just step into recursion
  }
};
template<unsigned int I>
struct WordAdder<0, I, false> {
  template<typename T, typename U>
  __attribute__((always_inline)) static inline void add(T word, U* hammingArray) {} // done
};


// add a complete member to the hammingArray[]
template<unsigned int DIMENSION, unsigned int PARITY_MATRIX_COLUMN, unsigned int SIZE, bool SMALL=(SIZE <= sizeof(machine_word_t))>
class MemberAdder {
private:
  // rotation for the next byte
  static const unsigned int NEXT_WORD_PARITY_MATRIX_COLUMN = HammingCodeParityMatrix::Rotate<DIMENSION, PARITY_MATRIX_COLUMN>::RESULT;
  typedef MemberAdder<DIMENSION, NEXT_WORD_PARITY_MATRIX_COLUMN, SIZE - sizeof(machine_word_t)> NextWordAdder; // use for next word

public:
  static const unsigned int NEXT_PARITY_MATRIX_COLUMN = NextWordAdder::NEXT_PARITY_MATRIX_COLUMN; // recursion until next member

  template<typename T>
  __attribute__((always_inline)) static inline void add(const void* member, T* hammingArray, T* parity) {
    MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, sizeof(machine_word_t)>::add(member, hammingArray, parity); // add full machine word
    NextWordAdder::add(static_cast<const machine_word_t*>(member)+1, hammingArray, parity); // recursion
  }

  template<typename T>
  __attribute__((always_inline)) static inline void parity(const void* member, T* parity) {
    MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, sizeof(machine_word_t)>::parity(member, parity); // process full machine word
    NextWordAdder::parity(static_cast<const machine_word_t*>(member)+1, parity); // recursion
  }

  template<typename T>
  __attribute__((always_inline)) static inline void repair(void* member, const unsigned int syndrome, const T bitpos) {
    MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, sizeof(machine_word_t)>::repair(member, syndrome, bitpos); // repair full machine word
    NextWordAdder::repair(static_cast<machine_word_t*>(member)+1, syndrome, bitpos); // recursion
  }
};

template<unsigned int DIMENSION, unsigned int PARITY_MATRIX_COLUMN, unsigned int SIZE>
class MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, SIZE, true> {
public:
  // no bytes will follow: this is the last rotation
  static const unsigned int NEXT_PARITY_MATRIX_COLUMN = HammingCodeParityMatrix::Rotate<DIMENSION, PARITY_MATRIX_COLUMN>::RESULT;

  template<typename T>
  __attribute__((always_inline)) static inline void add(const void* member, T* hammingArray, T* parity) {
    const typename MemberAccess<T, SIZE>::Type value = MemberAccess<T, SIZE>::readValue(member);
    if((PARITY_MATRIX_COLUMN & 0x1) == 0) {
      // only add to global parity if this machine-word is *not* covered by hammingArray[0],
      // which is finally added to the global parity
      *parity ^= value;
    }
    WordAdder<PARITY_MATRIX_COLUMN>::add(value, hammingArray);
  }

  template<typename T>
  __attribute__((always_inline)) static inline void parity(const void* member, T* parity) {
    *parity ^= MemberAccess<T, SIZE>::readValue(member);
  }

  template<typename T>
  __attribute__((always_inline)) static inline void repair(void* member, const unsigned int syndrome, const T bitpos) {
    if(syndrome == PARITY_MATRIX_COLUMN) {
      // error found, fix it:
      typename MemberAccess<T, SIZE>::Type value = MemberAccess<T, SIZE>::readValue(member);
      value ^= static_cast<typename MemberAccess<T, SIZE>::Type>(bitpos);
      MemberAccess<T, SIZE>::writeValue(member, value);
    }
  }
};

template<unsigned int DIMENSION, unsigned int PARITY_MATRIX_COLUMN>
class MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, 0, true> {
public:
  static const unsigned int NEXT_PARITY_MATRIX_COLUMN = PARITY_MATRIX_COLUMN; // don't rotate
  template<typename T>
  __attribute__((always_inline)) static inline void add(const void* member, T* hammingArray, T* parity) {} // done
  template<typename T>
  __attribute__((always_inline)) static inline void parity(const void* member, T* parity) {} // done
  template<typename T>
  __attribute__((always_inline)) static inline void repair(void* member, const unsigned int syndrome, const T bitpos) {} // done
};


//-----------------------------------------------------------------------------------------------------------------

// generic function that repairs a full block of data (e.g., a large array)
template<typename T>
__attribute__((noinline)) void array_repair(void* member, const unsigned int syndrome, const T bitpos,
                                                     unsigned int loops, unsigned int v, unsigned int DIM);
template<typename T>
void array_repair(void* member, const unsigned int syndrome, const T bitpos,
                          unsigned int loops, unsigned int v, unsigned int DIM) {
  // 'v' is the PARITY_MATRIX_COLUMN
  for(; loops > 0; --loops) {
    if(syndrome == v) {
      // error found, fix it:
      typename MemberAccess<T, sizeof(machine_word_t)>::Type value = MemberAccess<T, sizeof(machine_word_t)>::readValue(member);
      value ^= static_cast<typename MemberAccess<T, sizeof(machine_word_t)>::Type>(bitpos);
      MemberAccess<T, sizeof(machine_word_t)>::writeValue(member, value);
      return; // we're finished here, only one syndrome can match
    }
    member = (void*) (((char*)member) + sizeof(machine_word_t)); // next word
    v = HammingCodeParityMatrix::rotate(DIM, v); // runtime computation!
  }
}

// generic function that adds a full block of data (e.g., a large array)
template<typename T>
__attribute__((noinline)) void array_add(const void* member, T* hammingArray, T* parity,
                                         unsigned int loops, unsigned int v, unsigned int DIM);
template<typename T>
void array_add(const void* member, T* hammingArray, T* parity,
               unsigned int loops, unsigned int v, unsigned int DIM) {
  // 'v' is the PARITY_MATRIX_COLUMN
  for(; loops > 0; --loops) {
    const typename MemberAccess<T, sizeof(machine_word_t)>::Type value = MemberAccess<T, sizeof(machine_word_t)>::readValue(member);
    if((v & 0x1) == 0) {
      // only add to global parity if this machine-word is *not* covered by hammingArray[0],
      // which is finally added to the global parity
      *parity ^= value;
    }

    // runtime variant of WordAdder
    unsigned int curr = v;
    unsigned int i = 0;
    while(curr != 0) {
      if((curr & 0x1) == 0x1) { // lowest bit set
        hammingArray[i] ^= value; // xor 'addition'
      }
      curr = curr >> 1;
      ++i;
    }

    member = (void*) (((char*)member) + sizeof(machine_word_t)); // next word
    v = HammingCodeParityMatrix::rotate(DIM, v); // runtime computation!
  }
}

// select unrolled or looping repair function, depending on SIZE
template<unsigned int DIMENSION, unsigned int PARITY_MATRIX_COLUMN, unsigned int SIZE, bool UNROLL>
struct MemberAdderSelector {
  typedef MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, SIZE> SELECT; // unrolled variant
};
template<unsigned int DIMENSION, unsigned int PARITY_MATRIX_COLUMN, unsigned int SIZE>
struct MemberAdderSelector<DIMENSION, PARITY_MATRIX_COLUMN, SIZE, false> {
  struct SELECT {
    static const unsigned int LOOPS = SIZE / sizeof(machine_word_t);
    static const unsigned int REMAINDER = SIZE % sizeof(machine_word_t);
    static const unsigned int REMAINDER_PARITY_MATRIX_COLUMN = MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, (REMAINDER!=0) ? (SIZE-REMAINDER) : 0>::NEXT_PARITY_MATRIX_COLUMN;

    template<typename T>
    __attribute__((always_inline)) static inline void repair(void* member, const unsigned int syndrome, const T bitpos) {
      array_repair(member, syndrome, bitpos, LOOPS, PARITY_MATRIX_COLUMN, DIMENSION);
      // repair the remainder
      void* remainder = (void*) (((char*)member) + (LOOPS*sizeof(machine_word_t)));
      MemberAdder<DIMENSION, REMAINDER_PARITY_MATRIX_COLUMN, REMAINDER>::repair(remainder, syndrome, bitpos);
    }

    template<typename T>
    __attribute__((always_inline)) static inline void add(const void* member, T* hammingArray, T* parity) {
      array_add(member, hammingArray, parity, LOOPS, PARITY_MATRIX_COLUMN, DIMENSION);
      // add-in the remainder
      void* remainder = (void*) (((char*)member) + (LOOPS*sizeof(machine_word_t)));
      MemberAdder<DIMENSION, REMAINDER_PARITY_MATRIX_COLUMN, REMAINDER>::add(remainder, hammingArray, parity);
    }
  };
};

//-----------------------------------------------------------------------------------------------------------------

// XOR checksum
template<typename T, unsigned SIZE, bool SMALL=(SIZE <= sizeof(T)), bool UNROLL= (SIZE <= (XOR_UNROLL_THRESHOLD*sizeof(T)) ) >
struct XORSUM;

// one single word (or smaller than <T>)
template<typename T, unsigned SIZE>
struct XORSUM<T, SIZE, true, true> {
  __attribute__((always_inline)) inline static T gen(T sum, const void* data) {
    return sum ^ MemberAccess<T, SIZE>::readValue(data);
  }
};

// end of recursion
template<typename T>
struct XORSUM<T, 0, true, true> {
  __attribute__((always_inline)) inline static T gen(T sum, const void* data) {
    return sum;
  }
};

// primary recursive template (unrolled)
template<typename T, unsigned SIZE>
struct XORSUM<T, SIZE, false, true> {
  __attribute__((always_inline)) inline static T gen(T sum, const void* data) {
    return XORSUM<T, SIZE - sizeof(T)>::gen(XORSUM<T, sizeof(T), true, true>::gen(sum, data), ((T*)data) + 1 );
  }
};

// primary template generating a loop (not unrolled)
template<typename T, unsigned SIZE>
struct XORSUM<T, SIZE, false, false> {

  enum { UNROLL_FACTOR = UnrollFactor<T, SIZE, XOR_UNROLL_THRESHOLD/2>::BEST_FIT, // instructions per loop
         BYTES_PER_LOOP = UNROLL_FACTOR * sizeof(T), // checksummed bytes per loop
         LOOPS = SIZE / BYTES_PER_LOOP, // number of loops to execute
         REMAINDER = SIZE % BYTES_PER_LOOP }; // remaining bytes to add in

  __attribute__((always_inline)) inline static T gen(T sum, const void* data) {
    for(unsigned int i=0; i<LOOPS; i++) {
      sum = XORSUM<T, BYTES_PER_LOOP, false, true>::gen(sum, data);
      data = ((T*)data)+(UNROLL_FACTOR);
    }
    return XORSUM<T, REMAINDER>::gen(sum, data); // add in remainder (unrolled)
  }
};


//-----------------------------------------------------------------------------------------------------------------

template<typename MemberInfo, typename LAST>
struct HammingCodeInfo {
  struct EXEC {
    static const bool STATIC = LAST::STATIC;
    static const unsigned int DIMENSION = LAST::DIMENSION;
    static const bool UNROLL = LAST::UNROLL;
    static const unsigned int SIZE = (SizeOfChecksummed<MemberInfo, STATIC>::SIZE); // [bytes]
    static const unsigned int WORDS = LAST::WORDS + SIZE/sizeof(machine_word_t)
                                                  + ((SIZE % sizeof(machine_word_t) != 0) ? 1 : 0); // number of word-sized fields (rounded up)
    static const unsigned int PARITY_MATRIX_COLUMN = LAST::NEXT_PARITY_MATRIX_COLUMN;  // use this one for the current member
    static const unsigned int NEXT_PARITY_MATRIX_COLUMN = MemberAdder<DIMENSION, PARITY_MATRIX_COLUMN, SIZE>::NEXT_PARITY_MATRIX_COLUMN;
  };
};

template<bool tSTATIC, unsigned int tDIMENSION, bool tUNROLL=true>
struct HammingCodeInfoInit {
  // initial EXEC
  static const bool STATIC = tSTATIC;
  static const bool UNROLL = tUNROLL;
  static const unsigned int WORDS = 0;
  static const unsigned int DIMENSION = tDIMENSION;
  static const unsigned int NEXT_PARITY_MATRIX_COLUMN = 3;
};

template<typename MemberInfo, typename LAST>
struct HammingCodeGenerate {
  // compile-time calculations
  typedef typename HammingCodeInfo<MemberInfo, LAST>::EXEC EXEC;
  static const bool UNROLL = EXEC::UNROLL || (EXEC::SIZE <= (sizeof(machine_word_t)*3)); // don't unroll for arrays larger than 3 words

  // GENERATE (both the Hamming Code and the overall parity)
  template<typename T, typename U>
  __attribute__((always_inline)) static inline void exec(T obj, U* hammingArray, U* parity) {
    //MemberAdder<EXEC::DIMENSION, EXEC::PARITY_MATRIX_COLUMN, EXEC::SIZE>::add((const void*)MemberInfo::pointer(obj), hammingArray, parity);
    MemberAdderSelector<EXEC::DIMENSION, EXEC::PARITY_MATRIX_COLUMN, EXEC::SIZE, UNROLL>::SELECT::add((const void*)MemberInfo::pointer(obj), hammingArray, parity);
  }
};


template<typename MemberInfo, typename LAST>
struct HammingCodeRepair {
  // compile-time calculations
  typedef typename HammingCodeInfo<MemberInfo, LAST>::EXEC EXEC;
  static const bool UNROLL = (EXEC::SIZE <= (sizeof(machine_word_t)*7)); // don't unroll for arrays larger than 7 words

  // REPAIR (one particular bit position, located by the syndrome)
  template<typename T, typename U>
  __attribute__((always_inline)) static inline void exec(T obj, const unsigned int syndrome, const U bitpos) {
    //MemberAdder<EXEC::DIMENSION, EXEC::PARITY_MATRIX_COLUMN, EXEC::SIZE>::repair((void*)MemberInfo::pointer(obj), syndrome, bitpos);
    MemberAdderSelector<EXEC::DIMENSION, EXEC::PARITY_MATRIX_COLUMN, EXEC::SIZE, UNROLL>::SELECT::repair((void*)MemberInfo::pointer(obj), syndrome, bitpos);
  }
};


template<typename MemberInfo, typename LAST>
struct HammingCodeParity {
  // compile-time calculations
  typedef typename HammingCodeInfo<MemberInfo, LAST>::EXEC EXEC;

  // PARITY (only compute the overall parity)
  template<typename T, typename U>
  __attribute__((always_inline)) static inline void exec(T obj, U* parity) {
    //MemberAdder<EXEC::DIMENSION, EXEC::PARITY_MATRIX_COLUMN, EXEC::SIZE>::parity((const void*)MemberInfo::pointer(obj), parity);
    *parity = XORSUM<U, EXEC::SIZE>::gen(*parity, (const void*)MemberInfo::pointer(obj));
  }
};

//-----------------------------------

// Compute the amount of redundancy needed for a regular (non-extended) Hamming Code
// M := number of words (or bits) to be protected ; R := number of parity words (or bits) to allocate
template<unsigned int M, unsigned int R=2, bool SATISFIED=((M + R + 1) <= (1 << R)) >
struct RequiredRedundancy {
  // Evaluate the following formula:
  // (M + R + 1) <= 2^R         with 2^R = (1 << R)
  static const unsigned int RESULT = RequiredRedundancy<M, (R+1)>::RESULT;
};
template<unsigned int M, unsigned int R>
struct RequiredRedundancy<M, R, true> {
  static const unsigned int RESULT = R;
};

//-----------------------------------
#if 0
//TODO: somehow slower than naive loop -- may be due to inlining limits -- find out!
template<unsigned int DIM, unsigned int I=DIM>
struct ClearHammigArray {
  __attribute__((always_inline)) inline static void clear(unsigned int* hammingArray) {
    hammingArray[ (DIM-I) ] = 0; // store a zero ...
    ClearHammigArray<DIM, I-1>::clear(hammingArray); // ... in each element
  }
};
template<unsigned int DIM>
struct ClearHammigArray<DIM, 0> {
  __attribute__((always_inline)) inline static void clear(unsigned int* hammingArray) {}
};
#endif
//-----------------------------------

template<typename TypeInfo, bool STATIC=false, unsigned tSIZE=
// once again, puma is not willing to accept this ... :-(
#ifndef __puma
JPTL::MemberIterator<TypeInfo, SizeOfNonPublic, SizeOfNonPublicInit<STATIC> >::EXEC::SIZE
#else
0
#endif
>
class ChecksummingHamming : public ChecksummingBase<TypeInfo, STATIC> { // ChecksummingBase provides: get_dirty() and reset_dirty()
  public:
  static const unsigned int SIZE = tSIZE;
  typedef typename TypeInfo::That T;

  // number of word-sized fields to protect
  static const unsigned int WORDS = JPTL::MemberIterator<TypeInfo, HammingCodeInfo, HammingCodeInfoInit<STATIC, 2> >::EXEC::WORDS;
  // the above template parameter (DIMENSION) is set to 1, as we don't know the DIMENSION right here
  static const unsigned int DIMENSION = RequiredRedundancy<WORDS>::RESULT; // in WORDS
  
  private:
  enum { CHECKSUM_INIT = STATIC ? 0x1 : (TypeInfo::HASHCODE & 0xFFFF) };
  machine_word_t hammingArray[DIMENSION];
  machine_word_t parity; // extended Hamming Code for SEC-DED
  
  // helper method to obtain Checksumming sub-object for a given object pointer/type
  __attribute__((always_inline)) inline static ChecksummingHamming& self(T* obj) { return Get<T, STATIC>::self(obj); }

  static bool __repair(T* obj) __attribute__((noinline));

  public:
  // make the checksum publicly readable for other purposes
  template<typename U>
  __attribute__((always_inline)) inline static bool getChecksum(T* obj, U* checksum) {
    if(self(obj).get_dirty() == 0) {
      const unsigned int version = self(obj).get_version();
      *checksum = self(obj).parity;
      if( (self(obj).get_dirty() == 0) && (version == self(obj).get_version()) ) {
        return true; // checksum valid
      }
    }
    return false; // checksum invalid: object in use
  }

  __attribute__((always_inline)) inline static bool __check(T* obj) {
    const unsigned int version = self(obj).get_version(); // remember which checksum we're verifying
    machine_word_t parity = self(obj).parity;
    JPTL::MemberIterator<TypeInfo, HammingCodeParity, HammingCodeInfoInit<STATIC, DIMENSION> >::exec(obj, &parity);
    if(parity != CHECKSUM_INIT) {
      // checksum error ... now let's find the cause
      // test whether we had not been interrupted while verifying the checksum
      if( (self(obj).get_dirty() == 0) && (version == self(obj).get_version()) ) {
        // okay, we found bit errors ... so let's repair
        return __repair(obj);
      }
    }
    return true; // fast path: no errors found (with parity check) -> only detects an odd number of errors
  }

  __attribute__((always_inline)) inline static void __generate(T* obj) {
    // dirty has to be set to *before* this function is entered (and before the locker is unlocked), see: LockAdviceInvoker.ah
    for(unsigned int i=0; i<DIMENSION; i++) {
      self(obj).hammingArray[i] = 0;
    }
    //ClearHammigArray<DIMENSION>::clear(self(obj).hammingArray); // loop-unrolled template metaprogram (slower, somehow)
    self(obj).parity = CHECKSUM_INIT;
    JPTL::MemberIterator<TypeInfo, HammingCodeGenerate, HammingCodeInfoInit<STATIC, DIMENSION> >::exec(obj, self(obj).hammingArray, &self(obj).parity);
    // re-construct the global parity from hammingArray[0]
    self(obj).parity ^= self(obj).hammingArray[0];
    self(obj).inc_version(); // increment version counter
    self(obj).reset_dirty();
  }
  
  // ctor for static checksum, only: initialization on startup, before "main"
  ChecksummingHamming() {
    if(STATIC == true){
      this->__dirty();
      __generate(0);
    }
  }
};

template<typename TypeInfo, bool STATIC, unsigned tSIZE>
bool ChecksummingHamming<TypeInfo, STATIC, tSIZE>::__repair(T* obj) {
  // stop preemption from now (FIXME: only for T::SYNCHRONIZED==1)
  StopPreemption stop; // constructor/destructor pattern

  // The detected error could have been fixed by another thread,
  // while we had been waiting for the StopPreemption lock
  // Thus, check again ...
  if(self(obj).get_dirty() == 0) {
    // checksum is still valid (not dirty)
    unsigned int version = self(obj).get_version(); // remember which checksum we're verifying

    // re-calculate parity and hamming code
    machine_word_t hammingArray[DIMENSION];
    machine_word_t parity = self(obj).parity ^ CHECKSUM_INIT;
    for(unsigned int i=0; i<DIMENSION; i++) {
      hammingArray[i] = self(obj).hammingArray[i];
    }
    JPTL::MemberIterator<TypeInfo, HammingCodeGenerate, HammingCodeInfoInit<STATIC, DIMENSION, false> >::exec(obj, hammingArray, &parity);
    // re-construct the global parity from hammingArray[0]
    parity ^= (hammingArray[0] ^ self(obj).hammingArray[0]);

    if( (parity == 0) || (self(obj).get_dirty() != 0) || (version != self(obj).get_version()) ) {
      return true; // this error has been fixed already by someone else
    }
    //return false; // for DEBUG only: catch false-positives

    // we have a real error somewhere ... let's find out
    // It is ensured (by the parity) that the replicas won't match (while repairing)

    // 'sum' up all Hamming parity bits (to see if they indicate an error somewhere)
    machine_word_t error_detected = 0;
    for(unsigned int i=0; i<DIMENSION; i++) {
      error_detected |= hammingArray[i];
    }

    // in case all syndromes are zero, the overall parity bit must be corrupt (and the payload data intact)
    if(error_detected == 0) {
      self(obj).parity ^= parity; // fix the overall parity bit
      return true; // error fixed
    }

    // TODO: the overall parity bit does not cover the hamming parity bits, at the moment.
    //       -> errors in the hamming parity bits will look as uncorrectable errors
    //       -> however, we enter the correction function only if there are errors in the payload data

    // check for uncorrectable errors (i.e. double bit errors affecting the same stripe/column)
    // in such a case, the overall parity bit is even, but the syndrome is *not* zero
    // => correction possible if and only if both the syndrome and the parity indicate an error
    if(error_detected != parity) {
      return false; // can't fix -> abort
    }

    // locate the errors
    for(machine_word_t bitpos=1; error_detected != 0; bitpos = bitpos << 1) {
      if( (error_detected & 0x1) != 0 ) { // bit position affected?
        unsigned int syndrome = 0; // calculate a syndrome for each bit position
        for(unsigned int i=0; i<DIMENSION; i++) {
          if( (hammingArray[i] & bitpos) != 0 ) {
            syndrome |= (1<<i);
          }
        }
        // cout << TypeInfo::signature() << ": syndrome: " << syndrome << " (bitpos: " << bitpos << ")" << endl;
        // fix the particular error:
        JPTL::MemberIterator<TypeInfo, HammingCodeRepair, HammingCodeInfoInit<STATIC, DIMENSION> >::exec(obj, syndrome, bitpos);
      }
      error_detected = error_detected >> 1;
    }
    errorCorrected(); // we had been successful (repairing payload data)
  }
  return true; // dirty bit set ... fine, object already in use
}


// null checksum class
template<typename TypeInfo, bool STATIC>
class ChecksummingHamming<TypeInfo, STATIC, 0> : public ChecksummingBase<TypeInfo, STATIC, 0> {
  public:
  enum { SIZE = 0 };
  __attribute__((always_inline)) inline static bool __check(typename TypeInfo::That* obj) { return true; }
  __attribute__((always_inline)) inline static void __generate(typename TypeInfo::That* obj) {}
  template<typename U>
  __attribute__((always_inline)) inline static bool getChecksum(typename TypeInfo::That* obj, U* checksum) { *checksum = 0; return true; }
};

} //CoolChecksum

#endif /* __CHECKSUMMING_HAMMING_H__ */

