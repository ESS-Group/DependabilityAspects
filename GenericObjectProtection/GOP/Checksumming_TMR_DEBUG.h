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

#ifndef __CHECKSUMMING__TMRDEBUG_H__
#define __CHECKSUMMING__TMRDEBUG_H__

#include "ChecksummingBase.h"
#include "ObjectSize.h"
#include "JPTL.h"
#include "StopPreemption.h"
#include "MemoryBarriers.h"

#include "Checksumming_TMR.h"

//#include <cyg/infra/testcase.h>
//#include <cyg/infra/diag.h> // diag_printf
#include <stdio.h>
//#include <iostream>
//using namespace std;


namespace CoolChecksum {


template<typename MemberInfo, typename LAST>
struct TMRRepair_Debug {
  // compile-time calculations
  typedef typename TMRInfo<MemberInfo, LAST>::EXEC EXEC;

  template<typename T>
  __attribute__((always_inline)) inline static void exec(T obj, unsigned char* dstArray, bool* proceed) {
    if(EXEC::MEMBER_IS_CHECKSUMMED == true) {
      //XXX: there is no need to correct bit errors in the copies, since they will be overwritten on next non-const function
      if(__builtin_memcmp(&dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX_1], (const void*)MemberInfo::pointer(obj), EXEC::SIZE) != 0) { // == 0 <=> is equal
        // error found
        if(__builtin_memcmp(&dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX_1], &dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX_2], EXEC::SIZE) == 0) {
          // original is faulty -> fixing it
          __builtin_memcpy((void*)MemberInfo::pointer(obj), &dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX_1], EXEC::SIZE);
          printf("err: %s\n", MemberInfo::name());
          errorCorrected();
        }
        else if(__builtin_memcmp((const void*)MemberInfo::pointer(obj), &dstArray[EXEC::CURRENT_SHADOW_ARRAY_INDEX_2], EXEC::SIZE) != 0) {
          // all three copies differ -> won't fix
          *proceed = false;
        }
      }
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
class ChecksummingTMRDebug : public ChecksummingBase<TypeInfo, STATIC> { // ChecksummingBase provides: get_dirty() and reset_dirty()
  public:
  enum { SIZE = tSIZE };
  typedef typename TypeInfo::That T;
  
  private:
  enum { ALIGNED_SIZE = ((SIZE + sizeof(void*) -1) / sizeof(void*)) * sizeof(void*) }; // [bytes]
  void* shadowAttribs[ (ALIGNED_SIZE/sizeof(void*)) * 2 ];

  // helper method to obtain Checksumming sub-object for a given object pointer/type
  __attribute__((always_inline)) inline static ChecksummingTMRDebug& self(T* obj) { return Get<T, STATIC>::self(obj); }

  // helper method to obtain the shadow-attribute array
  __attribute__((always_inline)) inline static unsigned char* getShadowAttribs(T* obj) {
    return (unsigned char*) (self(obj).shadowAttribs);
  }

  static bool __repair(T* obj) __attribute__((noinline));

  public:
  __attribute__((always_inline)) inline static bool __check(T* obj) {
    const unsigned int version = self(obj).get_version(); // remember which replicas we're verifying
    int errros_found = 0;
    JPTL::MemberIterator<TypeInfo, TMRCheck, TMRInit<STATIC, ALIGNED_SIZE> >::exec(obj, getShadowAttribs(obj), &errros_found);
    if(errros_found != 0) {
      // error(s) found ... now let's find the cause
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
    JPTL::MemberIterator<TypeInfo, TMRCopy, TMRInit<STATIC, ALIGNED_SIZE> >::exec(obj, getShadowAttribs(obj));
    self(obj).inc_version(); // increment version counter
    self(obj).reset_dirty();
  }
  
  // ctor for static checksum, only: initialization on startup, before "main"
  ChecksummingTMRDebug() {
    if(STATIC == true){
      this->__dirty();
      __generate(0);
    }
  }
};

template<typename TypeInfo, bool STATIC, unsigned tSIZE>
bool ChecksummingTMRDebug<TypeInfo, STATIC, tSIZE>::__repair(T* obj) {
  // stop preemption from now (FIXME: only for T::SYNCHRONIZED==1)
  StopPreemption stop; // constructor/destructor pattern

  // The detected error could have been fixed by another thread,
  // while we had been waiting for the StopPreemption lock
  // Thus, check again ...
  if(self(obj).get_dirty() == 0) {
    // checksum is still valid (not dirty)
    unsigned int version = self(obj).get_version(); // remember which replicas we're verifying
    int errros_found = 0;
    JPTL::MemberIterator<TypeInfo, TMRCheck, TMRInit<STATIC, ALIGNED_SIZE> >::exec(obj, getShadowAttribs(obj), &errros_found);
    if( (errros_found == 0) || (self(obj).get_dirty() != 0) || (version != self(obj).get_version()) ) {
      return true; // this error has been fixed already by someone else
    }
    //return false; // for DEBUG only: catch false-positives

    // we have a real error somewhere ... let's find out
    // TODO FIXME XXX: ensure the replicas won't match (while repairing)
    bool result = true;
    JPTL::MemberIterator<TypeInfo, TMRRepair_Debug, TMRInit<STATIC, ALIGNED_SIZE> >::exec(obj, getShadowAttribs(obj), &result);
    return result;
  }
  return true; // dirty bit set ... fine, object already in use
}


// null checksum class
template<typename TypeInfo, bool STATIC>
class ChecksummingTMRDebug<TypeInfo, STATIC, 0> : public ChecksummingBase<TypeInfo, STATIC, 0> {
  public:
  enum { SIZE = 0 };
  __attribute__((always_inline)) inline static bool __check(typename TypeInfo::That* obj) { return true; }
  __attribute__((always_inline)) inline static void __generate(typename TypeInfo::That* obj) {}
};

} //CoolChecksum

#endif /* __CHECKSUMMING__TMRDEBUG_H__ */

