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
	// #0. 기존의 어떤 Context에서 Data에 직접 읽고 쓴 후, 이를 Lock을 한 후 Set하고, 다른 Context에서 읽을 때는 항상 Lock을 걸고 Copy하는 구조에 대하여, 하나의 클래스를 통한 인터페이스 제작을 목표로 함
	// !0. 기본적으로 아래의 조건을 만족할 때의 사용을 제안한다. 
	//	 - 0. Single Context Write( Master Context ) - Multi Context Read( Slave Context )
	//	 - 1. Copy 비용이 적은 Type에서의 처리.
	//	 - 2. Master Context에서의 Read Call이 많고, Write Call이 적으며, Read Context에서의 Access가 적을 때.

	// BroadcastPtr Ver 0.1 : threadId == context, thread가 고정되는 구조에서 사용할 수 있는 클래스 제작
	// BroadcastPtr Ver 0.2 : Context Key 용도의 클래스 제작
	// BroadcastPtr Ver 0.3 : threadId를 통한 방식 Deprecated 처리. 오히려 더 프로그래머의 실수를 유발하기 쉬움.
	// BroadcastPtr Ver 0.4 : 이름변경 SingleReadMultiWritePtr -> Replication -> BroadcastPtr
	// BroadcastPtr Ver 0.5 : Sub ContextKey를 발급하지 않고, 템플릿으로 이미 생성된 Context Key를 통해 처리하도록 수정
	// BroadcastPtr Ver 0.6 : SYNC_TYPE에 따라, 복사 혹은 더블링 하도록 처리, Context가 없을 때는 Get이 아닌 Copy를 사용하도록 함수명 수정
	// BroadcastPtr Ver 0.7 : Context Key를 지니지 못했더라도, Const Data Ref를 인자로 받는 Read Only Task를 통해, 복사 없이 처리할 수 있는 방안 추가

	enum class SYNC_TYPE
	{
		COPY,      // = Master 데이터를 Slave로 복사
		DOUBLING,  // = Master에 한 행동을 동일하게 Slave에 행함
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

			// 분명히 성능적으로는 손해이지만, nullptr인 상태에서의 문제가 더 크다고 생각하기 때문에, 이 부분에서 기본 생성자를 호출하여 처리를 한다.
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
			const std::function< bool/* = 마스터 데이터 변경 여부 */( _DataType& ) >& func,
			const SYNC_TYPE                                                           syncType = SYNC_TYPE::COPY )
		{
			if ( func( *m_masterData ) )
			{
				// MasterData가 변경되었을 때만, Lock을 잡고, SlaveData의 변경을 시도한다.
				
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
					// 슬레이브에 마스터 데이터와 동일한 함수를 실행했으나 실패할 경우, 강제로 카피해준다.
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
#endif

#pragma endregion
}

template < class _ContextKey, class _DataType >
using WsyBroadcastPtr = WonSY::Concurrency::BroadcastPtr< _ContextKey, _DataType >;

using BROADCAST_SYNC_TYPE = WonSY::Concurrency::SYNC_TYPE;

//template < class _Type >
//using WsyReplicationPtr = WonSY::Concurrency::ReplicationPtr_ThreadId< _Type >;