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
	
#include "HttpServerRequestHandler.h"


int log_message(const struct mg_connection *conn, const char *message) 
{
	fprintf(stderr, "%s\n", message);
	return 0;
}

static struct CivetCallbacks _callbacks;
const struct CivetCallbacks * getCivetCallbacks() 
{
	memset(&_callbacks, 0, sizeof(_callbacks));
	_callbacks.log_message = &log_message;
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
	  
	bool handle(CivetServer *server, struct mg_connection *conn)
	{
		bool ret = false;
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		
		std::cout << "uri:" << req_info->request_uri << std::endl;
				
		// read input
		Json::Value  in = this->getInputMessage(req_info, conn);
			
		// invoke API implementation
		Json::Value out(m_func(req_info, in));
			
		// fill out
		if (out.isNull() == false)
		{
			std::string answer(Json::writeString(m_jsonWriterBuilder,out));
			std::cout << "answer:" << answer << std::endl;	

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
	bool handleGet(CivetServer *server, struct mg_connection *conn)
	{
		return handle(server, conn);
	}
	bool handlePost(CivetServer *server, struct mg_connection *conn)
	{
		return handle(server, conn);
	}

  private:
	HttpServerRequestHandler::httpFunction      m_func;	  
	Json::StreamWriterBuilder                   m_jsonWriterBuilder;
	Json::CharReaderBuilder                     m_jsonReaderbuilder;
  
	Json::Value getInputMessage(const struct mg_request_info *req_info, struct mg_connection *conn) {
		Json::Value  jmessage;

		// read input
		long long tlen = req_info->content_length;
		if (tlen > 0)
		{
			std::string body;
			long long nlen = 0;
			const long long bufSize = 1024;
			char buf[bufSize];
			while (nlen < tlen) {
				long long rlen = tlen - nlen;
				if (rlen > bufSize) {
					rlen = bufSize;
				}
				rlen = mg_read(conn, buf, (size_t)rlen);
				if (rlen <= 0) {
					break;
				}
				body.append(buf, rlen);

				nlen += rlen;
			}

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


class WebsocketHandler: public WebsocketHandlerInterface {	
	public:
		WebsocketHandler(HttpServerRequestHandler::wsFunction & func): m_func(func) {
		}
		
		virtual bool publish(int opcode, const char* buffer, unsigned int size) {
			for (auto ws : m_ws) {
				mg_websocket_write((struct mg_connection *)ws, opcode, buffer, size);
			}
			return (m_ws.size() != 0);
		}
		
	private:
		HttpServerRequestHandler::httpFunction      m_func;	
		std::list<const struct mg_connection *>     m_ws;	
		Json::StreamWriterBuilder                   m_jsonWriterBuilder;
	
		virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn) {
			printf("WS connected\n");
			m_ws.push_back(conn);
			return true;
		}

		virtual void handleReadyState(CivetServer *server, struct mg_connection *conn) {
			printf("WS ready\n");
		}

		virtual bool handleData(CivetServer *server,
					struct mg_connection *conn,
					int bits,
					char *data,
					size_t data_len) {
			int opcode = bits&0xf;
			printf("WS got %lu bytes %u\n", (long unsigned)data_len, opcode);
						
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
			printf("WS closed\n");
			m_ws.remove(conn);		
		}
		
};

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(std::map<std::string,httpFunction>& func, std::map<std::string,wsFunction>& wsfunc, const std::vector<std::string>& options) 
	: CivetServer(options, getCivetCallbacks())
{
	// register handlers
	for (auto it : func) {
		this->addHandler(it.first, new RequestHandler(it.second));
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
