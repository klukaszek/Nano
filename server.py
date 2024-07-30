#!/usr/bin/env python3

import http.server
import ssl
from http.server import SimpleHTTPRequestHandler

class CORSHTTPServer(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

def run(server_class=http.server.HTTPServer, handler_class=CORSHTTPServer, port=8080):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    # Set up SSL context
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain('cert.pem', 'key.pem')  # Update with your certificate paths
    httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
    print(f"HTTPS Server serving at https://localhost:{port}")
    httpd.serve_forever()

if __name__ == "__main__":
    run()
