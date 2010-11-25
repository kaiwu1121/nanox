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

#include <string.h>
#include "system.hpp"
#include "config.hpp"
#include "plugin.hpp"
#include "schedule.hpp"
#include "barrier.hpp"
#include "nanos-int.h"
#include "copydata.hpp"
#include "os.hpp"

#ifdef SPU_DEV
#include "spuprocessor.hpp"
#endif

#ifdef GPU_DEV
#include "gpuprocessor_fwd.hpp"
#endif

using namespace nanos;

/* This macro computes the memory offset for a given element (ce) with taking into account its own *type*
 * and the *base* and *size* of the previous element e.g.:
 *
 *    +---------+---+------+          It is important to realize that first two parameters 
 *    |++++pe+++|···|++ce++|          refer to previous element and only *type* parameter refers
 *    +---------+---+------+          to current element (which we want to align). This is because
 *    ^         ^   ^                 we want to add padding to the previous structure in order to
 *    base   size   align (return)    aling the current one.
 *
 * If we use this macro with *base* and *size* of the last element (and with any type given as a parameter
 * we get the size of the whole chunk.
 */
#define NANOS_ALIGNED_MEMORY_OFFSET(base,size,alignment) \
   ( ((uintptr_t)(base+size+alignment-1)) & (~(uintptr_t)(alignment-1)) )

namespace nanos {
  System::Init externInit __attribute__((weak));
}

System nanos::sys;

// default system values go here
System::System () :
      _numPEs( 1 ), _deviceStackSize( 0 ), _bindThreads( true ), _profile( false ), _instrument( false ),
      _verboseMode( false ), _executionMode( DEDICATED ), _initialMode(POOL), _thsPerPE( 1 ), _untieMaster(true),
      _delayedStart(false), _useYield(true), _synchronizedStart(true), _throttlePolicy ( NULL ),
      _defSchedule( "default" ), _defThrottlePolicy( "numtasks" ), 
      _defBarr( "posix" ), _defInstr ( "empty_trace" ), _defArch("smp"),
      _initializedThreads ( 0 ), _targetThreads ( 0 ),
      _instrumentation ( NULL ), _defSchedulePolicy( NULL ), _directory(), _pmInterface( NULL )
{
   verbose0 ( "NANOS++ initializing... start" );
   // OS::init must be called here and not in System::start() as it can be too late
   // to locate the program arguments at that point
   OS::init();
   config();
   if ( !_delayedStart ) {
      start();
   }
   verbose0 ( "NANOS++ initializing... end" );
}

void System::loadModules ()
{
   verbose0 ( "Configuring module manager" );

   PluginManager::init();

   verbose0 ( "Loading modules" );

   // load host processor module
   verbose0( "loading SMP support" );

   if ( !PluginManager::load ( "pe-"+getDefaultArch() ) )
      fatal0 ( "Couldn't load host support" );

   ensure( _hostFactory,"No default host factory" );

#ifdef GPU_DEV
   verbose0( "loading GPU support" );

   if ( !PluginManager::load ( "pe-gpu" ) )
      fatal0 ( "Couldn't load GPU support" );
#endif

   // load default schedule plugin
   verbose0( "loading " << getDefaultSchedule() << " scheduling policy support" );

   if ( !PluginManager::load ( "sched-"+getDefaultSchedule() ) )
      fatal0 ( "Couldn't load main scheduling policy" );

   ensure( _defSchedulePolicy,"No default system scheduling factory" );

   verbose0( "loading " << getDefaultThrottlePolicy() << " throttle policy" );

   if ( !PluginManager::load( "throttle-"+getDefaultThrottlePolicy() ) )
      fatal0( "Could not load main cutoff policy" );

   ensure(_throttlePolicy, "No default throttle policy");

   verbose0( "loading " << getDefaultBarrier() << " barrier algorithm" );

   if ( !PluginManager::load( "barrier-"+getDefaultBarrier() ) )
      fatal0( "Could not load main barrier algorithm" );

   if ( !PluginManager::load( "instrumentation-"+getDefaultInstrumentation() ) )
      fatal0( "Could not load " + getDefaultInstrumentation() + " instrumentation" );


   ensure( _defBarrFactory,"No default system barrier factory" );

}


