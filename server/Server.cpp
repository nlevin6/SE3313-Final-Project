#include "socketserver.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>

using namespace Sync;

std::atomic<bool> terminateServer(false);  // Global atomic flag to control server termination


class Lobby {
public:
    Lobby() : running(true), lobbyId(GetNextLobbyId()) {
        // Constructor no longer starts the thread
    }

    void Start() {
        if (!running) {
            std::cout << "Lobby is already stopped or has not started yet." << std::endl;
            return;
        }
        lobbyThread = std::thread(&Lobby::lobbyThreadFunction, this);
    }

    virtual ~Lobby() {
        if (running) {
            running = false;
            if (lobbyThread.joinable()) {
                lobbyThread.join();
            }
        }
    }

    bool AddPlayer(Socket player) {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (players.size() < 2) {
            players.push_back(std::move(player));
            std::cout << "Player successfully added. Total players now: " << players.size() << std::endl;
            return true;
        } else {
            std::cerr << "Lobby is full. Cannot add more players." << std::endl;
            return false;
        }
    }

    size_t PlayerCount() const {
        return players.size();
    }

    int GetLobbyId() const {
        return lobbyId;
    }

    static int GetNextLobbyId() {
        return nextLobbyId.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::vector<Socket> players;
    std::mutex playersMutex;
    std::atomic<bool> running;
    std::thread lobbyThread;
    int lobbyId;
    static std::atomic<int> nextLobbyId;

    void lobbyThreadFunction() {
        std::lock_guard<std::mutex> lock(playersMutex);
        std::cout << "Starting thread function with " << players.size() << " players." << std::endl;
        int playerId = 1;
        for (auto& player : players) {
            std::cout << "Launching thread for player " << playerId << std::endl;
            std::thread([this, &player, playerId]() {
                HandlePlayer(player, playerId);
            }).detach();
            playerId++;
        }
        std::cout << "All player threads launched." << std::endl;
    }

    void HandlePlayer(Socket playerSocket, int playerId) {
        while (running) {
            ByteArray data;
            int bytesRead = playerSocket.Read(data);
            if (bytesRead > 0) {
                std::string choice(data.v.begin(), data.v.end());
                ProcessPlayerChoice(playerId, choice);
            } else {
                std::cerr << "Failed to read data or connection closed for player " << playerId << std::endl;
                break;
            }
        }
    }

    void ProcessPlayerChoice(int playerId, const std::string &choice) {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (choice == "done") {
            if (playerId - 1 < players.size()) {
                players.erase(players.begin() + (playerId - 1));
                std::cout << "Player " + std::to_string(playerId) + " has left the lobby." << std::endl;
            }
            playerChoices.erase(playerId);
            return;
        }
        if (IsValidChoice(choice)) {
            playerChoices[playerId] = choice;
            CheckAllPlayersChoices();
        } else {
            SendDataToPlayer(playerId, "Invalid choice. Try again.");
        }
    }

    bool IsValidChoice(const std::string &choice) {
        return choice == "rock" || choice == "paper" || choice == "scissors";
    }

    void CheckAllPlayersChoices() {
        if (playerChoices.size() == players.size()) {
            std::string result = DetermineWinner();
            std::cout << result << std::endl; // Assuming result processing/displaying is handled here
            playerChoices.clear();
        }
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

    void SendDataToPlayer(int playerId, const std::string &message) {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (playerId - 1 < players.size()) {
            try {
                players[playerId - 1].Write(message);
            } catch (const std::string &error) {
                std::cerr << "Error sending data to player: " << error << std::endl;
            }
        }
    }

    std::unordered_map<int, std::string> playerChoices; // Store player choices
};

std::atomic<int> Lobby::nextLobbyId(1);  // Initialize static member

std::unordered_map<int, std::unique_ptr<Lobby>> lobbies;  // Lobbies in operation
std::mutex lobbiesMutex;  // Protect access to the lobbies map

void HandleClient(Socket client) {
    ByteArray data;
    client.Read(data);
    std::string choice(data.v.begin(), data.v.end());

    Lobby* allocatedLobby = nullptr;
    std::unique_lock<std::mutex> lock(lobbiesMutex);

    if (choice == "create") {
        auto newLobby = std::make_unique<Lobby>();
        int newLobbyId = newLobby->GetLobbyId();
        lobbies[newLobbyId] = std::move(newLobby);
        allocatedLobby = lobbies[newLobbyId].get();
        std::cout << "New Lobby created with ID " << newLobbyId << std::endl;
    } else if (choice == "join") {
        auto it = std::find_if(lobbies.begin(), lobbies.end(), [](const auto& pair) {
            return pair.second->PlayerCount() < 2;
        });
        if (it != lobbies.end()) {
            allocatedLobby = it->second.get();
            std::cout << "Joining existing lobby with ID " << it->first << std::endl;
        } else {
            std::cout << "No available lobby to join. Please try creating a new one." << std::endl;
        }
    }

    if (allocatedLobby && allocatedLobby->AddPlayer(std::move(client))) {
        std::cout << "Player successfully added to lobbyID " << allocatedLobby->GetLobbyId() << std::endl;
        if (allocatedLobby->PlayerCount() == 2) {  // Assuming 2 is the required number of players
            allocatedLobby->Start();
        }
    } else {
        std::cerr << "Player could not be added to the lobby." << std::endl;
    }

    lock.unlock();
}

void ReadServerInput(SocketServer &server) {
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input == "stop server") {
            std::cout << "Received request to stop server. Terminating..." << std::endl;
            server.Shutdown();
            terminateServer = true;
            break;
        }
    }
}

int main() {
    try {
        SocketServer server(3000);
        std::cout << "Server started. Waiting for players..." << std::endl;

        std::thread inputThread(ReadServerInput, std::ref(server));  // Start a thread to read server terminal input

        while (!terminateServer) {
            Socket client = server.Accept();
            std::thread clientThread(HandleClient, std::move(client));
            clientThread.detach();  // Detach client thread to let it run independently
        }

        inputThread.join();  // Wait for the input thread to finish
        
    } catch (const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }
    
    std::cout << "Server terminated gracefully." << std::endl;
    return 0;
}