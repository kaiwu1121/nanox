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

#ifndef _NANOS_BASE_REGIONS_DEPENDENCIES_DOMAIN
#define _NANOS_BASE_REGIONS_DEPENDENCIES_DOMAIN

#include "baseregionsdependenciesdomain_decl.hpp"
#include "basedependenciesdomain.hpp"


namespace nanos {

template<typename CONTAINER_T>
inline void BaseRegionsDependenciesDomain::finalizeReduction( CONTAINER_T &status, const BaseDependency& target )
{
   for ( typename CONTAINER_T::iterator it = status.begin(); it != status.end(); it++ ) {
      BaseDependenciesDomain::finalizeReduction( **it, target );
   }
}

template<typename CONTAINER_T>
inline void BaseRegionsDependenciesDomain::dependOnLastWriter( DependableObject &depObj, CONTAINER_T &statusContainer, BaseDependency const &target,
                                                               SchedulePolicySuccessorFunctor* callback )
{
   for ( typename CONTAINER_T::iterator it = statusContainer.begin(); it != statusContainer.end(); it++ ) {
      BaseDependenciesDomain::dependOnLastWriter( depObj, **it, target, callback );
   }
}

template<typename CONTAINER_T>
inline void BaseRegionsDependenciesDomain::dependOnReaders( DependableObject &depObj, CONTAINER_T &statusContainer, BaseDependency const &target, SchedulePolicySuccessorFunctor* callback )
{
   for ( typename CONTAINER_T::iterator it = statusContainer.begin(); it != statusContainer.end(); it++ ) {
      BaseDependenciesDomain::dependOnReaders( depObj, *(*it), target, callback );
   }
}

template <typename SOURCE_STATUS_T>
inline CommutationDO * BaseRegionsDependenciesDomain::setUpInitialCommutationDependableObject ( BaseDependency const &target, AccessType const &accessType, SOURCE_STATUS_T &sourceStatus, TrackableObject &targetStatus )
{
   if ( targetStatus.getCommDO() == NULL ) {
      // The commutation update has not been set up yet
      CommutationDO *initialCommDO = NEW CommutationDO( target, accessType.commutative );
      initialCommDO->setDependenciesDomain( this );
      initialCommDO->increasePredecessors();
      
      for ( typename SOURCE_STATUS_T::iterator it = sourceStatus.begin(); it != sourceStatus.end(); it++ ) {
         TrackableObject &sourceStatusFragment = **it;
         
         // add dependencies to all previous reads using a CommutationDO
         BaseDependenciesDomain::dependOnReaders( *initialCommDO, sourceStatusFragment, target, NULL );
         
         // NOTE: The regions version does not allow a write to take the place of the initialCommDo since there might be more than one due to several source regions
         BaseDependenciesDomain::dependOnLastWriter( *initialCommDO, sourceStatusFragment, target, NULL );
         
         // NOTE: We should probably check if the source subregion is completely within the target region and in that case eliminate it
      }
      
      {
         SyncLockBlock lock3( targetStatus.getReadersLock() );
         targetStatus.flushReaders();
      }
      initialCommDO->addWriteTarget( target );
      
      // Replace the lastWriter with the initial CommutationDO
      targetStatus.setLastWriter( *initialCommDO );
      
      return initialCommDO;
   } else {
      return NULL;
   }
}


template <typename SOURCE_STATUS_T>
inline void BaseRegionsDependenciesDomain::submitDependableObjectCommutativeDataAccess ( DependableObject &depObj, BaseDependency const &target, AccessType const &accessType, SOURCE_STATUS_T &sourceStatus, TrackableObject &targetStatus, SchedulePolicySuccessorFunctor* callback )
{
   // NOTE: Do not change the order
   CommutationDO *initialCommDO = setUpInitialCommutationDependableObject( target, accessType, sourceStatus, targetStatus );
   CommutationDO *commDO = setUpTargetCommutationDependableObject( target, accessType, targetStatus );
   
   // Add the Commutation object as successor of the current DO (depObj)
   depObj.addSuccessor( *commDO );
   
   // assumes no new readers added concurrently
   BaseDependenciesDomain::dependOnLastWriter( depObj, targetStatus, target, callback );

   // The dummy predecessor is to make sure that initialCommDO does not execute 'finished'
   // while depObj is being added as its successor
   if ( initialCommDO != NULL ) {
      initialCommDO->decreasePredecessors();
   }
}

template <typename SOURCE_STATUS_T>
inline void BaseRegionsDependenciesDomain::submitDependableObjectInoutDataAccess ( DependableObject &depObj, BaseDependency const &target, AccessType const &accessType, SOURCE_STATUS_T &sourceStatus, TrackableObject &targetStatus, SchedulePolicySuccessorFunctor* callback )
{
   finalizeReduction( sourceStatus, target );
   dependOnLastWriter( depObj, sourceStatus, target, callback );
   dependOnReaders( depObj, sourceStatus, target, callback );
   setAsWriter( depObj, targetStatus, target );
}

template <typename SOURCE_STATUS_T>
inline void BaseRegionsDependenciesDomain::submitDependableObjectInputDataAccess ( DependableObject &depObj, BaseDependency const &target, AccessType const &accessType, SOURCE_STATUS_T &sourceStatus, TrackableObject &targetStatus, SchedulePolicySuccessorFunctor* callback )
{
   finalizeReduction( sourceStatus, target );
   dependOnLastWriter( depObj, sourceStatus, target, callback );

   if ( !depObj.waits() ) {
      addAsReader( depObj, targetStatus );
   }
}

template <typename SOURCE_STATUS_T>
inline void BaseRegionsDependenciesDomain::submitDependableObjectOutputDataAccess ( DependableObject &depObj, BaseDependency const &target, AccessType const &accessType, SOURCE_STATUS_T &sourceStatus, TrackableObject &targetStatus, SchedulePolicySuccessorFunctor* callback )
{
   finalizeReduction( sourceStatus, target );
   dependOnLastWriter( depObj, sourceStatus, target, callback );
   dependOnReaders( depObj, sourceStatus, target, callback );
   setAsWriter( depObj, targetStatus, target );
}

}

#endif

