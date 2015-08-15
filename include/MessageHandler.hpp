//
//  MPEMessageHandler.hpp
//  Unknown Project
//
//  Copyright (c) 2013 William Lindmeier. All rights reserved.
//

#pragma once

/*
 
 MessageHandler:
 A minimal interface that the Protocol requires when parsing data.
 ClientBase is a subclass of MessageHandler.
 
 */
#include "cinder/app/App.h"

namespace mpe {
	
class Protocol;
	
class MessageHandler {
public:
	
	virtual ~MessageHandler(){}
	
	//! The current frame that each client is rendering.
	//! This is the only MPEMessageHandler function that the App should ever call.
	uint64_t getCurrentRenderFrame() const { return mCurrentRenderFrame; }
	
	//! Average data FPS (i.e. the number of server updates per second).
	float getUpdatesPerSecond() const { return mAvgUpdateDuration > 0 ? ( 1.0f / mAvgUpdateDuration ) : 0; }
	
protected:
	MessageHandler() :
	mCurrentRenderFrame(1), mFrameIsReady(false),
	mAvgUpdateDuration(0), mTimeLastMessage(0),
	mUpdateSampleInterval(5)
	{}
	
	//! The frame that every client should be rendering.
	//! This is set by the protocol and should never by called by your App.
	// TODO: Figure this out. It should be starting at 1 not at 0.
	virtual void setCurrentRenderFrame( uint64_t frameNum ) { mCurrentRenderFrame = frameNum + 1; }
	
	//! mFrameIsReady is set to true once the incoming server message is ready.
	//! This is set by the protocol and should never by called by your App.
	void setFrameIsReady(bool isFrameReady) {
		mFrameIsReady = isFrameReady;
		
		if ( mCurrentRenderFrame % mUpdateSampleInterval == 0 ) {
			calculateDFPS();
		}
	}
	
	uint64_t            mCurrentRenderFrame;
	bool                mFrameIsReady;
	
private:
	void calculateDFPS()
	{
		double now = ci::app::getElapsedSeconds();
		double frameDuration = ( now - mTimeLastMessage ) / mUpdateSampleInterval;
		if ( frameDuration > 0 ) {
			mAvgUpdateDuration = ( mAvgUpdateDuration * 0.9 ) + ( frameDuration * 0.1 );
		}
		else {
			mAvgUpdateDuration = frameDuration;
		}
		
		mTimeLastMessage = now;
	}
	
	float               mAvgUpdateDuration;
	double              mTimeLastMessage;
	int                 mUpdateSampleInterval;
	
	friend class Protocol;
};
	
class ClientMessageHandler : public MessageHandler {
public:
	virtual ~ClientMessageHandler(){}
	
protected:
	ClientMessageHandler() : MessageHandler() {}
	
	//! These are overridden in the MPEClient to handle data received from Server.
	virtual void receivedStringMessage(const std::string & dataMessage, uint32_t fromClientID ) = 0;
	virtual void receivedResetCommand() = 0;


private:
	
	friend class Protocol;
};
	
class ServerMessageHandler : public MessageHandler {
public:
	
	
	
protected:
	
private:
};

}
