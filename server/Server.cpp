#include "socketserver.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map> // Add this for unordered_map

using namespace Sync;

class Lobby
{
private:
    std::vector<Socket> players;
    std::mutex playersMutex;
    std::atomic<bool> running;
    std::unordered_map<int, std::string> playerChoices; // Tracks player choices in the game
    std::thread lobbyThread;
    int lobbyId;
    static std::atomic<int> nextLobbyId;

    void HandlePlayer(Socket playerSocket, int playerId)
    {
        while (running)
        {
            ByteArray data;
            int bytesRead = playerSocket.Read(data);
            std::string choice(data.v.begin(), data.v.end());

            ProcessPlayerChoice(playerId, choice);
        }
    }

    void CheckAllPlayersChoices()
    {
        std::cout << "1. playerChoices.size(): " << playerChoices.size() << std::endl;
        std::string result = DetermineWinner();
        //playerChoices.clear();
        //std::cout << "2. playerChoices.size(): " << playerChoices.size() << std::endl;
    }

    bool IsValidChoice(const std::string &choice)
    {
        return choice == "rock" || choice == "paper" || choice == "scissors";
    }

    bool NoPlayersLeft() const
    {
        return players.empty();
    }

    void ProcessPlayerChoice(int playerId, const std::string &choice)
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (choice == "done")
        {
            // Handle player wanting to leave
            if (playerId - 1 < players.size())
            {
                players.erase(players.begin() + (playerId - 1));
                std::cout << "Player " + std::to_string(playerId) + " has left the lobby." << std::endl;
            }
            playerChoices.erase(playerId);
            return;
        }

        if (IsValidChoice(choice))
        {
            playerChoices[playerId] = choice;
            CheckAllPlayersChoices();
        }
        else
        {
            SendDataToPlayer(playerId, "Invalid choice. Try again.");
        }
    }

    void SendDataToPlayer(int playerId, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (playerId - 1 < players.size())
        {
            try
            {
                players[playerId - 1].Write(message);
            }
            catch (const std::string &error)
            {
                std::cerr << "Error sending data to player: " << error << std::endl;
            }
        }
    }

    void lobbyThreadFunction()
    {
        // start a thread for each player to handle their input
        int playerId = 1;
        for (auto &player : players)
        {
            std::thread([this, player, playerId]()
                        { HandlePlayer(player, playerId); })
                .detach();
            playerId++;
        }
    }

    std::string DetermineWinner()
    {
        std::string choice1 = playerChoices[1];
        std::string choice2 = playerChoices[2];
        if (choice1 == choice2)
        {
            std::cout << "Draw" << std::endl;
            return "Draw";
        }
        else if ((choice1 == "rock" && choice2 == "scissors") ||
                 (choice1 == "scissors" && choice2 == "paper") ||
                 (choice1 == "paper" && choice2 == "rock"))
        {
            std::cout << "Player 2 wins" << std::endl;
            return "Player 2 wins!";
        }
        else
        {
            std::cout << "Player 1 wins" << std::endl;
            return "Player 1 wins!";
        }
    }

public:
    virtual ~Lobby()
    {
        // Ensure the thread is properly joined on destruction
        if (running)
        {
            running = false;
            if (lobbyThread.joinable())
            {
                lobbyThread.join();
            }
        }
    }

    // Add a player to the lobby
    bool AddPlayer(Socket player)
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (players.size() < 2)
        {
            std::cout << "Adding a new player to the lobby." << std::endl;
            players.push_back(std::move(player));
            std::cout << "Attempting to broadcast message..." << std::endl;

            // Create a ByteArray from the string literal
            Sync::ByteArray message("Welcome to the lobby!\n");

            for (auto &player : players)
            {
                std::cout << "Broadcasting to one player..." << std::endl; // Check if this gets printed
                try
                {
                    player.Write(message); // Use the ByteArray object
                }
                catch (const std::string &error)
                {
                    std::cerr << "Could not broadcast message to client: " << error << std::endl;
                }
            }
            return true;
        }
        else
        {
            std::cerr << "Lobby is full. Cannot add more players." << std::endl;
            return false;
        }
    }

    // Get the current player count
    size_t PlayerCount() const
    {
        return players.size();
    }

    int GetLobbyId() const
    {
        return lobbyId;
    }

    static int GetNextLobbyId()
    {
        return nextLobbyId.fetch_add(1, std::memory_order_relaxed);
    }

    Lobby() : running(true), lobbyId(GetNextLobbyId())
    {
        // Start the thread
        lobbyThread = std::thread(&Lobby::lobbyThreadFunction, this);
    }
};

