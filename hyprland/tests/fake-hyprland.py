#!/usr/bin/env python3
"""A stand-in for Hyprland's two sockets, to run raye-hypr-shell on another
Wayland compositor (a headless one in the tests, for pictures):

    fake-hyprland.py DIR [clients.json]

serves DIR/.socket.sock (j/clients, j/activewindow, j/monitors, dispatch)
and DIR/.socket2.sock, and prints every request it gets."""

import json
import os
import socket
import sys
import threading

DIR = sys.argv[1]
CLIENTS = json.load(open(sys.argv[2])) if len(sys.argv) > 2 else []
WIDTH = int(os.environ.get("FAKE_WIDTH", "1280"))
HEIGHT = int(os.environ.get("FAKE_HEIGHT", "800"))

MONITORS = [{
    "id": 0, "name": os.environ.get("FAKE_MONITOR", "HEADLESS-1"), "x": 0, "y": 0,
    "width": WIDTH, "height": HEIGHT, "scale": 1.0, "transform": 0, "focused": True,
    "reserved": [0, 0, 0, 48], "activeWorkspace": {"id": 1, "name": "1"},
}]


def answer(q):
    if q == "j/clients":
        return json.dumps(CLIENTS)
    if q == "j/activewindow":
        active = [c for c in CLIENTS if c.get("focusHistoryID") == 0]
        return json.dumps(active[0] if active else {})
    if q == "j/monitors":
        return json.dumps(MONITORS)
    return "ok"


def serve(path, handler):
    if os.path.exists(path):
        os.unlink(path)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.bind(path)
    s.listen(16)
    while True:
        conn, _ = s.accept()
        threading.Thread(target=handler, args=(conn,), daemon=True).start()


def requests(conn):
    q = conn.recv(65536).decode()
    print("request:", q, flush=True)
    conn.sendall(answer(q).encode())
    conn.close()


def events(conn):
    # held open: the shell only reads from it
    threading.Event().wait()


os.makedirs(DIR, exist_ok=True)
threading.Thread(target=serve, args=(os.path.join(DIR, ".socket2.sock"), events), daemon=True).start()
serve(os.path.join(DIR, ".socket.sock"), requests)
