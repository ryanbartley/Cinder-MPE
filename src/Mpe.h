//
//  MPEProtocol.hpp
//  Unknown Project
//
//  Copyright (c) 2013 William Lindmeier. All rights reserved.
//

#pragma once

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <deque>

#include "cinder/app/App.h"
#include "cinder/Camera.h"
#include "cinder/Rect.h"
#include "cinder/Utilities.h"
#include "cinder/Log.h"
#include "asio/asio.hpp"

namespace ci = cinder;
/*

Protocol:
This class converts actions to/from strings that the server undertands.
Protocol can be subclassed to use with other servers, if it's ever updated / modified.

*/

namespace mpe {

class MessageHandler;
class ClientMessageHandler;
class ServerMessageHandler;

using ClientRef				= std::shared_ptr<class Client>;
using ServerRef				= std::shared_ptr<class Server>;
using UpdateFrameCallback	= std::function<void( uint64_t )>;
using ResetCallback			= std::function<void()>;
using DataMessageCallback	= std::function<void( const std::string &, const uint32_t )>;
using TcpSocketRef			= std::shared_ptr<asio::ip::tcp::socket>;
using AcceptorRef			= std::shared_ptr<asio::ip::tcp::acceptor>;

namespace protocol {
const static std::string CONNECT_SYNCHRONOUS = "S";
const static std::string DONE_RENDERING = "D";
const static std::string DATA_MESSAGE = "T";
const static std::string NEXT_FRAME = "G";
const static std::string CONNECT_ASYNCHRONOUS = "A";
const static std::string RESET_ALL = "R";
const static std::string TOGGLE_PAUSE = "P";

const static std::string kMessageTerminus = "\n";
const static std::string kDataMessageDelimiter = "|";
const static std::string kIncomingMessageDelimiter;

std::string cleanMessage( std::string message );
std::string syncClientID( uint32_t clientID );
std::string syncClientID( uint32_t clientID, const std::string & name );
std::string asyncClientID( uint32_t clientID );
std::string asyncClientID( uint32_t clientID, const std::string & clientName, 
						   bool shouldReceiveDataMessages = false );
std::string renderComplete( uint32_t clientID, uint64_t frameNum );
std::string reset();
std::string togglePause();
std::string dataMessage( const std::string & msg );
std::string dataMessage( const std::string & msg, const std::vector<uint32_t> & toClientIDs );

// The TCP client listens to the socket until it reaches the delimiter,
// at which point the string is parsed.
const std::string& messageDelimiter();
const std::string& dataMessageDelimiter();
std::string nextFrame( uint64_t frameNum );

void parseClient( const std::string & serverMessage, ClientMessageHandler *handler );
void parseServer( const std::string &clientMessage, ServerMessageHandler *handler );
} // namespace protocol

/*

MessageHandler:
A minimal interface that the Protocol requires when parsing data.
ClientBase is a subclass of MessageHandler.

*/

class MessageHandler {
public:
	virtual ~MessageHandler() = default;

	//! The current frame that each client is rendering.
	//! This is the only MPEMessageHandler function that the App should ever call.
	uint64_t getCurrentRenderFrame() const { return mCurrentRenderFrame; }
	//! Average data FPS (i.e. the number of server updates per second).
	float getUpdatesPerSecond() const { return mAvgUpdateDuration > 0 ? ( 1.0f / mAvgUpdateDuration ) : 0; }

protected:
	MessageHandler();
	//! The frame that every client should be rendering.
	//! This is set by the protocol and should never by called by your App.
	// TODO: Figure this out. It should be starting at 1 not at 0.
	virtual void setCurrentRenderFrame( uint64_t frameNum ) { mCurrentRenderFrame = frameNum + 1; }
	//! mFrameIsReady is set to true once the incoming server message is ready.
	//! This is set by the protocol and should never by called by your App.
	void setFrameIsReady( bool isFrameReady );