std::atomic<int> Lobby::nextLobbyId(1); // Initialize static member

std::atomic<bool> terminateServer(false);           // Global atomic flag to control server termination
std::vector<std::thread> clientThreads;             // Vector to store client threads
std::atomic<int> playerCount(0);                    // Global atomic variable to track player count
std::vector<Socket> connectedClients;               // Vector to store connected clients
std::mutex clientMutex;                             // Mutex to protect access to connectedClients vector
std::unordered_map<int, int> messageCount;          // Map to store message count for each player
std::unordered_map<int, std::string> playerChoices; // Store player choices
std::unordered_map<int, std::unique_ptr<Lobby>> lobbies;             // Lobbies in operation
std::mutex lobbiesMutex;                            // Protect access to the lobbies map

void BroadcastToAll(const ByteArray &data, int playerId)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (auto &client : connectedClients)
    {
        try
        {
            client.Write(data);
        }
        catch (const std::string &error)
        {
            std::cerr << "Error sending data to client: " << error << std::endl;
        }
    }
    std::cout << "Broadcasted message from Player " << playerId << ": " << data.ToString() << std::endl;
}

void SendDataToPlayer(const ByteArray &data, int playerId)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (size_t i = 0; i < connectedClients.size(); ++i)
    {
        if (i + 1 == static_cast<size_t>(playerId))
        {
            Socket &client = connectedClients[i];

            try
            {

                client.Write(data);
                std::cout << "Sent data to Player " << playerId << ": " << data.ToString() << std::endl;
                return; // Exit the loop once data is sent to the player
            }
            catch (const std::string &error)
            {
                std::cerr << "Error sending data to client: " << error << std::endl;
                return; // Exit the loop if there's an error sending data
            }
        }
    }
    // If the loop completes without finding the player ID, print an error message
    std::cerr << "Player " << playerId << " not found." << std::endl;
}

void HandleClient(Socket client)
{

    ByteArray data;

    client.Read(data);

    std::string choice(data.v.begin(), data.v.end());

    Lobby *allocatedLobby = nullptr;
    std::unique_lock<std::mutex> lock(lobbiesMutex);

    if (choice == "create")
    {
        auto newLobby = std::unique_ptr<Lobby>(new Lobby);
        int newLobbyId = newLobby->GetLobbyId();
        lobbies[newLobbyId] = std::move(newLobby); 
        allocatedLobby = lobbies[newLobbyId].get();
        std::cout << "New Lobby created with ID " << newLobbyId << std::endl;
    }
    else if (choice == "join")
    {
        // Attempt to find a lobby with space for more players
        auto it = std::find_if(lobbies.begin(), lobbies.end(), [](const std::pair<const int, std::unique_ptr<Lobby>>& pair) { 
            return pair.second->PlayerCount() < 2; 
        });

        if (it != lobbies.end())
        {

            allocatedLobby = it->second.get();
            std::cout << "Joining existing lobby with ID " << it->first << std::endl;
        }
        else
        {
            // Handle case where no available lobby exists
            std::cout << "No available lobby to join. Please try creating a new one." << std::endl;
        }
    }

    if (allocatedLobby != nullptr && allocatedLobby->AddPlayer(std::move(client)))
    {
        std::cout << "Player successfully added to lobbyID " << allocatedLobby->GetLobbyId() << std::endl;
        // Send confirmation to client
    }
    else
    {
        std::cerr << "Player could not be added to the lobby." << std::endl;
        // Send error to the client
    }

    lock.unlock();

    // Below this is the old handle client, before the Lobby
    // These would normally be handled inside Lobby's while loop per Lobby instance
}

// Continuously read input from server terminal
void ReadServerInput(SocketServer &server)
{
    std::string input;
    while (true)
    {
        std::getline(std::cin, input);
        // If user wants to stop the server to gracefully terminate it
        if (input == "stop server")
        {
            std::cout << "Received request to stop server. Terminating..." << std::endl;
            terminateServer = true;
            server.Shutdown();
            break;
        }
    }
}

int main()
{
    try
    {
        SocketServer server(3000);
        std::cout << "Server started. Waiting for players..." << std::endl;

        // Start a thread to continuously read input from server terminal
        std::thread inputThread(ReadServerInput, std::ref(server));

        while (!terminateServer)
        {
            Socket client = server.Accept();
            std::thread clientThread(HandleClient, std::move(client));
            clientThread.detach(); // Detach client thread to let it run independently
        }

        // Wait for the input thread to finish
        inputThread.join();
    }
    catch (const std::string &error)
    {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }
    std::cout << "Server terminated gracefully." << std::endl;
    return 0;
}
