#include "core/Application.h"
#include <iostream>

int main() {
	try {
		Fishy::Application app;
		app.Run();
	} catch (const std::exception& e) {
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return -1;
	}
	return 0;
}
