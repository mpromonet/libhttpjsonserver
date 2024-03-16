/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** RequestHandler.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "json/json.h"
#include "CivetServer.h"

#include "HttpServerRequestHandler.h"

/* ---------------------------------------------------------------------------
**  Civet HTTP callback 
** -------------------------------------------------------------------------*/
class RequestHandler : public CivetHandler
{
  public:
	RequestHandler(HttpServerRequestHandler::httpFunction & func, const CivetCallbacks * callbacks): m_func(func), m_callbacks(callbacks) {
	}
	  
	void log_message(const struct mg_connection *conn, const char *message) {
		if (m_callbacks->log_message) {
			m_callbacks->log_message(conn, message);
		}
	}

	bool handle(CivetServer *server, struct mg_connection *conn)
	{
		bool ret = false;
		const struct mg_request_info *req_info = mg_get_request_info(conn);
		
		log_message(conn, req_info->request_uri );
				
		// read input
		Json::Value  in = this->getInputMessage(req_info, conn);
			
		// invoke API implementation
		Json::Value out(m_func(req_info, in));
			
		// fill out
		if (out.isNull() == false)
		{
			std::string answer(Json::writeString(m_jsonWriterBuilder,out));
			log_message(conn, answer.c_str());	

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
	const CivetCallbacks *                      m_callbacks;
  
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

