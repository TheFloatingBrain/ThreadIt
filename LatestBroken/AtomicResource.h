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
#include <ThreadItAtomic.h>

namespace LibThreadIt
{
	struct MacroAtomic {
		virtual void AquireAll() = 0;
		virtual void ReleaseAll() = 0;
	};
	/*This structure is nessisary to ensure that uneeded THREAD_HANDLES 
	are not made, while at the same time, atomics can be managed easily.
	For example: 
		void SomeFunction( THREAD_HANDLE& threadHandle ) {
			//Oops! The handle is not made yet.//
			auto atomic = MakeAtomic( &someGlobal, threadHandle );
		}
		auto handle = ThreadIt( JOIN, SomeFunction, handle );
	*/
	struct AtomicResource : public MacroAtomic
	{
		template< typename ATOMIC_TYPE_T >
		Atomic< ATOMIC_TYPE_T > Branch( ATOMIC_TYPE_T* threadSensitiveData )
		{
			std::string id = typeid( ATOMIC_TYPE_T* ).name();
			const unsigned int AMOUNT_OF_ATOMICS = atomics.size();
			for( unsigned int i = 0; i < AMOUNT_OF_ATOMICS; ++i )
			{
				if( atomics[ i ]->id.compare( id ) == 0 )
				{
					auto atomicToCopy = reinterpret_cast< 
							LibThreadIt::Atomic< ATOMIC_TYPE_T >* >( atomics[ i ].get() );
					if( atomicToCopy->atomicData == threadSensitiveData )
						return LibThreadIt::Atomic< ATOMIC_TYPE_T >( ( *atomicToCopy ) );
				}
			}
			auto newAtomic = std::make_shared< Atomic< ATOMIC_TYPE_T > >();
			newAtomic->atomicData = threadSensitiveData;
			atomics.push_back( newAtomic );
			return Atomic< ATOMIC_TYPE_T >( ( *( newAtomic.get() ) ) );
		}
		virtual void AquireAll()
		{
			const unsigned int AMOUNT_OF_ATOMICS = atomics.size();
			for( unsigned int i = 0; i < AMOUNT_OF_ATOMICS; ++i )
				atomics[ i ]->AtomicAquire();
		}
		virtual void ReleaseAll()
		{
			const unsigned int AMOUNT_OF_ATOMICS = atomics.size();
			for( unsigned int i = 0; i < AMOUNT_OF_ATOMICS; ++i )
				atomics[ i ]->Release();
		}
		std::vector< std::shared_ptr< BaseAtomic > > GetAtomics() {
			return atomics;
		}
		void SetAtomics( std::vector< std::shared_ptr< BaseAtomic > > atomics_ ) {
			atomics = atomics_;
		}
		std::vector< std::shared_ptr< BaseAtomic > >* ReferenceAtomics() {
			return &atomics;
		}
		protected: 
			std::vector< std::shared_ptr< BaseAtomic > > atomics;
	};
	#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
		typedef std::atomic< bool > AUTO_ATOMIC_TARGATE;
	#endif
	struct AutoAtomic
	{
		#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
			std::atomic< bool >* atomic;
		#endif
		explicit AutoAtomic( AUTO_ATOMIC_TARGATE* targate )
		{
			#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
				atomic = targate;
				bool expected = false;
				while( ( !atomic->compare_exchange_weak( expected, true, 
						std::memory_order_release, std::memory_order_acquire ) ) )
					expected = false;
			#endif
		}
		~AutoAtomic()
		{
			#ifdef THREAD_IT_HAS_CPP_STANDARD_ATOMIC
				atomic->store( false, std::memory_order_release );
			#endif
		}
	};
	struct AtomicManager : public MacroAtomic
	{
		AtomicResource atomicStorage;
		AUTO_ATOMIC_TARGATE stateGuard;
		template< typename ATOMIC_TYPE_T >
		Atomic< ATOMIC_TYPE_T > Branch( ATOMIC_TYPE_T* threadSensitiveData )
		{
			//Because of the uniform - initialization syntax.//
			#ifdef THREAD_IT_NACL_PLATFORM
				AutoAtomic{ &stateGuard };
			#endif
			return Atomic< ATOMIC_TYPE_T >( atomicStorage.Branch( threadSensitiveData ) );
		}
		virtual void AquireAll()
		{
			//Because of the uniform - initialization syntax.//
			#ifdef THREAD_IT_NACL_PLATFORM
				AutoAtomic{ &stateGuard };
			#endif
			atomicStorage.AquireAll();
		}
		virtual void ReleaseAll()
		{
			//Because of the uniform - initialization syntax.//
			#ifdef THREAD_IT_NACL_PLATFORM
				AutoAtomic{ &stateGuard };
			#endif
			atomicStorage.ReleaseAll();
		}
	};
	std::shared_ptr< LibThreadIt::AtomicManager > MakeAtomicResource();
}
#ifdef THREAD_IT_EASY_THREAD
	typedef std::shared_ptr< LibThreadIt::AtomicManager > ATOMIC_RESOURCE;
#endif
