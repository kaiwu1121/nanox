/*************************************************************************************/
/*      Copyright 2009 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the NANOS++ library.                                    */
/*                                                                                   */
/*      NANOS++ is free software: you can redistribute it and/or modify              */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      NANOS++ is distributed in the hope that it will be useful,                   */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with NANOS++.  If not, see <http://www.gnu.org/licenses/>.             */
/*************************************************************************************/

#ifndef _NANOS_SYSTEM_H
#define _NANOS_SYSTEM_H

#include "system_decl.hpp"
#include "processingelement.hpp"
#include "throttle.hpp"
#include <vector>
#include <string>
#include "schedule.hpp"
#include "threadteam.hpp"
#include "slicer.hpp"
#include "nanos-int.h"
#include "dependency.hpp"
#include "instrumentor_decl.hpp"

using namespace nanos;

// methods to access configuration variable         
inline void System::setNumPEs ( int npes ) { _numPEs = npes; }

inline int System::getNumPEs () const { return _numPEs; }

inline void System::setDeviceStackSize ( int stackSize ) { _deviceStackSize = stackSize; }

inline int System::getDeviceStackSize () const {return _deviceStackSize; }

inline void System::setBinding ( bool set ) { _bindThreads = set; }

inline bool System::getBinding () const { return _bindThreads; }

inline System::ExecutionMode System::getExecutionMode () const { return _executionMode; }

inline bool System::getVerbose () const { return _verboseMode; }

inline void System::setInitialMode ( System::InitialMode mode ) { _initialMode = mode; }

inline System::InitialMode System::getInitialMode() const { return _initialMode; }

inline void System::setThsPerPE( int ths ) { _thsPerPE = ths; }

inline void System::setDelayedStart ( bool set) { _delayedStart = set; }

inline bool System::getDelayedStart () const { return _delayedStart; }

inline int System::getThsPerPE() const { return _thsPerPE; }

inline int System::getTaskNum() const { return _schedStats._totalTasks.value(); }

inline int System::getIdleNum() const { return _schedStats._idleThreads.value(); }

inline int System::getReadyNum() const { return _schedStats._readyTasks.value(); }

inline int System::getRunningTasks() const
{
   return _workers.size() - _schedStats._idleThreads.value();
}

inline int System::getNumWorkers() const { return _workers.size(); }

inline void System::setThrottlePolicy( ThrottlePolicy * policy ) { _throttlePolicy = policy; }

inline const std::string & System::getDefaultSchedule() const { return _defSchedule; }

inline const std::string & System::getDefaultThrottlePolicy() const { return _defThrottlePolicy; }

inline const std::string & System::getDefaultBarrier() const { return _defBarr; }

inline const std::string & System::getDefaultInstrumentor() const { return _defInstr; }

inline void System::setHostFactory ( peFactory factory ) { _hostFactory = factory; }

inline void System::setDefaultBarrFactory ( barrFactory factory ) { _defBarrFactory = factory; }

inline Slicer * System::getSlicer( const std::string &label ) const 
{ 
   Slicers::const_iterator it = _slicers.find(label);
   if ( it == _slicers.end() ) return NULL;
   return (*it).second;
}

inline Instrumentor * System::getInstrumentor ( void ) const { return _instrumentor; }

inline void System::setInstrumentor ( Instrumentor *instr ) { _instrumentor = instr; }

inline void System::registerSlicer ( const std::string &label, Slicer *slicer) { _slicers[label] = slicer; }

inline void System::setDefaultSchedulePolicy ( SchedulePolicy *policy ) { _defSchedulePolicy = policy; }
inline SchedulePolicy * System::getDefaultSchedulePolicy ( ) const  { return _defSchedulePolicy; }

inline SchedulerStats & System::getSchedulerStats () { return _schedStats; }

#endif

