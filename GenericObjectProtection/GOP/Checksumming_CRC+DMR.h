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

#ifndef __CHECKSUMMING_CRCDMR_H__
#define __CHECKSUMMING_CRCDMR_H__

#include "ChecksummingBase.h"
#include "ObjectSize.h"
#include "JPTL.h"
#include "StopPreemption.h"
#include "MemoryBarriers.h"

#include "Checksumming_CRC.h" // for CRC stuff
#include "Checksumming_SUM+DMR.h" // for DMR stuff

//#include <cyg/infra/diag.h> // diag_printf
//#include <stdio.h>
//#include <iostream>
//using namespace std;


namespace CoolChecksum {

template<typename MemberInfo, typename LAST>
struct CopyAndCRC {
  // compile-time calculations
  typedef typename DMRInfo<MemberInfo, LAST>::EXEC EXEC;

  template<typename T>
  __attribute__((always_inline)) inline static void exec(T obj, unsigned int* crc32, unsigned char* dstArray) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      //cout << "copying " << MemberInfo::name() << endl;
      __builtin_memcpy(&dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX], (const void*)MemberInfo::pointer(obj), EXEC::SIZE); // pointer to this member
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
class ChecksummingCRCDMR : public ChecksummingBase<TypeInfo, STATIC> { // ChecksummingBase provides: get_dirty() and reset_dirty()
  public:
  enum { SIZE = tSIZE };
  typedef typename TypeInfo::That T;
  
  private:
  unsigned int crc32; //TODO: uint32_t!
  void* shadowAttribs[ (SIZE + sizeof(void*) -1) / sizeof(void*) ];

  // helper method to obtain Checksumming sub-object for a given object pointer/type
  __attribute__((always_inline)) inline static ChecksummingCRCDMR& self(T* obj) { return Get<T, STATIC>::self(obj); }

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
        // okay, we found bit errors ... so let's repair
        return __repair(obj);
      }
    }
    return true;
  }

  __attribute__((always_inline)) inline static void __generate(T* obj) {
    // dirty has to be set to *before* this function is entered (and before the locker is unlocked), see: LockAdviceInvoker.ah
    unsigned int crc32_tmp = 0xFFFFFFFF; // calculate crc32, and store intermediate results on our own stack
    JPTL::MemberIterator<TypeInfo, CopyAndCRC, DMRInit<STATIC> >::exec(obj, &crc32_tmp, getShadowAttribs(obj));
    self(obj).crc32 = crc32_tmp; // finally, update the object's crc32 by a single copy instruction
    self(obj).inc_version(); // increment version counter
    self(obj).reset_dirty();
  }
  
  // ctor for static checksum, only: initialization on startup, before "main"
  ChecksummingCRCDMR() {
    if(STATIC == true){
      this->__dirty();
      __generate(0);
    }
  }
};	

template<typename TypeInfo, bool STATIC, unsigned tSIZE>
bool ChecksummingCRCDMR<TypeInfo, STATIC, tSIZE>::__repair(T* obj) {
  // stop preemption from now (FIXME: only for T::SYNCHRONIZED==1)
  StopPreemption stop; // constructor/destructor pattern

  // The detected error could have been fixed by another thread,
  // while we had been waiting for the StopPreemption lock
  // Thus, check again ...
  if(self(obj).get_dirty() == 0) {
    // checksum is still valid (not dirty)
    unsigned int version = self(obj).get_version(); // remember which checksum we're verifying
    unsigned int crc32_tmp = 0xFFFFFFFF;
    JPTL::MemberIterator<TypeInfo, CRCOnly, DMRInit<STATIC> >::exec(obj, &crc32_tmp); //TODO: don't unroll (rare case)
    if( (self(obj).crc32 == crc32_tmp) || (self(obj).get_dirty() != 0) || (version != self(obj).get_version()) ) {
      return true; // this error has been fixed already by someone else
    }
    //return false; // for DEBUG only: catch false-positives

    // we have a real error somewhere ... let's find out
    unsigned int crc32_shadow = CRC<SIZE>::gen(0xFFFFFFFF, getShadowAttribs(obj)); //TODO *real* function for that? //TODO: don't unroll (rare case)
    if(crc32_shadow == self(obj).crc32) {
      // real object is faulty!
      self(obj).crc32 = ~crc32_shadow; // ensure the checksum won't match (while repairing)
      JPTL::MemberIterator<TypeInfo, CopyRepair, DMRInit<STATIC> >::exec(obj, getShadowAttribs(obj));
      self(obj).crc32 = crc32_shadow; // restore proper checksum
      errorCorrected();
    }
    else if(crc32_shadow == crc32_tmp) {
      // FIXME: either checksum is faulty, OR, both real object and shadowAttribs and the checksum are faulty!!!
      // crc32 is faulty
      self(obj).crc32 = crc32_tmp;
    }
    else {
      // wtf? (bit errors in both the shadow array AND crc32/real object!)
      return false;
    }
  }
  return true; // dirty bit set ... fine, object already in use
}


// null checksum class
template<typename TypeInfo, bool STATIC>
class ChecksummingCRCDMR<TypeInfo, STATIC, 0> : public ChecksummingBase<TypeInfo, STATIC, 0> {
  public:
  enum { SIZE = 0 };
  __attribute__((always_inline)) inline static bool __check(typename TypeInfo::That* obj) { return true; }
  __attribute__((always_inline)) inline static void __generate(typename TypeInfo::That* obj) {}
  template<typename U>
  __attribute__((always_inline)) inline static bool getChecksum(typename TypeInfo::That* obj, U* checksum) { *checksum = 0; return true; }
};

} //CoolChecksum

#endif /* __CHECKSUMMING_CRCDMR_H__ */

