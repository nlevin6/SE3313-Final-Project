import socket

def main():
    server_ip = '127.0.0.1'
    server_port = 3000

    # Create a client socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        client_socket.connect((server_ip, server_port))
        print("Successfully connected")

        # Keep connection alive until user types 'done'
        while True:
            user_input = input("Enter done to close: ")
            if user_input == 'done':
                break

    except Exception as e:
        print(f"An error has occured {e}")

    finally:
        client_socket.close()
        print("Connection was closed.")

if __name__ == "__main__":
    main()