	uint64_t            mCurrentRenderFrame;
	bool                mFrameIsReady;

private:
	void calculateDFPS();

	float               mAvgUpdateDuration;
	double              mTimeLastMessage;
	int                 mUpdateSampleInterval;
};

class ClientMessageHandler : public MessageHandler {
public:
	virtual ~ClientMessageHandler() = default;

protected:
	ClientMessageHandler() = default;

	//! These are overridden in the MPEClient to handle data received from Server.
	virtual void receivedStringMessage( const std::string & dataMessage, uint32_t fromClientID ) = 0;
	virtual void receivedResetCommand() = 0;

private:
	friend void protocol::parseClient( const std::string & serverMessage, ClientMessageHandler *handler );
};

class ClientBase : public ClientMessageHandler {
public:

	ClientBase() : ClientMessageHandler() {};

	virtual                     ~ClientBase() {};

	// Misc Accessors
	virtual uint32_t            getClientID() const = 0;
	virtual const std::string&  getClientName() const = 0;

	// Async vs Sync
	// async == doesn't sync frames with other clients
	// sync == renders frames in-step with other clients
	virtual bool                isAsynchronousClient() const = 0;

	// Screen Dimensions
	virtual const ci::Rectf&    getVisibleRect() const = 0;
	virtual void                setVisibleRect( const ci::Rectf & rect ) = 0;
	virtual const ci::ivec2&    getMasterSize() const = 0;

	// Hit testing
	virtual bool                isOnScreen( float x, float y ) = 0;
	virtual bool                isOnScreen( const ci::vec2 & pos ) = 0;
	virtual bool                isOnScreen( float x, float y, float w, float h ) = 0;
	virtual bool                isOnScreen( const ci::Rectf & rect ) = 0;

	// Connection
	virtual void                start() = 0;
	virtual void                start( const std::string & hostname, uint16_t port ) = 0;
	virtual void                stop() = 0;
	virtual void                togglePause() = 0;
	virtual void                resetAll() = 0;
	virtual bool                isConnected() const = 0;

	// Loop
	virtual void                update() = 0;

	// Sending Data
	// Data sent to the server is broadcast to every client.
	// The sending App will receive its own data and should act on it when it's received,
	// rather than before it's sent, so all of the clients are in sync.
	virtual void                sendMessage( const std::string & message ) = 0; // née broadcast
	// Send data to specific client IDs
	virtual void                sendMessage( const std::string & message,
											 const std::vector<uint32_t> & clientIds ) = 0;

};

class Client : public ClientBase, public std::enable_shared_from_this<Client> {
public:

	virtual ~Client();

	//! Creates a MPE client with settings from \a jsonSettingsFile. Takes an optional boost::asio::io_service, uses cinder App's io_service by default. Takes an optional thread boolean, defaults to false. Set this if you've run the io_service on a different thread.
	static ClientRef create( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service = ci::app::App::get()->io_service() );

	//! Uses hostname and port to start a Connection with the MPE Server. Most of the time
	//! the settings file will provide these.
	void			start( const std::string &hostname, uint16_t port ) override;
	//! Stops the connection to the server.
	virtual void	stop() override;
	//! Returns whether the connection to the server is still open.
	bool			isConnected() const override { return mIsConnected && mTcpSocket->is_open(); }
	//! Updates the client and processes all received messages.
	virtual void	update() override;

	static void setupCamera( const ClientRef &client, ci::CameraPersp &cam, float zPosition );
	static ci::mat4 getClientModelTransform( const ClientRef &client );

