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

#ifndef _NANOS_SYSTEM_DECL_H
#define _NANOS_SYSTEM_DECL_H

#include "processingelement_decl.hpp"
#include "throttle_decl.hpp"
#include <vector>
#include <string>
#include "schedule_decl.hpp"
#include "threadteam_decl.hpp"
#include "slicer_decl.hpp"
#include "nanos-int.h"
#include "dependency_decl.hpp"
#include "instrumentation_decl.hpp"
#include "network.hpp"
#include "directory_decl.hpp"
#include "pminterface_decl.hpp"
#include "cache_map_decl.hpp"

#include "barrier_decl.hpp"


namespace nanos
{

// This class initializes/finalizes the library
// All global variables MUST be declared inside

   class System
   {
      public:
         // constants
         typedef enum { DEDICATED, SHARED } ExecutionMode;
         typedef enum { POOL, ONE_THREAD } InitialMode;

         typedef void (*Init) ();

      private:
         // types
         typedef std::vector<PE *>         PEList;
         typedef std::vector<BaseThread *> ThreadList;
         typedef std::map<std::string, Slicer *> Slicers;
         
         // configuration variables
         int                  _numPEs;
         int                  _deviceStackSize;
         bool                 _bindThreads;
         bool                 _profile;
         bool                 _instrument;
         bool                 _verboseMode;
         ExecutionMode        _executionMode;
         InitialMode          _initialMode;
         int                  _thsPerPE;
         bool                 _untieMaster;
         bool                 _delayedStart;
         bool                 _useYield;
         bool                 _synchronizedStart;

         // cluster arch
         bool                 _useCluster;
         bool                 _isMaster;
         Atomic<unsigned int> _preMainBarrier;
         Atomic<unsigned int> _preMainBarrierLast;

         //cutoff policy and related variables
         ThrottlePolicy      *_throttlePolicy;
         SchedulerStats       _schedStats;
         SchedulerConf        _schedConf;

         /*! names of the scheduling, cutoff, barrier and instrumentation plugins */
         std::string          _defSchedule;
         std::string          _defThrottlePolicy;
         std::string          _defBarr;
         std::string          _defInstr;

         std::string          _defArch;

         /*! factories for scheduling, pes and barriers objects */
         peFactory            _hostFactory;
         barrFactory          _defBarrFactory;

         PEList               _pes;
         ThreadList           _workers;
        
         /*! It counts how many threads have finalized their initialization */
         Atomic<unsigned int> _initializedThreads;
         /*! This counts how many threads we're waiting to be initialized */
         unsigned int         _targetThreads;

         Slicers              _slicers; /**< set of global slicers */

         /*! Cluster: GasNet conduit to use and system Network object */
         std::string          _currentConduit;
         Network              _net;

         Instrumentation     *_instrumentation; /**< Instrumentation object used in current execution */
         SchedulePolicy      *_defSchedulePolicy;

         // Mempory access directory
         Directory            _directory;

         // Programming model interface
         PMInterface *        _pmInterface;

         // CacheMap register
         CacheMap             _cacheMap;
         
         std::set<void *> _dataToInv;
         volatile void * _dataToInvAddr;
         Lock _dataToInvLock;
         std::set<void *> _dataToIncVer;
         Lock _dataToIncVerLock;
         Directory *_myFavDir;
         WD *slaveParentWD;
         BaseThread *_masterGpuThd;

         // disable copy constructor & assignment operation
         System( const System &sys );
         const System & operator= ( const System &sys );

         void config ();
         void loadModules();
         
         PE * createPE ( std::string pe_type, int pid );

      public:
         /*! \brief System default constructor
          */
         System ();
         /*! \brief System destructor
          */
         ~System ();

         void start ();
         void finish ();

         void submit ( WD &work );
         void submitWithDependencies (WD& work, size_t numDeps, Dependency* deps);
         void waitOn ( size_t numDeps, Dependency* deps);
         void inlineWork ( WD &work );

         void createWD (WD **uwd, size_t num_devices, nanos_device_t *devices,
                        size_t data_size, int data_align, void ** data, WG *uwg,
                        nanos_wd_props_t *props, size_t num_copies, nanos_copy_data_t **copies, nanos_translate_args_t translate_args );

         void createSlicedWD ( WD **uwd, size_t num_devices, nanos_device_t *devices, size_t outline_data_size,
                        int outline_data_align, void **outline_data, WG *uwg, Slicer *slicer, size_t slicer_data_size,
                        int slicer_data_align, SlicerData *&slicer_data, nanos_wd_props_t *props, size_t num_copies,
                        nanos_copy_data_t **copies );

         void duplicateWD ( WD **uwd, WD *wd );
         void duplicateSlicedWD ( SlicedWD **uwd, SlicedWD *wd );

        /* \brief prepares a WD to be scheduled/executed.
         * \param work WD to be set up
         */
         void setupWD( WD &work, WD *parent );

         // methods to access configuration variable         
         void setNumPEs ( int npes );

         int getNumPEs () const;

