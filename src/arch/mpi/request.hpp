
#ifndef REQUEST_H
#define REQUEST_H

#include "status.hpp"

#include <mpi.h>

namespace nanos {
namespace mpi {

class request;

inline bool test_impl( request &req );

template < StatusKind kind >
inline bool test_impl( request &req, status<kind> &st );

class request
{
	private:
		MPI_Request _value;

	public:
		typedef MPI_Request value_type;
	
		request() :
			_value( MPI_REQUEST_NULL )
		{
		}
	
		request( request &o ) :
			_value( o._value )
		{
		}

		request( MPI_Request value ) :
			_value( value )
		{
		}

		/**
		 * Destructor for class request
		 * Note: it cannot be virtual.
		 * Otherwise this class will not be standard
		 * layout and can not be represented in memory
		 * just by the MPI_Request itself (there will be
		 * gaps or some extra things that the compiler will
		 * need for rtti stuff).
		 */
		~request()
		{
		}
	
		request& operator=( value_type other )
		{
			_value = other;
			return *this;
		}
	
		/**
		 * Transfers MPI_Request ownership
		 */
		request& operator=( request &other )
		{
			_value = other._value;
			other._value = MPI_REQUEST_NULL;
			return *this;
		}
	
		bool test()
		{
			int flag;
			MPI_Test(
						&_value,
						&flag,
						MPI_STATUS_IGNORE
					);
			return flag == 1;
		}

		bool isNull() const
		{
			return _value == MPI_REQUEST_NULL;
		}
	
		value_type* data()
		{
			return &_value;
		}
	
		operator value_type () const
		{
			return _value;
		}

		operator value_type& ()
		{
			return _value;
		}

		operator value_type* ()
		{
			return &_value;
		}

		template <typename Iterator>
		static void wait_all( Iterator begin, Iterator end );

		template <typename Iterator>
		static bool test_all( Iterator begin, Iterator end );
};

class persistent_request : public request
{
	public:
		persistent_request() :
			request()
		{
		}

		persistent_request( request const& o ) :
			request( o )
		{
		}

		persistent_request( persistent_request const& o ) :
			request( o )
		{
		}

		/**
		 * This destructor cannot be virtual
		 * \see request::~request()
		 */
		~persistent_request()
		{
		}

		void free()
		{
			MPI_Request_free( data() );
		}

		void start()
		{
			MPI_Start( data() );
		}

		void cancel()
		{
			MPI_Cancel( data() );
		}

		operator MPI_Request* ()
		{
			return request::data();
		}

		template< typename Iterator >
		static void start_all( Iterator begin, Iterator end );
};

template < typename Iterator >
void request::wait_all( Iterator begin, Iterator end )
{
	std::vector<MPI_Request> requests( begin, end );
	MPI_Waitall( requests.size(), &requests[0], MPI_STATUSES_IGNORE );
}

template < typename Iterator >
bool request::test_all( Iterator begin, Iterator end )
{
	int flag;
	std::vector<MPI_Request> requests( begin, end );
	MPI_Testall( requests.size(), &requests[0], &flag, MPI_STATUSES_IGNORE );
	return flag == 1;
}

template < typename Iterator >
void persistent_request::start_all( Iterator begin, Iterator end )
{
	std::vector<MPI_Request> requests( begin, end );
	MPI_Startall( requests.size(), &requests[0] );
}

template <>
void persistent_request::start_all( std::vector<persistent_request>::iterator begin,
                                    std::vector<persistent_request>::iterator end )
{
	size_t size = std::distance(begin,end);
	MPI_Startall( size, *begin );
}

} // namespace mpi
} // namespace nanos

#endif // REQUEST_H
