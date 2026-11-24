#!/usr/bin/env python3
"""Stand-in for the lights' web server, for working on web/index.html without a board.

    python3 tools/mock_server.py [--port 8080]

then open http://localhost:8080 (or this computer's address from a phone on the same
Wi-Fi). It serves the page exactly as the firmware does (built by tools/embed_web.py,
with the QR library inline) and answers the same API as src/web/WebApi.cpp, keeping
state in memory. Edits to web/ show up on reload. GET /__requests lists the requests received (used by the
browser tests).
"""
import argparse
import json
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

sys.path.insert(0, str(Path(__file__).resolve().parent))
from embed_web import build_page  # noqa: E402

MODES = ["spectrum-snappy", "spectrum-smooth", "solid", "gradient", "rainbow", "twinkle"]
NAME_MAX_BYTES = 24


def tidy_name(name, slot):
    """Same rules as tidyPresetName in the firmware."""
    name = "".join(c for c in (name or "") if ord(c) >= 0x20 and ord(c) != 0x7F)
    out = ""
    for c in name:
        if len((out + c).encode("utf-8")) > NAME_MAX_BYTES:
            break
        out += c
    out = out.strip(" ")
    return out or f"Preset {slot + 1}"


class Lights:
    def __init__(self):
        self.lock = threading.Lock()
        self.revision = 1
        self.s = dict(on=True, mode="spectrum-smooth", brightness=64, colourA="#FFA03C",
                      colourB="#2850FF", spectrumColour="rainbow")
        self.presets = [None] * 8
        self.names = [f"Preset {i + 1}" for i in range(8)]
        self.started = time.time()
        self.learning_until = 0
        self.requests = []

    def state(self):
        presets = []
        for i in range(8):
            p = {"slot": i + 1, "name": self.names[i], "used": self.presets[i] is not None}
            if self.presets[i]:
                p.update({k: self.presets[i][k] for k in ("mode", "colourA", "colourB", "spectrumColour")})
            presets.append(p)
        return {
            "revision": self.revision, **self.s, "modes": MODES, "presets": presets,
            "status": {
                "version": "mock", "uptime": int(time.time() - self.started),
                "wifi": {"connected": True, "ssid": "Home Wi-Fi", "rssi": -58, "ip": "192.168.3.234",
                         "url": "http://192.168.3.234/", "localUrl": "http://spectrum.local/"},
                "audio": {"ok": True, "silent": False, "learningNoise": time.time() < self.learning_until,
                          "loadPercent": 43},
            },
        }

    def apply_state(self, f):
        nxt = dict(self.s)
        if "on" in f:
            if f["on"] not in ("1", "0", "true", "false"):
                return "'on' must be 1 or 0"
            nxt["on"] = f["on"] in ("1", "true")
        if "mode" in f:
            if f["mode"] not in MODES:
                return f"unknown mode '{f['mode']}'"
            nxt["mode"] = f["mode"]
        if "brightness" in f:
            if not re.fullmatch(r"\d+", f["brightness"]) or int(f["brightness"]) > 255:
                return "'brightness' must be 0-255"
            nxt["brightness"] = int(f["brightness"])
        for k in ("colourA", "colourB"):
            if k in f:
                if not re.fullmatch(r"#?[0-9a-fA-F]{6}", f[k]):
                    return f"'{k}' must be #RRGGBB"
                nxt[k] = "#" + f[k].lstrip("#").upper()
        if "spectrumColour" in f:
            if f["spectrumColour"] not in ("rainbow", "fixed"):
                return "'spectrumColour' must be rainbow or fixed"
            nxt["spectrumColour"] = f["spectrumColour"]
        if nxt != self.s:
            self.s = nxt
            self.revision += 1
        return ""

    def preset(self, action, f):
        slot = f.get("slot", "")
        if not re.fullmatch(r"[1-8]", slot):
            return "'slot' must be 1-8"
        i = int(slot) - 1
        if action == "save":
            self.presets[i] = dict(self.s)
            if "name" in f:
                self.names[i] = tidy_name(f["name"], i)
        elif action == "recall":
            if not self.presets[i]:
                return "that preset is empty"
            on = self.s["on"]
            self.s = dict(self.presets[i], on=on)
        elif action == "rename":
            if "name" not in f:
                return "'name' is required"
            if not self.presets[i]:
                return "that preset is empty"
            self.names[i] = tidy_name(f["name"], i)
        elif action == "delete":
            self.presets[i] = None
            self.names[i] = f"Preset {i + 1}"
        else:
            return f"unknown preset action '{action}'"
        self.revision += 1
        return ""

    def action(self, name):
        if name == "learn-noise":
            self.learning_until = time.time() + 10
        elif name == "forget-wifi":
            pass
        elif name == "reset-settings":
            self.__init__()
        else:
            return f"unknown action '{name}'"
        self.revision += 1
        return ""


LIGHTS = Lights()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send(self, code, body, ctype="application/json"):
        data = body.encode("utf-8") if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = urlparse(self.path).path
        with LIGHTS.lock:
            LIGHTS.requests.append({"method": "GET", "path": path, "t": time.time()})
            if path == "/api/state":
                return self.send(200, json.dumps(LIGHTS.state()))
            if path == "/__requests":
                return self.send(200, json.dumps(LIGHTS.requests))
        if path in ("/", "/index.html"):
            return self.send(200, build_page(), "text/html; charset=utf-8")
        self.send(404, json.dumps({"error": "not found"}))

    def do_POST(self):
        path = urlparse(self.path).path
        length = int(self.headers.get("Content-Length", 0))
        fields = {k: v[0] for k, v in parse_qs(self.rfile.read(length).decode("utf-8"),
                                               keep_blank_values=True).items()}
        with LIGHTS.lock:
            LIGHTS.requests.append({"method": "POST", "path": path, "fields": fields, "t": time.time()})
            if path == "/api/state":
                err = LIGHTS.apply_state(fields)
            elif path.startswith("/api/preset/"):
                err = LIGHTS.preset(path.rsplit("/", 1)[1], fields)
            elif path.startswith("/api/action/"):
                err = LIGHTS.action(path.rsplit("/", 1)[1])
            else:
                err = "not found"
            if err:
                return self.send(400, json.dumps({"error": err}))
            return self.send(200, json.dumps(LIGHTS.state()))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    print(f"Mock lights at http://localhost:{args.port}  (Ctrl+C to stop)")
    server.serve_forever()


if __name__ == "__main__":
    main()
