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

#ifndef __STOP_PREEMPTION_H__
#define __STOP_PREEMPTION_H__

// Architecture dependent code to enable/disable preemption
// For example, on x86/AMD64, interrupts could be disabled

#ifdef __unix__
#include <pthread.h>

namespace CoolChecksum {

// This class implements the C++ constructor/destructor pattern
class StopPreemption {
private:
  pthread_mutex_t* get_mutex() const {
    static pthread_mutex_t mutex;
    return &mutex;
  }

public:
  StopPreemption() {
    pthread_mutex_lock(get_mutex());
  }

  ~StopPreemption() {
    pthread_mutex_unlock(get_mutex());
  }
};

} // namespace CoolChecksum


#else /* ! __unix__ */

namespace CoolChecksum {
#warning "StopPreemption not implemented!"
class StopPreemption {};
}

#endif /* __unix__ */

#endif /* __STOP_PREEMPTION_H__ */
