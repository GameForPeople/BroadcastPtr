/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#pragma once

#define WONSY_CONCURRENCY

#include <functional>
#include <shared_mutex>

#define NODISCARD [[nodiscard]]

namespace WonSY::Concurrency
{
#pragma region [ Replication Ptr ]

	// !0. 미완입니다.
	// !1. 안정성이 보장되지 않습니다.

	// #0. 기존의 어떤 Context에서 Data에 직접 읽고 쓴 후, 이를 Lock을 한 후 Set하고, 다른 Context에서 읽을 때는 항상 Lock을 걸고 Copy하는 구조에 대하여, 하나의 클래스를 통한 인터페이스 제작을 목표로 함
	// !0. 기본적으로 아래의 조건을 만족할 때의 사용을 제안한다. 
	//	 - 0. Single Context Write( Master Context ) - Multi Context Read( Slave Context )
	//	 - 1. Copy 비용이 적은 Type에서의 처리.
	//	 - 2. Master Context에서의 Read Call이 많고, Write Call이 적으며, Read Context에서의 Access가 적을 때.

	// SingleReadMultiWritePtr Ver 0.1 : threadId == context, thread가 고정되는 구조에서 사용할 수 있는 클래스 제작
	// SingleReadMultiWritePtr Ver 0.2 : Context Key 용도의 클래스 제작
	
#define DELETE_COPY_AND_MOVE( type )      \
	type( const type& ) = delete;             \
	type& operator=( const type& ) = delete;  \
	type( type&& ) = delete;                  \
	type& operator=( type&& ) = delete;       \

	struct ContextKey { /* temp :: template < class _Type > friend struct ReplicationPtr_ContextKey;  private */ public: explicit ContextKey() = default; DELETE_COPY_AND_MOVE(ContextKey) };
	
	using ContextKeyPtr = std::unique_ptr< ContextKey >;

	template < class _Type >
	struct ReplicationPtr_ThreadId
	{
#pragma region [ Def ]

		using _TypePtr       = _Type*;
		using _TypeRef       = _Type&;
		using _TypeConstPtr  = _Type const*;
		using _TypeConstRef  = const _Type&;

#pragma endregion
#pragma region [ Public Func ]
		ReplicationPtr_ThreadId()
			: m_masterData( nullptr           )
			, m_slaveData ( nullptr           )
			, m_slaveLock (                   )
			, m_threadId  ( std::thread::id() )
		{
		}

		ReplicationPtr_ThreadId( const std::function< _TypePtr() >& func )
			: m_masterData( nullptr           )
			, m_slaveData ( nullptr           )
			, m_slaveLock (                   )
			, m_threadId  ( std::thread::id() )
		{
			Attach( func );
		}

		~ReplicationPtr_ThreadId()
		{
			if ( std::this_thread::get_id() != m_threadId )
			{
				// 경고로그 출력
			}

			if ( m_masterData ) { delete m_masterData; }

			std::lock_guard local( m_slaveLock );
			if ( m_slaveData ) { delete m_slaveData; }
		}

		void Attach( const std::function< _TypePtr() >& func )
		{
			if (
				m_threadId != std::thread::id() &&
				m_threadId != std::this_thread::get_id() )
			{
				// 경고로그 출력
				return;
			}

			std::call_once( m_flag, 
				[ & ]() 
				{ 
					m_threadId = std::this_thread::get_id();
					
					if ( func )
					{
						m_masterData = func();
					}

					_Sync();
				} );
		}

		void Get( const std::function< void( _TypeConstRef ) >& func )
		{
			if ( std::this_thread::get_id() == m_threadId )
			{
				func( *m_masterData );
			}
			else
			{
				if ( std::thread::id() == m_threadId )
					return;

				const _Type copyData = [ slaveData = m_slaveData, &slaveLock = m_slaveLock ]()
					{
						std::shared_lock localLock( slaveLock );
						return *slaveData;
					}();
				
				func( copyData );
			}
		}

		const _Type GetCopy()
		{
			if ( std::this_thread::get_id() == m_threadId )
			{
				return *m_masterData;
			}
			else
			{
				std::shared_lock localLock( m_slaveLock );
				return *m_slaveData;
			}
		};

		bool Set( const std::function< bool( _TypeRef ) >& func )
		{
			if ( std::this_thread::get_id() != m_threadId )
			{
				// 경고로그 출력
				return false;
			}

			if ( func( *m_masterData ) )
			{
				// MasterData가 변경되었을 때만, Lock을 잡고, SlaveData의 변경을 시도한다.

				if ( const bool slaveReplicateResult =
					[ & ]
					{
						std::lock_guard local( m_slaveLock );
						return func( *m_slaveData );
					}(); !slaveReplicateResult )
				{
					_Sync();
				}

				return true;
			}
			else
			{
				// 아 이게 고민되네;
				return false;
			}
		};