void System::config ()
{
   Config config;

   if ( externInit != NULL ) {
      externInit();
   }
   if ( !_pmInterface ) {
      // bare bone run
      _pmInterface = new PMInterface();
   }

   verbose0 ( "Preparing library configuration" );

   config.setOptionsSection ( "Core", "Core options of the core of Nanos++ runtime"  );

   config.registerConfigOption ( "num_pes", new Config::PositiveVar( _numPEs ), "Defines the number of processing elements" );
   config.registerArgOption ( "num_pes", "pes" );
   config.registerEnvOption ( "num_pes", "NX_PES" );

   config.registerConfigOption ( "stack-size", new Config::PositiveVar( _deviceStackSize ), "Defines the default stack size for all devices" );
   config.registerArgOption ( "stack-size", "stack-size" );
   config.registerEnvOption ( "stack-size", "NX_STACK_SIZE" );

   config.registerConfigOption ( "no-binding", new Config::FlagOption( _bindThreads, false), "Disables thread binding" );
   config.registerArgOption ( "no-binding", "disable-binding" );

   config.registerConfigOption( "no-yield", new Config::FlagOption( _useYield, false), "Do not yield on idle and condition waits");
   config.registerArgOption ( "no-yield", "disable-yield" );

   config.registerConfigOption ( "verbose", new Config::FlagOption( _verboseMode), "Activates verbose mode" );
   config.registerArgOption ( "verbose", "verbose" );

#if 0
   FIXME: implement execution modes (#146)
   Config::MapVar<ExecutionMode> map( _executionMode );
   map.addOption( "dedicated", DEDICATED).addOption( "shared", SHARED );
   config.registerConfigOption ( "exec_mode", &map, "Execution mode" );
   config.registerArgOption ( "exec_mode", "mode" );
#endif

   config.registerConfigOption ( "schedule", new Config::StringVar ( _defSchedule ), "Defines the scheduling policy" );
   config.registerArgOption ( "schedule", "schedule" );
   config.registerEnvOption ( "schedule", "NX_SCHEDULE" );

   config.registerConfigOption ( "throttle", new Config::StringVar ( _defThrottlePolicy ), "Defines the throttle policy" );
   config.registerArgOption ( "throttle", "throttle" );
   config.registerEnvOption ( "throttle", "NX_THROTTLE" );

   config.registerConfigOption ( "barrier", new Config::StringVar ( _defBarr ), "Defines barrier algorithm" );
   config.registerArgOption ( "barrier", "barrier" );
   config.registerEnvOption ( "barrier", "NX_BARRIER" );

   config.registerConfigOption ( "instrumentation", new Config::StringVar ( _defInstr ), "Defines instrumentation format" );
   config.registerArgOption ( "instrumentation", "instrumentation" );
   config.registerEnvOption ( "instrumentation", "NX_INSTRUMENTATION" );

   config.registerConfigOption ( "no-sync-start", new Config::FlagOption( _synchronizedStart, false), "Disables synchronized start" );
   config.registerArgOption ( "no-sync-start", "disable-synchronized-start" );

   _schedConf.config(config);
   _pmInterface->config(config);

   verbose0 ( "Reading Configuration" );
   config.init();
}

PE * System::createPE ( std::string pe_type, int pid )
{
   // TODO: lookup table for PE factories
   // in the mean time assume only one factory

   return _hostFactory( pid );
}

void System::start ()
{
   loadModules();

   // Instrumentation startup
   NANOS_INSTRUMENT ( sys.getInstrumentation()->initialize() );
   verbose0 ( "Starting runtime" );

   _pmInterface->start();

   int numPes = getNumPEs();

   _pes.reserve ( numPes );

   PE *pe = createPE ( "smp", 0 );
   _pes.push_back ( pe );
   _workers.push_back( &pe->associateThisThread ( getUntieMaster() ) );

   WD &mainWD = *myThread->getCurrentWD();
   
   if ( _pmInterface->getInternalDataSize() > 0 )
     // TODO: is this properly aligned?
     mainWD.setInternalData(new char[_pmInterface->getInternalDataSize()]);
      
   _pmInterface->setupWD(mainWD);

   /* Renaming currend thread as Master */
   myThread->rename("Master");

   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseOpenStateEvent (NANOS_STARTUP) );

   _targetThreads = getThsPerPE() * numPes;
#ifdef GPU_DEV
   _targetThreads += nanos::ext::GPUDD::getGPUCount();
#endif

   //start as much threads per pe as requested by the user
   for ( int ths = 1; ths < getThsPerPE(); ths++ ) {
      _workers.push_back( &pe->startWorker( ));
   }

   int p;
   for ( p = 1; p < numPes ; p++ ) {
      pe = createPE ( "smp", p );
      _pes.push_back ( pe );

      //starting as much threads per pe as requested by the user

      for ( int ths = 0; ths < getThsPerPE(); ths++ ) {
         _workers.push_back( &pe->startWorker() );
      }
   }

#ifdef GPU_DEV
   int gpuC;
   for ( gpuC = 0; gpuC < nanos::ext::GPUDD::getGPUCount(); gpuC++ ) {
      PE *gpu = new nanos::ext::GPUProcessor( p++, gpuC );
      _pes.push_back( gpu );
      _workers.push_back( &gpu->startWorker() );
   }
#endif

#ifdef SPU_DEV
   PE *spu = new nanos::ext::SPUProcessor(100, (nanos::ext::SMPProcessor &) *_pes[0]);
   spu->startWorker();
#endif

   switch ( getInitialMode() )
   {
      case POOL:
         createTeam( _workers.size() );
         break;
      case ONE_THREAD:
         createTeam(1);
         break;
      default:
         fatal("Unknown inital mode!");
         break;
   }

   /* Master thread is ready and waiting for the rest of the gang */
   if ( getSynchronizedStart() )   
     threadReady();

   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseCloseStateEvent() );
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseOpenStateEvent (NANOS_RUNNING) );
}

