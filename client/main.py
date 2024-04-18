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

        # Let the player choose to create or join a game
        create_or_join = input("Do you want to create a new game or join an existing one? (create/join): ").lower()
        client_socket.send(create_or_join.encode())

        while True:
            if not waiting_for_result:  
                user_input = input("Enter rock, paper, scissors to play or 'done' to exit: ").lower()
                
                # Validate user input
                if user_input in valid_choices:
                    client_socket.send(user_input.encode())  # Send the choice to the server

                    if user_input == 'done':
                        break
                    waiting_for_result = True  
                else:
                    print("Invalid choice. Please enter rock, paper, scissors, or 'done' to exit.")
                    continue  

            # Block and wait for the server's response
            server_response = client_socket.recv(1024).decode()
            #print("Server response:", server_response)

            # Reset the waiting flag based on the server's response
            if "Waiting" not in server_response:
                waiting_for_result = False  

    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        # Close the connection
        client_socket.close()
        print("Connection closed.")

if __name__ == "__main__":
    main()
