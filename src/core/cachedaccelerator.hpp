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

#ifndef _NANOS_CACHED_ACCELERATOR
#define _NANOS_CACHED_ACCELERATOR

#include "cachedaccelerator_decl.hpp"
#include "accelerator_decl.hpp"
#include "cache.hpp"
#include "system.hpp"

using namespace nanos;

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::setCacheSize( size_t size )
{
   _cache.setSize( size );
}

template <class CacheDevice, class Policy>
inline bool CachedAccelerator<CacheDevice,Policy>::checkBlockingCacheAccessDependent( Directory& dir, uint64_t tag, size_t size, bool input, bool output )
{
   //std::cerr << "n:" << sys.getNetwork()->getNodeNum() << " register cache access: dir " << (void *) &dir << " addr " << (void *) tag << std::endl;
   return _cache.checkBlockingCacheAccess( dir, tag, size, input, output );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::registerCacheAccessDependent( Directory& dir, uint64_t tag, size_t size, bool input, bool output )
{
   //std::cerr << "n:" << sys.getNetwork()->getNodeNum() << " register cache access: dir " << (void *) &dir << " addr " << (void *) tag << std::endl;
   _cache.registerCacheAccess( dir, tag, size, input, output );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::unregisterCacheAccessDependent( Directory& dir, uint64_t tag, size_t size, bool output )
{
   //std::cerr << "n:" << sys.getNetwork()->getNodeNum() << " UNregister cache access: dir " << (void *) &dir << " addr " << (void *) tag << std::endl;
   _cache.unregisterCacheAccess( dir, tag, size, output );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::registerPrivateAccessDependent( Directory& dir, uint64_t tag, size_t size, bool input, bool output )
{
   _cache.registerPrivateAccess( dir, tag, size, input, output );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::unregisterPrivateAccessDependent( Directory& dir, uint64_t tag, size_t size )
{
   _cache.unregisterPrivateAccess( dir, tag, size );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::synchronize( CopyDescriptor &cd )
{
   _cache.synchronize( cd );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::synchronize( std::list<CopyDescriptor> &cds )
{
   _cache.synchronize( cds );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::waitInputDependent( uint64_t tag )
{
   _cache.waitInput( tag );
}

template <class CacheDevice, class Policy>
inline void* CachedAccelerator<CacheDevice,Policy>::getAddressDependent( uint64_t tag )
{
   return _cache.getAddress( tag );
}

template <class CacheDevice, class Policy>
inline void CachedAccelerator<CacheDevice,Policy>::copyToDependent( void *dst, uint64_t tag, size_t size )
{
   _cache.copyTo( dst, tag, size );
}

#endif
