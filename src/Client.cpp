//
//  Client.cpp
//  Test
//
//  Created by Ryan Bartley on 8/30/14.
//
//

#include "cinder/app/App.h"
#include "cinder/Json.h"
#include "Protocol.h"

#include <boost/signals2/shared_connection_block.hpp>

#include "Client.h"

using namespace std;
using namespace ci;
using namespace ci::app;

namespace mpe {
	
Client::Client( const DataSourceRef &jsonSettingsFile, boost::asio::io_service &service, bool thread )
: ClientBase(), mIsConnected(false), mPort( 0 ), mHostname( "" ),
	mTcpClient( TcpClient::create( service ) ),
	mIsThreaded( thread ), mMessageMutex( make_shared<std::mutex>() ),
	mLastFrameConfirmed( 0 ), mClientName( "" ), mClientID( 0 ),
	mIsAsync( false ), mAsyncReceivesData( false )
{
	loadSettings( jsonSettingsFile );
	
	mAppUpdateConnection = ci::app::App::get()->getSignalUpdate().connect( std::bind( &Client::update, this ) );
	
	start();
}
	
Client::~Client()
{
	stop();
}
	
ClientRef Client::create( const ci::DataSourceRef &jsonSettingsFile, boost::asio::io_service &service, bool thread )
{
	return ClientRef( new Client( jsonSettingsFile, service, thread ) );
}
	
void Client::setupCamera( const ClientRef &client, ci::CameraPersp &cam, float zPosition )
{
	auto masterSize = client->getMasterSize();
	auto viewportRect = client->getVisibleRect();
	auto localSize = viewportRect.getSize();
	auto localOrigin = vec2( viewportRect.x1, viewportRect.y1 );
	
	cam.setAspectRatio( localSize.x / localSize.y );
	
	vec3 eye( masterSize.x / 2.0f, masterSize.y / 2.0f, zPosition );
	vec3 target( masterSize.x / 2.0f, masterSize.y / 2.0f, 0);
	vec3 up( 0.0f, -1.0f, 0.0f );
	cam.lookAt(eye, target, up);
	
	float horizCenterMaster = masterSize.x / 2.0f;
	float horizCenterView = localOrigin.x + ( localSize.x * 0.5f );
	float horizPxShift = horizCenterMaster - horizCenterView;
	float horizOffset = ( horizPxShift / localSize.x ) * -2.0f;
	
	float vertCenterMaster = masterSize.y / 2.0f;
	float vertCenterView = localOrigin.y + ( localSize.y * 0.5);
	float vertPxShift = vertCenterMaster - vertCenterView;
	float vertOffset = ( vertPxShift / localSize.y ) * 2.0f;
	
	cam.setLensShift(horizOffset, vertOffset);
}
	
ci::mat4 Client::getClientModelTransform( const ClientRef &client )
{
	
}
	
void Client::start( const std::string &hostname, uint16_t port )
{
	mHostname = hostname;
	mPort = port;
	start();
}
	
void Client::stop()
{
	mIsConnected = false;
	if( mTcpSession ) {
		mTcpSession->close();
		mTcpSession.reset();
	}
}

void Client::update()
{
	mFrameIsReady = false;
	
	if ( isConnected() ) {
		{
			std::lock_guard<std::mutex> guard( *mMessageMutex );
			if ( mMessages.size() ) {
				// There may be more than 1 message in the read.
				while( ! mMessages.empty() ) {
					auto & message = mMessages.front();
					if ( message.size() > 0) {
						CI_LOG_V("I'm about to parse a message");
						Protocol::parseClient( message, this );
					}
					CI_LOG_V("Current message" + message );
					mMessages.pop_front();
				}
				
			}
		}
		
		
		if ( mFrameIsReady && ! mIsAsync ) {
			// You always need an updateCallback if synchronous.
			CI_ASSERT( mUpdateCallback );
			CI_LOG_V("I'm updating the current frame.");
			mUpdateCallback( getCurrentRenderFrame() );
		}
	}
	else {
//		if( mTcp ) {
//			start();
//		}
	}
}
	
void Client::togglePause()
{
	auto msg = Protocol::togglePause();
	mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}

void Client::resetAll()
{
	auto msg = Protocol::reset();
	mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}
	
void Client::sendMessage( const std::string &message )
{
	auto msg = Protocol::dataMessage( message );
	mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}

void Client::sendMessage( const std::string &message, const std::vector<uint32_t> &clientIds )
{
	auto msg = Protocol::dataMessage( message, clientIds );
	mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}
	
void Client::doneRendering()
{
	if( mTcpSession ) {
		if( mLastFrameConfirmed < mCurrentRenderFrame ) {
			CI_LOG_V("Confirming done with render");
			auto msg = Protocol::renderComplete( mClientID, mCurrentRenderFrame );
			mTcpSession->write( TcpSession::stringToBuffer( msg ) );
			mLastFrameConfirmed = mCurrentRenderFrame;
		}
	}
}
	
void Client::loadSettings( const ci::DataSourceRef &settingsJsonFile )
{
	JsonTree settingsDoc = JsonTree(settingsJsonFile).getChild( "settings" );
	cout << settingsDoc << endl;
	try {
		JsonTree node = settingsDoc.getChild( "asynchronous" );
		mIsAsync = node.getValue<bool>();
	}
	catch (JsonTree::ExcChildNotFound e) {
		CI_LOG_V("No asynchronous flag set, assuming false");
		mIsAsync = false;
	}
	
	if ( mIsAsync ) {
		try {
			JsonTree node = settingsDoc.getChild( "asynchreceive" );
			mAsyncReceivesData = node.getValue<bool>();
			 
		}
		catch ( JsonTree::ExcChildNotFound e ) {
			CI_LOG_V("No asynchreceive flag set, assuming false");
			mAsyncReceivesData = false;
		}
	}
	
	try {
		JsonTree clientId = settingsDoc.getChild( "client_id" );
		mClientID = clientId.getValue<int>();
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		CI_LOG_E("Could not find client ID.\n");
	}
	
	try
	{
		JsonTree node = settingsDoc.getChild( "name" );
		mClientName = node.getValue<string>();
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		CI_LOG_V("No Client Name provided, using default");
		if ( mIsAsync ) {
			mClientName = "Async client " + std::to_string(mClientID);
		}
		else {
			mClientName = "Sync client " + std::to_string(mClientID);
		}
	}
	
	try {
		JsonTree server = settingsDoc.getChild( "server" );
		mHostname = server["ip"].getValue<string>();
		mPort = server["port"].getValue<uint16_t>();
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		CI_LOG_E( "Could not find server and port settings.\n" );
	}
	
	try {
		JsonTree localDimensions = settingsDoc["local_dimensions"];
		JsonTree localLocation = settingsDoc["local_location"];
		uint32_t width = localDimensions["width"].getValue<uint32_t>();
		uint32_t height = localDimensions["height"].getValue<uint32_t>();
		int x = localLocation["x"].getValue<int>();
		int y = localLocation["y"].getValue<int>();
		mLocalViewportRect = Rectf( x, y, x+width, y+height );
		
		// Force the window size based on the settings XML.
		ci::app::setWindowSize( width, height );
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		if ( !mIsAsync ) {
			// Async controller doesn't need to know about the dimensions
			CI_LOG_E(e.what() << " Could not find local dimensions settings for synchronous client.\n");
		}
	}
	
	try {
		JsonTree masterDimension = settingsDoc.getChild( "master_dimensions" );
		uint32_t width = masterDimension["width"].getValue<uint32_t>();
		uint32_t height = masterDimension["height"].getValue<uint32_t>();
		mMasterSize = ivec2(width, height);
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		if ( !mIsAsync ) {
			// Async controller doesn't need to know about the dimensions
			CI_LOG_E(e.what() << " Could not find master dimensions settings, for synchronous client.\n");
		}
	}
	
	try {
		JsonTree fullscreenNode = settingsDoc.getChild("go_fullscreen");
		bool boolFull = fullscreenNode.getValue<bool>();
		if ( boolFull ) {
			ci::app::setFullScreen(true);
		}
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		// Not required
		CI_LOG_V("No 'go_fullscreen' flag set. Not required.");
	}
	
	try {
		JsonTree offset = settingsDoc.getChild("offset_window");
		bool boolOff = offset.getValue<bool>();
		if ( boolOff ) {
			ci::app::setWindowPos( ivec2( mLocalViewportRect.x1, mLocalViewportRect.y1 ) );
		}
	}
	catch ( JsonTree::ExcChildNotFound e ) {
		// Not required
		CI_LOG_V("No 'offset_window' flag set. Not required.");
	}
}

void Client::start()
{
	if( mIsConnected ) {
		stop();
	}
	
	mTcpClient->connectConnectEventHandler( &Client::onConnect, this );
	mTcpClient->connectErrorEventHandler( &Client::onError, this );
	
	CI_LOG_V("Connecting");
	mTcpClient->connect( mHostname, mPort );
}
	
void Client::onConnect( TcpSessionRef session )
{
	CI_LOG_V( "Established Connection with " << mHostname << " on " << mPort );
	
	mTcpSession = session;
	mIsConnected = true;
	
	auto weak = std::weak_ptr<Client>( shared_from_this() );
	
	mTcpSession->connectCloseEventHandler( std::bind( []( std::weak_ptr<Client> &weakInst ){
		auto sharedInst = weakInst.lock();
		if( sharedInst ) {
			CI_LOG_I("Connection closed");
			sharedInst->mIsConnected = false;
		}
	}, std::move(weak) ) );
	
	mTcpSession->connectErrorEventHandler( &Client::onError, this );
	mTcpSession->connectReadEventHandler( &Client::onRead, this );
	mTcpSession->connectWriteEventHandler( &Client::onWrite, this );
	
	sendClientId();
}
	
void Client::onRead( ci::Buffer buffer )
{
	std::lock_guard<std::mutex> guard( *mMessageMutex );
	auto msg = TcpSession::bufferToString( buffer );
	CI_LOG_V("Received a message " << msg);
	auto msgs = ci::split( msg, Protocol::messageDelimiter() );
	mMessages.push_back( msgs[0] );
}
	
void Client::onWrite( size_t bytesTransferred )
{
	CI_LOG_V( bytesTransferred << " Bytes Transferred" );
	mTcpSession->read( Protocol::messageDelimiter() );
}
	
void Client::onError( std::string err, size_t bytesTransferred )
{
	CI_LOG_E( err << " Bytes Transferred: " << bytesTransferred );
}

void Client::sendClientId()
{
	if (mIsAsync) {
		auto msg = Protocol::asyncClientID( mClientID, mClientName, mAsyncReceivesData );
		mTcpSession->write( TcpSession::stringToBuffer( msg ) );
	}
	else {
		auto msg = Protocol::syncClientID( mClientID, mClientName );
		mTcpSession->write( TcpSession::stringToBuffer( msg ) );
	}
}
	
void Client::setCurrentRenderFrame( uint64_t frameNum )
{
	MessageHandler::setCurrentRenderFrame( frameNum );
	// mLastFrameConfirmed has to reset when the current render frame is.
	CI_LOG_V("Frame Numbers: Last: " << mLastFrameConfirmed << " Current: " << mCurrentRenderFrame );
	mLastFrameConfirmed = mCurrentRenderFrame - 1;
}
	
void Client::receivedResetCommand()
{
	CI_LOG_V("Received Reset command, Current Frame number: " << mCurrentRenderFrame );
	if( mResetCallback )
		mResetCallback();
}
	
void Client::receivedStringMessage( const std::string &dataMessage, const uint32_t fromClientId )
{
	if( mDataMessageCallback )
		mDataMessageCallback( dataMessage, fromClientId );
}
	


}