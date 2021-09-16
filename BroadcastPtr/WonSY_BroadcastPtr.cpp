/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#include "WonSY_BroadcastPtr.h"

#include <map>
#include <iostream>

namespace WonSY::Concurrency
{
#if DEPRECATED_THREAD_ID != true
	void TestReplicationPtr_ThreadId()
	{
		using namespace std::chrono_literals;
		using Cont = std::map< std::string, int >;

		// �⺻���� ����
		{
			// create repPtr and Attached this thread;
			WsyReplicationPtr< std::string > repPtr( []() { return new std::string( "123" ); } );
			
			{
				// ���� �����ڿ� �����մϴ�.

				WsyReplicationPtr< std::string > repPtr;
				repPtr.Attach( []() { return new std::string( "123" ); } );
			}
			
			// Set
			{
				// ������ ���, ���� ��뿡 ����, �Ʒ� �� �Լ� �� ������ ���� �Լ��� �ٸ���. 
				
				// Master Data�� ������ ���� �õ��ϰ�, ���� �� Slave Data���� �����ϰ� ���۽�Ų��.
				repPtr.Set( []( auto& data ) { data = "ABC"; return true; } );

				// Master Data�� ���� ���� �Ŀ�, Slave Data���� ���� �����Ѵ�.
				repPtr.Set( "ABC" );
			}

			// Get
			{
				// ���� �� ������ �ȵ��..

				// Master Context�� ���, ���� �����͸� ����, Slave Context�� ��� Lock�� �ɰ� Copy�� �� ��, �� ���� ���ؽ�Ʈ�� �ѱ�ϴ�.
				repPtr.Get( []( const auto& data ) { std::cout << data << std::endl; } );

				// ���纻�� �����մϴ�.
				std::cout << repPtr.GetCopy() << std::endl;

				// unique_ptr�� �ѱ�ϴ�. Master Context�� ���, ������ ���Ͽ� �����ϰ�, Slave Context�� ���, Lock �ɰ� ������ �� �ش� �����͸� Unique_ptr�� �� ���� �����մϴ�.
				// std::cout << *( repPtr.GetPtr() ) << std::endl;
			}
		}

		// �׽�Ʈ
		{
			WsyReplicationPtr< Cont > dataPtr;

			std::thread writeThread = static_cast< std::thread >( 
				[ & ]()
				{
					// init and Assign right, this Thread
					dataPtr.Attach( []() { return new Cont(); } );

					for ( int i = 0; i < 10; ++i )
					{
						int retSize;
						if ( const bool setResult = dataPtr.Set(
							[ & ]( Cont& cont )
							{
								retSize = cont.size();
								if ( retSize > 5 )
									return false;

								return ( cont.insert( { std::to_string( retSize ), retSize } ) ).second;
							} ); true )
						{
							setResult
								? std::cout << "Write - Set Success! Value : " << retSize << std::endl
								: std::cout << "Write - Set Fail! Value : " << retSize << std::endl;
						}
					
						std::this_thread::sleep_for( 100ms );
					}
				} );

			std::thread readThread = static_cast< std::thread >(
				[ & ]()
				{
					for ( int i = 0; i < 10; ++i )
					{
						dataPtr.Get(
							[ & ]( const Cont& cont )
							{
								std::cout << "Read - Value : " << cont.size() << std::endl;
							} );

						std::this_thread::sleep_for( 100ms );
					}
				} );

			std::thread writeFailThread = static_cast< std::thread >(
				[ & ]()
				{
					// Call Fail Set!!
					for ( int i = 0; i < 20; ++i )
					{
						dataPtr.Set( []( Cont& cont ) { cont.clear(); return true; } );
					
						std::this_thread::sleep_for( 50ms );
					}
				} );

			writeThread.join();
			readThread.join();
			writeFailThread.join();

			std::cout << "Finish!" << std::endl;
		}
	}
#endif

