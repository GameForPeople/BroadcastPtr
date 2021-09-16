/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#pragma once

#define WONSY_CONCURRENCY

#include <functional>
#include <shared_mutex>

#define NODISCARD            [[nodiscard]]
#define DEPRECATED_THREAD_ID true

namespace WonSY::Concurrency
{
#pragma region [ BroadcastPtr ]
	// #0. ������ � Context���� Data�� ���� �а� �� ��, �̸� Lock�� �� �� Set�ϰ�, �ٸ� Context���� ���� ���� �׻� Lock�� �ɰ� Copy�ϴ� ������ ���Ͽ�, �ϳ��� Ŭ������ ���� �������̽� ������ ��ǥ�� ��
	// !0. �⺻������ �Ʒ��� ������ ������ ���� ����� �����Ѵ�. 
	//	 - 0. Single Context Write( Master Context ) - Multi Context Read( Slave Context )
	//	 - 1. Copy ����� ���� Type������ ó��.
	//	 - 2. Master Context������ Read Call�� ����, Write Call�� ������, Read Context������ Access�� ���� ��.

	// BroadcastPtr Ver 0.1 : threadId == context, thread�� �����Ǵ� �������� ����� �� �ִ� Ŭ���� ����
	// BroadcastPtr Ver 0.2 : Context Key �뵵�� Ŭ���� ����
	// BroadcastPtr Ver 0.3 : threadId�� ���� ��� Deprecated ó��. ������ �� ���α׷����� �Ǽ��� �����ϱ� ����.
	// BroadcastPtr Ver 0.4 : �̸����� SingleReadMultiWritePtr -> Replication -> BroadcastPtr
	// BroadcastPtr Ver 0.5 : Sub ContextKey�� �߱����� �ʰ�, ���ø����� �̹� ������ Context Key�� ���� ó���ϵ��� ����
	// BroadcastPtr Ver 0.6 : SYNC_TYPE�� ����, ���� Ȥ�� ���� �ϵ��� ó��, Context�� ���� ���� Get�� �ƴ� Copy�� ����ϵ��� �Լ��� ����
	// BroadcastPtr Ver 0.7 : Context Key�� ������ ���ߴ���, Const Data Ref�� ���ڷ� �޴� Read Only Task�� ����, ���� ���� ó���� �� �ִ� ��� �߰�

	enum class SYNC_TYPE
	{
		COPY,      // = Master �����͸� Slave�� ����
		DOUBLING,  // = Master�� �� �ൿ�� �����ϰ� Slave�� ����
	};

	template < class _ContextKeyType, class _DataType >
	class BroadcastPtr
	{
#pragma region [ Def ]
#pragma endregion

#pragma region [ Public Func ]
	public:
		BroadcastPtr( const std::function< _DataType*() >& initFunc /*= nullptr*/ )
			: m_masterData( nullptr )
			, m_slaveData ( nullptr )
			, m_slaveLock (         )
		{
			// multi-thread safe?
			
			if ( initFunc )
			{
				m_masterData = initFunc();
				m_slaveData  = m_masterData ? new _DataType( *m_masterData ) : nullptr;
			}

			// �и��� ���������δ� ����������, nullptr�� ���¿����� ������ �� ũ�ٰ� �����ϱ� ������, �� �κп��� �⺻ �����ڸ� ȣ���Ͽ� ó���� �Ѵ�.
			if ( !m_masterData )
			{
				m_masterData = new _DataType();
				m_slaveData  = new _DataType( *m_masterData );
			}
		}

		~BroadcastPtr()
		{
			// unsafe
			if ( m_masterData ) { delete m_masterData; }

			std::lock_guard local( m_slaveLock );
			if ( m_slaveData ) { delete m_slaveData; }
		}

		NODISCARD const _DataType& Get( const _ContextKeyType& )
		{
			return *m_masterData;
		}

		const _DataType GetCopy()
		{
			std::shared_lock localLock( m_slaveLock );
			
			// copy!!
			return m_slaveData ? *m_slaveData : _DataType();
		};

		const void RunReadOnlyTask( const std::function< void( const _DataType& ) >& func )
		{
			std::shared_lock localLock( m_slaveLock );
			func( *m_slaveData );
		}

		void Set( const _ContextKeyType& contextKey, const _DataType& data )
		{
			*m_masterData = data;
			_CopyMasterToSlave( contextKey );
		}

		bool Set( 
			const _ContextKeyType&                                                    contextKey,
			const std::function< bool/* = ������ ������ ���� ���� */( _DataType& ) >& func,
			const SYNC_TYPE                                                           syncType = SYNC_TYPE::COPY )
		{
			if ( func( *m_masterData ) )
			{
				// MasterData�� ����Ǿ��� ����, Lock�� ���, SlaveData�� ������ �õ��Ѵ�.
				
				if ( syncType == SYNC_TYPE::COPY )
				{
					_CopyMasterToSlave( contextKey );
					return true;
				}
				else if ( const bool slaveReplicateResult =
					[ & ]
					{
						std::lock_guard local( m_slaveLock );
						return func( *m_slaveData );
					}(); !slaveReplicateResult )
				{
					// �����̺꿡 ������ �����Ϳ� ������ �Լ��� ���������� ������ ���, ������ ī�����ش�.
					_CopyMasterToSlave( contextKey );
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
		void _CopyMasterToSlave( const _ContextKeyType& )
		{
			_DataType* tempPtr = m_masterData ? new _DataType( *m_masterData ) : nullptr;
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
		_DataType*        m_masterData;

		_DataType*        m_slaveData;
		std::shared_mutex m_slaveLock;
#pragma endregion

	};

	void TestBroadcastPtr();

#if DEPRECATED_THREAD_ID != true
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
#endif

#pragma endregion
}

template < class _ContextKey, class _DataType >
using WsyBroadcastPtr = WonSY::Concurrency::BroadcastPtr< _ContextKey, _DataType >;

using BROADCAST_SYNC_TYPE = WonSY::Concurrency::SYNC_TYPE;

//template < class _Type >
//using WsyReplicationPtr = WonSY::Concurrency::ReplicationPtr_ThreadId< _Type >;