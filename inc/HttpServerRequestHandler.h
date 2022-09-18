/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerRequestHandler.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <list>
#include <map>
#include <functional>

#include "json/json.h"
#include "CivetServer.h"

class WebsocketHandlerInterface : public CivetWebSocketHandler
{
	public:
		virtual bool publish(int opcode, const char* buffer, unsigned int size) = 0;
};

/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
class HttpServerRequestHandler : public CivetServer
{
	public:
		typedef std::function<Json::Value(const struct mg_request_info *, const Json::Value &)> httpFunction;
		typedef std::function<Json::Value(const struct mg_request_info *, const Json::Value &)> wsFunction;
	
		HttpServerRequestHandler(std::map<std::string,httpFunction>& httpfunc, std::map<std::string,wsFunction>& wsfunc, const std::vector<std::string>& options, int (*)(const struct mg_connection *, const char *) = NULL); 
		virtual ~HttpServerRequestHandler();
	
		void publishTxt(const std::string & wsurl, const char* buf, unsigned int size);
		void publishBin(const std::string & wsurl, const char* buf, unsigned int size);
				
	protected:
		std::map<std::string, WebsocketHandlerInterface*> m_wsHandler;
};


