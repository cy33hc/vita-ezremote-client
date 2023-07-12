
from urllib.parse import urlparse
from urllib.parse import parse_qs
import http.server
import socketserver
import webbrowser
import requests
import time
import argparse

port = None
client_id = None
client_secret = None
token_json = None
success_auth = False

class CustomHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    responsed = False

    def do_GET(self):
        global client_id
        global client_secret
        global token_json
        global success_auth
        global port

        parsed = urlparse(self.path)
        if parsed.path == '/auth':
            url = parse_qs(parsed.query)["url"][0]
            client_id = parse_qs(parsed.query)["client_id"][0]
            client_secret = parse_qs(parsed.query)["client_secret"][0]
            success_auth = False
            webbrowser.open(url, new=0, autoraise=True)
            self.send_response(200)
            self.end_headers()
            self.wfile.write(bytes("Authentication started successfully\r\n", "utf-8"))
        elif parsed.path == '/get_auth':
            if not success_auth:
                self.send_response(428)
                self.end_headers()
                self.wfile.write(bytes("Waiting for user response\r\n", "utf-8"))
            else:
                print("==== Access Token ====")
                print(token_json)
                self.send_response(200)
                self.end_headers()
                self.wfile.write(bytes(str(token_json), "utf-8"))
        elif parsed.path == '/google_auth':
            auth_code = parse_qs(parsed.query)["code"][0]
            print("==== Authentication Code ====")
            print(auth_code)
            post_data = ("code=" + auth_code + 
                        "&client_id=" + client_id + 
                        "&client_secret=" + client_secret + 
                        "&redirect_uri=http%3A//localhost%3A" + str(port) + "/google_auth" + 
                        "&grant_type=authorization_code")
            header = {"Content-Type": "application/x-www-form-urlencoded"}
            response = requests.post("https://oauth2.googleapis.com/token", headers=header, data=post_data)

            if (response.status_code >= 200 and response.status_code < 300):
                token_json = response.json()
                print("==== Access Token ====")
                print(token_json)
                success_auth = True
                self.send_response(200)
                self.end_headers()
                self.wfile.write(bytes("Login Successful. Wait for at least 15 secs for the Vita to get the access token before you stop this server and return to the Vita.\r\n", "utf-8"))
            else:
                self.send_response(500)
                self.end_headers()
                self.wfile.write(bytes(str(response.text)+"\r\n", "utf-8"))

def parse_args():
    """Parse and save command line arguments"""
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", default=8080, type=int,
                        help="Port to run server on. Default 8080")
    args = parser.parse_args()

    return args

def main():
    """ main method """
    global port
    args = parse_args()
    port = args.port

    handler = CustomHttpRequestHandler
    server=socketserver.TCPServer(("", port), handler)
    print("Server started at port " + str(port) + ". Press CTRL+C to close the server.")
    try:
            server.serve_forever()
    except KeyboardInterrupt:
            server.server_close()
            print("Server Closed")

if __name__ == "__main__":
    main()