		bool Set( _TypeConstRef data )
		{
			if ( std::this_thread::get_id() != m_threadId )
			{
				// 경고로그 출력
				return false;
			}

			*m_masterData = data;

			_Sync();
		}
#pragma endregion

#pragma region [ Private Func ]
	private:
		void _Sync()
		{
			if ( std::this_thread::get_id() != m_threadId )
				return;

			if ( m_masterData )
			{
				auto tempPtr = new _Type( *m_masterData );
				{
					std::lock_guard local( m_slaveLock );
					std::swap( m_slaveData, tempPtr );
				}

				if ( tempPtr )
					delete tempPtr;
			}
			else
			{
				_TypePtr tempPtr = nullptr;
				{
					std::lock_guard local( m_slaveLock );
					std::swap( m_slaveData, tempPtr );
				}

				if ( tempPtr )
					delete tempPtr;
			}
		}

#pragma endregion

#pragma region [ Member Var ]
	private:
		std::thread::id   m_threadId;
		std::once_flag    m_flag;

		_TypePtr          m_masterData;

		_TypePtr          m_slaveData;
		std::shared_mutex m_slaveLock;
#pragma endregion
	};

	void TestReplicationPtr_ThreadId();

	template < class _Type >
	struct ReplicationPtr_ContextKey
	{
#pragma region [ Def ]

		using _TypePtr = _Type*;
		using _TypeRef = _Type&;
		using _TypeConstPtr = _Type const*;
		using _TypeConstRef = const _Type&;

#pragma endregion
#pragma region [ Public Func ]
		ReplicationPtr_ContextKey()
			: m_masterData( nullptr )
			, m_slaveData( nullptr )
			, m_slaveLock()
		{
		}

		~ReplicationPtr_ContextKey()
		{
			// unsafe
			if ( m_masterData ) { delete m_masterData; }

			std::lock_guard local( m_slaveLock );
			if ( m_slaveData ) { delete m_slaveData; }
		}

		NODISCARD std::optional< ContextKeyPtr > Attach( const std::function< _TypePtr() >& func )
		{
			// for NRVO
			std::optional< ContextKeyPtr > retContextKey{};

			if ( m_doOnceFlag.test_and_set() )
			{
				return retContextKey;//return std::nullopt;
			}

			retContextKey.emplace();

			if ( func )
			{
				m_masterData = func();
			}

			_Sync( *( retContextKey.value().get() ) );

			return retContextKey;
		}

		NODISCARD const _Type& Get( const ContextKey& )
		{
			return *m_masterData;
		}

		const _Type Get()
		{
			std::shared_lock localLock( m_slaveLock );
			return *m_slaveData;
		};

		void Set( const ContextKey& contextKey, _TypeConstRef data )
		{
			*m_masterData = data;

			_Sync( contextKey );
		}

		bool Set( const ContextKey& contextKey, const std::function< bool( _TypeRef ) >& func )
		{
			if ( func( *m_masterData ) )
			{
				// MasterData가 변경되었을 때만, Lock을 잡고, SlaveData의 변경을 시도한다.

				if ( const bool slaveReplicateResult =
					[ & ]
					{
						std::lock_guard local( m_slaveLock );
						return func( *m_slaveData );
					}( ); !slaveReplicateResult )
				{
					_Sync( contextKey );
				}

				return true;
			}
			else
			{
				return false;
			}
		}

#pragma endregion

#pragma region [ Private Func ]
	private:
		void _Sync( const ContextKey& )
		{
			_TypePtr tempPtr = m_masterData ? new _Type( *m_masterData ) : nullptr;
			{
				std::lock_guard local( m_slaveLock );
				std::swap( m_slaveData, tempPtr );
			}

			if ( tempPtr )
				delete tempPtr;
		}

#pragma endregion

#pragma region [ Member Var ]
	private:
		_TypePtr          m_masterData;

		_TypePtr          m_slaveData;
		std::shared_mutex m_slaveLock;

		std::atomic_flag  m_doOnceFlag;
#pragma endregion

	};

	void TestReplicationPtr_ContextKey();

#pragma endregion
}

template < class _Type >
using WsyReplicationPtr = WonSY::Concurrency::ReplicationPtr_ThreadId< _Type >;

template < class _Type >
using WsyReplicationCKPtr = WonSY::Concurrency::ReplicationPtr_ContextKey< _Type >;