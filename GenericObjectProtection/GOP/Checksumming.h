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

#ifndef __CHECKSUMMING_H__
#define __CHECKSUMMING_H__

#include "ChecksummingVariant.h"

// the following are just typedefs with template arguments (workaround until C++11)

#if   defined (__GENERIC_OBJECT_PROTECTION_SUMDMR__)
#include "Checksumming_SUM+DMR.h"
namespace CoolChecksum {
  template<typename TypeInfo, bool STATIC=false, unsigned dummy1=0, unsigned dummy2=0>
  class Checksumming : public ChecksummingSUMDMR<TypeInfo, STATIC> {};
} //CoolChecksum

#elif defined (__GENERIC_OBJECT_PROTECTION_CRCDMR__)
#include "Checksumming_CRC+DMR.h"
namespace CoolChecksum {
  template<typename TypeInfo, bool STATIC=false, unsigned dummy1=0, unsigned dummy2=0>
  class Checksumming : public ChecksummingCRCDMR<TypeInfo, STATIC> {};
} //CoolChecksum

#elif defined (__GENERIC_OBJECT_PROTECTION_CRC__)
#include "Checksumming_CRC.h"
namespace CoolChecksum {
template<typename TypeInfo, bool STATIC=false, unsigned dummy1=0, unsigned dummy2=0>
class Checksumming : public ChecksummingCRC<TypeInfo, STATIC> {};
} //CoolChecksum

#elif defined (__GENERIC_OBJECT_PROTECTION_TMR__)
#include "Checksumming_TMR.h"
namespace CoolChecksum {
  template<typename TypeInfo, bool STATIC=false, unsigned dummy1=0, unsigned dummy2=0>
  class Checksumming : public ChecksummingTMR<TypeInfo, STATIC> {};
} //CoolChecksum

#elif defined (__GENERIC_OBJECT_PROTECTION_TMRDEBUG__)
#include "Checksumming_TMR_DEBUG.h"
namespace CoolChecksum {
  template<typename TypeInfo, bool STATIC=false, unsigned dummy1=0, unsigned dummy2=0>
  class Checksumming : public ChecksummingTMRDebug<TypeInfo, STATIC> {};
} //CoolChecksum

#elif defined (__GENERIC_OBJECT_PROTECTION_HAMMING__)
#include "Checksumming_Hamming.h"
#include "Checksumming_SUM+DMR.h"
namespace CoolChecksum {
  template<typename TypeInfo, bool STATIC=false, unsigned tSIZE=
  // once again, puma is not willing to accept this ... :-(
  #ifndef __puma
  JPTL::MemberIterator<TypeInfo, SizeOfNonPublic, SizeOfNonPublicInit<STATIC> >::EXEC::SIZE,
  #else
  0,
  #endif
  // use SUM+DMR for up to 3 full words, and Hamming for larger objects
  // at the size of 3 full word, both variants have the same amount of redundancy
  unsigned SELECTOR = (tSIZE <= (sizeof(machine_word_t)*3)) ? 1 : 0 // machine_word_t taken from 'Checksumming_Hamming.h'
  >
  class Checksumming {}; // no rule found: compile-time assertion

  template<typename TypeInfo, bool STATIC, unsigned tSIZE>
  class Checksumming<TypeInfo, STATIC, tSIZE, 0> : public ChecksummingHamming<TypeInfo, STATIC, tSIZE> {};
  template<typename TypeInfo, bool STATIC, unsigned tSIZE>
  class Checksumming<TypeInfo, STATIC, tSIZE, 1> : public ChecksummingSUMDMR<TypeInfo, STATIC, tSIZE> {};
} //CoolChecksum

#else
#error "No 'Checksumming' variant defined"
// null checksum class to make the compiler happy
namespace CoolChecksum {
template<typename TypeInfo, bool STATIC=false, unsigned dummy1=0, unsigned dummy2=0>
class Checksumming {
  public:
  enum { SIZE = 0 };
  __attribute__((always_inline)) inline static bool __check(...) { return true; }
  __attribute__((always_inline)) inline static void __generate(...) {}
  __attribute__((always_inline)) inline void __dirty() const {}
};
} //CoolChecksum
#endif

#endif /* __CHECKSUMMING_H__ */
