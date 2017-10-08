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

#ifndef __MEMORY_BARRIERS_H__
#define __MEMORY_BARRIERS_H__

// do we have an SMP system?
#define GENERIC_OBJECT_PROTECTION_SMP_SUPPORT 1

namespace CoolChecksum {

// compiler-only memory barrier
__attribute__((always_inline)) inline void barrier() { asm volatile("":::"memory"); }

#if GENERIC_OBJECT_PROTECTION_SMP_SUPPORT


#if defined(__i386__) || defined(__x86_64__)

   // guarantees that every load in struction that precedes in program order the LFENCE instruction
   // is globally visible before any load instruction that follows the LFENCE instruction is globally visible.
  //__attribute__((always_inline)) inline void lfence() { asm volatile("lfence":::"memory"); }

  // guarantees that every store instruction that precedes in program order the SFENCE instruction
  // is globally visible before any store instruction that follows the SFENCE instruction is globally visible.
  //__attribute__((always_inline)) inline void sfence() { asm volatile("sfence":::"memory"); }

  // guarantees that every load and store instruction that precedes in program order the MFENCE instruction
  // is globally visible before any load or store instruction that follows the MFENCE instruction is globally visible.
  __attribute__((always_inline)) inline void mfence() { asm volatile("mfence":::"memory"); }

#else

  // full hardware memory barrier, for both load and stores (portable gcc intrinsic)
  //__attribute__((always_inline)) inline void lfence() { __sync_synchronize(); }
  //__attribute__((always_inline)) inline void sfence() { __sync_synchronize(); }
  __attribute__((always_inline)) inline void mfence() { __sync_synchronize(); } 

#endif


#else // ! GENERIC_OBJECT_PROTECTION_SMP_SUPPORT

  // compiler memory barriers are sufficient
  //__attribute__((always_inline)) inline void lfence() { asm volatile("":::"memory"); }
  //__attribute__((always_inline)) inline void sfence() { asm volatile("":::"memory"); }
  __attribute__((always_inline)) inline void mfence() { asm volatile("":::"memory"); }

#endif


} //namespace CoolChecksum

#endif /* __MEMORY_BARRIERS_H__ */
