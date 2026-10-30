#!/usr/bin/env python3
"""
Web Bridge for C Chat Server Simulator
Connects browser front-end clients to the C POSIX socket server over TCP.
Uses Python Standard Library only (http.server, socket, threading, json).
"""

import http.server
import socket
import threading
import json
import uuid
import time
import os
import sys
from urllib.parse import urlparse, parse_qs

CHAT_SERVER_HOST = "127.0.0.1"
CHAT_SERVER_PORT = 8080
HTTP_PORT = 8081

# Active sessions: session_id -> { 'socket': socket, 'queue': list, 'lock': Lock, 'running': bool }
sessions = {}
sessions_lock = threading.Lock()
_last_c_check_time = 0
_last_c_alive = False

def socket_reader_thread(session_id, sock):
    sock.settimeout(1.0)
    buffer = ""
    while True:
        with sessions_lock:
            session = sessions.get(session_id)
            if not session or not session['running']:
                break

        try:
            data = sock.recv(4096)
            if not data:
                # Server closed connection
                with sessions_lock:
                    if session_id in sessions:
                        sessions[session_id]['queue'].append({"type": "disconnect", "data": "Server closed connection"})
                        sessions[session_id]['running'] = False
                break

            text = data.decode('utf-8', errors='ignore')
            buffer += text
            lines = buffer.split('\n')
            # Keep remainder in buffer
            buffer = lines[-1]

            for line in lines[:-1]:
                if line.strip():
                    with sessions_lock:
                        if session_id in sessions:
                            sessions[session_id]['queue'].append({"type": "message", "data": line})

        except socket.timeout:
            continue
        except Exception as e:
            with sessions_lock:
                if session_id in sessions:
                    sessions[session_id]['queue'].append({"type": "error", "data": str(e)})
                    sessions[session_id]['running'] = False
            break

    try:
        sock.close()
    except Exception:
        pass


class ChatBridgeHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)

        if path == '/' or path == '/index.html':
            self.serve_index()
        elif path == '/api/events':
            self.handle_sse(query)
        elif path == '/api/status':
            global _last_c_check_time, _last_c_alive
            now = time.time()
            if any(s.get('running') for s in sessions.values()):
                c_alive = True
            elif now - _last_c_check_time < 10.0:
                c_alive = _last_c_alive
            else:
                c_alive = False
                try:
                    test_s = socket.socket()
                    test_s.settimeout(0.5)
                    test_s.connect((CHAT_SERVER_HOST, CHAT_SERVER_PORT))
                    test_s.close()
                    c_alive = True
                except Exception:
                    pass
                _last_c_check_time = now
                _last_c_alive = c_alive
            self.send_json({"server_running": c_alive, "server_port": CHAT_SERVER_PORT, "active_sessions": len(sessions)})
        else:
            self.send_error(404, "Not Found")

    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(length).decode('utf-8') if length > 0 else '{}'
        try:
            data = json.loads(body)
        except Exception:
            data = {}

        if path == '/api/connect':
            self.handle_connect()
        elif path == '/api/send':
            self.handle_send(data)
        elif path == '/api/disconnect':
            self.handle_disconnect(data)
        else:
            self.send_error(404, "Not Found")

    def serve_index(self):
        index_path = os.path.join(os.path.dirname(__file__), 'web', 'index.html')
        if not os.path.exists(index_path):
            self.send_error(404, "web/index.html not found")
            return
        with open(index_path, 'rb') as f:
            content = f.read()
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def send_json(self, data, status=200):
        body = json.dumps(data).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def handle_connect(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((CHAT_SERVER_HOST, CHAT_SERVER_PORT))
            session_id = str(uuid.uuid4())

            with sessions_lock:
                sessions[session_id] = {
                    'socket': s,
                    'queue': [],
                    'running': True
                }

            t = threading.Thread(target=socket_reader_thread, args=(session_id, s), daemon=True)
            t.start()

            self.send_json({"success": True, "session_id": session_id})
        except Exception as e:
            self.send_json({"success": False, "error": f"Failed to connect to C Chat Server: {str(e)}"}, 500)

    def handle_send(self, data):
        session_id = data.get('session_id')
        message = data.get('message', '')

        with sessions_lock:
            session = sessions.get(session_id)

        if not session or not session['running']:
            self.send_json({"success": False, "error": "Invalid or disconnected session"}, 400)
            return

        try:
            # Ensure newline termination
            if not message.endswith('\n'):
                message += '\n'
            session['socket'].sendall(message.encode('utf-8'))
            self.send_json({"success": True})
        except Exception as e:
            self.send_json({"success": False, "error": str(e)}, 500)

    def handle_disconnect(self, data):
        session_id = data.get('session_id')
        with sessions_lock:
            session = sessions.pop(session_id, None)

        if session:
            session['running'] = False
            try:
                session['socket'].sendall(b"/quit\n")
                session['socket'].close()
            except Exception:
                pass
        self.send_json({"success": True})

    def handle_sse(self, query):
        session_id = query.get('session_id', [None])[0]
        with sessions_lock:
            session = sessions.get(session_id)

        if not session:
            self.send_error(400, "Session not found")
            return

        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Cache-Control', 'no-cache')
        self.send_header('Connection', 'keep-alive')
        self.end_headers()

        # Stream loop
        while True:
            with sessions_lock:
                sess = sessions.get(session_id)
                if not sess or not sess['running']:
                    break
                items_to_send = list(sess['queue'])
                sess['queue'].clear()

            for item in items_to_send:
                payload = f"data: {json.dumps(item)}\n\n"
                try:
                    self.wfile.write(payload.encode('utf-8'))
                    self.wfile.flush()
                except Exception:
                    return

            time.sleep(0.1)


def main():
    print(f"Starting Web Bridge on http://localhost:{HTTP_PORT}...")
    print(f"Bridging to C Chat Server at {CHAT_SERVER_HOST}:{CHAT_SERVER_PORT}")
    server = http.server.ThreadingHTTPServer(('0.0.0.0', HTTP_PORT), ChatBridgeHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down web bridge.")
        server.server_close()


if __name__ == '__main__':
    main()