	// Communication with the server from this client below...
	/*********************************************************/
	//! Tells the MPE server to broadcast a pause to all clients.
	void			togglePause() override;
	//! Tells the MPE server to broadcast a hard reset to all clients.
	void			resetAll() override;
	//! Sends \a message as a data message, which will be broadcast to all clients.
	void			sendMessage( const std::string &message ) override;
	//! Sends \a message as a data message, which will only be broadcast to clients in \a clientIds.
	void			sendMessage( const std::string &message, const std::vector<uint32_t> &clientIds ) override;
	//! Called by the Cinder App to announce that rendering is done. The MPE Server waits until all clients are doneRendering before it sends out an order to go to the next frame. Only inform the server if this is a new frame. It's possible that a given frame is rendered multiple times if the server update is slower than the app loop.
	void			doneRendering();
	//! Returns a bool whether you should update to the next frame.
	bool			shouldUpdate() { return mFrameIsReady; }

	// Local settings
	/****************/
	//! Returns the id of this client.
	uint32_t			getClientID() const override { return mClientID; }
	//! Returns the name of this client.
	const std::string&	getClientName() const override { return mClientName; }
	//! Returns whether this client is asynchronous. If it's asynchronus, it doesn't wait for
	//! a new frame to continue.
	bool				isAsynchronousClient() const override { return mIsAsync; }
	//! Returns the total screen size.
	const ci::ivec2&	getMasterSize() const override { return mMasterSize; }
	//! Returns the Viewport Dimensions.
	const ci::Rectf&	getVisibleRect() const override { return mLocalViewportRect; }
	//! Sets the Viewport Dimensions used for setting scissor and viewport.
	void				setVisibleRect( const ci::Rectf & rect ) override { mLocalViewportRect = rect; }
	//! Returns whether this point is inside your screen.
	bool			isOnScreen( const ci::vec2 &pos ) override { return mLocalViewportRect.contains( pos ); }
	//! Returns whether this point is inside your screen.
	bool			isOnScreen( float x, float y ) override { return isOnScreen( ci::vec2( x, y ) ); }
	//! Returns whether this rect is inside your screen.
	bool			isOnScreen( const ci::Rectf &rect ) override { return mLocalViewportRect.intersects( rect ); }
	//! Returns whether this rect is inside your screen.
	bool			isOnScreen( float x1, float y1, float x2, float y2 ) override
	{ return isOnScreen( ci::Rectf( x1, y1, x2, y2 ) ); }
	//! Returns a pair of Vec2's which will be used for scissoring and viewport size.
	inline std::pair<ci::vec2, ci::vec2> getGlWindowInfo()
	{
		return std::make_pair( ci::vec2( mLocalViewportRect.x1, mLocalViewportRect.y1 ),
							   ci::vec2( mLocalViewportRect.x2, mLocalViewportRect.y2 ) );
	}

	// Callbacks to deal with messages from the server below...
	/*********************************************************/
	//! Sets the function, with signature void( uint64_t ), to be called
	//! when the cinder app should update the frame
	void setUpdateFrameCallback( const UpdateFrameCallback& updateFrameFunc ) { mUpdateCallback = updateFrameFunc; }
	template<class F, class T>
	void setUpdateFrameCallback( F function, T* instance )
	{ mUpdateCallback = std::bind( function, instance, std::placeholders::_1 ); }
	//! Sets the function, with signature void(), to be called when the
	//! server requests that all clients reset.
	void setResetCallback( const ResetCallback& resetFunc ) { mResetCallback = resetFunc; }
	template<class F, class T>
	void setResetCallback( F function, T* instance )
	{ mResetCallback = std::bind( function, instance ); }
	//! Sets the function, with signature void ( const std::string &, const uint32_t ),
	//! to be called when the client receives a data message.
	void setDataMessageCallback( const DataMessageCallback& dataMessageFunc ) { mDataMessageCallback = dataMessageFunc; }
	template<class F, class T>
	void setDataMessageCallback( F function, T* instance )
	{ mDataMessageCallback = std::bind( function, instance, std::placeholders::_1, std::placeholders::_2 ); }


protected:
	Client( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service );

	//! Load's settings from a json file from the constructor.
	void loadSettings( const ci::DataSourceRef &settingsJsonFile );

	//! Starts the client.
	virtual void		start() override;

