#!/usr/bin/env python3
# requirements: pycryptodome
from Crypto.PublicKey import ECC
from Crypto.Signature import eddsa
from hashlib import sha256
from pathlib import Path
import base64
import configparser
import gzip
import http.server
import json
import os
import shutil
import socketserver
import subprocess
import sys
import tempfile
import threading
import time

UPDATE_KEY_TEST = ECC.construct(
    curve="Ed25519",
    seed=bytes.fromhex(
        "543a581db60008bbb978a464e136d686dbc9d594119e928b5276bece3d583d81"
    ),
)

HTTP_SERVER_ADDR = ("localhost", 8042)
DOLPHIN_UPDATE_SERVER_URL = f"http://{HTTP_SERVER_ADDR[0]}:{HTTP_SERVER_ADDR[1]}"


class Manifest:
    def __init__(self, path: Path):
        self.path = path
        self.entries = {}
        for p in self.path.glob("**/*.*"):
            if not p.is_file():
                continue
            digest = sha256(p.read_bytes()).digest()[:0x10].hex()
            self.entries[digest] = p.relative_to(self.path).as_posix()

    def get_signed(self):
        manifest = "".join(
            f"{name}\t{digest}\n" for digest, name in self.entries.items()
        )
        manifest = manifest.encode("utf-8")
        sig = eddsa.new(UPDATE_KEY_TEST, "rfc8032").sign(manifest)
        manifest += b"\n" + base64.b64encode(sig) + b"\n"
        return gzip.compress(manifest)

    def get_path(self, digest):
        return self.path.joinpath(self.entries.get(digest))


class HTTPRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/update/check/v1/updater-test"):
            self.send_response(200)
            self.end_headers()
            self.wfile.write(
                bytes(
                    json.dumps(
                        {
                            "status": "outdated",
                            "content-store": DOLPHIN_UPDATE_SERVER_URL + "/content/",
                            "changelog": [],
                            "old": {"manifest": DOLPHIN_UPDATE_SERVER_URL + "/old"},
                            "new": {
                                "manifest": DOLPHIN_UPDATE_SERVER_URL + "/new",
                                "name": "updater-test",
                                "hash": bytes(range(32)).hex(),
                            },
                        }
                    ),
                    "utf-8",
                )
            )
        elif self.path == "/old":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(self.current.get_signed())
        elif self.path == "/new":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(self.next.get_signed())
        elif self.path.startswith("/content/"):
            self.send_response(200)
            self.end_headers()
            digest = "".join(self.path[len("/content/") :].split("/"))
            path = self.next.get_path(digest)
            self.wfile.write(gzip.compress(path.read_bytes()))
        elif self.path.startswith("/update-test-done/"):
            self.send_response(200)
            self.end_headers()
            HTTPRequestHandler.dolphin_pid = int(self.path[len("/update-test-done/") :])
            self.done.set()


def http_server():
    with socketserver.TCPServer(HTTP_SERVER_ADDR, HTTPRequestHandler) as httpd:
        httpd.serve_forever()


def create_entries_in_ini(ini_path: Path, entries: dict):
    config = configparser.ConfigParser()
    if ini_path.exists():
        config.read(ini_path)
    else:
        ini_path.parent.mkdir(parents=True, exist_ok=True)

    for section, options in entries.items():
        if not config.has_section(section):
            config.add_section(section)
        for option, value in options.items():
            config.set(section, option, value)

    with ini_path.open("w") as f:
        config.write(f)


if __name__ == "__main__":
    dolphin_bin_path = Path(sys.argv[1])

    threading.Thread(target=http_server, daemon=True).start()

    with tempfile.TemporaryDirectory(suffix=" ¿ 🐬") as tmp_dir:
        tmp_dir = Path(tmp_dir)

        tmp_dolphin = tmp_dir.joinpath("dolphin")
        print(f"install to {tmp_dolphin}")
        shutil.copytree(dolphin_bin_path.parent, tmp_dolphin)
        tmp_dolphin.joinpath("portable.txt").touch()
        create_entries_in_ini(
            tmp_dolphin.joinpath("User/Config/Dolphin.ini"),
            {
                "Analytics": {"Enabled": "False", "PermissionAsked": "True"},
                "AutoUpdate": {"UpdateTrack": "updater-test"},
            },
        )

        tmp_dolphin_next = tmp_dir.joinpath("dolphin_next")
        print(f"install next to {tmp_dolphin_next}")
        # XXX copies from just-created dir so Dolphin.ini is kept
        shutil.copytree(tmp_dolphin, tmp_dolphin_next)
        tmp_dolphin_next.joinpath("updater-test-file").write_text("test")
        tmp_dolphin_next.joinpath("updater-test-filἑ").write_text("test")
        with tmp_dolphin_next.joinpath("build_info.txt").open("a") as f:
            print("test", file=f)
        for ext in ("exe", "dll"):
            for path in tmp_dolphin_next.glob("**/*." + ext):
                data = bytearray(path.read_bytes())
                richpos = data[:0x200].find(b"Rich")
                if richpos < 0:
                    continue
                data[richpos : richpos + 4] = b"DOLP"
                path.write_bytes(data)

        HTTPRequestHandler.current = Manifest(tmp_dolphin)
        HTTPRequestHandler.next = Manifest(tmp_dolphin_next)
        HTTPRequestHandler.done = threading.Event()

        tmp_env = os.environ
        tmp_env.update({"DOLPHIN_UPDATE_SERVER_URL": DOLPHIN_UPDATE_SERVER_URL})
        tmp_dolphin_bin = tmp_dolphin.joinpath(dolphin_bin_path.name)
        result = subprocess.run(tmp_dolphin_bin, env=tmp_env)
        assert result.returncode == 0

        assert HTTPRequestHandler.done.wait(60 * 2)
        # works fine but raises exceptions...
        try:
            os.kill(HTTPRequestHandler.dolphin_pid, 0)
        except:
            pass
        try:
            os.waitpid(HTTPRequestHandler.dolphin_pid, 0)
        except:
            pass

        failed = False
        for path in tmp_dolphin_next.glob("**/*.*"):
            if not path.is_file():
                continue
            path_rel = path.relative_to(tmp_dolphin_next)
            if path_rel.parts[0] == "User":
                continue
            new_path = tmp_dolphin.joinpath(path_rel)
            if not new_path.exists():
                print(f"missing: {new_path}")
                failed = True
                continue
            if (
                sha256(new_path.read_bytes()).digest()
                != sha256(path.read_bytes()).digest()
            ):
                print(f"bad digest: {new_path} {path}")
                failed = True
                continue
        assert not failed

        print(tmp_dolphin.joinpath("User/Logs/Updater.log").read_text())
        # while True: time.sleep(1)
