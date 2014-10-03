//
//  Protocol.cpp
//  Test
//
//  Created by Ryan Bartley on 8/30/14.
//
//

#include "Protocol.h"

namespace mpe {
	
const std::string Protocol::CONNECT_SYNCHRONOUS = "S";
const std::string Protocol::DONE_RENDERING = "D";
const std::string Protocol::DATA_MESSAGE = "T";
const std::string Protocol::NEXT_FRAME = "G";
const std::string Protocol::CONNECT_ASYNCHRONOUS = "A";
const std::string Protocol::RESET_ALL = "R";
const std::string Protocol::TOGGLE_PAUSE = "P";
	
const std::string Protocol::kMessageTerminus = "\n";
const std::string Protocol::kDataMessageDelimiter = "|";

}