	void TestBroadcastPtr()
	{
		using namespace std::chrono_literals;
		using Cont = std::map< std::string, int >;

		struct TestContextKey{};

		// �⺻���� ����
		{
			struct TestUnit
			{
				int m_value = 0;

				TestUnit()                                 { std::cout << "default ctor" << std::endl;          }
				~TestUnit()                                { std::cout << "dtor" << std::endl;                  }
				TestUnit( const TestUnit& )                { std::cout << "Copy ctor" << std::endl;             }
				TestUnit& operator=( const TestUnit& )     { std::cout << "Copy Op" << std::endl; return *this; }
				TestUnit( TestUnit&& ) noexcept            { std::cout << "Move ctor" << std::endl;             }
				TestUnit& operator=( TestUnit&& ) noexcept { std::cout << "Move Op" << std::endl; return *this; }
			};

			WsyBroadcastPtr< TestContextKey, TestUnit > broadCastPtr( []() { return new TestUnit(); } );
			
			// Get
			{
				// TestContextKey Context!
				{
					TestContextKey testContextKey;

					// ContextKey�� �����, Locking����, ������� Const Reference�� �޾ƿ�.
					// !0. nullptr�� ���� Reference�� ��ȯ�� �� �ֱ� ������, Null Ref�� �� �ֽ��ϴ�.
					const auto& retUnit1 = broadCastPtr.Get( testContextKey );

					// Context ���δ���, Lock and Copy�� ������ �� ����.( RVO ��� ) ������ �ٸ� Context�� �ѱ� ������ �ƴ϶�� ��ȿ�����̴�. ( �ٸ� Context�� �ѱ���� key�������� ��ȿ�� �� )
					const auto retUnit2 = broadCastPtr.GetCopy();
				}

				// other Context!
				{
					// �ٸ� Context�� ��쿡�� Key�� ���� ������, Lock and Copy�� �����ϴ�.
					const auto retUnit = broadCastPtr.GetCopy();
				
					// �����͸� �������� �ʰ�, ������ ������ �ϸ� �ҰŶ��, ReadOnly��ü�� ó���Ѵ�.
					int sumValue = 0;
					broadCastPtr.RunReadOnlyTask( 
						// read lock
						[ &sumValue ]( const auto& data )
						{
							sumValue += data.m_value;
						} );
				}
			}

			// Set
			{
				// TestContextKey Context!
				{
					TestContextKey testContextKey;

					// Master Data�� ���� ���� �Ŀ�, Slave Data���� ���� �����Ѵ�.
					broadCastPtr.Set( testContextKey, TestUnit() );
					
					// Master �����͸� �����Ͽ� ó���ϰ��� �� ��쿡�� �Ʒ��� ���� ó���մϴ�.
					broadCastPtr.Set( testContextKey,
						[]( auto& data )
						{
							// �ش� �Լ����� ������ ���� Master �����͸� �����Ͽ����� True�� ��ȯ�ϰ�, Master �������� ��������� ���� ��� false�� �����ϴ� ���Դϴ�.
							if ( data.m_value > 3 )
								return false;

							if ( data.m_value == 0 )
							{
								data.m_value = 1;
								return true;
							}

							data.m_value = 2;
							return true;
						}, BROADCAST_SYNC_TYPE::DOUBLING );
						// �� �� 3��° ���ڷ� BROADCAST_SYNC_TYPE�� ������ �� �ֽ��ϴ�. ( Default�� BROADCAST_SYNC_TYPE::?? �ǳ��ٲ� )
						//	�� ��� ��, ������ �� ���� ����� Task�� ���, data�� ���� ��� �� ��Ȳ�� ���� �ٸ� ������ ����˴ϴ�.
						// 0. BROADCAST_SYNC_TYPE::COPY     : Master Data �������� ���� ������ ���� ���, Slave Data�� �����Ѵ�.
						// 1. BROADCAST_SYNC_TYPE::DOUBLING : Master Data �������� ���� ������ ���� ���, Slave Data�� ������ ������ �����Ͽ� �����ϰ� �մϴ�.
				}

				// other Context!
				{
					// �ٸ� Context�� ��쿡�� Write�� ���ѵȴ�. Master Data�� ������ �� �ִ� Thread�� Context�� �ش��ϴ� ContextKey�� �������Ѵ�.
					broadCastPtr;
				}
			}
		}

		// ���Ἲ �׽�Ʈ
		{
			std::cout << "start! ���Ἲ �׽�Ʈ " << std::endl;

			using _DataType = std::map< int, std::string >;
			WsyBroadcastPtr< TestContextKey, _DataType > broadCastPtr( nullptr );
			const int loopCount       = 1000;
			const int readThreadCount = 3;

			std::thread writeThread = static_cast< std::thread >( [ & ]()
				{
					TestContextKey testContextKey;

					for ( int i = 0; i < loopCount; ++i )
					{
						broadCastPtr.Set( testContextKey,
							[ & ]( _DataType& data )
							{
								//if ( data.size() > ( loopCount / 2 ) )
								//	return false;

								return data.insert( { i, std::to_string( i ) } ).second;
							} );

						std::this_thread::sleep_for( 10ms );
					}

					broadCastPtr.Set( testContextKey, _DataType() );
				} );

			std::vector< std::thread > readThreadCont;
			for ( int i = 0; i < readThreadCount; ++i )
			{
				readThreadCont.emplace_back( 
					static_cast< std::thread >( 
						[ & ]()
						{
							for ( int i = 0; i < loopCount; ++i )
							{
								const auto data     = broadCastPtr.GetCopy();
								const int  sumValue = [ & ]()
									{
										int tempValue = 0;
										for ( const auto& ele : data )
										{
											tempValue += ele.first;
										}

										return tempValue;
									}();
							}
						} ) );
			}

			writeThread.join();
			for ( auto& th : readThreadCont ) { th.join(); }

			std::cout << "���Ἲ �׽�Ʈ ��! " << std::endl;
		}

		// ���� �׽�Ʈ
		{
			const int loopCount       = 10000;
			const int readThreadCount = 3;

			{
				// string
				WsyBroadcastPtr< TestContextKey, std::string > broadCastPtr( nullptr );
				const auto chekFunc = [ & ]( auto& broadCastPtr, const std::string& name, const BROADCAST_SYNC_TYPE syncType )
				{
					const auto startTime = std::chrono::high_resolution_clock::now();
					
					std::cout << "start! " << name << std::endl;

					std::thread writeThread = static_cast< std::thread >( [ & ]()
						{
							TestContextKey testContextKey;

							for ( int i = 0; i < loopCount; ++i )
							{
								broadCastPtr.Set( testContextKey,
									[ & ]( auto& data )
									{
										if ( loopCount % 2 )
											return false;
										
										data = "ABCDE : " + std::to_string( loopCount );
										return true;
									}, syncType );
							}
						} );

					std::vector< std::thread > readThreadCont;
					for ( int i = 0; i < readThreadCount; ++i )
					{
						readThreadCont.emplace_back( 
							static_cast< std::thread >( 
								[ & ]()
								{
									for ( int i = 0; i < loopCount; ++i )
									{
										const auto data     = broadCastPtr.GetCopy();
										const int  sumValue = [ & ]()
											{
												int tempValue = 0;
												for ( auto ele : data )
												{
													tempValue += static_cast< int >( data.size() );
												}

												return tempValue;
											}();
									}
								} ) );
					}

					writeThread.join();
					for ( auto& th : readThreadCont ) { th.join(); }

					std::cout << "end! " << name << " : " << std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::high_resolution_clock::now() - startTime ).count() << " msecs\n";
				};

				chekFunc( broadCastPtr, "String - DOUBLING", BROADCAST_SYNC_TYPE::DOUBLING );
				
				broadCastPtr.Set( TestContextKey(), std::string() );

				chekFunc( broadCastPtr, "String - Copy", BROADCAST_SYNC_TYPE::COPY );
			}

			{
				// map
				struct TestUnit
				{
					std::array< char, 1000 > m_buffer;
				};

				WsyBroadcastPtr< TestContextKey, std::map< int, TestUnit > > broadCastPtr( nullptr );
				const auto chekFunc = [ & ]( auto& broadCastPtr, const std::string& name, const BROADCAST_SYNC_TYPE syncType )
				{
					const auto startTime = std::chrono::high_resolution_clock::now();
					
					std::cout << "start! " << name << std::endl;

					std::thread writeThread = static_cast< std::thread >( [ & ]()
						{
							TestContextKey testContextKey;

							for ( int i = 0; i < loopCount; ++i )
							{
								broadCastPtr.Set( testContextKey,
									[ & ]( auto& data )
									{
										if ( i % 2 )
											return false;
										
										return data.insert( { i, TestUnit() } ).second;
									}, syncType );
							}
						} );

					std::vector< std::thread > readThreadCont;
					for ( int i = 0; i < readThreadCount; ++i )
					{
						readThreadCont.emplace_back( 
							static_cast< std::thread >( 
								[ & ]()
								{
									for ( int i = 0; i < loopCount; ++i )
									{
										const auto data     = broadCastPtr.GetCopy();
										const int  sumValue = [ & ]()
											{
												int tempValue = 0;
												for ( const auto& ele : data )
												{
													tempValue += static_cast< int >( ele.second.m_buffer.size() );
												}

												return tempValue;
											}();
									}
								} ) );
					}

					writeThread.join();
					for ( auto& th : readThreadCont ) { th.join(); }

					std::cout << "end! " << name << " : " << std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::high_resolution_clock::now() - startTime ).count() << " msecs\n";
				};

				chekFunc( broadCastPtr, "map - DOUBLING", BROADCAST_SYNC_TYPE::DOUBLING );
				
				broadCastPtr.Set( TestContextKey(), std::map< int, TestUnit >() );

				chekFunc( broadCastPtr, "map - Copy", BROADCAST_SYNC_TYPE::COPY );

				// �� ���� �� copy�� �� ������..
			}
		}
	}
}