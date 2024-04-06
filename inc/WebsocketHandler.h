/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** WebsocketHandler.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "json/json.h"
#include "CivetServer.h"

#include "HttpServerRequestHandler.h"

class WebsocketHandler: public WebsocketHandlerInterface {	
	public:
		WebsocketHandler(HttpServerRequestHandler::wsFunction & func, const CivetCallbacks * callbacks): m_func(func), m_callbacks(callbacks) {
		}

		WebsocketHandler(const CivetCallbacks * callbacks): m_func([](const struct mg_request_info *, const Json::Value &) -> Json::Value{ return Json::Value(); }), m_callbacks(callbacks) {
		}

		bool publish(int opcode, const char* buffer, unsigned int size) override {
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			for (auto ws : m_ws) {
				mg_websocket_write((struct mg_connection *)ws, opcode, buffer, size);
			}
			return (m_ws.size() != 0);
		}

		int getNbConnections() override {
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			return m_ws.size();
		}

		Json::Value getConnections() {
			Json::Value json(Json::arrayValue);
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			for (auto ws : m_ws) {
				const mg_request_info *req_info = mg_get_request_info(ws);
				std::string addr(req_info->remote_addr);
				std::string port(std::to_string(req_info->remote_port));
				json.append(addr + ":" + port);
			}
			return json;
		}
			
	protected:
		void log_message(const struct mg_connection *conn, const char *message) {
			if (m_callbacks->log_message) {
				m_callbacks->log_message(conn, message);
			}
		}

		virtual bool handleConnection(CivetServer *server, const struct mg_connection *conn) {
			log_message(conn, "WS connected");	
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			m_ws.push_back(conn);
			return true;
		}

		virtual void handleReadyState(CivetServer *server, struct mg_connection *conn) {
			log_message(conn, "WS ready");	
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
			log_message(conn, "WS closed");	
			const std::lock_guard<std::mutex> lock(m_cnxMutex);
			m_ws.remove(conn);		
		}

	private:
		HttpServerRequestHandler::httpFunction      m_func;	
		std::list<const struct mg_connection *>     m_ws;	
		Json::StreamWriterBuilder                   m_jsonWriterBuilder;
		std::mutex                                  m_cnxMutex; 
		const CivetCallbacks *                      m_callbacks;

};

