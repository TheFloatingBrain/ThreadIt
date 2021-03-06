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
#include <AtomicResource.h>

namespace LibThreadIt
{
	//Interfaces for the user.//
	enum JOIN_OR_DETACH {
		JOIN = 0, 
		DETACH = 1
	};
	enum THREAD_ATOMIC_MANAGMENT {
		AQUIRE_ALL_ON_START = 0, 
		DO_NOT_AQUIRE_ALL_ON_START = 1
	};
	struct ThreadHandle : public LibThreadIt::MacroAtomic
	{
		virtual void Join() = 0;
		virtual void Detach() = 0;
		/*This does not mean the function did not run if this is false, 
		it may mean it was void.*/
		virtual bool ResultIsValid() = 0;
		//Is any data on the thread, not being volitile right now?//
		virtual bool DataIsSafe() = 0;
		virtual void AquireAll() {
			atomicPool->AquireAll();
		}
		virtual void ReleaseAll() {
			atomicPool->ReleaseAll();
		}
		template< typename RETURN_DATA_T >
		auto GetResult() -> RETURN_DATA_T {

			return dynamic_cast< CallItLater::Advanced::AppliedProcedureWithResult< 
					RETURN_DATA_T >* >( procedureToRun.get() )->GetResult();

		}
		template< typename ATOMIC_TYPE_T >
		Atomic< ATOMIC_TYPE_T > Branch( ATOMIC_TYPE_T* threadSensitiveData ) {
			return Atomic< ATOMIC_TYPE_T >( 
					atomicPool->Branch< ATOMIC_TYPE_T >( threadSensitiveData ) );
		}
		void SetAtomicPool( std::shared_ptr< LibThreadIt::AtomicManager > atomicPool_ ) {
			atomicPool = atomicPool_;
		}
		std::shared_ptr< LibThreadIt::AtomicManager > GetAtomicPool() {
			return atomicPool;
		}
		THREAD_ATOMIC_MANAGMENT GetManagementBehavior() {
			return managmentBehavior;
		}
		void SetManagmentBehavior( THREAD_ATOMIC_MANAGMENT managmentBehavior_ ) {
			managmentBehavior = managmentBehavior_;
		}
		std::shared_ptr< CallItLater::AppliedProcedure > GetProcedureToRun() {
			return procedureToRun;
		}
		void SetProcedureToRun( std::shared_ptr< CallItLater::AppliedProcedure > procedureToRun_ ) {
			procedureToRun = procedureToRun_;
		}
		protected: 
			THREAD_ATOMIC_MANAGMENT managmentBehavior;
			std::shared_ptr< LibThreadIt::AtomicManager > atomicPool;
			//The procedure to run.//
			std::shared_ptr< CallItLater::AppliedProcedure > procedureToRun;
	};
	namespace Implementation
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			//A wrapper to easily keep track of the state of the mutex.//
			class GoogleNativeClientMutex
			{
				pthread_mutex_t stateGuard;
				bool wasLocked, isValid;
				public: 
					explicit GoogleNativeClientMutex() : wasLocked( false ), isValid( false ) {
					}
					GoogleNativeClientMutex( GoogleNativeClientMutex& other ) = default;
					void Initialize() {
						isValid = true;
						pthread_mutex_init( &stateGuard, NULL );
					}
					bool Destroy()
					{
						//Sanity check.//
						if( isValid == true )
						{
							//Update the state.//
							pthread_mutex_destroy( &stateGuard );
							isValid = false;
							return ( true );
						}
						return ( false );
					}
					//'true' for success 'false' for failure.//
					bool UnLock()
					{
						//Sanity check.//
						if( wasLocked == true && isValid == true ) {
							pthread_mutex_unlock( &stateGuard );
							return ( true );
						}
						return ( false );
					}
					void Lock() {
						pthread_mutex_lock( &stateGuard );
						wasLocked = true; /*Update the stete.*/
					}
					//'true' = did lock, 'false' = did not lock.//
					bool TryLock()
					{
						//Sanity check, and return the result.//
						if( wasLocked == true && isValid == true )
							return( pthread_mutex_trylock( &stateGuard ) == 0 ? true : false );
						return ( false );
					}
					//Encapsulation.//
					bool GetIsValid() {
						return isValid;
					}
					pthread_mutex_t GetStateGuard() {
						return stateGuard;
					}
					bool GetWasLocked() {
						return wasLocked;
					}
					void SetStateGuard( pthread_mutex_t stateGuard_ ) {
						stateGuard = stateGuard_;
					}
					void SetWasLocked( bool wasLocked_ ) {
						wasLocked = wasLocked_;
					}
					void SetIsValid( bool isValid_ ) {
						isValid = isValid_;
					}
			};
			//Make things easier.//
			typedef std::shared_ptr< GoogleNativeClientMutex > SHARED_MUTEX;
			typedef GoogleNativeClientMutex MUTEX;
			/*Forward declaration, because this function needs to know about GoogleNativeClientThreadHandle, and 
			vise - versa.*/
			void* GoogleNativeClientRunOnThread( void* threadHandle );
			struct GoogleNativeClientThreadHandle : public LibThreadIt::ThreadHandle
			{
				//Nice constructor.//
				explicit GoogleNativeClientThreadHandle( JOIN_OR_DETACH threadBehavior_, 
						std::shared_ptr< ThreadHandle > root, 
						std::shared_ptr< CallItLater::AppliedProcedure > callItLaterProcedure ) : 
						threadBehavior( threadBehavior_ )
				{
					/*Continue the tree, use the passed in mutex as this thread handles mutex
					so it knows when another thread is not in action.*/
					auto castedRoot = ( ( GoogleNativeClientThreadHandle* ) root.get() );
					stateGuard = castedRoot->GetStateGuard();
					dataIsSafe = true;
					procedureToRun = callItLaterProcedure;
				}
				explicit GoogleNativeClientThreadHandle( 
						JOIN_OR_DETACH threadBehavior_, 
						std::shared_ptr< CallItLater::AppliedProcedure > callItLaterProcedure ) : 
						threadBehavior( threadBehavior_ )
				{
					//Begin the tree.//
					stateGuard = std::make_shared< GoogleNativeClientMutex >();
					//Prepare the mutex.//
					stateGuard->Initialize();
					dataIsSafe = true;
					procedureToRun = callItLaterProcedure;
				}
				~GoogleNativeClientThreadHandle()
				{
					//Manage resources.//
					if( threadBehavior == JOIN )
						Join();
					else
						Detach();
				}
				//Clean up everything, and update the state.//
				virtual void Join()
				{
					pthread_join( threadHandle, NULL );
					stateGuard->UnLock();
					dataIsSafe = true;
				}
				//Clean up everything, and update the state.//
				virtual void Detach()
				{
					pthread_detach( threadHandle );
					stateGuard->UnLock();
					dataIsSafe = true;
				}
				virtual bool DataIsSafe() {
					return dataIsSafe.load( std::memory_order_acquire );
				}
				void SetDataIsSafe( bool dataIsSafe_ ) {
					dataIsSafe.store( dataIsSafe_, std::memory_order_release );
				}
				void Run()
				{
					//No client, dont touch anything!//
					dataIsSafe.store( false, std::memory_order_release );
					//Is the mutex good? If not initialize it.//
					if( stateGuard->GetIsValid() == false )
						stateGuard->Initialize();
					/*Has it been locked before? If so, is another thread running, catch the first "ride, " 
					after another thread is finished.*/
					if( stateGuard->GetWasLocked() == false )
						stateGuard->Lock();
					else
						while( stateGuard->TryLock() == false );
					if( managmentBehavior == AQUIRE_ALL_ON_START )
						AquireAll();
					//Start the thread!//
					pthread_create( &threadHandle, NULL, 
							&GoogleNativeClientRunOnThread, ( ( void* ) this ) );
				}
				/*Execute the function on the thread, ANYTHING that needs to be protected is inside this function, 
				hence, when it is finished, all reasources can be released.*/
				void RunOnThread() {
					procedureToRun->ExecuteFunction();
				}
				virtual bool ResultIsValid() {
					return dataIsSafe.load( std::memory_order_acquire );
				}
				//Need to change the class instance, and cast to an AppliedMethod? Do it!//
				template< typename CLASS_T >
				void SetClassInstance( CLASS_T* classInstance ) {
					auto appliedMethodToRun = ( ( CallItLater::AppliedMethod< CLASS_T >* ) procedureToRun.get() );
					appliedMethodToRun->SetInstance( classInstance );
				}
				SHARED_MUTEX GetStateGuard() {
					return stateGuard;
				}
				protected: 
					//Should this join threads, or detach them by default?//
					JOIN_OR_DETACH threadBehavior;
					//Keeps track of the state of the client's data with respect to saftey.//
					std::atomic< bool > dataIsSafe;
					//Guards all data.//
					SHARED_MUTEX stateGuard;
					//The thread.//
					pthread_t threadHandle;
			};
		#endif
	}
	//To get a root.//
	template< typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > ThreadItInitialize( THREAD_ATOMIC_MANAGMENT managmentBehavior, 
			JOIN_OR_DETACH threadBehavior, RETURN_TYPE_T(* functionToRun )( ARGUMENTS_T... ), ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, CallItLater::MakeAppliedProcedure< RETURN_TYPE_T, ARGUMENTS_T... >( functionToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( std::make_shared< LibThreadIt::AtomicManager >() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	//To continue the tree.//
	template< typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > ThreadIt( THREAD_ATOMIC_MANAGMENT managmentBehavior, 
			std::shared_ptr< ThreadHandle > parent, JOIN_OR_DETACH threadBehavior, 
			RETURN_TYPE_T(* functionToRun )( ARGUMENTS_T... ), ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, parent, 
					CallItLater::MakeAppliedProcedure< RETURN_TYPE_T, ARGUMENTS_T... >( functionToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( std::make_shared< LibThreadIt::AtomicManager >() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	template< typename CLASS_T, typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > MethodThreadItInitialize( THREAD_ATOMIC_MANAGMENT managmentBehavior, 
			CLASS_T* classInstance, JOIN_OR_DETACH threadBehavior, 
			RETURN_TYPE_T(CLASS_T::* methodToRun )( ARGUMENTS_T... ), 
			ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, CallItLater::MakeAppliedMethod< CLASS_T, RETURN_TYPE_T, ARGUMENTS_T... >( 
					classInstance, methodToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( std::make_shared< LibThreadIt::AtomicManager >() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	template< typename CLASS_T, typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > MethodThreadIt( THREAD_ATOMIC_MANAGMENT managmentBehavior, 
			std::shared_ptr< ThreadHandle > parent, CLASS_T* classInstance, JOIN_OR_DETACH threadBehavior, 
			RETURN_TYPE_T(CLASS_T::* methodToRun )( ARGUMENTS_T... ), ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, parent, 
					CallItLater::MakeAppliedMethod< CLASS_T, RETURN_TYPE_T, ARGUMENTS_T... >( 
					classInstance, methodToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( std::make_shared< LibThreadIt::AtomicManager >() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	//////////////////////////////
		template< typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > ThreadItInitializeWithPool( std::shared_ptr< LibThreadIt::AtomicManager > atomicPool, 
			THREAD_ATOMIC_MANAGMENT managmentBehavior, JOIN_OR_DETACH threadBehavior, RETURN_TYPE_T(* functionToRun )( ARGUMENTS_T... ), ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, CallItLater::MakeAppliedProcedure< RETURN_TYPE_T, ARGUMENTS_T... >( functionToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( LibThreadIt::MakeAtomicResource() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	//To continue the tree.//
	template< typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > ThreadItWithPool( THREAD_ATOMIC_MANAGMENT managmentBehavior, 
			std::shared_ptr< LibThreadIt::AtomicManager > atomicPool, 
			std::shared_ptr< ThreadHandle > parent, JOIN_OR_DETACH threadBehavior, 
			RETURN_TYPE_T(* functionToRun )( ARGUMENTS_T... ), ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, parent, 
					CallItLater::MakeAppliedProcedure< RETURN_TYPE_T, ARGUMENTS_T... >( functionToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( LibThreadIt::MakeAtomicResource() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	template< typename CLASS_T, typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > MethodThreadItInitializeWithPool( 
			THREAD_ATOMIC_MANAGMENT managmentBehavior, std::shared_ptr< LibThreadIt::AtomicManager > atomicPool, 
			CLASS_T* classInstance, JOIN_OR_DETACH threadBehavior, 
			RETURN_TYPE_T(CLASS_T::* methodToRun )( ARGUMENTS_T... ), 
			ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, CallItLater::MakeAppliedMethod< CLASS_T, RETURN_TYPE_T, ARGUMENTS_T... >( 
					classInstance, methodToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( LibThreadIt::MakeAtomicResource() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	template< typename CLASS_T, typename RETURN_TYPE_T, typename... ARGUMENTS_T >
	std::shared_ptr< LibThreadIt::ThreadHandle > MethodThreadItWithPool( 
			THREAD_ATOMIC_MANAGMENT managmentBehavior, std::shared_ptr< LibThreadIt::AtomicManager > atomicPool, 
			std::shared_ptr< ThreadHandle > parent, CLASS_T* classInstance, JOIN_OR_DETACH threadBehavior, 
			RETURN_TYPE_T(CLASS_T::* methodToRun )( ARGUMENTS_T... ), ARGUMENTS_T... arguments )
	{
		#ifdef THREAD_IT_NACL_PLATFORM
			auto threadHandle = std::make_shared< LibThreadIt::Implementation::GoogleNativeClientThreadHandle >( 
					threadBehavior, parent, 
					CallItLater::MakeAppliedMethod< CLASS_T, RETURN_TYPE_T, ARGUMENTS_T... >( 
					classInstance, methodToRun, arguments... ) );
			threadHandle->SetManagmentBehavior( managmentBehavior );
			threadHandle->SetAtomicPool( LibThreadIt::MakeAtomicResource() );
			threadHandle->Run();
			return threadHandle;
		#endif
	}
	template< typename ATOMIC_TYPE_T >
	LibThreadIt::Atomic< ATOMIC_TYPE_T > MakeAtomic( std::shared_ptr< LibThreadIt::ThreadHandle > handle, ATOMIC_TYPE_T* data ) {
		LibThreadIt::Atomic< ATOMIC_TYPE_T > atomic( handle->Branch( data ) );
		return atomic;
	}
	template< typename ATOMIC_TYPE_T >
	LibThreadIt::Atomic< ATOMIC_TYPE_T > MakeAtomic( std::shared_ptr< LibThreadIt::AtomicManager > resource, ATOMIC_TYPE_T* data ) {
		LibThreadIt::Atomic< ATOMIC_TYPE_T > atomic( resource->Branch( data ) );
		return atomic;
	}
}
#ifdef THREAD_IT_EASY_THREAD
	typedef std::shared_ptr< LibThreadIt::ThreadHandle > THREAD_HANDLE;
#endif
