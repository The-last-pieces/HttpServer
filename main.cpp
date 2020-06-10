#include "http.h"

int main(int arg, const char* args[]) {

	std::locale::global(std::locale(""));
	int port = 8802;
	string IP = "59.110.229.171";
	if (arg >= 2) {
		port = stoi(args[1]);
	}
	if (arg >= 3) {
		IP = args[2];
	}

	try
	{
		Http server(port, IP);
		server.ListenPort();
	}
	catch (const std::exception& err)
	{
		cout << err.what() << endl;
	}

	system("pause");
	return 0;
}