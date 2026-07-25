import http.server, socketserver, os, datetime
os.chdir(r"c:\Users\zhouyoukang\dao-workspace\daoshu")
LOG=r"c:\Users\zhouyoukang\dao-workspace\daoshu\access.log"
class H(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *a):
        with open(LOG,"a",encoding="utf-8") as f:
            f.write(datetime.datetime.now().isoformat()+" "+(fmt%a)+"\n")
    def do_GET(self):
        if self.path.startswith("/ping"):
            self.log_message("BEACON %s", self.path)
            self.send_response(200); self.send_header("Content-Type","text/plain")
            self.end_headers(); self.wfile.write(b"ok"); return
        super().do_GET()
socketserver.ThreadingTCPServer.allow_reuse_address=True
with socketserver.ThreadingTCPServer(("0.0.0.0",8899),H) as s:
    s.serve_forever()
