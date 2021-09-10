/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#include "WonSY_ReplicationPtr.h"

#include <map>
#include <iostream>
#include <utility>>

namespace WonSY::Concurrency
{
#if DEPRECATED_THREAD_ID != true
	void TestReplicationPtr_ThreadId()
	{
		using namespace std::chrono_literals;
		using Cont = std::map< std::string, int >;

		// 기본적인 사용법
		{
			// create repPtr and Attached this thread;
			WsyReplicationPtr< std::string > repPtr( []() { return new std::string( "123" ); } );
			
			{
				// 위의 생성자와 동일합니다.

				WsyReplicationPtr< std::string > repPtr;
				repPtr.Attach( []() { return new std::string( "123" ); } );
			}
			
			// Set
			{
				// 람다의 비용, 복사 비용에 따라, 아래 두 함수 중 성능이 좋은 함수가 다르다. 
				
				// Master Data의 수정을 먼저 시도하고, 성공 시 Slave Data에도 동일하게 동작시킨다.
				repPtr.Set( []( auto& data ) { data = "ABC"; return true; } );

				// Master Data에 복사 대입 후에, Slave Data에도 복사 생성한다.
				repPtr.Set( "ABC" );
			}

			// Get
			{
				// 뭔가 다 마음에 안든다..

				// Master Context일 경우, 원본 데이터를 전달, Slave Context일 경우 Lock을 걸고 Copy를 한 후, 고 놈의 컨텍스트를 넘깁니다.
				repPtr.Get( []( const auto& data ) { std::cout << data << std::endl; } );

				// 복사본을 전달합니다.
				std::cout << repPtr.GetCopy() << std::endl;

				// unique_ptr를 넘깁니다. Master Context일 경우, 원본에 대하여 전달하고, Slave Context일 경우, Lock 걸고 복사한 후 해당 데이터를 Unique_ptr로 또 만들어서 전달합니다.
				// std::cout << *( repPtr.GetPtr() ) << std::endl;
			}
		}

		// 테스트
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

		// 기본적인 사용법
		{
			WsyBroadcastPtr< TestContextKey, std::string > broadCastPtr( []() { return new std::string( "안녕!" ); } );
			
			// Get
			{
				// TestContextKey Context!
				{
					TestContextKey testContextKey;

					// ContextKey를 사용해, Locking없고, 복사없이 Const Reference로 받아옴.
					// !0. nullptr에 대한 Reference를 반환할 수 있기 때문에, Null Ref일 수 있습니다.
					const std::string& retString1 = broadCastPtr.Get( testContextKey );

					// Context 내부더라도, Lock and Copy로 가져올 수 있음. ( RVO 기대 )
					const std::string retString2 = broadCastPtr.Get();
				}

				// other Context!
				{
					// 다른 Context일 경우에는 Key가 없기 때문에, Lock and Copy만 가능하다.
					const std::string retString2 = broadCastPtr.Get();
				}
			}

			// Set
			{
				// TestContextKey Context!
				{
					TestContextKey testContextKey;

					// Master Data에 복사 대입 후에, Slave Data에도 복사 생성한다.
					broadCastPtr.Set( testContextKey, "Set By Copy!" );
					
					// Master 데이터를 참조하여 처리하고자 할 경우에는 아래와 같이 처리합니다.
					broadCastPtr.Set( testContextKey,
						[]( auto& data )
						{
							// 해당 함수에서 주의할 점은 Master 데이터를 변경하였으면 True를 반환하고, Master 데이터의 변경사항이 없을 경우 false를 리턴하는 것입니다.
							if ( data.size() > 100 )
								return false;

							if ( data.size() == 0 )
							{
								data = "123";
								return true;
							}

							data = "ABC";
							return true;
						}, BROADCAST_SYNC_TYPE::COPY );
						// 이 때 3번째 인자로 BROADCAST_SYNC_TYPE를 전달할 수 있습니다. ( Default는 BROADCAST_SYNC_TYPE::DOUBLING )
						//	두 방식 중, 성능이 더 좋은 방식은 Task의 비용, data의 복사 비용 등 상황에 따라 다릅니다.
						// 0. BROADCAST_SYNC_TYPE::COPY     : Master Data 데이터의 변경 사항이 있을 경우, Slave Data에 복사한다.
						// 1. BROADCAST_SYNC_TYPE::DOUBLING : Master Data 데이터의 변경 사항이 있을 경우, Slave Data에 동일한 동작을 수행하여 동일하게 합니다.
				}

				// other Context!
				{
					// 다른 Context일 경우에는 Write가 제한된다. Master Data에 접근할 수 있는 Thread의 Context는 해당하는 ContextKey를 가져야한다.
					broadCastPtr;
				}
			}
		}

		// 무결성 테스트
		if ( false )
		{
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
								if ( data.size() > loopCount / 2 )
									return false;

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
								const auto data     = broadCastPtr.Get();
								const int  sumValue = [ & ]()
									{
										int tempValue = 0;
										for ( const auto& ele : data )
										{
											tempValue += ele.first;
										}

										return tempValue;
									}();

								std::this_thread::sleep_for( 10ms );
							}
						} ) );
			}

			writeThread.join();
			for ( auto& th : readThreadCont ) { th.join(); }
		}

		// 성능 테스트
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
										const auto data     = broadCastPtr.Get();
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
					bool m_buffer[ 1000 ];
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
										const auto data     = broadCastPtr.Get();
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

				chekFunc( broadCastPtr, "map - DOUBLING", BROADCAST_SYNC_TYPE::DOUBLING );
				
				broadCastPtr.Set( TestContextKey(), std::map< int, TestUnit >() );

				chekFunc( broadCastPtr, "map - Copy", BROADCAST_SYNC_TYPE::COPY );
			}
		}
	}
}