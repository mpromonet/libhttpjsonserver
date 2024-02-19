/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerRequestHandler.cpp
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <mutex>
	
#include "HttpServerRequestHandler.h"
#include "RequestHandler.h"
#include "WebsocketHandler.h"


int defaultLogger(const struct mg_connection *conn, const char *message) 
{
	fprintf(stderr, "%s\n", message);
	return 0;
}

static struct CivetCallbacks _callbacks;
const struct CivetCallbacks * getCivetCallbacks(int (*logger)(const struct mg_connection *, const char *)) 
{
	memset(&_callbacks, 0, sizeof(_callbacks));
	if (logger) {
		_callbacks.log_message = logger;
	} else {
		_callbacks.log_message = &defaultLogger;
	}
	return &_callbacks;
}

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(std::map<std::string,httpFunction>& func, std::map<std::string,wsFunction>& wsfunc, const std::vector<std::string>& options, int (*logger)(const struct mg_connection *, const char *)) 
	: CivetServer(options, getCivetCallbacks(logger))
{
	// register handlers
	for (auto it : func) {
		this->addHandler(it.first, new RequestHandler(it.second, getCivetCallbacks(logger)));
	} 	
	
	// register WS handlers
	for (auto it : wsfunc) {
		WebsocketHandler* handler = new WebsocketHandler(it.second, getCivetCallbacks(logger));
		this->addWebSocketHandler(it.first, handler);
		m_wsHandler[it.first] = handler;
	} 		
}	

HttpServerRequestHandler::~HttpServerRequestHandler() {
}

void HttpServerRequestHandler::publishBin(const std::string & uri, const char* buffer, unsigned int size)
{
	publish(uri, MG_WEBSOCKET_OPCODE_BINARY, buffer, size);
}

void HttpServerRequestHandler::publishTxt(const std::string & uri, const char* buffer, unsigned int size)
{
	publish(uri, MG_WEBSOCKET_OPCODE_TEXT, buffer, size);
}

void HttpServerRequestHandler::publish(const std::string & uri, int opcode, const char* buffer, unsigned int size)
{
	WebsocketHandlerInterface* handler = NULL;
	std::map<std::string,WebsocketHandlerInterface*>::iterator it = m_wsHandler.find(uri);
	if (it != m_wsHandler.end())
	{
		handler = it->second;
	}
	if (handler) {
		handler->publish(opcode, buffer, size);
	}	
}
