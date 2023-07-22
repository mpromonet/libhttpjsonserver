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
**  Civet HTTP callback 
** -------------------------------------------------------------------------*/
class RequestHandler : public CivetHandler
{
  public:
	RequestHandler(HttpServerRequestHandler::httpFunction & func): m_func(func) {
	}

	virtual ~RequestHandler() {
	}	
	  
	virtual bool handle(CivetServer *server, struct mg_connection *conn)
	{
		bool ret = false;
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		
		_callbacks.log_message(conn, req_info->request_uri );	
				
		// read input
		Json::Value  in = this->getInputMessage(req_info, conn);
			
		// invoke API implementation
		Json::Value out(m_func(req_info, in));
			
		// fill out
		if (out.isNull() == false)
		{
			std::string answer(Json::writeString(m_jsonWriterBuilder,out));
			_callbacks.log_message(conn, answer.c_str());	

			mg_printf(conn,"HTTP/1.1 200 OK\r\n");
			mg_printf(conn,"Access-Control-Allow-Origin: *\r\n");
			mg_printf(conn,"Content-Type: application/json\r\n");
			mg_printf(conn,"Content-Length: %zd\r\n", answer.size());
			mg_printf(conn,"Connection: close\r\n");
			mg_printf(conn,"\r\n");
			mg_write(conn,answer.c_str(),answer.size());
			
			ret = true;
		}			
		
		return ret;
	}
	virtual bool handleGet(CivetServer *server, struct mg_connection *conn)
	{
		return handle(server, conn);
	}
	virtual bool handlePost(CivetServer *server, struct mg_connection *conn)
	{
		return handle(server, conn);
	}

  protected:
	HttpServerRequestHandler::httpFunction      m_func;	  
	Json::StreamWriterBuilder                   m_jsonWriterBuilder;
	Json::CharReaderBuilder                     m_jsonReaderbuilder;
  
	Json::Value getInputMessage(const struct mg_request_info *req_info, struct mg_connection *conn) {
		Json::Value  jmessage;

		// read input
		long long tlen = req_info->content_length;
		if (tlen > 0)
		{
			std::string body = CivetServer::getPostData(conn);

			// parse in
			std::unique_ptr<Json::CharReader> reader(m_jsonReaderbuilder.newCharReader());
			std::string errors;
			if (!reader->parse(body.c_str(), body.c_str() + body.size(), &jmessage, &errors))
			{
				std::cout << "Received unknown message:" << body << " errors:" << errors << std::endl;
			}
		}
		return jmessage;
	}	
};

class SSERequestHandler : public RequestHandler
{
  public:
	SSERequestHandler(HttpServerRequestHandler::httpFunction & func): RequestHandler(func) {
	}

	virtual ~SSERequestHandler() {

	}
	  
	bool handle(CivetServer *server, struct mg_connection *conn)
	{
		bool ret = false;
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		
		_callbacks.log_message(conn, req_info->request_uri );	
				
		// read input
		Json::Value  in = this->getInputMessage(req_info, conn);
			
		// invoke API implementation
		Json::Value out(m_func(req_info, in));
			

		mg_printf(conn,"HTTP/1.1 200 OK\r\n");
		mg_printf(conn,"Access-Control-Allow-Origin: *\r\n");
		mg_printf(conn,"Content-Type: text/event-stream\r\n");
		mg_printf(conn,"Transfer-Encoding: chunked\r\n");
		mg_printf(conn,"Connection: keep-alive\r\n");
		mg_printf(conn,"\r\n");

		m_connectionList.push_back(conn);
		
		return true;
	}

	virtual bool publish(const char* buffer, unsigned int size) {
		const std::lock_guard<std::mutex> lock(m_cnxMutex);
		for (auto conn : m_connectionList) {
			mg_printf(conn, "data: %s\n\n", buffer);
		}
		return true;
	}

	std::mutex                     m_cnxMutex; 
	std::list<mg_connection *>     m_connectionList;	
};


class WebsocketHandler: public WebsocketHandlerInterface {	
	public:
		WebsocketHandler(HttpServerRequestHandler::wsFunction & func): m_func(func) {
		}
		
