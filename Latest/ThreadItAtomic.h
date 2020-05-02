/*
Copyright (C) 2013 Christopher A. Greeley

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <iostream>
#include <CallItLater.h>
#define THREAD_IT_NACL_PLATFORM
//#define THREAD_IT_NACL_PLATFORM_DEBUG
#define THREAD_IT_HAS_CPP_STANDARD_ATOMIC
#ifdef THREAD_IT_NACL_PLATFORM
	#define _GLIBCXX_HAS_GTHREADS
	#define _GLIBCXX_USE_CLOCK_MONOTONIC
	#include <pthread.h>
	#include <functional>
	#include <cstdatomic>
	#include <iostream>
	#include <vector>
	#include <string>
	#include <typeinfo>
	#include <utility>
	#include <memory>
#endif
#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
	#include <ppapi/c/ppb_instance.h>
	#include <ppapi/c/pp_module.h>
	#include <ppapi/c/pp_var.h>
	#include <ppapi/c/ppb.h>
	#include <ppapi/c/ppb_input_event.h>
	#include <ppapi/cpp/instance.h>
	#include <ppapi/cpp/instance_handle.h>
	#include <ppapi/cpp/module.h>
	#include <ppapi/cpp/var.h>
	#include <ppapi/cpp/completion_callback.h>
#endif
#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
	#ifdef THREAD_IT_NACL_PLATFORM
		#include <cstdatomic>
	#else
		#include <atomic>
	#endif
#endif
#define THREAD_IT_EASY_THREAD
#include <utility>
#include <algorithm>
#include <sstream>
namespace LibThreadIt
{
	struct BaseAtomic
	{
		std::string id;
		#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
			std::string name;
			pp::Instance* debugger;
		#endif
		#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
			std::shared_ptr< std::atomic< bool > > isBusy;
		#endif
		//So the resources can be aquired without needing to know the type.//
		virtual bool AtomicAquire() = 0;
		virtual bool Release() = 0;
	};
	template< typename ATOMIC_TYPE_T >
	struct Atomic : public BaseAtomic
	{
		ATOMIC_TYPE_T* atomicData;
		bool didWrite;
		#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
			void Debug( std::string message )
			{
				std::stringstream messageBuffer;
				messageBuffer << "From atomic " << name << " with isBusy at " << isBusy->load( std::memory_order_acquire ) << 
						" with address " << isBusy.get() << " and didWrite at " << didWrite << ": " << message;
				debugger->PostMessage( messageBuffer.str() );
			}
		#endif
		explicit Atomic()
		{
			didWrite = false;
			id = typeid( ATOMIC_TYPE_T* ).name();
			#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
				isBusy = std::make_shared< std::atomic< bool > >( false );
			#endif
		}
		Atomic( const Atomic< ATOMIC_TYPE_T >& other )
		{
			didWrite = other.didWrite;
			id = other.id;
			isBusy = other.isBusy;
			atomicData = other.atomicData;
		}
		~Atomic() {
			Release();
		}
		ATOMIC_TYPE_T* operator*() {
			return Aquire();
		}
		virtual bool AtomicAquire()
		{
			/*In case the data got corrupted somewhere 
			in some thread along the line. Unlikly, but 
			why not be sure.*/
			bool status = true;
			bool expected = false;
			#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
				Debug( "waiting" );
			#endif
			if( didWrite == false )
			{
				#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
					Debug( "needed to wait" );
				#endif
				#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
					while( ( !isBusy->compare_exchange_weak( expected, true, 
							std::memory_order_release, std::memory_order_acquire ) ) )
						expected = false;
				#endif
			}
			else
				status = false;
			didWrite = true;
			#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
				Debug( "finished waiting" );
			#endif
			return status;
		}
		ATOMIC_TYPE_T* Aquire() {
			AtomicAquire();
			return atomicData;
		}
		virtual bool Release()
		{
			#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
				Debug( "releasing" );
			#endif
			/*In case the data got corrupted somewhere 
			in some thread along the line. Unlikly, but 
			why not be sure.*/
			bool status = true;
			if( didWrite == true )
			{
				#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
					Debug( "needed to release" );
				#endif
				#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
					isBusy->store( false, std::memory_order_release );
				#endif
			}
			else
				status = false;
			didWrite = false;
			#ifdef THREAD_IT_NACL_PLATFORM_DEBUG
				Debug( "released" );
			#endif
			return status;
		}
	};
}
