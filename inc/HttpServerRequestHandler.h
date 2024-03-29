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
#include <mutex>

#include "json/json.h"
#include "CivetServer.h"

class WebsocketHandlerInterface : public CivetWebSocketHandler
{
	public:
		virtual bool publish(int opcode, const char* buffer, unsigned int size) = 0;
		virtual int getNbConnections() = 0;
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
		void publishJSON(const std::string & wsurl, const Json::Value & data);
		void publishBin(const std::string & wsurl, const char* buf, unsigned int size);
		void addWebSocket(const std::string & wsurl, wsFunction func = [](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { return in; });
		void addWebSocket(const std::string & wsurl, WebsocketHandlerInterface* handler);
		void removeWebSocket(const std::string & wsurl);

		int getNbConnections(const std::string & wsurl) {
			std::map<std::string,WebsocketHandlerInterface*>::iterator it = m_wsHandler.find(wsurl);
			if (it != m_wsHandler.end()) {
				return it->second->getNbConnections();
			}
			return 0;
		}

		const CivetCallbacks* getCallbacks() { return m_callbacks; }
				
	protected:
		void publish(const std::string & wsurl, int opcode, const char* buf, unsigned int size);
		const CivetCallbacks*                             m_callbacks;
		std::map<std::string, WebsocketHandlerInterface*> m_wsHandler;
};


