//
//  Server.h
//  Test
//
//  Created by Ryan Bartley on 8/31/14.
//
//

#pragma once

#include "TcpServer.h"

#include "ServerBase.hpp"

namespace mpe {
	
using ServerRef = std::shared_ptr<class Server>;
	
class Server : public ServerBase, std::enable_shared_from_this<Server> {
public:
	static ServerRef create( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service = ci::app::App::get()->io_service(), bool thread = false );
	
	virtual void start() override;
	virtual void start( uint16_t port ) override;
	virtual void stop() override;
	virtual void update() override;
	
	struct ClientConnection {
		ClientConnection( const TcpSessionRef &session, const ServerRef &mParent );
		
		~ClientConnection();
		
		void onError( std::string error, size_t bytesTransferred );
		void onClose();
		void onRead( const ci::BufferRef &buffer );
		void onWrite( size_t bytesTransferred );
		
		void write( std::string &message );
		
		uint32_t getId() const { return mId; }
		
		std::shared_ptr<std::mutex>& getMessageMutex() { return mMessageMutex; }
		
	private:
		TcpSessionRef					mSession;
		ServerRef						mParent;
		std::string						mName;
		std::deque<std::string>			mMessages;
		std::shared_ptr<std::mutex>		mMessageMutex;
		uint64_t						mFrameNumber;
		uint32_t						mId;
		bool							mIsAsync;
		bool							mShouldReceiveData;
		
		friend class Server;
	};
	
private:
	Server( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service, bool thread );
	
	void loadSettings( const ci::DataSourceRef &jsonSettingsFile );
	
	void onAccept( TcpSessionRef session );
	void onError( std::string error, size_t bytesTransferred );
	void onCancel();
	
	using ClientConnectionRef	= std::shared_ptr<ClientConnection>;
	using Connections			= std::vector<ClientConnectionRef>;
	
	TcpServerRef			mTcpServer;
	Connections				mTcpConnections;
	uint16_t				mPort;
	
	std::deque<std::string> mMessages;
	std::mutex				mMessagesMutex;
	
	const uint16_t			mTotalAllowedConnections;
	const uint8_t			mFPSUpdate;
	bool					mIsThreaded;
};
	
}