System::~System ()
{
   if ( !_delayedStart ) finish();
}

void System::finish ()
{
   /* Instrumentation: First removing RUNNING state from top of the state statck */
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseCloseStateEvent() );
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseOpenStateEvent(NANOS_SHUTDOWN) );

   verbose ( "NANOS++ shutting down.... init" );
   verbose ( "Wait for main workgroup to complete" );
   myThread->getCurrentWD()->waitCompletion();

   // we need to switch to the main thread here to finish
   // the execution correctly
   myThread->getCurrentWD()->tieTo(*_workers[0]);
   Scheduler::switchToThread(_workers[0]);
   
   ensure(getMyThreadSafe()->getId() == 0, "Main thread not finishing the application!");

   verbose ( "Joining threads... phase 1" );
   // signal stop PEs

   for ( unsigned p = 1; p < _pes.size() ; p++ ) {
      _pes[p]->stopAll();
   }

   verbose ( "Joining threads... phase 2" );

   // shutdown instrumentation
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseCloseStateEvent() );
   NANOS_INSTRUMENT ( sys.getInstrumentation()->finalize() );

   ensure( _schedStats._readyTasks == 0, "Ready task counter has an invalid value!");

   // join
   for ( unsigned p = 1; p < _pes.size() ; p++ ) {
      delete _pes[p];
   }

   _pmInterface->finish();

   verbose ( "NANOS++ shutting down.... end" );
}

/*! \brief Creates a new WD
 *
 *  This function creates a new WD, allocating memory space for device ptrs and
 *  data when necessary. 
 *
 *  \param [in,out] uwd is the related addr for WD if this parameter is null the
 *                  system will allocate space in memory for the new WD
 *  \param [in] num_devices is the number of related devices
 *  \param [in] devices is a vector of device descriptors 
 *  \param [in] data_size is the size of the related data
 *  \param [in,out] data is the related data (allocated if needed)
 *  \param [in] uwg work group to relate with
 *  \param [in] props new WD properties
 *  \param [in] num_copies is the number of copy objects of the WD
 *  \param [in] copies is vector of copy objects of the WD
 *
 *  When it does a full allocation the layout is the following:
 *
 *  +---------------+
 *  |     WD        |
 *  +---------------+
 *  |    data       |
 *  +---------------+
 *  |  dev_ptr[0]   |
 *  +---------------+
 *  |     ....      |
 *  +---------------+
 *  |  dev_ptr[N]   |
 *  +---------------+
 *  |     DD0       |
 *  +---------------+
 *  |     ....      |
 *  +---------------+
 *  |     DDN       |
 *  +---------------+
 *  |    copy0      |
 *  +---------------+
 *  |     ....      |
 *  +---------------+
 *  |    copyM      |
 *  +---------------+
 *  |   PM Data     |
 *  +---------------+
 *
 */
