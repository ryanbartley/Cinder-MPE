//
//  Client.h
//  Test
//
//  Created by Ryan Bartley on 8/30/14.
//
//

#pragma once

#include <deque>

#include "cinder/Rect.h"

#include "ClientBase.hpp"
#include "TcpClient.h"

namespace mpe {
	
using ClientRef				= std::shared_ptr<class Client>;
using UpdateFrameCallback	= std::function<void ( uint64_t )>;
using ResetCallback			= std::function<void()>;
using DataMessageCallback	= std::function<void ( const std::string &, const uint32_t )>;
using io_service_ref		= std::shared_ptr<boost::asio::io_service>;
	
class Client : public ClientBase, public std::enable_shared_from_this<Client> {
public:
	
	virtual ~Client();
	
	//! Creates a MPE client with settings from \a jsonSettingsFile. Takes an optional boost::asio::io_service, uses cinder App's io_service by default. Takes an optional thread boolean, defaults to false. Set this if you've run the io_service on a different thread.
	static ClientRef create( const ci::DataSourceRef &jsonSettingsFile, boost::asio::io_service &service = ci::app::App::get()->io_service(), bool thread = false );
	
	//! Uses hostname and port to start a Connection with the MPE Server. Most of the time
	//! the settings file will provide these.
	void			start( const std::string &hostname, uint16_t port ) override;
	//! Stops the connection to the server.
	virtual void	stop() override;
	//! Returns whether the connection to the server is still open.
	bool			isConnected() const override { return mIsConnected && mTcpSession->getSocket()->is_open(); }
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
	//! Returns whether this client is threaded. This is a const variable set when the client is created.
	bool				isThreaded() const override { return mIsThreaded; }
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
	Client( const ci::DataSourceRef &jsonSettingsFile, boost::asio::io_service &service, bool thread );
	
	//! Load's settings from a json file from the constructor.
	void loadSettings( const ci::DataSourceRef &settingsJsonFile );
	
	//! Starts the client.
	virtual void		start() override;
	
	//! Internal Callback for TcpClient, which caches session and sets up the session callbacks.
	virtual void		onConnect( TcpSessionRef session );
	//! Internal Callback for TcpClient and TcpSession, which presents errors.
	virtual void		onError( std::string err, size_t bytesTransferred );
	//! Internal Callback for TcpSession when TcpSession has read something.
	virtual void		onRead( ci::Buffer buffer );
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
	boost::signals2::connection		mAppUpdateConnection;
	
	// A connection to the server.
    TcpClientRef					mTcpClient;
	TcpSessionRef					mTcpSession;
	bool							mIsConnected;
	uint16_t                        mPort;					// settings
    std::string                     mHostname;				// settings
	
	// Threaded details
	const bool						mIsThreaded;
	std::shared_ptr<std::mutex>		mMessageMutex;
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
	
}