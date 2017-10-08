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

#ifndef __CHECKSUMMING_CRC_H__
#define __CHECKSUMMING_CRC_H__

#include "ChecksummingBase.h"
#include "ObjectSize.h"
#include "JPTL.h"
#include "MemoryBarriers.h"

#include "Checksumming_SUM+DMR.h" // for DMRInfo

//#include <cyg/infra/diag.h> // diag_printf
//#include <stdio.h>
//#include <iostream>
//using namespace std;


namespace CoolChecksum {

// Determine the maximum of instructions generated for a single member
enum { CRC_UNROLL_THRESHOLD = 32 };


// Hardware-accelerated CRC-32C (using CRC32 instruction)
template<unsigned SIZE, bool UNROLL= (SIZE <= (CRC_UNROLL_THRESHOLD*sizeof(void*)) ) >
struct CRC;

// basic machine instruction specializations
template<>
struct CRC<1, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return __builtin_ia32_crc32qi(crc, *(unsigned int*) data);
  }
};
template<>
struct CRC<2, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return __builtin_ia32_crc32hi(crc, *(unsigned int*) data);
  }
};
template<>
struct CRC<4, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return __builtin_ia32_crc32si(crc, *(unsigned int*) data);
  }
};

template<>
struct CRC<8, true> {
#if defined(__x86_64__)
  __attribute__((always_inline)) inline static unsigned long long gen(unsigned long long crc, const void* data) {
    // gcc defines the 64-bit-operand intrinsic with 'unsigned long long' arguments and result:
    // --> unsigned long long __builtin_ia32_crc32di (unsigned long long, unsigned long long)
    return __builtin_ia32_crc32di(crc, *(unsigned long long*) data);
#else
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return CRC<4>::gen(CRC<4>::gen(crc, data), ((const char*) data)+4);
#endif
  }
};

// compound specialization(s)
template<>
struct CRC<3, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return CRC<1>::gen(CRC<2>::gen(crc, data), ((const char*) data)+2);
  }
};
template<>
struct CRC<5, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return CRC<1>::gen(CRC<4>::gen(crc, data), ((const char*) data)+4);
  }
};
template<>
struct CRC<6, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return CRC<2>::gen(CRC<4>::gen(crc, data), ((const char*) data)+4);
  }
};
template<>
struct CRC<7, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return CRC<3>::gen(CRC<4>::gen(crc, data), ((const char*) data)+4);
  }
};

// end of recursion
template<>
struct CRC<0, true> {
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    return crc;
  }
};

// primary recursive template (unrolled)
template<unsigned SIZE, bool UNROLL>
struct CRC {
#if defined(__x86_64__)
  // the recursive function needs to carry 'unsigned long long' values in 64-bit mode.
  // otherwise, the gcc interleaves each crc32q instruction with a 32-bit mov instruction.
  __attribute__((always_inline)) inline static unsigned long long gen(unsigned long long crc, const void* data) {
#else
  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
#endif
    return CRC<SIZE-8>::gen(CRC<8>::gen(crc, data), ((const char*) data)+8);
  }
};

// primary template generating a loop (not unrolled)
template<unsigned SIZE>
struct CRC<SIZE, false> {

  enum { UNROLL_FACTOR = UnrollFactor<void*, SIZE, CRC_UNROLL_THRESHOLD/2>::BEST_FIT, // instructions per loop
         BYTES_PER_LOOP = UNROLL_FACTOR * sizeof(void*), // checksummed bytes per loop
         LOOPS = SIZE / BYTES_PER_LOOP, // number of loops to execute
         REMAINDER = SIZE % BYTES_PER_LOOP }; // remaining bytes to add in

  __attribute__((always_inline)) inline static unsigned int gen(unsigned int crc, const void* data) {
    for(unsigned int i=0; i<LOOPS; i++) {
      crc = CRC<BYTES_PER_LOOP>::gen(crc, data);
      data = ((const char*)data)+BYTES_PER_LOOP;
    }
    return CRC<REMAINDER>::gen(crc, data); // add in remainder (unrolled)
  }
};


template<typename MemberInfo, typename LAST>
struct CRCOnly {
  // compile-time calculations
  typedef typename DMRInfo<MemberInfo, LAST>::EXEC EXEC;