void System::createWD ( WD **uwd, size_t num_devices, nanos_device_t *devices, size_t data_size, int data_align,
                        void **data, WG *uwg, nanos_wd_props_t *props, size_t num_copies, nanos_copy_data_t **copies )
{
   ensure(num_devices > 0,"WorkDescriptor has no devices");

   size_t  size_WD = (*uwd == NULL)? sizeof(WD):0;
   size_t align_WD = (*uwd == NULL)? __alignof__(WD):1;

   size_t  size_Data = (data != NULL && *data == NULL)? data_size:0;
   size_t align_Data = (data != NULL && *data == NULL)? data_align:1;

   size_t  size_DPtrs = sizeof(DD *) * num_devices;
   size_t align_DPtrs = __alignof__( DD * );

   size_t  size_DDs = 0;
   for ( unsigned int i = 0; i < num_devices; i++ ) size_DDs += devices[i].dd_size;
   size_t align_DDs = __alignof__(DeviceData);

   size_t  size_Copies = (copies != NULL && *copies == NULL)? num_copies*sizeof(CopyData):0;
   size_t align_Copies = (copies != NULL && *copies == NULL)? __alignof__( nanos_copy_data_t ):1;

   size_t  size_PMD = _pmInterface->getInternalDataSize();
   size_t align_PMD = (size_PMD)? _pmInterface->getInternalDataAlignment():1;

   size_t offset_WD, offset_Data, offset_DPtrs, offset_DDs, offset_Copies, offset_PMD, total_size;

   offset_WD     = NANOS_ALIGNED_MEMORY_OFFSET(0, 0, align_WD);
   offset_Data   = NANOS_ALIGNED_MEMORY_OFFSET(offset_WD, size_WD, align_Data);
   offset_DPtrs  = NANOS_ALIGNED_MEMORY_OFFSET(offset_Data, size_Data, align_DPtrs);
   offset_DDs    = NANOS_ALIGNED_MEMORY_OFFSET(offset_DPtrs, size_DPtrs, align_DDs);
   offset_Copies = NANOS_ALIGNED_MEMORY_OFFSET(offset_DDs, size_DDs, align_Copies);
   offset_PMD    = NANOS_ALIGNED_MEMORY_OFFSET(offset_Copies, size_Copies,align_PMD);
   total_size    = NANOS_ALIGNED_MEMORY_OFFSET(offset_PMD,size_PMD,1);

   char *chunk = 0;

   /* Standard C++ ISO/IEC 14882:2003(E)
    *
    * 3.7.3.1 Allocation functions
    * The pointer returned shall be suitably aligned so that it can be converted to a
    * pointer of any complete object type and then used to access the object or array
    * in the storage allocated (until the storage is explicitly deallocated by a call
    * to a corresponding deallocation function).
    */
   if ( total_size ) chunk = new char[total_size];

   // allocating WD and DATA
   if ( *uwd == NULL ) *uwd = (WD *) (chunk + offset_WD);
   if ( data != NULL && *data == NULL ) *data = (chunk + offset_Data);

   // allocating Device Data
   DD **dev_ptrs = ( DD ** ) (chunk + offset_DPtrs);
   char *dd_location = chunk + offset_DDs;
   for ( unsigned int i = 0 ; i < num_devices ; i ++ ) {
      dev_ptrs[i] = ( DD* ) devices[i].factory( dd_location , devices[i].arg );
      dd_location += devices[i].dd_size;
   }

   ensure ((num_copies==0 && copies==NULL) || (num_copies!=0 && copies!=NULL), "Number of copies and copy data conflict" );

   // allocating copy-ins/copy-outs
   if ( copies != NULL && *copies == NULL ) *copies = ( CopyData * ) (chunk + offset_Copies);

   WD * wd =  new (*uwd) WD( num_devices, dev_ptrs, data_size, align_Data, data != NULL ? *data : NULL,
                             num_copies, (copies != NULL)? *copies : NULL );

   // initializing internal data data 
   if ( size_PMD > 0) wd->setInternalData( chunk + offset_PMD );

   // add to workgroup
   if ( uwg != NULL ) {
      WG * wg = ( WG * )uwg;
      wg->addWork( *wd );
   }

   // set properties
   if ( props != NULL ) {
      if ( props->tied ) wd->tied();
      if ( props->tie_to ) wd->tieTo( *( BaseThread * )props->tie_to );
   }
}

/*! \brief Creates a new Sliced WD
 *
 *  This function creates a new Sliced WD, allocating memory space for device ptrs and
 *  data when necessary. Also allocates Slicer Data object which is related with the WD.
 *
 *  \param [in,out] uwd is the related addr for WD if this parameter is null the
 *                  system will allocate space in memory for the new WD
 *  \param [in] num_devices is the number of related devices
 *  \param [in] devices is a vector of device descriptors 
 *  \param [in] outline_data_size is the size of the related data
 *  \param [in,out] outline_data is the related data (allocated if needed)
 *  \param [in] uwg work group to relate with
 *  \param [in] slicer is the related slicer which contains all the methods to manage
 *              this WD
 *  \param [in] slicer_data_size is the size of the related slicer data
 *  \param [in,out] data used as the slicer data (allocated if needed)
 *  \param [in] props new WD properties
 *
 *  When it does a full allocation the layout is the following:
 *
 *  +---------------+
 *  |   slicedWD    |
 *  +---------------+
 *  |    data       |
 *  +---------------+
 *  |  dev_ptr[0]   |
 *  +---------------+
 *  |     ....      |
 *  +---------------+
 *  |  dev_ptr[N]   |
 *  +---------------+
 *  |     DD0       |
 *  +---------------+
 *  |     ....      |
 *  +---------------+
 *  |     DDN       |
 *  +---------------+
 *  |    copy0      |
 *  +---------------+
 *  |     ....      |
 *  +---------------+
 *  |    copyM      |
 *  +---------------+
 *  |  SlicerData   |
 *  +---------------+
 *  |   PM Data     |
 *  +---------------+
 *
 */
