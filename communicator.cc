#include "communicator.h"

Communicator* Communicator::instance = nullptr;

Communicator* Communicator::getInstance(string socketName){
	if(instance != nullptr){
		return instance;
	}
	log("communicator", "Trying to create socket on: " + socketName);
	instance = new Communicator(socketName);
    log("communicator", "Socket successfully created");
	setUpInterruptHandler();
	return instance;
}

Communicator::Communicator(string socketName) {
	this->socketName = socketName;
	this->createSocket();
}

void Communicator::createSocket() {
	struct sockaddr_un server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, this->socketName.c_str(), sizeof(server_addr.sun_path) - 1);
	
	server = socket(PF_UNIX, SOCK_STREAM, 0);
	if(!server){
		perror("Unable to get socket");
		exit(-1);
	}
	int enable = 1;
	if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
		perror("Unable to set socket options");
		exit(-1);
	}
	
	if(::bind(server, (const struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		perror("Unable to bind the socket");
		exit(-1);
	}
	
	if(listen(server, SOMAXCONN) < 0){
		perror("Unable to listen at socket");
		exit(-1);
	}
}

void Communicator::acceptConnections() {
	struct sockaddr_in client_addr;
	socklen_t clientLength = sizeof(client_addr);
    
    log("communicator", "Listening for new connections on socket: " + this->socketName);
	client = accept(server, (struct sockaddr *) &client_addr, &clientLength);
    log("communicator", "Got connection from client: " + to_string(client));
}

string Communicator::getRequest() {
    
    int contentLength = -1;
    //Get content length of the message
    int result = recv(client, &contentLength, sizeof(int), 0);
    if(result <= 0){
        throw std::runtime_error("Connection closed");
    }
    log("communicator", "Got new message with content length: " + to_string(contentLength));
    
    int bytesRead = 0;
	char *buffer = (char *) malloc(contentLength + 1);
    
    while(bytesRead < contentLength){
        result = TEMP_FAILURE_RETRY(recv(client, buffer + bytesRead, contentLength - bytesRead, 0));
        if(result <= 0){
            throw std::runtime_error("Connection closed");
        }
        bytesRead += result;
    }
    log("communicator", "Message successfully received");
	string content = string(buffer, bytesRead);
    free(buffer);
	return content;
}

Communicator::~Communicator() {
    log("communicator", "Trying to unlink: " + this->socketName);
	unlink(this->socketName.c_str());
    log("communicator", "Successfully unlinked: " + this->socketName);
}

void Communicator::setUpInterruptHandler() {
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = instance->interrupt;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
}

void Communicator::interrupt(int) {
    log("communicator", "Trying to unlink: " + instance->socketName);
	unlink(instance->socketName.c_str());
    log("communicator", "Successfully unlinked: " + instance->socketName);
}
