#ifndef MEMORYOPS_DECL
#define MEMORYOPS_DECL

#include "addressspace_decl.hpp"
#include "memcachecopy_fwd.hpp"
namespace nanos {
class BaseOps {
   public:
   struct OwnOp {
      DeviceOps         *_ops;
      global_reg_t       _reg;
      unsigned int       _version;
      memory_space_id_t  _location;
      OwnOp( DeviceOps *ops, global_reg_t reg, unsigned int version, memory_space_id_t location );
      OwnOp( OwnOp const &op );
      OwnOp &operator=( OwnOp const &op );
      bool operator<( OwnOp const &op ) const {
         return _ops < op._ops;
      }
      void commitMetadata() const;
   };
   private:
   std::set< OwnOp > _ownDeviceOps;
   std::set< DeviceOps * > _otherDeviceOps;

   public:
   BaseOps();
   ~BaseOps();
   std::set< DeviceOps * > &getOtherOps();
   void insertOwnOp( DeviceOps *ops, global_reg_t reg, unsigned int version, memory_space_id_t location );
   bool isDataReady();
};

class BaseAddressSpaceInOps : public BaseOps {
   protected:
   typedef std::map< SeparateMemoryAddressSpace *, TransferList > MapType;
   MapType _separateTransfers;

   public:
   BaseAddressSpaceInOps();
   virtual ~BaseAddressSpaceInOps();

   void addOp( SeparateMemoryAddressSpace *from, global_reg_t const &reg, unsigned int version );

   virtual void addOpFromHost( global_reg_t const &reg, unsigned int version );
   virtual void issue( WD const &wd );

   virtual void prepareRegions( MemCacheCopy *memCopies, unsigned int numCopies, WD const &wd );
   virtual unsigned int getVersionNoLock( global_reg_t const &reg );

   virtual void copyInputData( global_reg_t const &reg, unsigned int version, bool output, NewLocationInfoList const &locations );
   virtual void allocateOutputMemory( global_reg_t const &reg, unsigned int version );
};

typedef BaseAddressSpaceInOps HostAddressSpaceInOps;

class SeparateAddressSpaceInOps : public BaseAddressSpaceInOps {
   protected:
   SeparateMemoryAddressSpace &_destination;
   TransferList _hostTransfers;

   public:
   SeparateAddressSpaceInOps( SeparateMemoryAddressSpace &destination );
   ~SeparateAddressSpaceInOps();

   virtual void addOpFromHost( global_reg_t const &reg, unsigned int version );
   virtual void issue( WD const &wd );

   virtual void prepareRegions( MemCacheCopy *memCopies, unsigned int numCopies, WD const &wd );
   virtual unsigned int getVersionNoLock( global_reg_t const &reg );

   virtual void copyInputData( global_reg_t const &reg, unsigned int version, bool output, NewLocationInfoList const &locations );
   virtual void allocateOutputMemory( global_reg_t const &reg, unsigned int version );
};

class SeparateAddressSpaceOutOps : public BaseOps {
   typedef std::map< SeparateMemoryAddressSpace *, TransferList > MapType;
   MapType _transfers;

   public:
   SeparateAddressSpaceOutOps();
   ~SeparateAddressSpaceOutOps();

   void addOp( SeparateMemoryAddressSpace *from, global_reg_t const &reg, unsigned int version, DeviceOps *ops );
   void issue( WD const &wd );
};

}

#endif /* MEMORYOPS_DECL */