void System::createSlicedWD ( WD **uwd, size_t num_devices, nanos_device_t *devices, size_t outline_data_size,
                        int outline_data_align, void **outline_data, WG *uwg, Slicer *slicer, size_t slicer_data_size,
                        int slicer_data_align, SlicerData *&slicer_data, nanos_wd_props_t *props, size_t num_copies,
                        nanos_copy_data_t **copies )
{
   ensure(num_devices > 0,"WorkDescriptor has no devices");

   size_t  size_WD = (*uwd == NULL)? sizeof(SlicedWD):0;
   size_t align_WD = (*uwd == NULL)? __alignof__(SlicedWD):1;

   size_t  size_Data = (outline_data != NULL && *outline_data == NULL)? outline_data_size:0;
   size_t align_Data = (outline_data != NULL && *outline_data == NULL)? outline_data_align:1;

   size_t  size_DPtrs = sizeof(DD *) * num_devices;
   size_t align_DPtrs = __alignof__( DD * );

   size_t  size_DDs = 0;
   for ( unsigned int i = 0; i < num_devices; i++ ) size_DDs += devices[i].dd_size;
   size_t align_DDs = __alignof__(DeviceData);

   size_t  size_Copies = (copies != NULL && *copies == NULL)? num_copies*sizeof(CopyData):0;
   size_t align_Copies = (copies != NULL && *copies == NULL)? __alignof__( nanos_copy_data_t ):1;

   size_t  size_SData = (slicer_data == NULL)? slicer_data_size:0;
   size_t align_SData = (slicer_data == NULL)? slicer_data_align:1;

   size_t  size_PMD = _pmInterface->getInternalDataSize();
   size_t align_PMD = (size_PMD)? _pmInterface->getInternalDataAlignment():1;

   size_t offset_WD, offset_Data, offset_DPtrs, offset_DDs, offset_Copies, offset_PMD, offset_SData, total_size;

   offset_WD     = NANOS_ALIGNED_MEMORY_OFFSET(0, 0, align_WD);
   offset_Data   = NANOS_ALIGNED_MEMORY_OFFSET(offset_WD, size_WD, align_Data);
   offset_DPtrs  = NANOS_ALIGNED_MEMORY_OFFSET(offset_Data, size_Data, align_DPtrs);
   offset_DDs    = NANOS_ALIGNED_MEMORY_OFFSET(offset_DPtrs, size_DPtrs, align_DDs);
   offset_Copies = NANOS_ALIGNED_MEMORY_OFFSET(offset_DDs, size_DDs, align_Copies);
   offset_PMD    = NANOS_ALIGNED_MEMORY_OFFSET(offset_Copies, size_Copies, align_PMD);
   offset_SData  = NANOS_ALIGNED_MEMORY_OFFSET(offset_PMD, size_PMD, align_SData);
   total_size    = NANOS_ALIGNED_MEMORY_OFFSET(offset_SData, size_SData, 1);

   char *chunk = 0;

   /* Standard C++ ISO/IEC 14882:2003(E)
    *
    * 3.7.3.1 Allocation functions
    * The pointer returned shall be suitably aligned so that it can be converted to a
    * pointer of any complete object type and then used to access the object or array
    * in the storage allocated (until the storage is explicitly deallocated by a call
    * to a corresponding deallocation function).
    */
   if ( total_size ) chunk = new char[total_size];

   // allocating WD and DATA
   if ( *uwd == NULL ) *uwd = (SlicedWD *) (chunk + offset_WD);
   if ( outline_data != NULL && *outline_data == NULL ) *outline_data = (chunk + offset_Data);

   // allocating Device Data
   DD **dev_ptrs = ( DD ** ) (chunk + offset_DPtrs);
   char *dd_location = chunk + offset_DDs;
   for ( unsigned int i = 0 ; i < num_devices ; i ++ ) {
      dev_ptrs[i] = ( DD* ) devices[i].factory( dd_location , devices[i].arg );
      dd_location += devices[i].dd_size;
   }

   ensure ((num_copies==0 && copies==NULL) || (num_copies!=0 && copies!=NULL), "Number of copies and copy data conflict" );

   // allocating copy-ins/copy-outs
   if ( copies != NULL && *copies == NULL ) *copies = ( CopyData * ) (chunk + offset_Copies);

   // allocating Slicer Data
   if ( slicer_data == NULL ) slicer_data = (SlicerData *) (chunk + offset_SData);

   SlicedWD * wd =  new (*uwd) SlicedWD( *slicer, slicer_data_size, align_SData,*slicer_data, num_devices, dev_ptrs, 
                       outline_data_size, align_Data, outline_data != NULL ? *outline_data : NULL, num_copies,
                       (copies == NULL) ? NULL : *copies );

   // initializing internal data data 
   if ( size_PMD > 0) wd->setInternalData( chunk + offset_PMD );

   // add to workgroup
   if ( uwg != NULL ) {
      WG * wg = ( WG * )uwg;
      wg->addWork( *wd );
   }

   // set properties
   if ( props != NULL ) {
      if ( props->tied ) wd->tied();
      if ( props->tie_to ) wd->tieTo( *( BaseThread * )props->tie_to );
   }
}

