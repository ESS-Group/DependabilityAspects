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

#ifndef __CHECKSUMMING_SUMDMR_H__
#define __CHECKSUMMING_SUMDMR_H__

#include "ChecksummingBase.h"
#include "ObjectSize.h"
#include "JPTL.h"
#include "StopPreemption.h"
#include "MemoryBarriers.h"

//#include <cyg/infra/diag.h> // diag_printf
//#include <stdio.h>
//#include <iostream>
//using namespace std;


namespace CoolChecksum {

// Determine the maximum of instructions generated for a single member
enum { SUM_UNROLL_THRESHOLD = 32 };


// Two's complement checksum
template<unsigned SIZE, bool UNROLL= (SIZE <= (SUM_UNROLL_THRESHOLD*sizeof(long)) ) >
struct TWOSUM;

// basic machine instruction specializations
template<>
struct TWOSUM<sizeof(char), true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return sum + *(char*) data;
  }
};
template<>
struct TWOSUM<sizeof(short), sizeof(short) != sizeof(char)> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return sum + *(short*) data;
  }
};
template<>
struct TWOSUM<sizeof(int), (sizeof(int) != sizeof(short)) && (sizeof(int) != sizeof(long))> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return sum + *(int*) data;
  }
};
template<>
struct TWOSUM<sizeof(long), true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return sum + *(long*) data;
  }
};

// compound specialization(s)
template<>
struct TWOSUM<3, true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return TWOSUM<1>::gen(TWOSUM<2>::gen(sum, data), ((const char*) data)+2);
  }
};
template<>
struct TWOSUM<5, true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return TWOSUM<1>::gen(TWOSUM<4>::gen(sum, data), ((const char*) data)+4);
  }
};
template<>
struct TWOSUM<6, true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return TWOSUM<2>::gen(TWOSUM<4>::gen(sum, data), ((const char*) data)+4);
  }
};
template<>
struct TWOSUM<7, true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return TWOSUM<3>::gen(TWOSUM<4>::gen(sum, data), ((const char*) data)+4);
  }
};

// end of recursion
template<>
struct TWOSUM<0, true> {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return sum;
  }
};

// primary recursive template (unrolled)
template<unsigned SIZE, bool UNROLL>
struct TWOSUM {
  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    return TWOSUM<SIZE - sizeof(long)>::gen(TWOSUM<sizeof(long)>::gen(sum, data), ((long*)data) + 1 );
  }
};

// primary template generating a loop (not unrolled)
template<unsigned SIZE>
struct TWOSUM<SIZE, false> {

  enum { UNROLL_FACTOR = UnrollFactor<long, SIZE, SUM_UNROLL_THRESHOLD/2>::BEST_FIT, // instructions per loop
         BYTES_PER_LOOP = UNROLL_FACTOR * sizeof(long), // checksummed bytes per loop
         LOOPS = SIZE / BYTES_PER_LOOP, // number of loops to execute
         REMAINDER = SIZE % BYTES_PER_LOOP }; // remaining bytes to add in

  __attribute__((always_inline)) inline static long gen(long sum, const void* data) {
    for(unsigned int i=0; i<LOOPS; i++) {
      sum = TWOSUM<BYTES_PER_LOOP>::gen(sum, data);
      data = ((long*)data)+(UNROLL_FACTOR);
    }
    return TWOSUM<REMAINDER>::gen(sum, data); // add in remainder (unrolled)
  }
};



template<typename MemberInfo, typename LAST>
struct DMRInfo {
  struct EXEC {
    // CONST TYPE INFO
    enum { STATIC = LAST::STATIC,
           MEMBER_IS_CHECKSUMMED = MemberDetails<MemberInfo, STATIC>::IS_CHECKSUMMED, //TODO: (SIZE==0) yields the same
           // size of the current member:
           SIZE = SizeOfChecksummed<MemberInfo, STATIC>::SIZE, // [bytes]
           // padding required before the current member:
           PADDING = Alignment<LAST::NEXT_SHADOW_ARRAY_INDEX, SIZE>::PADDING,
           // offset where the current member should start (with padding):
           CURRENT_SHADOW_ARRAY_INDEX = LAST::NEXT_SHADOW_ARRAY_INDEX + PADDING,
           // offset where the next member starts (without its own padding):
           NEXT_SHADOW_ARRAY_INDEX = LAST::NEXT_SHADOW_ARRAY_INDEX + PADDING + SIZE };
  };
};
template<bool tSTATIC>
struct DMRInit {
  enum { NEXT_SHADOW_ARRAY_INDEX = 0,
         STATIC = tSTATIC };
};

