//
//  ServerBase.hpp
//  Test
//
//  Created by Ryan Bartley on 8/31/14.
//
//

#pragma once

#include "Messagehandler.hpp"

namespace mpe {
	
class ServerBase : public ServerMessageHandler {
public:
	
	ServerBase() : ServerMessageHandler() {}
	
	virtual void start() = 0;
	virtual void start( uint16_t ) = 0;
	
	virtual void stop() = 0;
	
	virtual void update() = 0;
	
	
protected:
};
	
}
