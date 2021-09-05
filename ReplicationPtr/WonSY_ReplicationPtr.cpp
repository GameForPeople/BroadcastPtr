/*
	Copyright 2021, Won Seong-Yeon. All Rights Reserved.
		KoreaGameMaker@gmail.com
		github.com/GameForPeople
*/

#include "WonSY_ReplicationPtr.h"

#include <map>
#include <iostream>

namespace WonSY::Concurrency
{
	void TestReplicationPtr()
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
}