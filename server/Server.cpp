#include "socketserver.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map> // Add this for unordered_map

using namespace Sync;

std::atomic<bool> terminateServer(false); // Global atomic flag to control server termination
std::vector<std::thread> clientThreads;   // Vector to store client threads
std::atomic<int> playerCount(0);          // Global atomic variable to track player count
std::vector<Socket> connectedClients;     // Vector to store connected clients
std::mutex clientMutex;                   // Mutex to protect access to connectedClients vector
std::unordered_map<int, int> messageCount; // Map to store message count for each player
std::unordered_map<int, std::string> playerChoices; // Store player choices


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

std::string DetermineWinner() {
    std::string choice1 = playerChoices[1];
    std::string choice2 = playerChoices[2];
    if (choice1 == choice2) {
        return "Draw";
    } else if ((choice1 == "rock" && choice2 == "scissors") || 
               (choice1 == "scissors" && choice2 == "paper") || 
               (choice1 == "paper" && choice2 == "rock")) {
        return "Player 1 wins!";
    } else {
        return "Player 2 wins!";
    }
}

void SendDataToPlayer(const ByteArray& data, int playerId) {
    std::lock_guard<std::mutex> lock(clientMutex);
    for (size_t i = 0; i < connectedClients.size(); ++i) {
        if (i + 1 == static_cast<size_t>(playerId)) {
            Socket& client = connectedClients[i];
           
            try {
                
                client.Write(data);
                std::cout << "Sent data to Player " << playerId << ": " << data.ToString() << std::endl;
                return; // Exit the loop once data is sent to the player
            } catch (const std::string& error) {
                std::cerr << "Error sending data to client: " << error << std::endl;
                return; // Exit the loop if there's an error sending data
            }
        }
    }
    // If the loop completes without finding the player ID, print an error message
    std::cerr << "Player " << playerId << " not found." << std::endl;
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
        messageCount[playerId] == 0; // Initialize message count for the player
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
                --playerCount;
                break;
            }

            //messeage sent
              ++messageCount[playerId];
            std::string choice(data.v.begin(), data.v.end());
            if(choice != "rock" && choice != "paper" && choice != "scissors" && choice != "done") {
                std::cerr << "Invalid choice from Player " << playerId << std::endl;
                continue; // Skip to the next iteration if the choice is invalid
            }

            if(choice == "done") {
                std::cout << "Player " << playerId << " has left the game." << std::endl;
                --playerCount;
                break; // Exit if player wants to leave
            }

            {
                std::lock_guard<std::mutex> lock(clientMutex);
                playerChoices[playerId] = choice; // Store player choice
            }

            // Check if both players have made their choices
            if (playerChoices.size() == 2) {
                std::string result = DetermineWinner();
                ByteArray resultData;
                resultData.v.assign(result.begin(), result.end());
                
                BroadcastToAll(resultData, playerId); // Send result to both players
                playerChoices.clear(); // Clear choices for next round
            } else {
                // Notify waiting for other player
                std::string waitMsg = "Waiting for the other player...";
                ByteArray waitData;
                waitData.v.assign(waitMsg.begin(), waitMsg.end());
                SendDataToPlayer(waitData, playerId);
            }
        }
    } catch (const std::string &error) {
        std::cerr << "Error: " << error << std::endl;
    }
            // Convert received message to upper case
    //         std::transform(data.v.begin(), data.v.end(), data.v.begin(), ::toupper);

    //         // Broadcast the transformed data to all clients
    //         if (messageCount[1]+messageCount[2] == 2) {
    //             //TODO: add game logic
    //             SendDataToPlayer(data,playerId);
            
    //             //TODO: add the winner data to this data 
    //             // BroadcastToAll(data, playerId);
                
    //             messageCount[playerId] = 0; // Fixed assignment operator
    //         }
    //         else{ 
               
    //             SendDataToPlayer(data,playerId);
    //             std::cerr << "Error sending data to client: "<< playerId << std::endl;

    //         }

    //     }
    // } catch (const std::string &error) {
    //     std::cerr << "Error: " << error << std::endl;
    // }

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
