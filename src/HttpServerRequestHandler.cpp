/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerRequestHandler.cpp
** 
** -------------------------------------------------------------------------*/

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


static struct CivetCallbacks _callbacks;
const struct CivetCallbacks * getCivetCallbacks(int (*logger)(const struct mg_connection *, const char *)) 
{
	memset(&_callbacks, 0, sizeof(_callbacks));
	_callbacks.log_message = logger;
	return &_callbacks;
}

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(std::map<std::string,httpFunction>& func, std::map<std::string,wsFunction>& wsfunc, const std::vector<std::string>& options, int (*logger)(const struct mg_connection *, const char *)) 
	: CivetServer(options, getCivetCallbacks(logger)),
	  m_callbacks(getCivetCallbacks(logger))
{
	// register handlers
	for (auto it : func) {
		this->addHandler(it.first, new RequestHandler(it.second, m_callbacks));
	} 	
	
	// register WS handlers
	for (auto it : wsfunc) {
		this->addWebSocket(it.first, it.second);
	} 		
}

void HttpServerRequestHandler::addWebSocket(const std::string & uri, wsFunction func)
{
	WebsocketHandler* handler = new WebsocketHandler(func, m_callbacks);
	this->addWebSocket(uri, handler);
}

void HttpServerRequestHandler::addWebSocket(const std::string & uri, WebsocketHandlerInterface* handler)
{
	this->addWebSocketHandler(uri, handler);
	m_wsHandler[uri] = handler;
}

void HttpServerRequestHandler::removeWebSocket(const std::string & uri)
{
	std::map<std::string,WebsocketHandlerInterface*>::iterator it = m_wsHandler.find(uri);
	if (it != m_wsHandler.end())
	{
		this->removeWebSocketHandler(uri);
		delete it->second;
		m_wsHandler.erase(it);
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

void HttpServerRequestHandler::publishJSON(const std::string & uri, const Json::Value & data)
{
	Json::StreamWriterBuilder builder;
	std::string buffer = Json::writeString(builder, data);
	publish(uri, MG_WEBSOCKET_OPCODE_TEXT, buffer.c_str(), buffer.size());
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