template<typename MemberInfo, typename LAST>
struct CopyAndSum {
  // compile-time calculations
  typedef typename DMRInfo<MemberInfo, LAST>::EXEC EXEC;

  template<typename T>
  __attribute__((always_inline)) inline static void exec(T obj, long* checksum, unsigned char* dstArray) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      //cout << "copying " << MemberInfo::name() << endl;
      __builtin_memcpy(&dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX], (const void*)MemberInfo::pointer(obj), EXEC::SIZE); // pointer to this member
      *checksum = TWOSUM<EXEC::SIZE>::gen(*checksum, (const void*) MemberInfo::pointer(obj));
    }
  }
};

template<typename MemberInfo, typename LAST>
struct SumOnly {
  // compile-time calculations
  typedef typename DMRInfo<MemberInfo, LAST>::EXEC EXEC;

  template<typename T>
  __attribute__((always_inline)) inline static void exec(T obj, long* checksum) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      *checksum = TWOSUM<EXEC::SIZE>::gen(*checksum, (const void*) MemberInfo::pointer(obj));
    }
  }
};

template<typename MemberInfo, typename LAST>
struct SumShadow {
  // compile-time calculations
  typedef typename DMRInfo<MemberInfo, LAST>::EXEC EXEC;

  __attribute__((always_inline)) inline static void exec(long* checksum, unsigned char* dstArray) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      *checksum = TWOSUM<EXEC::SIZE>::gen(*checksum, &dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX]);
    }
  }
};

template<typename MemberInfo, typename LAST>
struct CopyRepair {
  // compile-time calculations
  typedef typename DMRInfo<MemberInfo, LAST>::EXEC EXEC;

  template<typename T>
  __attribute__((always_inline)) inline static void exec(T obj, unsigned char* dstArray) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      //if(__builtin_memcmp(&dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX], (const void*)MemberInfo::pointer(obj), EXEC::SIZE) != 0) {
        //cout << "[!!!] ERROR in attribute '" << MemberInfo::name() << "'" << endl;
        //cout << "located at addr: " << (unsigned int)addr << endl;
      //  while(1);
      //}
      // just overwrite the complete (faulty) object
      __builtin_memcpy((void*)MemberInfo::pointer(obj), &dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX], EXEC::SIZE); // fix it
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
class ChecksummingSUMDMR : public ChecksummingBase<TypeInfo, STATIC> { // ChecksummingBase provides: get_dirty() and reset_dirty()
  public:
  enum { SIZE = tSIZE };
  typedef typename TypeInfo::That T;
  
  private:
  enum { CHECKSUM_INIT = STATIC ? 0x1 : (TypeInfo::HASHCODE & 0xFFFF) };
  long checksum;
  void* shadowAttribs[ (SIZE + sizeof(void*) -1) / sizeof(void*) ];

  // helper method to obtain Checksumming sub-object for a given object pointer/type
  __attribute__((always_inline)) inline static ChecksummingSUMDMR& self(T* obj) { return Get<T, STATIC>::self(obj); }

  // helper method to obtain the shadow-attribute array
  __attribute__((always_inline)) inline static unsigned char* getShadowAttribs(T* obj) {
    return (unsigned char*) (self(obj).shadowAttribs);
  }

  static bool __repair(T* obj) __attribute__((noinline));

  public:
  // make the checksum publicly readable for other purposes
  template<typename U>
  __attribute__((always_inline)) inline static bool getChecksum(T* obj, U* checksum) {
    if(self(obj).get_dirty() == 0) {
      const unsigned int version = self(obj).get_version();
      *checksum = self(obj).checksum;
      if( (self(obj).get_dirty() == 0) && (version == self(obj).get_version()) ) {
        return true; // checksum valid
      }
    }
    return false; // checksum invalid: object in use
  }

