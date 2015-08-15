//
//  Server.cpp
//  Test
//
//  Created by Ryan Bartley on 8/31/14.
//
//

#include "Server.h"
#include "Protocol.h"

namespace mpe {

Server::ClientConnection::ClientConnection( const TcpSessionRef &session, const ServerRef &parent )
: mSession( session ), mParent( parent ), mId( 0 ), mIsAsync( false ), mMessageMutex( new std::mutex() )
{
	mSession->connectReadEventHandler( [&]( const ci::BufferRef &buffer ) {
		auto msgs = ci::split( TcpSession::bufferToString( buffer ), Protocol::messageDelimiter() );
		
		auto msg = ci::split( msgs[0], Protocol::dataMessageDelimiter() );
		
		if( msg[0] == Protocol::CONNECT_ASYNCHRONOUS && msg.size() == 4 ) {
			mIsAsync = true;
			mId = atoi( msg[1].c_str() );
			mName = msg[2];
			mShouldReceiveData = (msg[3] == "true");
		}
		else if( msg[0] == Protocol::CONNECT_SYNCHRONOUS && msg.size() == 3 ) {
			mIsAsync = false;
			mId = atoi( msg[1].c_str() );
			mName = msg[2];
			mShouldReceiveData = true;
		}
		else {
			CI_LOG_E("This message doesn't contain what is needed" << buffer);
		}
		// Now that we have our info let's connect the normal onRead callback
		mSession->connectReadEventHandler( &ClientConnection::onRead, this );
	});
	mSession->connectCloseEventHandler( &ClientConnection::onClose, this );
	mSession->connectErrorEventHandler( &ClientConnection::onError, this );
	mSession->connectWriteEventHandler( &ClientConnection::onWrite, this );
	
	mSession->read( Protocol::messageDelimiter() );
}
	
Server::ClientConnection::~ClientConnection()
{
	if( mSession )
		mSession->close();
}
	
void Server::ClientConnection::onRead( const ci::BufferRef &buffer )
{
	std::lock_guard<std::mutex> lock( *mMessageMutex );
	auto msgs = ci::split( TcpSession::bufferToString( buffer ), Protocol::messageDelimiter() );
	mMessages.insert( mMessages.end(), msgs.begin(), msgs.end() );
}
	
void Server::ClientConnection::onWrite( size_t bytesTransferred )
{
	mSession->read( Protocol::messageDelimiter() );
}
	
void Server::ClientConnection::onClose()
{
	// TODO: Remove itself
}
	
void Server::ClientConnection::onError( std::string error, size_t bytesTransferred )
{
	
}
	
Server::Server( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service, bool thread )
: mTcpServer( TcpServer::create( service ) ), mTotalAllowedConnections( 1 ), mPort( 0 ), mFPSUpdate( 60 ), mIsThreaded( thread )
{
	loadSettings( jsonSettingsFile );
	start();
}
	
ServerRef Server::create( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service, bool thread )
{
	return ServerRef();  //new Server( jsonSettingsFile, service, thread ) );
}
	
void Server::start( uint16_t port )
{
	mPort = port;
	start();
}
	
void Server::start()
{
	mTcpServer->connectAcceptEventHandler( &Server::onAccept, this );
	mTcpServer->connectErrorEventHandler( &Server::onError, this );
	mTcpServer->connectCancelEventHandler( &Server::onCancel, this );
	
	mTcpServer->accept( mPort );
}
	
void Server::update()
{
	mFrameIsReady = false;
	
	std::map<uint32_t, std::vector<std::string>> idMessages;
	
	for( auto & client : mTcpConnections ) {
		std::lock_guard<std::mutex> lock( *client->getMessageMutex() );
		std::vector<std::string> messages( client->mMessages.size() );
		std::move( client->mMessages.begin(), client->mMessages.end(), messages.begin() );
		client->mMessages.clear();
		idMessages.emplace( std::make_pair( client->getId(), messages ) );
	}
}
	
void Server::onAccept( TcpSessionRef session )
{
	mTcpConnections.emplace_back( new ClientConnection( session, shared_from_this() ) );
	if( mTotalAllowedConnections > mTcpConnections.size() ) {
		mTcpServer->accept( mPort );
	}
	else {
		mTcpServer->cancel();
	}
}
	

	
}