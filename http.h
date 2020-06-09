#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

#pragma comment(lib, "ws2_32.lib") 

#define HTTP_BUF_SIZE      1024     /* 缓冲区的大小 */
#define HTTP_FILENAME_LEN   256     /* 文件名长度 */

#define head_status "Status"
#define head_serv "Server: "
#define head_range "Accept-Ranges: "
#define head_length "Content-Length: "
#define head_connect "Connection: "
#define head_type "Content-Type: "	
#define code_type " ;charset=utf-8"
#define mes_line "\r\n"


using Json = map<string, string>;

class Http {
private:
	static Json Content_Type;//type映射关系
	static Json Defined_Head;//默认响应头

	WSADATA wsa_data;//全局数据

	int port = 8802;//负责通信的端口
	string IP = "127.0.0.1";//监听IP
	SOCKET client_socket = 0, local_socket = 0;//客户端,服务端(本地)的套接字  
	sockaddr_in client_info, server_info;//客户端,服务端信息
public:
	Http(int _port, const string& ip) {
		WSAStartup(MAKEWORD(2, 0), &wsa_data);
		IP = ip;
		setport(_port);
	}

	//绑定端口
	void setport(int _port) {
		port = _port;
		if (client_socket) {
			closesocket(client_socket);
		}

		//创建套接字
		client_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (INVALID_SOCKET == client_socket) {
			string errinfo = "[Web] Fail to create socket, error = " + to_string(WSAGetLastError());
			throw exception(errinfo.c_str());
		}

		server_info.sin_family = AF_INET;
		server_info.sin_port = htons(port);
		server_info.sin_addr.s_addr = inet_addr(IP.c_str());
		//监听所有IP : htonl(INADDR_ANY);

		//绑定套接字
		if (SOCKET_ERROR == bind(client_socket, (struct sockaddr*) & server_info, sizeof(server_info))) {
			closesocket(client_socket);
			string errinfo = "[Web] Fail to bind, error = " + to_string(WSAGetLastError());
			throw exception(errinfo.c_str());
		}
	}

	void ListenPort() {
		//开始监听
		listen(client_socket, SOMAXCONN);
		while (true) {
			Json buff;
			if (receive(buff)) {
				//检测到请求
				Json resp = onreadystatechange(buff);//处理请求
				respond(makeMessage(resp));//响应信息
			}
		}
	}

	~Http() {
		if (client_socket) {
			closesocket(client_socket);
		}
		WSACleanup();
	}
private:
	//发送响应消息
	bool respond(string buf) {
		if (send(local_socket, buf.c_str(), buf.size(), 0) == SOCKET_ERROR)
		{
			printf("[Web] Fail to send, error = %d\n", WSAGetLastError());
			return false;
		}
		else {
			printf("[Web] Send message : \n%s\n", buf.c_str());
			return true;
		}
	}

	//接收请求消息
	bool receive(Json& buf) {
		int len = sizeof(client_info);
		local_socket = accept(client_socket, (struct sockaddr*) & client_info, &len);//等待有效信息
		if (local_socket == INVALID_SOCKET) /* 接受失败 */
		{
			string errinfo = "[Web] Fail to accept, error = " + to_string(WSAGetLastError());
			throw exception(errinfo.c_str());
		}
		else {
			printf("[Web] Accepted address:[%s], port:[%d]\n",
				inet_ntoa(client_info.sin_addr), ntohs(client_info.sin_port));
		}
		char* rec = new char[HTTP_BUF_SIZE] {0};
		int mes_len = recv(local_socket, rec, HTTP_BUF_SIZE, 0);

		if (mes_len == SOCKET_ERROR) /* 接收失败 */
		{
			closesocket(local_socket);
			string errinfo = "[Web] Fail to recv, error = " + to_string(WSAGetLastError());
			throw exception(errinfo.c_str());
		}
		else {
			printf("[Web] Get %d byte message:\n%s\n", mes_len, rec);
		}

		if (!mes_len) {
			return false;//跳过空消息
		}

		//获取源信息
		string info = rec;
		buf = makeJson(info);

		return  buf["<filepath>"] == "page/backcontrol";
	}