  __attribute__((always_inline)) inline static bool __check(T* obj) {
    const unsigned int version = self(obj).get_version(); // remember which checksum we're verifying
    long checksum_tmp = CHECKSUM_INIT;
    JPTL::MemberIterator<TypeInfo, SumOnly, DMRInit<STATIC> >::exec(obj, &checksum_tmp);
    if(self(obj).checksum != checksum_tmp) {
      // checksum error ... now let's find the cause
      // test whether we had not been interrupted while verifying the checksum
      if( (self(obj).get_dirty() == 0) && (version == self(obj).get_version()) ) {
        // okay, we found bit errors ... so let's repair
        return __repair(obj);
      }
    }
    return true;
  }

  __attribute__((always_inline)) inline static void __generate(T* obj) {
    // dirty has to be set to *before* this function is entered (and before the locker is unlocked), see: LockAdviceInvoker.ah
    long checksum_tmp = CHECKSUM_INIT; // calculate checksum, and store intermediate results on our own stack
    JPTL::MemberIterator<TypeInfo, CopyAndSum, DMRInit<STATIC> >::exec(obj, &checksum_tmp, getShadowAttribs(obj));
    self(obj).checksum = checksum_tmp; // finally, update the object's checksum by a single copy instruction
    self(obj).inc_version(); // increment version counter
    self(obj).reset_dirty();
  }
  
  // ctor for static checksum, only: initialization on startup, before "main"
  ChecksummingSUMDMR() {
    if(STATIC == true){
      this->__dirty();
      __generate(0);
    }
  }
};

template<typename TypeInfo, bool STATIC, unsigned tSIZE>
bool ChecksummingSUMDMR<TypeInfo, STATIC, tSIZE>::__repair(T* obj) {
  // stop preemption from now (FIXME: only for T::SYNCHRONIZED==1)
  StopPreemption stop; // constructor/destructor pattern

  // The detected error could have been fixed by another thread,
  // while we had been waiting for the StopPreemption lock
  // Thus, check again ...
  if(self(obj).get_dirty() == 0) {
    // checksum is still valid (not dirty)
    unsigned int version = self(obj).get_version(); // remember which checksum we're verifying
    long checksum_tmp = CHECKSUM_INIT;
    JPTL::MemberIterator<TypeInfo, SumOnly, DMRInit<STATIC> >::exec(obj, &checksum_tmp); //TODO: don't unroll (rare case)
    if( (self(obj).checksum == checksum_tmp) || (self(obj).get_dirty() != 0) || (version != self(obj).get_version()) ) {
      return true; // this error has been fixed already by someone else
    }
    //return false; // for DEBUG only: catch false-positives

    // we have a real error somewhere ... let's find out
    long checksum_shadow = CHECKSUM_INIT;
    // do not use TWOSUM<SIZE>::gen(...) directly, since small members (char, short, ...) would be added differently
    JPTL::MemberIterator<TypeInfo, SumShadow, DMRInit<STATIC> >::exec(&checksum_shadow, getShadowAttribs(obj)); //TODO: don't unroll (rare case)
    if(checksum_shadow == self(obj).checksum) {
      // real object is faulty!
      self(obj).checksum = ~checksum_shadow; // ensure the checksum won't match (while repairing)
      JPTL::MemberIterator<TypeInfo, CopyRepair, DMRInit<STATIC> >::exec(obj, getShadowAttribs(obj));
      self(obj).checksum = checksum_shadow; // restore proper checksum
      errorCorrected();
    }
    else if(checksum_shadow == checksum_tmp) {
      // FIXME: either checksum is faulty, OR, both real object and shadowAttribs and the checksum are faulty!!!
      // checksum is faulty
      self(obj).checksum = checksum_tmp;
    }
    else {
      // wtf? (bit errors in both the shadow array AND checksum/real object!)
      return false;
    }
  }
  return true; // dirty bit set ... fine, object already in use
}


// null checksum class
template<typename TypeInfo, bool STATIC>
class ChecksummingSUMDMR<TypeInfo, STATIC, 0> : public ChecksummingBase<TypeInfo, STATIC, 0> {
  public:
  enum { SIZE = 0 };
  __attribute__((always_inline)) inline static bool __check(typename TypeInfo::That* obj) { return true; }
  __attribute__((always_inline)) inline static void __generate(typename TypeInfo::That* obj) {}
  template<typename U>
  __attribute__((always_inline)) inline static bool getChecksum(typename TypeInfo::That* obj, U* checksum) { *checksum = 0; return true; }
};

} //CoolChecksum

#endif /* __CHECKSUMMING_SUMDMR_H__ */

