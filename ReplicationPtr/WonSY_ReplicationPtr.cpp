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
}