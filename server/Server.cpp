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
    static int nextLobbyId;

    
    void HandlePlayer(Socket playerSocket, int playerId) {
        while (running) {
            ByteArray data;
            int bytesRead = playerSocket.Read(data);

            // Handle disconnection
            if (bytesRead <= 0) {
                // Properly manage disconnection...
                BroadcastMessage("Player " + std::to_string(playerId) + " disconnected.");
                return; // Exit this player's thread
            }

            std::string choice(data.v.begin(), data.v.end());
            ProcessPlayerChoice(playerId, choice);
        }
    }
    
    void CheckAllPlayersChoices() {
        if (playerChoices.size() == players.size()) {
            // All players have made their choices
            std::string result = DetermineWinner();
            BroadcastMessage(result);
            playerChoices.clear(); // Prepare for next round, assuming there will be a next round
        }
    }

    bool IsValidChoice(const std::string& choice) {
        return choice == "rock" || choice == "paper" || choice == "scissors";
    }

    void RemovePlayer(int playerId) {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (playerId - 1 < players.size()) {
            players.erase(players.begin() + (playerId - 1));
        }
        playerChoices.erase(playerId);
        BroadcastMessage("Player " + std::to_string(playerId) + " has left the lobby.");
    }

    void ProcessPlayerChoice(int playerId, const std::string& choice) {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (choice == "done") {
            // Handle player wanting to leave
            RemovePlayer(playerId);
            return;
        }

        if (IsValidChoice(choice)) {
            playerChoices[playerId] = choice;
            CheckAllPlayersChoices();
        } else {
            SendDataToPlayer(playerId, "Invalid choice. Try again.");
        }
    }

    void SendDataToPlayer(int playerId, const std::string& message) {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (playerId - 1 < players.size()) {
            try {
                players[playerId - 1].Write(message);
            } catch (const std::string& error) {
                std::cerr << "Error sending data to player: " << error << std::endl;
            }
        }
    }

    void lobbyThreadFunction()
    {
        //start a thread for each player to handle their input
        int playerId = 1;
        for (auto& player : players) {
            std::thread([this, player, playerId]() { HandlePlayer(player, playerId); }).detach();
            playerId++;
        }
    }

    std::string DetermineWinner() {
        std::string choice1 = playerChoices[1];
        std::string choice2 = playerChoices[2];
        if (choice1 == choice2)
        {
            return "Draw";
        }
        else if ((choice1 == "rock" && choice2 == "scissors") ||
                (choice1 == "scissors" && choice2 == "paper") ||
                (choice1 == "paper" && choice2 == "rock"))
        {
            return "Player 1 wins!";
        }
        else
        {
            return "Player 2 wins!";
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
            players.push_back(std::move(player));
            BroadcastMessage("A new player has joined the lobby.");
            return true;
        }
        return false;
    }

    // Get the current player count
    size_t PlayerCount() const
    {
        return players.size();
    }

    // Broadcast a message to all players in the lobby
    void BroadcastMessage(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        // Send the message to all connected players
        for (auto& player : players){
            try {
                player.Write(message);
            } catch (const std::string& error){
                std::cerr << "Could not broadcast message to client: " << error << std::endl;
            }
        }
    }

    int GetLobbyId() const
    {
        return lobbyId;
    }

    static int GetNextLobbyId()
    {
        return nextLobbyId++;
    }

    Lobby() : running(true), lobbyId(GetNextLobbyId())
    {
        // Start the thread
        lobbyThread = std::thread(&Lobby::lobbyThreadFunction, this);
    }
};

int Lobby::nextLobbyId = 1; // Initialize static member

std::atomic<bool> terminateServer(false);           // Global atomic flag to control server termination
std::vector<std::thread> clientThreads;             // Vector to store client threads
std::atomic<int> playerCount(0);                    // Global atomic variable to track player count
std::vector<Socket> connectedClients;               // Vector to store connected clients
std::mutex clientMutex;                             // Mutex to protect access to connectedClients vector
std::unordered_map<int, int> messageCount;          // Map to store message count for each player
std::unordered_map<int, std::string> playerChoices; // Store player choices
std::unordered_map<int, Lobby> lobbies;             // Lobbies in operation
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

/*
std::string DetermineWinner()
{
    std::string choice1 = playerChoices[1];
    std::string choice2 = playerChoices[2];
    if (choice1 == choice2)
    {
        return "Draw";
    }
    else if ((choice1 == "rock" && choice2 == "scissors") ||
             (choice1 == "scissors" && choice2 == "paper") ||
             (choice1 == "paper" && choice2 == "rock"))
    {
        return "Player 1 wins!";
    }
    else
    {
        return "Player 2 wins!";
    }
}*/

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
        int newLobbyId = Lobby::GetNextLobbyId();
        auto &lobby = lobbies[newLobbyId];
        allocatedLobby = &lobby;
        std::cout << "New Lobby created with ID " << newLobbyId << std::endl;
    }
    else if (choice == "join")
    {
        // Attempt to find a lobby with space for more players
        auto it = std::find_if(lobbies.begin(), lobbies.end(), [](const std::pair<const int, Lobby> &pair){ 
            return pair.second.PlayerCount() < 2; 
        });

        if (it != lobbies.end())
        {

            allocatedLobby = &(it->second);
            std::cout << "Joining existing lobby with ID " << it->first << std::endl;
        }
        else
        {
            // Handle case where no available lobby exists
            std::cout << "No available lobby to join. Please try creating a new one." << std::endl;
        }
    }

    if (allocatedLobby != nullptr && allocatedLobby->AddPlayer(std::move(client))){
        std::cout << "Player successfully added to lobbyID " << allocatedLobby->GetLobbyId() << std::endl;
        // Send confirmation to client
    }
    else {
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
