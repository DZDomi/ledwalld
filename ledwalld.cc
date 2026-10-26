#include <iostream>
#include <sstream>
#include <fstream>

#include "led-matrix.h"

#include "communicator.h"
#include "led.h"
#include "models/request.pb.h"
#include "util.h"

using namespace std;
using namespace util;

int main(int argc, char *argv[]) {
    
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    Led *led = Led::getInstance();
	Communicator *com = Communicator::getInstance("/var/run/led.sock");
	com->acceptConnections();
    
	while(true){
        string data;
        try {
            data = com->getRequest();
        } catch(std::runtime_error &e){
            log("main", e.what());
            //Try to wait for the next connection
            com->acceptConnections();
            continue;
        }
        
        ledwalld::Request request;
        request.ParseFromString(data);
        log("main", ledwalld::Request::Action_Name(request.action()) + " message from: " + request.sender());
        
		switch(request.action()){
            case ledwalld::Request::TEXT:
                led->printText(request.textrequest().text());
				break;
			case ledwalld::Request::PICTURE:
				//TODO: Implement Me
				break;
			case ledwalld::Request::GIF: {
                led->printPicture(request.gifrequest().gif());
				break;
            }
			case ledwalld::Request::VIDEO:
				//TODO: Implement Me
				break;
            case ledwalld::Request::CANCEL:
                led->cancelAction();
                break;
		}
	}
	delete com;
    google::protobuf::ShutdownProtobufLibrary();
    
    return EXIT_SUCCESS;
}
