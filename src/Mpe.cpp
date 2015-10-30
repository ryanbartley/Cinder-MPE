#include "Mpe.h"
#include "cinder/Json.h"

using namespace ci;
using namespace ci::app;
using namespace asio;
using namespace asio::ip;
using namespace std;

namespace mpe {

namespace protocol {

std::string cleanMessage( std::string message )
{
	// Make sure the delimiter isn't in the message
	if( message.find( dataMessageDelimiter() ) != std::string::npos ) {
		CI_LOG_W( dataMessageDelimiter()
				  << "' are not allowed in broadcast messages."
				  << " Replacing with an underscore.\n" );
		std::replace( message.begin(), message.end(), dataMessageDelimiter().at( 0 ), '_' );
	}
	if( message.find( messageDelimiter() ) != std::string::npos ) {
		std::string termID = "Newlines";
		if( messageDelimiter() != "\n" ) {
			termID = "'" + messageDelimiter() + "'";
		}
		CI_LOG_W( termID
				  << " are not allowed in broadcast messages."
				  << " Replacing with an underscore.\n" );
		std::replace( message.begin(), message.end(), dataMessageDelimiter().at( 0 ), '_' );
	}
	return std::move( message );
}

std::string syncClientID( const int clientID )
{
	return syncClientID( clientID, "Rendering Client " + std::to_string( clientID ) );
};

std::string syncClientID( int clientID, const std::string & name )
{
	return CONNECT_SYNCHRONOUS +
		dataMessageDelimiter() +
		std::to_string( clientID ) +
		dataMessageDelimiter() +
		name +
		messageDelimiter();
};

std::string asyncClientID( int clientID )
{
	return asyncClientID( clientID, "Non-Rendering Client " + std::to_string( clientID ) );
}

std::string asyncClientID( int clientID,
						   const std::string & clientName,
						   bool shouldReceiveDataMessages = false )
{
	return CONNECT_ASYNCHRONOUS +
		dataMessageDelimiter() +
		std::to_string( clientID ) +
		dataMessageDelimiter() +
		clientName +
		dataMessageDelimiter() +
		( shouldReceiveDataMessages ? "true" : "false" ) +
		messageDelimiter();
};

std::string renderComplete( uint32_t clientID, uint64_t frameNum )
{
	return DONE_RENDERING +
		dataMessageDelimiter() +
		std::to_string( clientID ) +
		dataMessageDelimiter() +
		std::to_string( frameNum ) +
		messageDelimiter();
};

std::string reset()
{
	return RESET_ALL +
		messageDelimiter();
}

std::string togglePause()
{
	return TOGGLE_PAUSE +
		messageDelimiter();
};

std::string dataMessage( const std::string & msg )
{
	return dataMessage( msg, std::vector<uint32_t>() );
};

std::string dataMessage( const std::string & msg, const std::vector<uint32_t> & toClientIDs )
{
	std::string sanitizedMessage = cleanMessage( msg );
	std::string sendMessage = DATA_MESSAGE + dataMessageDelimiter() + sanitizedMessage;
	for( int i = 0; i < toClientIDs.size(); ++i ) {
		if( i == 0 ) {
			sendMessage += dataMessageDelimiter();
		}
		else {
			sendMessage += ",";
		}
		sendMessage += std::to_string( toClientIDs[i] );
	}
	sendMessage += messageDelimiter();
	return sendMessage;
};

const std::string& messageDelimiter()
{
	return kMessageTerminus;
}

const std::string& dataMessageDelimiter()
{
	return kDataMessageDelimiter;
}

std::string nextFrame( uint64_t frameNum )
{
	return NEXT_FRAME +
		std::to_string( frameNum );
}

void parseClient( const std::string & serverMessage, ClientMessageHandler *handler )
{
	// Example server messages:
	// 1) G|19919|fromID,blahblahblah
	// 2) G|7
	// 3) G|21|fromID,blah1234|fromID,blah210|fromID,blah345623232
	//
	// Format:
	// [command]|[frame count]|[data message(s)]...
	//
	// • Command is always NEXT_FRAME ("G") in v.2
	//
	// • Data Messages will start with the senders Client ID followed by a comma.
	//

	std::vector<std::string> tokens = ci::split( serverMessage, dataMessageDelimiter() );
	std::string command = tokens[0];

	if( command == protocol::RESET_ALL ) {
		handler->setCurrentRenderFrame( 1 );
		handler->receivedResetCommand();
	}
	else if( command == protocol::NEXT_FRAME ) {
		std::string frameNum = tokens[1];
		handler->setCurrentRenderFrame( stol( frameNum ) );

		if( tokens.size() > 2 ) {
			// There are additional client messages
			for( int i = 2; i < tokens.size(); i++ ) {
				// Iterate over the messages and send them out
				std::string dataMessage = tokens[i];
				size_t firstComma = dataMessage.find_first_of( "," );
				if( firstComma != std::string::npos ) {
					int clientID = stoi( dataMessage.substr( 0, firstComma ) );
					std::string messageData = dataMessage.substr( firstComma + 1 );
					handler->receivedStringMessage( messageData, clientID );
				}
				else {
					CI_LOG_E( "Couldn't parse data message " << dataMessage );
				}
			}
		}

		handler->setFrameIsReady( true );
	}
	else {
		CI_LOG_E( "Don't know what to do with server message: " << serverMessage );
		return;
	}
}

void parseServer( const std::string &clientMessage, ServerMessageHandler *handler )
{

}

} // namespace Protocol

MessageHandler::MessageHandler() :
mCurrentRenderFrame( 1 ), mFrameIsReady( false ),
mAvgUpdateDuration( 0 ), mTimeLastMessage( 0 ),
mUpdateSampleInterval( 5 )
{
}

void MessageHandler::setFrameIsReady( bool isFrameReady )
{
	mFrameIsReady = isFrameReady;
	if( mCurrentRenderFrame % mUpdateSampleInterval == 0 ) {
		calculateDFPS();
	}
}

void MessageHandler::calculateDFPS()
{
	double now = cinder::app::getElapsedSeconds();
	double frameDuration = ( now - mTimeLastMessage ) / mUpdateSampleInterval;
	if( frameDuration > 0 ) {
		mAvgUpdateDuration = ( mAvgUpdateDuration * 0.9 ) + ( frameDuration * 0.1 );
	}
	else {
		mAvgUpdateDuration = frameDuration;
	}

	mTimeLastMessage = now;
}

Client::Client( const DataSourceRef &jsonSettingsFile, asio::io_service &service )
: ClientBase(), mIsConnected( false ), mPort( 0 ), mHostname( "" ),
	mTcpSocket( new tcp::socket( service ) ), mLastFrameConfirmed( 0 ), 
	mClientName( "" ), mClientID( 0 ), mIsAsync( false ), 
	mAsyncReceivesData( false )
{
	loadSettings( jsonSettingsFile );

	mAppUpdateConnection = ci::app::App::get()->getSignalUpdate().connect( std::bind( &Client::update, this ) );

	start();
}

Client::~Client()
{
	stop();
}

ClientRef Client::create( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service )
{
	return ClientRef( new Client( jsonSettingsFile, service ) );
}

void Client::setupCamera( const ClientRef &client, ci::CameraPersp &cam, float zPosition )
{
	auto masterSize = client->getMasterSize();
	auto viewportRect = client->getVisibleRect();
	auto localSize = viewportRect.getSize();
	auto localOrigin = vec2( viewportRect.x1, viewportRect.y1 );

	cam.setAspectRatio( localSize.x / localSize.y );

	vec3 eye( masterSize.x / 2.0f, masterSize.y / 2.0f, zPosition );
	vec3 target( masterSize.x / 2.0f, masterSize.y / 2.0f, 0 );
	vec3 up( 0.0f, -1.0f, 0.0f );
	cam.lookAt( eye, target, up );

	float horizCenterMaster = masterSize.x / 2.0f;
	float horizCenterView = localOrigin.x + ( localSize.x * 0.5f );
	float horizPxShift = horizCenterMaster - horizCenterView;
	float horizOffset = ( horizPxShift / localSize.x ) * -2.0f;

	float vertCenterMaster = masterSize.y / 2.0f;
	float vertCenterView = localOrigin.y + ( localSize.y * 0.5 );
	float vertPxShift = vertCenterMaster - vertCenterView;
	float vertOffset = ( vertPxShift / localSize.y ) * 2.0f;

	cam.setLensShift( horizOffset, vertOffset );
}

ci::mat4 Client::getClientModelTransform( const ClientRef &client )
{
	return ci::mat4();
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
	if( mTcpSocket->is_open() ) {
		mTcpSocket->close();
		mTcpSocket.reset();
	}
}

void Client::update()
{
	mFrameIsReady = false;

	if( isConnected() ) {
		{
			std::lock_guard<std::mutex> guard( mMessageMutex );
			if( mMessages.size() ) {
				// There may be more than 1 message in the read.
				while( !mMessages.empty() ) {
					auto & message = mMessages.front();
					if( message.size() > 0 ) {
						CI_LOG_V( "I'm about to parse a message" );
						protocol::parseClient( message, this );
					}
					CI_LOG_V( "Current message" + message );
					mMessages.pop_front();
				}

			}
		}


		if( mFrameIsReady && !mIsAsync ) {
			// You always need an updateCallback if synchronous.
			CI_ASSERT( mUpdateCallback );
			CI_LOG_V( "I'm updating the current frame." );
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
	auto msg = protocol::togglePause();
	// TODO: Implement writing ability in asio.
	// mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}

void Client::resetAll()
{
	auto msg = protocol::reset();
	// TODO: Implement writing ability in asio.
	// mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}

void Client::sendMessage( const std::string &message )
{
	auto msg = protocol::dataMessage( message );
	// TODO: Implement writing ability in asio.
	// mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}

void Client::sendMessage( const std::string &message, const std::vector<uint32_t> &clientIds )
{
	auto msg = protocol::dataMessage( message, clientIds );
	// TODO: Implement writing ability in asio.
	// mTcpSession->write( TcpSession::stringToBuffer( msg ) );
}

void Client::doneRendering()
{
	if( mTcpSocket->is_open() ) {
		if( mLastFrameConfirmed < mCurrentRenderFrame ) {
			CI_LOG_V( "Confirming done with render" );
			auto msg = protocol::renderComplete( mClientID, mCurrentRenderFrame );
			// TODO: Implement writing ability in asio.
			// mTcpSession->write( TcpSession::stringToBuffer( msg ) );
			mLastFrameConfirmed = mCurrentRenderFrame;
		}
	}
}

void Client::loadSettings( const ci::DataSourceRef &settingsJsonFile )
{
	JsonTree settingsDoc = JsonTree( settingsJsonFile ).getChild( "settings" );

	try {
		JsonTree node = settingsDoc.getChild( "asynchronous" );
		mIsAsync = node.getValue<bool>();
	}
	catch( JsonTree::ExcChildNotFound e ) {
		CI_LOG_V( "No asynchronous flag set, assuming false" );
		mIsAsync = false;
	}

	if( mIsAsync ) {
		try {
			JsonTree node = settingsDoc.getChild( "asynchreceive" );
			mAsyncReceivesData = node.getValue<bool>();

		}
		catch( JsonTree::ExcChildNotFound e ) {
			CI_LOG_V( "No asynchreceive flag set, assuming false" );
			mAsyncReceivesData = false;
		}
	}

	try {
		JsonTree clientId = settingsDoc.getChild( "client_id" );
		mClientID = clientId.getValue<int>();
	}
	catch( JsonTree::ExcChildNotFound e ) {
		CI_LOG_E( "Could not find client ID.\n" );
	}

	try {
		JsonTree node = settingsDoc.getChild( "name" );
		mClientName = node.getValue<string>();
	}
	catch( JsonTree::ExcChildNotFound e ) {
		CI_LOG_V( "No Client Name provided, using default" );
		if( mIsAsync ) {
			mClientName = "Async client " + std::to_string( mClientID );
		}
		else {
			mClientName = "Sync client " + std::to_string( mClientID );
		}
	}

	try {
		JsonTree server = settingsDoc.getChild( "server" );
		mHostname = server["ip"].getValue<string>();
		mPort = server["port"].getValue<uint16_t>();
	}
	catch( JsonTree::ExcChildNotFound e ) {
		CI_LOG_E( "Could not find server and port settings.\n" );
	}

	try {
		JsonTree localDimensions = settingsDoc["local_dimensions"];
		JsonTree localLocation = settingsDoc["local_location"];
		uint32_t width = localDimensions["width"].getValue<uint32_t>();
		uint32_t height = localDimensions["height"].getValue<uint32_t>();
		int x = localLocation["x"].getValue<int>();
		int y = localLocation["y"].getValue<int>();
		mLocalViewportRect = Rectf( x, y, x + width, y + height );

		// Force the window size based on the settings XML.
		ci::app::setWindowSize( width, height );
	}
	catch( JsonTree::ExcChildNotFound e ) {
		if( !mIsAsync ) {
			// Async controller doesn't need to know about the dimensions
			CI_LOG_E( e.what() << " Could not find local dimensions settings for synchronous client.\n" );
		}
	}

	try {
		JsonTree masterDimension = settingsDoc.getChild( "master_dimensions" );
		uint32_t width = masterDimension["width"].getValue<uint32_t>();
		uint32_t height = masterDimension["height"].getValue<uint32_t>();
		mMasterSize = ivec2( width, height );
	}
	catch( JsonTree::ExcChildNotFound e ) {
		if( !mIsAsync ) {
			// Async controller doesn't need to know about the dimensions
			CI_LOG_E( e.what() << " Could not find master dimensions settings, for synchronous client.\n" );
		}
	}

	try {
		JsonTree fullscreenNode = settingsDoc.getChild( "go_fullscreen" );
		bool boolFull = fullscreenNode.getValue<bool>();
		if( boolFull ) {
			ci::app::setFullScreen( true );
		}
	}
	catch( JsonTree::ExcChildNotFound e ) {
		// Not required
		CI_LOG_V( "No 'go_fullscreen' flag set. Not required." );
	}

	try {
		JsonTree offset = settingsDoc.getChild( "offset_window" );
		bool boolOff = offset.getValue<bool>();
		if( boolOff ) {
			ci::app::setWindowPos( ivec2( mLocalViewportRect.x1, mLocalViewportRect.y1 ) );
		}
	}
	catch( JsonTree::ExcChildNotFound e ) {
		// Not required
		CI_LOG_V( "No 'offset_window' flag set. Not required." );
	}
}

void Client::start()
{
	if( mIsConnected ) {
		stop();
	}
	// TODO: Implement ability in asio.
	/*mTcpClient->connectConnectEventHandler( &Client::onConnect, this );
	mTcpClient->connectErrorEventHandler( &Client::onError, this );*/

	CI_LOG_V( "Connecting" );
	//mTcpClient->connect( mHostname, mPort );
}

void Client::onConnect( TcpSocketRef session )
{
	CI_LOG_V( "Established Connection with " << mHostname << " on " << mPort );

	mTcpSocket = session;
	mIsConnected = true;

	//TODO: Implement in asio
	//auto weak = std::weak_ptr<Client>( shared_from_this() );

	/*mTcpSession->connectCloseEventHandler( std::bind( []( std::weak_ptr<Client> &weakInst ) {
		auto sharedInst = weakInst.lock();
		if( sharedInst ) {
			CI_LOG_I( "Connection closed" );
			sharedInst->mIsConnected = false;
		}
	}, std::move( weak ) ) );*/

	//mTcpSession->connectErrorEventHandler( &Client::onError, this );
	//mTcpSession->connectReadEventHandler( &Client::onRead, this );
	//mTcpSession->connectWriteEventHandler( &Client::onWrite, this );

	sendClientId();
}

void Client::onRead( const ci::BufferRef &buffer )
{
	std::lock_guard<std::mutex> guard( mMessageMutex );
	// TODO: Implement in asio
	//auto msg = TcpSession::bufferToString( buffer );
	//CI_LOG_V( "Received a message " << msg );
	//auto msgs = ci::split( msg, Protocol::messageDelimiter() );
	//mMessages.push_back( msgs[0] );
}

void Client::onWrite( size_t bytesTransferred )
{
	// TODO: Implement in asio
	CI_LOG_V( bytesTransferred << " Bytes Transferred" );
	//mTcpSession->read( Protocol::messageDelimiter() );
}

void Client::onError( std::string err, size_t bytesTransferred )
{
	CI_LOG_E( err << " Bytes Transferred: " << bytesTransferred );
}

void Client::sendClientId()
{
	/*if( mIsAsync ) {
		auto msg = protocol::asyncClientID( mClientID, mClientName, mAsyncReceivesData );
		mTcpSession->write( TcpSession::stringToBuffer( msg ) );
	}
	else {
		auto msg = Protocol::syncClientID( mClientID, mClientName );
		mTcpSession->write( TcpSession::stringToBuffer( msg ) );
	}*/
}

void Client::setCurrentRenderFrame( uint64_t frameNum )
{
	MessageHandler::setCurrentRenderFrame( frameNum );
	// mLastFrameConfirmed has to reset when the current render frame is set to keep them in line.
	mLastFrameConfirmed = mCurrentRenderFrame - 1;
}

void Client::receivedResetCommand()
{
	CI_LOG_V( "Received Reset command, Current Frame number: " << mCurrentRenderFrame );
	if( mResetCallback )
		mResetCallback();
}

void Client::receivedStringMessage( const std::string &dataMessage, const uint32_t fromClientId )
{
	if( mDataMessageCallback )
		mDataMessageCallback( dataMessage, fromClientId );
}

Server::ClientConnection::ClientConnection( const TcpSocketRef &session, const ServerRef &parent )
	: mSession( session ), mParent( parent ), mId( 0 ), mIsAsync( false ), mMessageMutex( new std::mutex() )
{
	//mSession->connectReadEventHandler( [&]( const ci::BufferRef &buffer ) {
	//	auto msgs = ci::split( TcpSession::bufferToString( buffer ), Protocol::messageDelimiter() );

	//	auto msg = ci::split( msgs[0], Protocol::dataMessageDelimiter() );

	//	if( msg[0] == Protocol::CONNECT_ASYNCHRONOUS && msg.size() == 4 ) {
	//		mIsAsync = true;
	//		mId = atoi( msg[1].c_str() );
	//		mName = msg[2];
	//		mShouldReceiveData = ( msg[3] == "true" );
	//	}
	//	else if( msg[0] == Protocol::CONNECT_SYNCHRONOUS && msg.size() == 3 ) {
	//		mIsAsync = false;
	//		mId = atoi( msg[1].c_str() );
	//		mName = msg[2];
	//		mShouldReceiveData = true;
	//	}
	//	else {
	//		CI_LOG_E( "This message doesn't contain what is needed" << buffer );
	//	}
	//	// Now that we have our info let's connect the normal onRead callback
	//	mSession->connectReadEventHandler( &ClientConnection::onRead, this );
	//} );
	/*mSession->connectCloseEventHandler( &ClientConnection::onClose, this );
	mSession->connectErrorEventHandler( &ClientConnection::onError, this );
	mSession->connectWriteEventHandler( &ClientConnection::onWrite, this );

	mSession->read( Protocol::messageDelimiter() );*/
}

Server::ClientConnection::~ClientConnection()
{
	if( mSession )
		mSession->close();
}

void Server::ClientConnection::onRead( const ci::BufferRef &buffer )
{
	std::lock_guard<std::mutex> lock( *mMessageMutex );
	/*auto msgs = ci::split( TcpSession::bufferToString( buffer ), Protocol::messageDelimiter() );
	mMessages.insert( mMessages.end(), msgs.begin(), msgs.end() );*/
}

void Server::ClientConnection::onWrite( size_t bytesTransferred )
{
	//mSession->read( Protocol::messageDelimiter() );
}

void Server::ClientConnection::onClose()
{
	// TODO: Remove itself
}

void Server::ClientConnection::onError( std::string error, size_t bytesTransferred )
{

}

Server::Server( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service, bool thread )
: mTotalAllowedConnections( 1 ), mPort( 0 ), mFPSUpdate( 60 ), mIsThreaded( thread )
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
	/*mTcpServer->connectAcceptEventHandler( &Server::onAccept, this );
	mTcpServer->connectErrorEventHandler( &Server::onError, this );
	mTcpServer->connectCancelEventHandler( &Server::onCancel, this );

	mTcpServer->accept( mPort );*/
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

void Server::onAccept( TcpSocketRef session )
{
	mTcpConnections.emplace_back( new ClientConnection( session, shared_from_this() ) );
	if( mTotalAllowedConnections > mTcpConnections.size() ) {
		//mTcpServer->accept( mPort );
	}
	else {
		//mTcpServer->cancel();
	}
}

}