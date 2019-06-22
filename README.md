# libhttpjsonserver
HTTP/WS server wrapping civetweb using lambda

Factorisation of http server code to implement JSON RPC over HTTP/ws

```
	// http api callbacks
	std::map<std::string,HttpServerRequestHandler::httpFunction> func;
	func["/echo"]   = [](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { 
		return in;
	};

	HttpServerRequestHandler httpServer(func, options);
	if (httpServer.getContext() != NULL)
	{
		std::cout << "Started on port:" << port << " webroot:" << webroot << std::endl; 
		signal(SIGINT, signal_handler);
		while (!exitFlag) {
			sleep(1);
		}
	}
```
