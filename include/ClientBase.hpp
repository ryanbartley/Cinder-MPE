//
//  MPEClient.h
//  Unknown Project
//
//  Copyright (c) 2013 William Lindmeier. All rights reserved.
//

#pragma once

#include "MessageHandler.hpp"

/*
 
 Client:
 This class is the interface through which your App communicates with an MPE server.
 
 Once you've subclassed your Cinder App from MPEApp, you construct a client by passing
 it a pointer to your app like so:
 
 Client::Create(this); // <-- called from your Cinder app setup()
 
 The client keeps track of the current frame that should be rendered (see
 MPEMessageHandler::getCurrentRenderFrame) and informs the server when it's complete. Once
 all of the clients have rendered the frame the server will send out the next frame number.
 
 */

namespace mpe
{

class ClientBase : public ClientMessageHandler {
public:
	
	ClientBase() : ClientMessageHandler(){};
    
    virtual                     ~ClientBase(){};
	
    // Misc Accessors
    virtual uint32_t            getClientID() const = 0;
    virtual bool                isThreaded() const = 0;
    virtual const std::string&  getClientName() const = 0;
	
    // Async vs Sync
    // async == doesn't sync frames with other clients
    // sync == renders frames in-step with other clients
    virtual bool                isAsynchronousClient() const = 0;
	
    // Screen Dimensions
    virtual const ci::Rectf&    getVisibleRect() const = 0;
    virtual void                setVisibleRect(const ci::Rectf & rect) = 0;
    virtual const ci::ivec2&    getMasterSize() const = 0;
	
    // Hit testing
    virtual bool                isOnScreen(float x, float y) = 0;
    virtual bool                isOnScreen(const ci::vec2 & pos) = 0;
    virtual bool                isOnScreen(float x, float y, float w, float h) = 0;
    virtual bool                isOnScreen(const ci::Rectf & rect) = 0;
	
    // Connection
    virtual void                start() = 0;
    virtual void                start(const std::string & hostname, uint16_t port) = 0;
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
}