		virtual bool publish(int opcode, const char* buffer, unsigned int size) {
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			for (auto ws : m_ws) {
				mg_websocket_write((mg_connection *)ws, opcode, buffer, size);
			}
			return (m_ws.size() != 0);
		}
		
	private:
		HttpServerRequestHandler::httpFunction      m_func;	
		std::list<const struct mg_connection *>     m_ws;	
		Json::StreamWriterBuilder                   m_jsonWriterBuilder;
		std::mutex                                  m_cnxMutex; 
	
		virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn) {
			_callbacks.log_message(conn, "WS connected");	
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			m_ws.push_back(conn);
			return true;
		}

		virtual void handleReadyState(CivetServer *server, struct mg_connection *conn) {
			_callbacks.log_message(conn, "WS ready");	
		}

		virtual bool handleData(CivetServer *server,
					struct mg_connection *conn,
					int bits,
					char *data,
					size_t data_len) {
			int opcode = bits&0xf;
						
			if (opcode == MG_WEBSOCKET_OPCODE_TEXT) {
				// parse in
				std::string body(data, data_len);
				Json::CharReaderBuilder builder;
				std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
				Json::Value in;
				if (!reader->parse(body.c_str(), body.c_str() + body.size(), &in, NULL))
				{
					std::cout << "Received unknown message:" << body << std::endl;
				}
						
				// invoke API implementation
				const struct mg_request_info *req_info = mg_get_request_info(conn);
				Json::Value out(m_func(req_info, in));
				
				std::string answer(Json::writeString(m_jsonWriterBuilder,out));
				mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, answer.c_str(), answer.size());
			}
			
			return true;
		}

		virtual void handleClose(CivetServer *server, const struct mg_connection *conn) {
			_callbacks.log_message(conn, "WS closed");	
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			m_ws.remove(conn);		
		}
		
};

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(std::map<std::string,httpFunction>& func, std::map<std::string,httpFunction>& ssefunc, std::map<std::string,wsFunction>& wsfunc, const std::vector<std::string>& options, int (*logger)(const struct mg_connection *, const char *)) 
	: CivetServer(options, getCivetCallbacks(logger))
{
	// register handlers
	for (auto it : func) {
		this->addHandler(it.first, new RequestHandler(it.second));
	} 	

	// register sse handlers
	for (auto it : ssefunc) {
		SSERequestHandler* handler = new SSERequestHandler(it.second);
		this->addHandler(it.first, handler);
		m_sseHandler[it.first] = handler;
	} 	

	// register WS handlers
	for (auto it : wsfunc) {
		WebsocketHandler* handler = new WebsocketHandler(it.second);
		this->addWebSocketHandler(it.first, handler);
		m_wsHandler[it.first] = handler;
	} 		
}	

HttpServerRequestHandler::~HttpServerRequestHandler() {
}

void HttpServerRequestHandler::publishBin(const std::string & uri, const char* buffer, unsigned int size)
{
	WebsocketHandlerInterface* handler = NULL;
	std::map<std::string,WebsocketHandlerInterface*>::iterator it = m_wsHandler.find(uri);
	if (it != m_wsHandler.end())
	{
		handler = it->second;
	}
	if (handler) {
		handler->publish(MG_WEBSOCKET_OPCODE_BINARY, buffer, size);
	}	
}

void HttpServerRequestHandler::publishTxt(const std::string & uri, const char* buffer, unsigned int size)
{
	WebsocketHandlerInterface* handler = NULL;
	std::map<std::string,WebsocketHandlerInterface*>::iterator it = m_wsHandler.find(uri);
	if (it != m_wsHandler.end())
	{
		handler = it->second;
	}
	if (handler) {
		handler->publish(MG_WEBSOCKET_OPCODE_TEXT, buffer, size);
	}	
}

void HttpServerRequestHandler::publishSSE(const std::string & uri, const char* buffer, unsigned int size)
{
	CivetHandler* handler = NULL;
	std::map<std::string,CivetHandler*>::iterator it = m_sseHandler.find(uri);
	if (it != m_sseHandler.end())
	{
		handler = it->second;
	}
	if (handler) {
		((SSERequestHandler*)handler)->publish(buffer, size);
	}	
}
