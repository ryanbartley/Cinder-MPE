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

#include "cinder/Rect.h"
#include "cinder/Utilities.h"
#include "cinder/Log.h"
#include "MessageHandler.hpp"

/*
 
 Protocol:
 This class converts actions to/from strings that the server undertands.
 Protocol can be subclassed to use with other servers, if it's ever updated / modified.
 
 */

namespace mpe {
	
using ProtocolRef = std::shared_ptr<class Protocol>;

class Protocol {
public:
	
	const static std::string CONNECT_SYNCHRONOUS;
	const static std::string DONE_RENDERING;
	const static std::string DATA_MESSAGE;
	const static std::string NEXT_FRAME;
	const static std::string CONNECT_ASYNCHRONOUS;
	const static std::string RESET_ALL;
	const static std::string TOGGLE_PAUSE;
	
	const static std::string kMessageTerminus;
	const static std::string kDataMessageDelimiter;
	const static std::string kIncomingMessageDelimiter;
    
    ~Protocol(){};
    
	inline static std::string cleanMessage( std::string message )
    {
        // Make sure the delimiter isn't in the message
        if ( message.find( dataMessageDelimiter() ) != std::string::npos)
        {
            CI_LOG_W( dataMessageDelimiter()
            << "' are not allowed in broadcast messages."
            << " Replacing with an underscore.\n");
            std::replace( message.begin(), message.end(), dataMessageDelimiter().at(0), '_' );
        }
        if ( message.find( messageDelimiter() ) != std::string::npos)
        {
            std::string termID = "Newlines";
            if ( messageDelimiter() != "\n") {
                termID = "'" + messageDelimiter() + "'";
            }
            CI_LOG_W( termID
            << " are not allowed in broadcast messages."
            << " Replacing with an underscore.\n");
            std::replace( message.begin(), message.end(), dataMessageDelimiter().at(0), '_' );
        }
		return std::move( message );
    }
	
    inline static std::string syncClientID( const int clientID )
    {
        return syncClientID( clientID, "Rendering Client " + std::to_string(clientID) );
    };
	
    inline static std::string syncClientID( const int clientID, const std::string & name )
    {
        return CONNECT_SYNCHRONOUS +
		dataMessageDelimiter() +
		std::to_string(clientID) +
		dataMessageDelimiter() +
		name +
		messageDelimiter();
    };
	
    inline static std::string asyncClientID( const int clientID )
    {
        return asyncClientID( clientID, "Non-Rendering Client " + std::to_string(clientID) );
    }
	
	inline static std::string asyncClientID( const int clientID,
								  const std::string & clientName,
								  bool shouldReceiveDataMessages = false )
    {
        return CONNECT_ASYNCHRONOUS +
		dataMessageDelimiter() +
		std::to_string(clientID) +
		dataMessageDelimiter() +
		clientName +
		dataMessageDelimiter() +
		(shouldReceiveDataMessages ? "true" : "false") +
		messageDelimiter();
    };
	
	inline static std::string renderComplete( uint32_t clientID, uint64_t frameNum )
    {
        return DONE_RENDERING +
		dataMessageDelimiter() +
		std::to_string(clientID) +
		dataMessageDelimiter() +
		std::to_string(frameNum) +
		messageDelimiter();
    };
	
	inline static std::string reset()
    {
        return RESET_ALL +
		messageDelimiter();
    };
	
	inline static std::string togglePause()
    {
        return TOGGLE_PAUSE +
		messageDelimiter();
    };
	
	inline static std::string dataMessage( const std::string & msg )
    {
        return dataMessage( msg, std::vector<uint32_t>() );
    };
	
	static std::string dataMessage(const std::string & msg, const std::vector<uint32_t> & toClientIDs)
    {
        std::string sanitizedMessage = cleanMessage( msg );
        std::string sendMessage = DATA_MESSAGE + dataMessageDelimiter() + sanitizedMessage;
        for ( int i = 0; i < toClientIDs.size(); ++i ){
            if (i == 0) {
                sendMessage += dataMessageDelimiter();
            }
            else {
                sendMessage += ",";
            }
            sendMessage += std::to_string(toClientIDs[i]);
        }
        sendMessage += messageDelimiter();
        return sendMessage;
    };
    
    // The TCP client listens to the socket until it reaches the delimiter,
    // at which point the string is parsed.
	inline static const std::string& messageDelimiter()
    {
        return kMessageTerminus;
    }
	
	inline static const std::string& dataMessageDelimiter()
    {
        return kDataMessageDelimiter;
    }
	
	inline static std::string nextFrame( uint64_t frameNum )
	{
		return NEXT_FRAME +
		std::to_string(frameNum);
	}
	
	inline static void parseClient( const std::string & serverMessage, ClientMessageHandler *handler )
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
		
        if ( command == Protocol::RESET_ALL ) {
            handler->setCurrentRenderFrame( 1 );
            handler->receivedResetCommand();
        }
        else if ( command == Protocol::NEXT_FRAME ) {
            std::string frameNum = tokens[1];
            handler->setCurrentRenderFrame( stol( frameNum ) );
			
            if ( tokens.size() > 2 ) {
                // There are additional client messages
                for ( int i = 2; i < tokens.size(); i++ ) {
                    // Iterate over the messages and send them out
                    std::string dataMessage = tokens[i];
                    size_t firstComma = dataMessage.find_first_of( "," );
                    if ( firstComma != std::string::npos ) {
                        int clientID = stoi( dataMessage.substr( 0, firstComma ) );
                        std::string messageData = dataMessage.substr( firstComma+1 );
                        handler->receivedStringMessage( messageData, clientID );
                    }
                    else {
                        CI_LOG_E("Couldn't parse data message " << dataMessage );
                    }
                }
            }
			
            handler->setFrameIsReady( true );
        }
        else
        {
            CI_LOG_E( "Don't know what to do with server message: " << serverMessage );
            return;
        }
    }
	
	inline static void parseServer( const std::string &clientMessage, ServerMessageHandler *handler )
	{
		
	}
};

}
