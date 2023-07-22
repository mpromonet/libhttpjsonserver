/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
** 
** 
** This is an Hello World sample of libhttpjsonserver
**
** -------------------------------------------------------------------------*/

#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

#include "HttpServerRequestHandler.h"

int exitFlag = false;


void signal_handler(int sig_num)
{
	exitFlag = true;
}

int main(int argc, char* argv[]) 
{	
	int c = 0;
	const char * port = "8080";
	std::string sslCertificate;
	std::string webroot = ".";
	std::string nbthreads;
	std::string passwdFile;
	std::string authDomain = "mydomain.com";
	
	while ((c = getopt (argc, argv, "hv::" "P:p:c:t:A:D:")) != -1)
	{
		switch (c)
		{
			case 'P': port = optarg; break;
			case 'p': webroot = optarg; break;	
			case 'c': sslCertificate = optarg; break;
			case 'w': webroot = optarg; break;
			case 'T': nbthreads = optarg; break;
			case 'A': passwdFile = optarg; break;
			case 'D': authDomain = optarg; break;			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] [-P port] [-p path]" << std::endl;
				std::cout << "\t -P port  : server port (default "<< port << ")" << std::endl;
				std::cout << "\t -p path  : server root path (default "<< webroot << ")" << std::endl;
				exit(0);
			}
		}
	}


	// http options
	std::vector<std::string> options;
	options.push_back("document_root");
	options.push_back(webroot);
	options.push_back("access_control_allow_origin");
	options.push_back("*");	
	options.push_back("listening_ports");
	options.push_back(port);
	options.push_back("enable_keep_alive");
	options.push_back("yes");
	if (!sslCertificate.empty()) {
		options.push_back("ssl_certificate");
		options.push_back(sslCertificate);
	}
	if (!nbthreads.empty()) {
		options.push_back("num_threads");
		options.push_back(nbthreads);
	}
	if (!passwdFile.empty()) {
		options.push_back("global_auth_file");
		options.push_back(passwdFile);
		options.push_back("authentication_domain");
		options.push_back(authDomain);
	}	
	
	// http api callbacks
	std::map<std::string,HttpServerRequestHandler::httpFunction> httpfunc;
	httpfunc["/echo"]   = [](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { 
		return in;
	};
	httpfunc["/help"]           = [&httpfunc](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { 
		Json::Value answer;
		for (auto it : httpfunc) {
			answer.append(it.first);
		}
		return answer;
	};	
	
	std::map<std::string,HttpServerRequestHandler::httpFunction> ssefunc;
	ssefunc["/sse"]   = [](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { 
		return "";
	};

	std::map<std::string,HttpServerRequestHandler::wsFunction> wsfunc;
	Json::StreamWriterBuilder jsonWriterBuilder;
	wsfunc["/ws"]  = [&jsonWriterBuilder](const struct mg_request_info *req_info, const Json::Value & in) -> Json::Value { 
		std::string msg(Json::writeString(jsonWriterBuilder,in));
		std::cout << "message:" << msg << std::endl; 
		return in;
	};

	HttpServerRequestHandler httpServer(httpfunc, ssefunc, wsfunc, options);
	if (httpServer.getContext() == NULL)
	{
		std::cout << "Cannot listen on port:" << port << std::endl; 
	}
	else
	{
		std::cout << "Started on port:" << port << " webroot:" << webroot << std::endl; 
		signal(SIGINT, signal_handler);
		int cnt = 0;
		while (!exitFlag) {
			sleep(1);
			std::ostringstream os;
			os << "{\"value\":";
			os << (cnt++);
			os << "}";
			std::string str = os.str();
			std::cout << "send:" << str << std::endl; 
			httpServer.publishTxt("/ws", str.c_str(), str.size());
			httpServer.publishBin("/ws", str.c_str(), str.size());
			httpServer.publishSSE("/sse", str.c_str(), str.size());
		}
	}
	
	return 0;
}