/*! \brief Duplicates a given WD
 *
 *  This function duplicates the given as a parameter WD copying all the
 *  related data (devices ptr, data and DD)
 *
 *  \param [out] uwd is the target addr for the new WD
 *  \param [in] wd is the former WD
 */
void System::duplicateWD ( WD **uwd, WD *wd)
{
   void *data = NULL;

   size_t  size_WD = (*uwd == NULL)? sizeof(WD):0;
   size_t align_WD = (*uwd == NULL)? __alignof__(WD):1;

   size_t  size_Data = wd->getDataSize();
   size_t align_Data = wd->getDataAlignment();

   size_t  size_DPtrs = sizeof(DD *) * wd->getNumDevices();
   size_t align_DPtrs = __alignof__( DD * );

   size_t  size_DDs = 0;
   for ( unsigned int i = 0; i < wd->getNumDevices(); i++ ) size_DDs += wd->getDevices()[i]->size();
   size_t align_DDs = __alignof__(DeviceData);

   size_t  size_Copies = sizeof( CopyData ) * wd->getNumCopies();
   size_t align_Copies = ( size_Copies )? __alignof__( nanos_copy_data_t ):1;

   size_t  size_PMD = _pmInterface->getInternalDataSize();
   size_t align_PMD = (size_PMD)? _pmInterface->getInternalDataAlignment():1;

   size_t offset_WD, offset_Data, offset_DPtrs, offset_DDs, offset_Copies, offset_PMD, total_size;

   offset_WD     = NANOS_ALIGNED_MEMORY_OFFSET(0, 0, align_WD);
   offset_Data   = NANOS_ALIGNED_MEMORY_OFFSET(offset_WD, size_WD, align_Data);
   offset_DPtrs  = NANOS_ALIGNED_MEMORY_OFFSET(offset_Data, size_Data, align_DPtrs);
   offset_DDs    = NANOS_ALIGNED_MEMORY_OFFSET(offset_DPtrs, size_DPtrs, align_DDs);
   offset_Copies = NANOS_ALIGNED_MEMORY_OFFSET(offset_DDs, size_DDs, align_Copies);
   offset_PMD    = NANOS_ALIGNED_MEMORY_OFFSET(offset_Copies, size_Copies,align_PMD);
   total_size    = NANOS_ALIGNED_MEMORY_OFFSET(offset_PMD,size_PMD,1);

   char *chunk = 0;

   if ( total_size ) chunk = new char[total_size];

   // allocating WD and DATA
   if ( *uwd == NULL ) *uwd = (WD *) (chunk + offset_WD);
   if ( size_Data != 0 ) {
      data = (chunk + offset_Data);
      memcpy ( data, wd->getData(), size_Data );
   }

   // allocating Device Data
   DD **dev_ptrs = ( DD ** ) (chunk + offset_DPtrs);
   char *dd_location = chunk + offset_DDs;
   for ( unsigned int i = 0 ; i < wd->getNumDevices(); i ++ ) {
      wd->getDevices()[i]->copyTo(dd_location);
      dev_ptrs[i] = ( DD* ) dd_location;
      dd_location += wd->getDevices()[i]->size();
   }

   // allocate copy-in/copy-outs
   CopyData *wdCopies = ( CopyData * ) (chunk + offset_Copies);
   char * chunk_iter = (chunk + offset_Copies);
   for ( unsigned int i = 0; i < wd->getNumCopies(); i++ ) {
      CopyData *wdCopiesCurr = ( CopyData * ) chunk_iter;
      *wdCopiesCurr = wd->getCopies()[i];
      chunk_iter += sizeof( CopyData );
   }

   // creating new WD 
   new (*uwd) WD( *wd, dev_ptrs, wdCopies , data);

   /* FIXME: (#391) Need to call setInternaData, and Copy it */
}

