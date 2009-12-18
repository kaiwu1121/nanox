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

#include "synchronizedcondition.hpp"
#include "basethread.hpp"
#include "schedule.hpp"

using namespace nanos;

/* \brief Wait until the condition has been satisfied
 */
void SynchronizedCondition::wait()
{
   int spins=100; // Has this to be configurable??

   myThread->getCurrentWD()->setSyncCond( this );

   while ( !checkCondition() ) {
      BaseThread *thread = getMyThreadSafe();
      WD * current = thread->getCurrentWD();
      current->setIdle();

      spins--;
      if ( spins == 0 ) {
         if ( !current->isBlocked() ) {
            lock();
            if ( !checkCondition() ) {
               setWaiter( current );

               WD *next = thread->getSchedulingGroup()->atBlock ( thread );

/*               if ( next ) {
                  sys._numReady--;
               } */

               if ( !next )
                  next = thread->getSchedulingGroup()->getIdle ( thread );
          
	       if ( next ) {
                  current->setBlocked();
                  thread->switchTo ( next ); // how do we unlock here?
               }
               else {
                  unlock();
               }
            } else {
               unlock();
            }
         } else {
            WD *next = thread->getSchedulingGroup()->atBlock ( thread );
          
/*            if ( next ) {
               sys._numReady--;
            }*/
          
            if ( !next )
               next = thread->getSchedulingGroup()->getIdle ( thread );
          
            if ( next ) {
               thread->switchTo ( next );
            }
         }
      spins = 100;
      }
   }
   myThread->getCurrentWD()->setReady();
   myThread->getCurrentWD()->setSyncCond( NULL );
}

/* \brief Signal the waiters if the condition has been satisfied. If they
 * are blocked they will be set to ready and enqueued.
 */
void SynchronizedCondition::signal()
{
   if ( checkCondition() ) {
      lock();
        while ( hasWaiters() ) {
           WD* wd = getAndRemoveWaiter();
           if ( wd->isBlocked() ) {
              wd->setReady();
              Scheduler::queue( *wd );
           }
        }
      unlock(); 
   }
}

