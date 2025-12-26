#include "core/Application.h"
#include <iostream>

/**
 * @brief Entry point of the application.
 */
int main() {
	try {
		// Initialize LogSystem
		Fishy::LogSystem::get().init();
		Fishy::LogSystem::get().info("Starting Fishy Engine...");
		Fishy::Application app;
		app.Run();
		Fishy::LogSystem::get().info("Fishy Engine shutting down normally");
	} catch (const std::exception& e) {
		Fishy::LogSystem::get().critical("Fatal Error: {}", e.what());
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return -1;
	}
	return 0;
}