         void setDeviceStackSize ( int stackSize );

         int getDeviceStackSize () const;

         void setBinding ( bool set );

         bool getBinding () const;

         ExecutionMode getExecutionMode () const;

         bool getVerbose () const;

         void setVerbose ( bool value );

         void setInitialMode ( InitialMode mode );
         InitialMode getInitialMode() const;

         void setDelayedStart ( bool set);

         bool getDelayedStart () const;

         bool useYield() const;

         int getThsPerPE() const;

         int getTaskNum() const;

         int getIdleNum() const;

         int getReadyNum() const;

         int getRunningTasks() const;

         int getNumWorkers() const;

         void setUntieMaster ( bool value );

         bool getUntieMaster () const;

         void setSynchronizedStart ( bool value );
         bool getSynchronizedStart ( void ) const;

         // team related methods
         BaseThread * getUnassignedWorker ( void );
         ThreadTeam * createTeam ( unsigned nthreads, void *constraints=NULL,
                                   bool reuseCurrent=true,  TeamData *tdata = 0 );

         BaseThread * getWorker( unsigned int n );

         void endTeam ( ThreadTeam *team );
         void releaseWorker ( BaseThread * thread );

         void setThrottlePolicy( ThrottlePolicy * policy );

         bool throttleTask();

         const std::string & getDefaultSchedule() const;

         const std::string & getDefaultThrottlePolicy() const;

         const std::string & getDefaultBarrier() const;

         const std::string & getDefaultInstrumentation() const;

         const std::string & getDefaultArch() const;
         
         const std::string & getCurrentConduit() const;
         
         void setDefaultArch( const std::string &arch );

         void setHostFactory ( peFactory factory );

         void setDefaultBarrFactory ( barrFactory factory );

         Slicer * getSlicer( const std::string &label ) const;

         Instrumentation * getInstrumentation ( void ) const;

         void setInstrumentation ( Instrumentation *instr );

         void registerSlicer ( const std::string &label, Slicer *slicer);

         void setDefaultSchedulePolicy ( SchedulePolicy *policy );
         
         SchedulePolicy * getDefaultSchedulePolicy ( ) const;

         SchedulerStats & getSchedulerStats ();
         SchedulerConf  & getSchedulerConf();

         bool useCluster( void ) const;

         bool isMaster( void ) const;
         void setMaster( bool );

         Network * getNetwork( void );

         void stopFirstThread( void );

         void setPMInterface (PMInterface *_pm);
         const PMInterface & getPMInterface ( void ) const;
         CacheMap& getCacheMap();

         void threadReady ();
         void preMainBarrier();
         
         void setMyFavDir( Directory * dir) { _myFavDir = dir; };
         void setSlaveParentWD( WD * wd ){ slaveParentWD = wd ; };
         WD* getSlaveParentWD( ){ return slaveParentWD ; };

	 void addInvData( void * addr ){ std::cerr << "Added to inval addr " << addr << std::endl; _dataToInvLock.acquire(); _dataToInvAddr = addr; _dataToInvLock.release(); while ( _dataToInvAddr != NULL) {} }
	 //void invThisData( void * addr )
	 void invThisData( )
         {
		 _dataToInvLock.acquire();
                 if ( _dataToInvAddr != NULL )
                 {
                         std::cerr << "I should invalidate data " << _dataToInvAddr << std::endl;
			 //DirectoryEntry *ent = _myFavDir->findEntry( (uint64_t) addr );
			 //if (ent != NULL) 
			 //{
			 //        if (ent->getOwner() != NULL )
			 //       	 if ( !ent->isInvalidated() )
			 //       	 {
			 //       		 std::list<uint64_t> tagsToInvalidate;
			 //       		 tagsToInvalidate.push_back( ( uint64_t ) addr );
			 //       		 //std::cerr  <<"n:" << gasnet_mynode() << " sync host (tag)" << std::endl;
			 //       		 _myFavDir->synchronizeHost( tagsToInvalidate );
			 //       		 //std::cerr <<"n:" << gasnet_mynode() << " go on with get req " << *(( float *) tagAddr )<< std::endl;
			 //       	 }
			 //}
                         _dataToInvAddr = NULL;
                 }
		 _dataToInvLock.release();
	 }



         void addIncVerData( void * addr ) { _dataToIncVerLock.acquire(); _dataToIncVer.insert( addr ); _dataToIncVerLock.release(); }
	 void incVerThisData( void * addr ) { 
		 _dataToIncVerLock.acquire(); 
		 std::set<void *>::iterator it = _dataToIncVer.find( addr ); 
		 if (it != _dataToIncVer.end()) 
		 {
			 _dataToIncVer.erase(it);  
                         std::cerr << "I should inc data " << addr << std::endl;
			 DirectoryEntry *ent = _myFavDir->findEntry( (uint64_t) addr );
			 if (ent != NULL) ent->increaseVersion();
                 }
		 _dataToIncVerLock.release(); 
	 }
   };

   extern System sys;

};

#endif