  template<typename T>
  __attribute__((always_inline)) inline static void exec(T obj, unsigned int* crc32) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      *crc32 = CRC<EXEC::SIZE>::gen(*crc32, (const void*) MemberInfo::pointer(obj));
    }
  }
};

//-----------------------------------


template<typename TypeInfo, bool STATIC=false, unsigned tSIZE=
// once again, puma is not willing to accept this ... :-(
#ifndef __puma
JPTL::MemberIterator<TypeInfo, SizeOfNonPublic, SizeOfNonPublicInit<STATIC> >::EXEC::SIZE
#else
0
#endif
>
class ChecksummingCRC : public ChecksummingBase<TypeInfo, STATIC> { // ChecksummingBase provides: get_dirty() and reset_dirty()
  public:
  enum { SIZE = tSIZE };
  typedef typename TypeInfo::That T;
  
  private:
  unsigned int crc32; //TODO: uint32_t!

  // helper method to obtain Checksumming sub-object for a given object pointer/type
  __attribute__((always_inline)) inline static ChecksummingCRC& self(T* obj) { return Get<T, STATIC>::self(obj); }

  public:
  // make the checksum publicly readable for other purposes
  template<typename U>
  __attribute__((always_inline)) inline static bool getChecksum(T* obj, U* checksum) {
    if(self(obj).get_dirty() == 0) {
      const unsigned int version = self(obj).get_version();
      *checksum = self(obj).crc32;
      if( (self(obj).get_dirty() == 0) && (version == self(obj).get_version()) ) {
        return true; // checksum valid
      }
    }
    return false; // checksum invalid: object in use
  }

  __attribute__((always_inline)) inline static bool __check(T* obj) {
    const unsigned int version = self(obj).get_version(); // remember which checksum we're verifying
    unsigned int crc32_tmp = 0xFFFFFFFF;
    JPTL::MemberIterator<TypeInfo, CRCOnly, DMRInit<STATIC> >::exec(obj, &crc32_tmp);
    if(self(obj).crc32 != crc32_tmp) {
      // checksum error ... now let's find the cause
      // test whether we had not been interrupted while verifying the checksum
      if( (self(obj).get_dirty() == 0) && (version == self(obj).get_version()) ) {
        return false; // real checksum error ... return "detected"
      }
    }
    return true;
  }
  
  __attribute__((always_inline)) inline static void __generate(T* obj) {
    // dirty has to be set to *before* this function is entered (and before the locker is unlocked), see: LockAdviceInvoker.ah

    // TODO: implement FINAL constant, maybe as TypeInfo::HASHCODE
    // An actual CRC looks like CRC(a) = crc(INIT,a) xor FINAL, rather than being a just pure long division.
    // Usually INIT and FINAL will be something like all 1s to complement the initial state and the final state.
    // The reason why INIT is chosen to be non-zero is to combat a phenomenon known as "zero blindness".
    // see: https://www.fpcomplete.com/user/edwardk/parallel-crc

    unsigned int crc32_tmp = 0xFFFFFFFF; // calculate crc32, and store intermediate results on our own stack
    JPTL::MemberIterator<TypeInfo, CRCOnly, DMRInit<STATIC> >::exec(obj, &crc32_tmp);
    self(obj).crc32 = crc32_tmp; // finally, update the object's crc32 by a single copy instruction
    self(obj).inc_version(); // increment version counter
    self(obj).reset_dirty();
  }
  
  // ctor for static checksum, only: initialization on startup, before "main"
  ChecksummingCRC() {
    if(STATIC == true){
      this->__dirty();
      __generate(0);
    }
  }
};	


// null checksum class
template<typename TypeInfo, bool STATIC>
class ChecksummingCRC<TypeInfo, STATIC, 0> : public ChecksummingBase<TypeInfo, STATIC, 0> {
  public:
  enum { SIZE = 0 };
  __attribute__((always_inline)) inline static bool __check(typename TypeInfo::That* obj) { return true; }
  __attribute__((always_inline)) inline static void __generate(typename TypeInfo::That* obj) {}
  template<typename U>
  __attribute__((always_inline)) inline static unsigned int getChecksum(typename TypeInfo::That* obj, U* checksum) { *checksum = 0; return true; }
};

} //CoolChecksum

#endif /* __CHECKSUMMING_CRC_H__ */