	//解析Json对象,生成http响应消息
	string makeMessage(Json& info) {
		/*
		200 ok //status
		Server:
		Accept-Ranges:
		Content-Length:
		Connection:
		Content-Type:; charset=utf-8

		file_info
		*/

		string mes = GetStatus(info[head_status]) + mes_line;						//状态码
		mes = mes + head_serv + "MnZn Service <0.1>" + mes_line;					//Server 服务器名称
		mes = mes + head_range + "bytes" + mes_line;								//接受范围
		mes = mes + head_length + to_string(info["<buffer>"].length()) + mes_line;	//源数据长度
		mes = mes + head_connect + "close" + mes_line;								//连接方式
		mes = mes + head_type + info["<filetype>"] + code_type + mes_line;			//文件类型

		mes += mes_line;//头部消息分割

		mes += info["<buffer>"];//文件信息

		return mes;

		/*stringstream ss; string filestr = getfile(info["<filename>"]);
		ss << "HTTP / 1.1 200 OK\r\nServer: MnZn Server <0.1>\r\n"
			"Accept-Ranges: bytes\r\nContent-Length:" << filestr.size() << "\r\nConnection: close\r\n"
			"Content-Type:text/html; charset=utf-8\r\n\r\n" << filestr;
		return ss.str();*/
	}
	//解析http请求消息,生成Json对象
	Json makeJson(const string& mes) {
		// "<filetype>" "<buffer>" "<filepath>" "<source_url>"
		// <> 表示固有属性, 其他为 请求中 ?后携带的参数

		Json info;
		stringstream ss(mes); string temp;
		ss >> temp; ss >> info["<source_url>"]; ss >> temp;//获取网页路径 和 请求参数
		while (ss >> temp) {
			temp.pop_back();
			temp = string("<") + temp + ">";
			ss.get();
			getline(ss, info[temp], '\r');
			ss.get();
		}
		string source = info["<source_url>"]; auto it = find(source.begin(), source.end(), '?');
		string file(source.begin() + 1, it);//文件路径
		string vars(it, source.end());  //参数列表
		if (vars.length()) {
			vars.erase(vars.begin());//去掉'?'字符
			vars.push_back('\0');

			int len = int(vars.length());
			string key, value;//设置键值
			for (int i = 0, j = 0; j < len; ++j) {
				if (vars[j] == '=') {
					key = string(vars.begin() + i, vars.begin() + j);
					i = j + 1;
				}
				else if (vars[j] == '&' || vars[j] == '\0') {
					value = string(vars.begin() + i, vars.begin() + j);
					i = j + 1;

					info[key] = value;
				}
			}
		}

		string suffix(find(file.begin(), file.end(), '.'), file.end());//文件后缀
		if (suffix.length()) {
			suffix.erase(suffix.begin());
		}
		else {
			suffix = "<NULL>";
		}

		info["<filetype>"] = suffix;
		info["<filepath>"] = file;
		info["<buffer>"] = getfile(file);//文件内容

		return info;
	}
	virtual Json onreadystatechange(Json& info) {
		//info储存了请求信息,该函数定义如何处理info对象
		Json obj, res;
		obj["<filetype>"] = "json";
		obj[head_status] = "200";
		obj["<buffer>"] = "";
		if (info["<filepath>"] != "page/backcontrol") {
			cout << info["<filepath>"] << "/*********/" << endl;
			return obj;
		}
		if (info.count("expre")) {
			res["isvalid"] = "true";
			res["result"] = to_string(stod(info["expre"]) * stod(info["expre"]));
		}
		else {
			res["isvalid"] = "false";
		}
		obj["<buffer>"] = Stringify(res);

		return obj;
	}

private:

	string getfile(const string& path) {
		ifstream  fs(path);
		if (!fs)
			return "";
		return string(istreambuf_iterator<char>(fs), istreambuf_iterator<char>());
	}
	static string Stringify(Json& obj) {
		string str = "{";
		for (auto&& node : obj) {
			//key
			str.push_back('\"');
			str += node.first;
			str += "\":";
			//value
			str.push_back('\"');
			str += node.second;
			str += "\",";
		}

		if (str.back() == ',')
			str.back() = '}';
		else
			str.push_back('}');

		return str;
	}
	static string GetStatus(const string& s_code) {
		int code = 0;
		istringstream ss(s_code);
		ss >> code;
		if (to_string(code) != s_code) {
			throw exception((string("http状态码异常 : ") + s_code).c_str());
		}

		switch (code)
		{
		case 200:
			return "HTTP / 1.1 200 OK";
		case 404:
			return "HTTP / 1.1 404 Not Found";
		default:
			throw exception((string("http状态码异常 : ") + s_code).c_str());
		}
	}

};

Json Http::Content_Type = {
	{"html",    "text/html"  },
	{"gif",     "image/gif"  },
	{"jpeg",    "image/jpeg" },
	{"js",	"application/x-javascript"},
	{"json","application/json"},
	{"<NULL>","text/html"}
};