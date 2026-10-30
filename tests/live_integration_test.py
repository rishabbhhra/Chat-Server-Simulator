#!/usr/bin/env python3
"""
Integration test for Multi-Client Chat Server Simulator
Simulates multiple clients connecting over TCP sockets and interacting.
"""
import subprocess
import socket
import time
import os
import sys

PORT = 8999
SERVER_BIN = "./bin/chat_server"

def recv_until(sock, expected_substr, timeout=3.0):
    sock.settimeout(timeout)
    data = ""
    start = time.time()
    while time.time() - start < timeout:
        try:
            chunk = sock.recv(4096).decode('utf-8', errors='ignore')
            if not chunk:
                break
            data += chunk
            if expected_substr in data:
                return data
        except socket.timeout:
            break
    return data

def main():
    print("--- Starting Multi-Client Chat Server on port %d ---" % PORT)
    server_proc = subprocess.Popen([SERVER_BIN, "-p", str(PORT), "-d", "live_data", "-l", "logs/live_test.log"])
    time.sleep(1)

    try:
        # Client 1: Rishabh
        c1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        c1.connect(("127.0.0.1", PORT))
        init_c1 = recv_until(c1, "Welcome to the Multi-Client Chat Server Simulator!")
        assert "Welcome" in init_c1, "Client 1 connection banner received"
        print("[Client 1] Connected")

        # Client 2: Rahul
        c2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        c2.connect(("127.0.0.1", PORT))
        init_c2 = recv_until(c2, "Welcome to the Multi-Client Chat Server Simulator!")
        assert "Welcome" in init_c2, "Client 2 connection banner received"
        print("[Client 2] Connected")

        # Rishabh registers & logs in
        c1.sendall(b"/register Rishabh pass123\n")
        resp = recv_until(c1, "SUCCESS")
        print("[Client 1 /register]:", resp.strip())

        c1.sendall(b"/login Rishabh pass123\n")
        resp = recv_until(c1, "Welcome back, Rishabh")
        print("[Client 1 /login]:", resp.strip())

        # Rahul registers & logs in
        c2.sendall(b"/register Rahul pass456\n")
        resp = recv_until(c2, "SUCCESS")
        print("[Client 2 /register]:", resp.strip())

        c2.sendall(b"/login Rahul pass456\n")
        resp = recv_until(c2, "Welcome back, Rahul")
        print("[Client 2 /login]:", resp.strip())

        # Both join #coding
        c1.sendall(b"/join coding\n")
        resp = recv_until(c1, "Switched to room #coding")
        print("[Client 1 /join coding]:", resp.strip())

        c2.sendall(b"/join coding\n")
        resp = recv_until(c2, "Switched to room #coding")
        print("[Client 2 /join coding]:", resp.strip())

        # Drain join notifications
        time.sleep(0.5)

        # Rishabh sends public message: "Is everyone ready for the C project?"
        print("[Client 1 sends]: Is everyone ready for the C project?")
        c1.sendall(b"Is everyone ready for the C project?\n")

        # Rahul receives it
        msg_for_c2 = recv_until(c2, "Rishabh: Is everyone ready for the C project?")
        print("[Client 2 received]:", msg_for_c2.strip())
        assert "Rishabh: Is everyone ready for the C project?" in msg_for_c2

        # Rahul replies: "Yes, I am working on mine."
        print("[Client 2 sends]: Yes, I am working on mine.")
        c2.sendall(b"Yes, I am working on mine.\n")

        # Rishabh receives Rahul's message
        msg_for_c1 = recv_until(c1, "Rahul: Yes, I am working on mine.")
        print("[Client 1 received]:", msg_for_c1.strip())
        assert "Rahul: Yes, I am working on mine." in msg_for_c1

        # Test Private Messaging
        print("[Client 1 sends PM to Rahul]")
        c1.sendall(b"/msg Rahul Can we review the code together?\n")
        pm_for_c2 = recv_until(c2, "[PM from Rishabh]: Can we review the code together?")
        print("[Client 2 received PM]:", pm_for_c2.strip())
        assert "Can we review the code together?" in pm_for_c2

        # Test /users
        c1.sendall(b"/users\n")
        users_resp = recv_until(c1, "ONLINE USERS")
        print("[Client 1 /users]:\n", users_resp.strip())
        assert "Rishabh" in users_resp and "Rahul" in users_resp

        # Test /rooms
        c1.sendall(b"/rooms\n")
        rooms_resp = recv_until(c1, "AVAILABLE ROOMS")
        print("[Client 1 /rooms]:\n", rooms_resp.strip())
        assert "coding" in rooms_resp

        # Test /history
        c1.sendall(b"/history\n")
        hist_resp = recv_until(c1, "MESSAGE HISTORY")
        print("[Client 1 /history]:\n", hist_resp.strip())
        assert "Is everyone ready for the C project?" in hist_resp

        # Test /stats
        c1.sendall(b"/stats\n")
        stats_resp = recv_until(c1, "SERVER STATISTICS")
        print("[Client 1 /stats]:\n", stats_resp.strip())
        assert "Active Online Users:    2" in stats_resp

        # Test /quit
        c1.sendall(b"/quit\n")
        c2.sendall(b"/quit\n")
        time.sleep(0.5)

        c1.close()
        c2.close()
        print("\n--> ALL LIVE NETWORK TESTS PASSED SUCCESSFULLY! <--")

    finally:
        server_proc.terminate()
        server_proc.wait()
        print("Server process terminated cleanly.")

if __name__ == "__main__":
    main()
