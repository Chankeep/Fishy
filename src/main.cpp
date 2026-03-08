#include "core/Application.h"
#include <iostream>

/**
 * @brief Entry point of the application.
 */
int main() {
	try {
		// Initialize LogSystem
		Fishy::LogSystem::get().init();
		FISHY_LOG_INFO("Starting Fishy Engine...");
		Fishy::Application app;
		app.run();
		FISHY_LOG_INFO("Fishy Engine shutting down normally");
	} catch (const std::exception& e) {
		FISHY_LOG_CRITICAL("Fatal Error: {}", e.what());
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return -1;
	}
	return 0;
}