	//! Internal Callback for TcpClient, which caches session and sets up the session callbacks.
	virtual void		onConnect( TcpSocketRef session );
	//! Internal Callback for TcpClient and TcpSession, which presents errors.
	virtual void		onError( std::string err, size_t bytesTransferred );
	//! Internal Callback for TcpSession when TcpSession has read something.
	virtual void		onRead( const ci::BufferRef &buffer );
	//! Internal Callback for TcpSession when TcpSession has finished a write
	virtual void		onWrite( size_t bytesTransferred );

	//! Called when we receive a resetCommand. Calls the ResetCallback if one is present.
	void			receivedResetCommand() override;
	//! Called when we receive a new render frame.
	void			setCurrentRenderFrame( uint64_t frameNum ) override;
	//! Called when we receive a data message. Calls the DataMessageCallback if one is present.
	virtual void	receivedStringMessage( const std::string &dataMessage, uint32_t fromClientId ) override;

	//! onConnect calls this when the TcpClient connects to the server.
	void sendClientId();


	// Member variables.

	// Callbacks used to communicate with your app
	UpdateFrameCallback				mUpdateCallback;
	ResetCallback					mResetCallback;
	DataMessageCallback				mDataMessageCallback;
	cinder::signals::Connection		mAppUpdateConnection;

	// A connection to the server.
	TcpSocketRef					mTcpSocket;
	bool							mIsConnected;
	asio::ip::tcp::endpoint			mDestinationEndpoint;

	// Message details
	std::mutex						mMessageMutex;
	std::deque<std::string>			mMessages;

	//! Rendering details.
	ci::Rectf                       mLocalViewportRect;		// settings
	ci::ivec2                       mMasterSize;			// settings
	uint64_t                        mLastFrameConfirmed;

	// Settings loaded from settings.xml
	std::string                     mClientName;			// settings
	uint32_t                        mClientID;				// settings
	bool                            mIsAsync;				// settings
	bool                            mAsyncReceivesData;		// settings
};

class ServerMessageHandler : public MessageHandler {
public:

protected:

private:
	friend void protocol::parseServer( const std::string &clientMessage, ServerMessageHandler *handler );
};

class ServerBase : public ServerMessageHandler {
public:

	ServerBase() : ServerMessageHandler() {}

	virtual void start() = 0;
	virtual void start( uint16_t ) = 0;
	virtual void stop() = 0;
	virtual void update() = 0;


protected:
};

class Server : public ServerBase, std::enable_shared_from_this<Server> {
public:
	static ServerRef create( const ci::DataSourceRef &jsonSettingsFile, asio::io_service &service = ci::app::App::get()->io_service(), bool thread = false );

	virtual void start() override;
	virtual void start( uint16_t port ) override;
	virtual void stop() override;
	virtual void update() override;

	struct ClientConnection {
		ClientConnection( const TcpSocketRef &session, const ServerRef &mParent );

		~ClientConnection();

		void onError( std::string error, size_t bytesTransferred );
		void onClose();
		void onRead( const ci::BufferRef &buffer );
		void onWrite( size_t bytesTransferred );

		void write( std::string &message );

		uint32_t getId() const { return mId; }

		std::shared_ptr<std::mutex>& getMessageMutex() { return mMessageMutex; }

	private:
		TcpSocketRef					mSession;
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

	void onAccept( TcpSocketRef session );
	void onError( std::string error, size_t bytesTransferred );
	void onCancel();

	using ClientConnectionRef = std::shared_ptr<ClientConnection>;
	using Connections = std::vector<ClientConnectionRef>;

	AcceptorRef				mTcpServer;
	Connections				mTcpConnections;
	uint16_t				mPort;

	std::deque<std::string> mMessages;
	std::mutex				mMessagesMutex;

	const uint16_t			mTotalAllowedConnections;
	const uint8_t			mFPSUpdate;
	bool					mIsThreaded;
};

}