/*! \brief Duplicates a given SlicedWD
 *
 *  This function duplicates the given as a parameter WD copying all the
 *  related data (devices ptr, data and DD)
 *
 *  \param [out] uwd is the target addr for the new WD
 *  \param [in] wd is the former WD
 */
void System::duplicateSlicedWD ( SlicedWD **uwd, SlicedWD *wd)
{
   void *data = NULL;
   void *slicer_data = NULL;

   size_t  size_WD = (*uwd == NULL)? sizeof(SlicedWD):0;
   size_t align_WD = (*uwd == NULL)? __alignof__(SlicedWD):1;

   size_t  size_Data = wd->getDataSize();
   size_t align_Data = wd->getDataAlignment();

   size_t  size_DPtrs = sizeof(DD *) * wd->getNumDevices();
   size_t align_DPtrs = __alignof__( DD * );

   size_t  size_DDs = 0;
   for ( unsigned int i = 0; i < wd->getNumDevices(); i++ ) size_DDs += wd->getDevices()[i]->size();
   size_t align_DDs = __alignof__(DeviceData);

   size_t  size_Copies = sizeof( CopyData ) * wd->getNumCopies();
   size_t align_Copies = ( size_Copies )? __alignof__( nanos_copy_data_t ):1;

   size_t  size_SData = wd->getSlicerDataSize();
   size_t align_SData = (size_SData)? wd->getSlicerDataAlignment():1;

   size_t  size_PMD = _pmInterface->getInternalDataSize();
   size_t align_PMD = (size_PMD)? _pmInterface->getInternalDataAlignment():1;

   size_t offset_WD, offset_Data, offset_DPtrs, offset_DDs, offset_Copies, offset_PMD, offset_SData, total_size;

   offset_WD     = NANOS_ALIGNED_MEMORY_OFFSET(0, 0, align_WD);
   offset_Data   = NANOS_ALIGNED_MEMORY_OFFSET(offset_WD, size_WD, align_Data);
   offset_DPtrs  = NANOS_ALIGNED_MEMORY_OFFSET(offset_Data, size_Data, align_DPtrs);
   offset_DDs    = NANOS_ALIGNED_MEMORY_OFFSET(offset_DPtrs, size_DPtrs, align_DDs);
   offset_Copies = NANOS_ALIGNED_MEMORY_OFFSET(offset_DDs, size_DDs, align_Copies);
   offset_PMD    = NANOS_ALIGNED_MEMORY_OFFSET(offset_Copies, size_Copies, align_PMD);
   offset_SData  = NANOS_ALIGNED_MEMORY_OFFSET(offset_PMD, size_PMD, align_SData);
   total_size    = NANOS_ALIGNED_MEMORY_OFFSET(offset_SData, size_SData, 1);

   char *chunk = 0;

   if ( total_size ) chunk = new char[total_size];

   // allocating WD and DATA
   if ( *uwd == NULL ) *uwd = (SlicedWD *) (chunk + offset_WD);
   if ( size_Data != 0 ) {
      data = (chunk + offset_Data);
      memcpy ( data, wd->getData(), size_Data );
   }

   // allocating Device Data
   DD **dev_ptrs = ( DD ** ) (chunk + offset_DPtrs);
   char *dd_location = chunk + offset_DDs;
   for ( unsigned int i = 0 ; i < wd->getNumDevices(); i ++ ) {
      wd->getDevices()[i]->copyTo(dd_location);
      dev_ptrs[i] = ( DD* ) dd_location;
      dd_location += wd->getDevices()[i]->size();
   }

   // allocate copy-in/copy-outs
   CopyData *wdCopies = ( CopyData * ) (chunk + offset_Copies);
   char * chunk_iter = (chunk + offset_Copies);
   for ( unsigned int i = 0; i < wd->getNumCopies(); i++ ) {
      CopyData *wdCopiesCurr = ( CopyData * ) chunk_iter;
      *wdCopiesCurr = wd->getCopies()[i];
      chunk_iter += sizeof( CopyData );
   }

   // copy SlicerData
   if ( size_SData != 0 ) {
      slicer_data = chunk + offset_SData;
      memcpy ( slicer_data, wd->getSlicerData(), size_SData );
   }

   // creating new SlicedWD 
   new (*uwd) SlicedWD( *(wd->getSlicer()), wd->getSlicerDataSize(), wd->getSlicerDataAlignment(),
                        *((SlicerData *)slicer_data), *((WD *)wd), dev_ptrs, wdCopies, data );

   /* FIXME: (#391) Need to call setInternaData, and Copy it */
}

