#! /usr/bin/python
import argparse
import socket
import sys

def send_command(action, item_type, file_path, host, port):
    """Formats and sends a command to the server with a 4-digit length prefix."""
    payload = f"{action.upper()} {item_type.upper()} {file_path}"
    
    total_length = len(payload)
    length_prefix = f"{total_length:04d} "
    
    full_message = f"{length_prefix}{payload}"
    print(f"Connecting to {host}:{port}...")
    print(f"Sending payload: '{full_message}'")
    
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            client_socket.settimeout(5.0)  
            client_socket.connect((host, port))
            client_socket.sendall(full_message.encode('utf-8'))
            
            response = client_socket.recv(1024).decode('utf-8')
            print(f"Server response: '{response}'")
    except ConnectionRefusedError:
        print(f"Error: Connection refused. Is the server running on port {port}?", file=sys.stderr)
    except socket.timeout:
        print("Error: Connection timed out.", file=sys.stderr)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)

def main():
    parser = argparse.ArgumentParser(
        description="CLI tool to send file/folder event notifications to a server."
    )
    
  
    parser.add_argument("action", choices=["ADD", "REMOVE", "add", "remove"], 
                        help="The action to perform.")
    parser.add_argument("type", choices=["FILE", "FOLDER", "file", "folder"], 
                        help="The type of target object.")
    parser.add_argument("path", help="The absolute or relative file system path.")
    

    parser.add_argument("--host", default="127.0.0.1", help="Server IP address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=4001, help="Server port configuration (default: 4001)")

    args = parser.parse_args()

    send_command(
        action=args.action,
        item_type=args.type,
        file_path=args.path,
        host=args.host,
        port=args.port
    )

if __name__ == "__main__":
    main()
