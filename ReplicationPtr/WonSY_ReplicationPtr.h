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

	// !0. �̿��Դϴ�.
	// !1. �������� ������� �ʽ��ϴ�.

	// #0. ������ � Context���� Data�� ���� �а� �� ��, �̸� Lock�� �� �� Set�ϰ�, �ٸ� Context���� ���� ���� �׻� Lock�� �ɰ� Copy�ϴ� ������ ���Ͽ�, �ϳ��� Ŭ������ ���� �������̽� ������ ��ǥ�� ��
	// !0. �⺻������ �Ʒ��� ������ ������ ���� ����� �����Ѵ�. 
	//	 - 0. Single Context Write( Master Context ) - Multi Context Read( Slave Context )
	//	 - 1. Copy ����� ���� Type������ ó��.
	//	 - 2. Master Context������ Read Call�� ����, Write Call�� ������, Read Context������ Access�� ���� ��.

	// SingleReadMultiWritePtr Ver 0.1 : threadId == context, thread�� �����Ǵ� �������� ����� �� �ִ� Ŭ���� ����
	// SingleReadMultiWritePtr Ver 0.2 : Context Key �뵵�� Ŭ���� ����
	
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
				// ���α� ���
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
				// ���α� ���
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
				// ���α� ���
				return false;
			}

			if ( func( *m_masterData ) )
			{
				// MasterData�� ����Ǿ��� ����, Lock�� ���, SlaveData�� ������ �õ��Ѵ�.

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
				// �� �̰� ��εǳ�;
				return false;
			}
		};

		bool Set( _TypeConstRef data )
		{
			if ( std::this_thread::get_id() != m_threadId )
			{
				// ���α� ���
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
				// MasterData�� ����Ǿ��� ����, Lock�� ���, SlaveData�� ������ �õ��Ѵ�.

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