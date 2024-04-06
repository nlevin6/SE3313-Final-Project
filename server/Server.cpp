#include "socketserver.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

using namespace Sync;

std::atomic<bool> terminateServer(false); // Global atomic flag to control server termination
std::vector<std::thread> clientThreads;   // Vector to store client threads
std::atomic<int> playerCount(0);          // Global atomic variable to track player count
std::vector<Socket> connectedClients;     // Vector to store connected clients
std::mutex clientMutex;                   // Mutex to protect access to connectedClients vector

void BroadcastToAll(const ByteArray& data, int playerId) {
    std::lock_guard<std::mutex> lock(clientMutex);
    for (auto& client : connectedClients) {
        try {
            client.Write(data);
        } catch (const std::string& error) {
            std::cerr << "Error sending data to client: " << error << std::endl;
        }
    }
    std::cout << "Broadcasted message from Player " << playerId << ": " << data.ToString() << std::endl;
}

void HandleClient(Socket client) {

    if (playerCount >= 2) {
        std::cout << "Lobby has reached maximum amount of players" << std::endl;
        return;
    }

    int playerId;
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        playerId = ++playerCount; // Increment player count and assign playerId
        connectedClients.push_back(client); // Add client to the list of connected clients
    }

    try {
        std::cout << "Player " << playerId << " connected" << std::endl;
        
        // As long as server is running, read data
        while (!terminateServer) {
            ByteArray data;
            int bytesRead = client.Read(data);

            // Message when client leaves server
            if (bytesRead <= 0) {
                std::cout << "Player " << playerId << " disconnected" << std::endl;
                break;
            }

            // Convert received message to upper case
            std::transform(data.v.begin(), data.v.end(), data.v.begin(), ::toupper);

            // Broadcast the transformed data to all clients
            BroadcastToAll(data, playerId);
        }
    } catch (const std::string &error) {
        std::cerr << "Error: " << error << std::endl;
    }

    // Remove the client from the list of connected clients
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        auto it = std::find(connectedClients.begin(), connectedClients.end(), client);
        if (it != connectedClients.end()) {
            connectedClients.erase(it);
        }
    }
}

// Continuously read input from server terminal
void ReadServerInput(SocketServer& server) {
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        // If user wants to stop the server to gracefully terminate it
        if (input == "stop server") {
            std::cout << "Received request to stop server. Terminating..." << std::endl;
            terminateServer = true;
            server.Shutdown();
            break;
        }
    }
}

int main() {
    try {
        SocketServer server(3000);
        std::cout << "Server started. Waiting for players..." << std::endl;

        // Start a thread to continuously read input from server terminal
        std::thread inputThread(ReadServerInput, std::ref(server));

        while (!terminateServer) {
            Socket client = server.Accept();
            std::thread clientThread(HandleClient, std::move(client));
            clientThread.detach(); // Detach client thread to let it run independently
        }

        // Wait for the input thread to finish
        inputThread.join();
    } catch (const std::string &error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }
    std::cout << "Server terminated gracefully." << std::endl;
    return 0;
}
