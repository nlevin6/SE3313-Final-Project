import socket

def main():
    server_ip = '127.0.0.1'
    server_port = 3000

    # Create a client socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # Connect to the server
        client_socket.connect((server_ip, server_port))
        print("Successfully connected to the server.")

        waiting_for_result = False  # Initialize the flag here
        valid_choices = ['rock', 'paper', 'scissors', 'done']  # Define valid choices

        while True:
            # Let the player choose to create or join a game
            create_or_join = input("Do you want to create a new game or join an existing one? (create/join): ").lower()
            if create_or_join in ['create', 'join']:
                client_socket.send(create_or_join.encode())
                break
            else:
                print("Invalid choice. Please enter 'create' or 'join'.")

        # Wait for server's response on creation/joining
        while True:
            server_response = client_socket.recv(1024).decode()
            print("Server response:", server_response)
            if "All players have joined" in server_response:
                break
            elif "No available lobby to join" in server_response:
                return

        while True:
            if not waiting_for_result:  
                user_input = input("Enter rock, paper, scissors to play or 'done' to exit: ").lower()
                
                # Validate user input
                if user_input in valid_choices:
                    if user_input == 'done':
                        client_socket.send(user_input.encode())
                        break
                    else:
                        client_socket.send(user_input.encode())
                        waiting_for_result = True  
                else:
                    print("Invalid choice. Please enter rock, paper, scissors, or 'done' to exit.")
                    continue  

            # Block and wait for the other player's response
            print("Waiting for your opponent")
            server_response = client_socket.recv(1024).decode()
            print("Server response:", server_response)

            waiting_for_result = False  

    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        # Close the connection
        client_socket.close()
        print("Connection closed.")

if __name__ == "__main__":
    main()