void System::setupWD ( WD &work, WD *parent )
{
   work.setParent ( parent );
   work.setDepth( parent->getDepth() +1 );

   // Prepare private copy structures to use relative addresses
   work.prepareCopies();

   // Invoke pmInterface
   _pmInterface->setupWD(work);
}

void System::submit ( WD &work )
{
   setupWD( work, myThread->getCurrentWD() );
   work.submit();
}

/*! \brief Submit WorkDescriptor to its parent's  dependencies domain
 */
void System::submitWithDependencies (WD& work, size_t numDeps, Dependency* deps)
{
   setupWD( work, myThread->getCurrentWD() );
   WD *current = myThread->getCurrentWD();
   current->submitWithDependencies( work, numDeps , deps);
}

/*! \brief Wait on the current WorkDescriptor's domain for some dependenices to be satisfied
 */
void System::waitOn( size_t numDeps, Dependency* deps )
{
   WD* current = myThread->getCurrentWD();
   current->waitOn( numDeps, deps );
}


void System::inlineWork ( WD &work )
{
   setupWD( work, myThread->getCurrentWD() );
   // TODO: choose actual (active) device...
   Scheduler::inlineWork( &work );
}

BaseThread * System:: getUnassignedWorker ( void )
{
   BaseThread *thread;

   for ( unsigned i  = 0; i < _workers.size(); i++ ) {
      if ( !_workers[i]->hasTeam() ) {
         thread = _workers[i];
         // recheck availability with exclusive access
         thread->lock();

         if ( thread->hasTeam() ) {
            // we lost it
            thread->unlock();
            continue;
         }

         thread->reserve(); // set team flag only

         thread->unlock();

         return thread;
      }
   }

   return NULL;
}

BaseThread * System::getWorker ( unsigned int n )
{
   if ( n < _workers.size() ) return _workers[n];
   else return NULL;
}

void System::releaseWorker ( BaseThread * thread )
{
   //TODO: destroy if too many?
   //TODO: to free or not to free team data?
   debug("Releasing thread " << thread << " from team " << thread->getTeam() );
   thread->leaveTeam();
}

ThreadTeam * System:: createTeam ( unsigned nthreads, void *constraints,
                                   bool reuseCurrent, TeamData *tdata )
{
   int thId;
   TeamData *data;

   if ( nthreads == 0 ) {
      nthreads = 1;
      nthreads = getNumPEs()*getThsPerPE();
   }
   
   SchedulePolicy *sched = 0;
   if ( !sched ) sched = sys.getDefaultSchedulePolicy();

   ScheduleTeamData *stdata = 0;
   if ( sched->getTeamDataSize() > 0 )
      stdata = sched->createTeamData(NULL);

   // create team
   ThreadTeam * team = new ThreadTeam( nthreads, *sched, stdata, *_defBarrFactory() );

   debug( "Creating team " << team << " of " << nthreads << " threads" );

   // find threads
   if ( reuseCurrent ) {
      nthreads --;

      thId = team->addThread( myThread );

      debug( "adding thread " << myThread << " with id " << toString<int>(thId) << " to " << team );

      
      if (tdata) data = &tdata[thId];
      else data = new TeamData();

      ScheduleThreadData *stdata = 0;
      if ( sched->getThreadDataSize() > 0 )
        stdata = sched->createThreadData(NULL);
      
//       data->parentTeam = myThread->getTeamData();

      data->setId(thId);
      data->setScheduleData(stdata);
      
      myThread->enterTeam( team,  data );

      debug( "added thread " << myThread << " with id " << toString<int>(thId) << " to " << team );
   }

   while ( nthreads > 0 ) {
      BaseThread *thread = getUnassignedWorker();

      if ( !thread ) {
         // alex: TODO: create one?
         break;
      }

      nthreads--;
      thId = team->addThread( thread );
      debug( "adding thread " << thread << " with id " << toString<int>(thId) << " to " << team );

      if (tdata) data = &tdata[thId];
      else data = new TeamData();

      ScheduleThreadData *stdata = 0;
      if ( sched->getThreadDataSize() > 0 )
        stdata = sched->createThreadData(NULL);

      data->setId(thId);
      data->setScheduleData(stdata);
      
      thread->enterTeam( team, data );
      debug( "added thread " << thread << " with id " << toString<int>(thId) << " to " << thread->getTeam() );
   }

   team->init();

   return team;
}

void System::endTeam ( ThreadTeam *team )
{
   if ( team->size() > 1 ) {
     fatal("Trying to end a team with running threads");
   }

//    if ( myThread->getTeamData()->parentTeam )
//    {
//       myThread->restoreTeam(myThread->getTeamData()->parentTeam);
//    }

   
   delete team;
}
