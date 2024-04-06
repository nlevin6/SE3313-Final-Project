#include "socket.h"
#include <iostream>
#include <string>

using namespace Sync;

int main() {
    try {
        std::cout << "I am a client" << std::endl;
        Socket client("127.0.0.1", 3000);
		client.Open();

        while (true) {
            std::string message;
            std::cout << "Enter a message: ";
            std::getline(std::cin, message);

			// Graceful termination
			if (message == "done") {
                client.Close();
                std::cout << "Client terminated gracefully." << std::endl;
                break;
            }

            // Send user input to the server
            client.Write(ByteArray(message));

            // Receive and display the response from the server
            ByteArray response;
            int bytesRead = client.Read(response);

			// In case client lost connection to the server
            if (bytesRead <= 0) {
                std::cerr << "Error: Failed to receive server response or server disconnected." << std::endl;
                client.Close();
				std::cout << "Connection to server lost. Client terminated gracefully." << std::endl;
				break;
            }
            std::cout << "Server response: " << response.ToString() << std::endl;
        }
    } catch (const std::string &error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }

    return 0;
}
