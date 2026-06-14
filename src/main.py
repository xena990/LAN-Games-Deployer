import json
import os
import queue
import subprocess
import socket
import threading
import time
import sys
import ctypes
import re
from dataclasses import dataclass
from datetime import datetime
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from tkinter import BOTH, END, LEFT, RIGHT, VERTICAL, X, Y, filedialog, messagebox, ttk
import tkinter as tk
from urllib.parse import quote, unquote, urlparse, parse_qs
import webbrowser
from typing import Optional
from urllib.request import urlopen, Request
from PIL import Image, ImageTk, ImageOps

APP_NAME = "LAN Games Deployer"
DISCOVERY_PORT = 50055
DISCOVERY_MAGIC = "LAN_GAMES_DEPLOYER_V1"
HTTP_PORT = 50111
ANNOUNCE_INTERVAL = 5
PEER_TIMEOUT = 20
FIREWALL_RULE_DISCOVERY = "LAN Games Deployer UDP Discovery"
FIREWALL_RULE_SHARE = "LAN Games Deployer TCP Share"
APP_BASE_DIR = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent.parent
DATA_DIR = APP_BASE_DIR / "data"
CONFIG_PATH = DATA_DIR / "config.json"
ASSETS_DIR = DATA_DIR / "assets" / "games"
LAN_ASSET_CACHE_DIR = DATA_DIR / "lan_assets"
HTTP_USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
STEAMGRIDDB_API_BASE = "https://www.steamgriddb.com/api/v2"
STEAMGRIDDB_API_KEY = os.environ.get("STEAMGRIDDB_API_KEY", "")


def slugify_game_name(name: str) -> str:
    out = []
    prev_dash = False
    for ch in name.lower():
        if ch.isalnum():
            out.append(ch)
            prev_dash = False
        else:
            if not prev_dash:
                out.append("-")
                prev_dash = True
    result = "".join(out).strip("-")
    return result


def normalize_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", name.lower())


def now_ts() -> float:
    return time.time()


def run_hidden(cmd: list[str], capture_output: bool = True, text: bool = True) -> subprocess.CompletedProcess:
    kwargs = {"shell": False, "capture_output": capture_output, "text": text}
    if os.name == "nt":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
    return subprocess.run(cmd, **kwargs)


def is_windows_admin() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def firewall_rule_exists(rule_name: str) -> bool:
    result = run_hidden(["netsh", "advfirewall", "firewall", "show", "rule", f"name={rule_name}"])
    return result.returncode == 0 and "No rules match the specified criteria." not in result.stdout


def add_firewall_rules_for_exe(exe_path: str) -> bool:
    commands = [
        [
            "netsh",
            "advfirewall",
            "firewall",
            "add",
            "rule",
            f"name={FIREWALL_RULE_DISCOVERY}",
            "dir=in",
            "action=allow",
            f"program={exe_path}",
            "protocol=UDP",
            "localport=50055",
            "profile=private",
        ],
        [
            "netsh",
            "advfirewall",
            "firewall",
            "add",
            "rule",
            f"name={FIREWALL_RULE_SHARE}",
            "dir=in",
            "action=allow",
            f"program={exe_path}",
            "protocol=TCP",
            "localport=50111",
            "profile=private",
        ],
    ]
    for cmd in commands:
        res = run_hidden(cmd)
        if res.returncode != 0:
            return False
    return True


def ensure_firewall_rules_on_startup(root: tk.Tk) -> None:
    exe_path = os.path.abspath(sys.executable if getattr(sys, "frozen", False) else __file__)
    already_ok = firewall_rule_exists(FIREWALL_RULE_DISCOVERY) and firewall_rule_exists(FIREWALL_RULE_SHARE)
    if already_ok:
        return

    if is_windows_admin():
        if not add_firewall_rules_for_exe(exe_path):
            messagebox.showwarning(APP_NAME, "Could not auto-add firewall rules. LAN discovery may be limited.")
        return

    should_elevate = messagebox.askyesno(
        APP_NAME,
        "Firewall rules are missing.\n\nAllow app to request Administrator rights now and add LAN firewall rules automatically?",
    )
    if not should_elevate:
        return

    params = " ".join([f'"{arg}"' for arg in sys.argv[1:]] + ["--setup-firewall"])
    rc = ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, params, None, 1)
    if rc <= 32:
        messagebox.showwarning(APP_NAME, "Administrator elevation was cancelled. You can still use the app, but LAN may not work on all PCs.")


def handle_setup_firewall_mode() -> bool:
    if "--setup-firewall" not in sys.argv:
        return False
    exe_path = os.path.abspath(sys.executable if getattr(sys, "frozen", False) else __file__)
    ok = add_firewall_rules_for_exe(exe_path)
    sys.exit(0 if ok else 1)


def load_config() -> dict:
    try:
        if CONFIG_PATH.exists():
            with CONFIG_PATH.open("r", encoding="utf-8") as f:
                return json.load(f)
    except Exception:
        return {}
    return {}


def save_config(config: dict) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    with CONFIG_PATH.open("w", encoding="utf-8") as f:
        json.dump(config, f, indent=2)


@dataclass
class GameEntry:
    name: str
    owner: str
    source: str  # local or remote
    local_path: str = ""
    launch_exe: str = ""
    cover_path: str = ""
    icon_path: str = ""
    remote_base_url: str = ""
    remote_game_key: str = ""
    remote_launch_exe: str = ""
    remote_icon_url: str = ""
    remote_banner_url: str = ""
    shared_items: list[dict] = None  # type: ignore


class GameShareState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.shared_root: str = ""
        self.games: dict[str, dict] = {}
        self.shared_items: list[dict] = []
        self.host_name = socket.gethostname()

    def set_shared_root(self, folder: str) -> None:
        with self.lock:
            self.shared_root = folder
            self.games = self.scan_games(folder)

    def set_shared_items(self, paths: list[str]) -> None:
        items: list[dict] = []
        for raw in paths:
            p = Path(raw)
            try:
                rp = p.resolve()
            except Exception:
                continue
            if not rp.exists():
                continue
            is_dir = rp.is_dir()
            size = 0
            if is_dir:
                try:
                    size = sum(f.stat().st_size for f in rp.rglob("*") if f.is_file())
                except Exception:
                    size = 0
            else:
                try:
                    size = rp.stat().st_size
                except Exception:
                    size = 0
            items.append(
                {
                    "path": str(rp),
                    "name": rp.name,
                    "is_dir": bool(is_dir),
                    "size": int(size),
                }
            )
        with self.lock:
            self.shared_items = items

    @staticmethod
    def rel_to_game_dir(game_dir: Path, target_file: Path) -> str:
        try:
            return str(target_file.resolve().relative_to(game_dir.resolve()))
        except Exception:
            return target_file.name

    def pick_launch_exe(self, game_dir: Path) -> str:
        # Fast path: prefer EXEs in the game root to avoid full recursive scans on startup.
        root_exes = [p for p in game_dir.glob("*.exe") if p.is_file()]
        exes = root_exes
        if not exes:
            exes = [p for p in game_dir.rglob("*.exe") if p.is_file()]
        if not exes:
            return ""

        folder_name = game_dir.name.lower()
        preferred = [p for p in exes if folder_name in p.stem.lower()]
        candidates = preferred if preferred else exes
        best = max(candidates, key=lambda p: p.stat().st_size)
        return str(best.resolve())

    def pick_cover_image(self, game_dir: Path) -> str:
        preferred_names = ["cover", "banner", "folder", "boxart", "art"]
        exts = [".png", ".gif", ".jpg", ".jpeg"]

        for base in preferred_names:
            for ext in exts:
                candidate = game_dir / f"{base}{ext}"
                if candidate.exists() and candidate.is_file():
                    return str(candidate.resolve())

        for p in game_dir.iterdir():
            if p.is_file() and p.suffix.lower() in exts:
                return str(p.resolve())

        return ""

    def get_asset_paths(self, game_name: str) -> tuple[str, str]:
        slug = slugify_game_name(game_name)
        d = ASSETS_DIR / slug
        icon = d / "icon.png"
        banner = d / "banner.png"
        return (str(icon.resolve()) if icon.exists() else "", str(banner.resolve()) if banner.exists() else "")

    def scan_games(self, folder: str) -> dict[str, dict]:
        result: dict[str, dict] = {}
        root = Path(folder)
        if not root.exists():
            return result

        for child in root.iterdir():
            if not child.is_dir():
                continue
            launch_exe = self.pick_launch_exe(child)
            if launch_exe:
                game_dir = Path(child.resolve())
                rel_launch = self.rel_to_game_dir(game_dir, Path(launch_exe))
                asset_icon, asset_banner = self.get_asset_paths(child.name)
                folder_cover = self.pick_cover_image(child)
                result[child.name] = {
                    "path": str(child.resolve()),
                    "launch_exe": launch_exe,
                    "launch_exe_rel": rel_launch,
                    "cover_path": asset_banner or folder_cover,
                    "icon_path": asset_icon,
                }

        return dict(sorted(result.items(), key=lambda kv: kv[0].lower()))

    def manifest(self) -> dict:
        with self.lock:
            return {
                "app": APP_NAME,
                "host": self.host_name,
                "shared_root": self.shared_root,
                "games": [
                    {
                        "name": name,
                        "launch_exe_rel": meta.get("launch_exe_rel", ""),
                        "has_icon": bool(self.get_asset_paths(name)[0]),
                        "has_banner": bool(self.get_asset_paths(name)[1]),
                    }
                    for name, meta in self.games.items()
                ],
                "shared_items": [
                    {
                        "id": f"{i}",
                        "name": it.get("name", ""),
                        "is_dir": bool(it.get("is_dir", False)),
                        "size": int(it.get("size", 0)),
                    }
                    for i, it in enumerate(self.shared_items)
                ],
                "timestamp": datetime.utcnow().isoformat(),
            }


class GameHttpHandler(BaseHTTPRequestHandler):
    state: GameShareState = None  # type: ignore

    def _send_json(self, payload: dict, status: int = 200) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, file_path: Path) -> None:
        if not file_path.exists() or not file_path.is_file():
            self._send_json({"error": "File not found"}, status=404)
            return
        size = file_path.stat().st_size
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(size))
        self.end_headers()
        with file_path.open("rb") as f:
            while True:
                chunk = f.read(1024 * 256)
                if not chunk:
                    break
                self.wfile.write(chunk)


    def do_GET(self) -> None:
        if self.path == "/manifest":
            self._send_json(self.state.manifest())
            return

        if self.path.startswith("/filelist/"):
            key = unquote(self.path.removeprefix("/filelist/"))
            with self.state.lock:
                game = self.state.games.get(key)
                game_path = game["path"] if game else ""
            if not game_path:
                self._send_json({"error": "Game not found"}, status=404)
                return
            root = Path(game_path)
            files = []
            total_bytes = 0
            for p in root.rglob("*"):
                if p.is_file():
                    rel = str(p.relative_to(root)).replace("\\", "/")
                    size = p.stat().st_size
                    total_bytes += size
                    files.append({"path": rel, "size": size})
            self._send_json({"game": key, "files": files, "total_bytes": total_bytes})
            return

        if self.path.startswith("/file"):
            parsed = urlparse(self.path)
            qs = parse_qs(parsed.query)
            game_name = unquote(qs.get("game", [""])[0])
            rel_path = unquote(qs.get("path", [""])[0])
            if not game_name or not rel_path:
                self._send_json({"error": "Missing game/path"}, status=400)
                return
            with self.state.lock:
                game = self.state.games.get(game_name)
                game_path = game["path"] if game else ""
            if not game_path:
                self._send_json({"error": "Game not found"}, status=404)
                return
            base = Path(game_path).resolve()
            target = (base / rel_path).resolve()
            try:
                target.relative_to(base)
            except Exception:
                self._send_json({"error": "Invalid path"}, status=400)
                return
            self._send_file(target)
            return

        if self.path.startswith("/sharedlist"):
            with self.state.lock:
                items = self.state.shared_items
            payload = []
            for idx, it in enumerate(items):
                root = Path(it.get("path", ""))
                files = []
                total_bytes = 0
                if root.exists():
                    if root.is_file():
                        sz = int(root.stat().st_size) if root.exists() else 0
                        total_bytes += sz
                        files.append({"path": root.name, "size": sz})
                    elif root.is_dir():
                        for p in root.rglob("*"):
                            if p.is_file():
                                rel = str(p.relative_to(root)).replace("\\", "/")
                                sz = int(p.stat().st_size)
                                total_bytes += sz
                                files.append({"path": rel, "size": sz})
                payload.append(
                    {
                        "id": f"{idx}",
                        "name": it.get("name", ""),
                        "is_dir": bool(it.get("is_dir", False)),
                        "files": files,
                        "total_bytes": total_bytes,
                    }
                )
            self._send_json({"items": payload})
            return

        if self.path.startswith("/sharedfile"):
            parsed = urlparse(self.path)
            qs = parse_qs(parsed.query)
            item_id = unquote(qs.get("item", [""])[0])
            rel_path = unquote(qs.get("path", [""])[0])
            if item_id == "" or not rel_path:
                self._send_json({"error": "Missing item/path"}, status=400)
                return
            try:
                idx = int(item_id)
            except Exception:
                self._send_json({"error": "Invalid item id"}, status=400)
                return
            with self.state.lock:
                items = self.state.shared_items
                item = items[idx] if 0 <= idx < len(items) else None
            if not item:
                self._send_json({"error": "Shared item not found"}, status=404)
                return
            root = Path(item.get("path", "")).resolve()
            if not root.exists():
                self._send_json({"error": "Shared item missing"}, status=404)
                return
            if root.is_file():
                target = root
            else:
                target = (root / rel_path).resolve()
                try:
                    target.relative_to(root)
                except Exception:
                    self._send_json({"error": "Invalid path"}, status=400)
                    return
            self._send_file(target)
            return

        if self.path.startswith("/asset"):
            parsed = urlparse(self.path)
            qs = parse_qs(parsed.query)
            game_name = unquote(qs.get("game", [""])[0])
            kind = unquote(qs.get("kind", [""])[0]).lower()
            if not game_name or kind not in ("icon", "banner"):
                self._send_json({"error": "Missing game/kind"}, status=400)
                return
            with self.state.lock:
                game = self.state.games.get(game_name)
                if not game:
                    self._send_json({"error": "Game not found"}, status=404)
                    return
            icon_path, banner_path = self.state.get_asset_paths(game_name)
            chosen = icon_path if kind == "icon" else banner_path
            if not chosen:
                self._send_json({"error": "Asset not found"}, status=404)
                return
            self._send_file(Path(chosen))
            return

        self._send_json({"error": "Not found"}, status=404)

    def log_message(self, fmt: str, *args) -> None:
        return


class LanDiscovery:
    def __init__(self, state: GameShareState, on_peer_update, http_port: int = HTTP_PORT):
        self.state = state
        self.on_peer_update = on_peer_update
        self.http_port = http_port
        self.running = False
        self.peers: dict[str, dict] = {}

    def start(self) -> None:
        self.running = True
        threading.Thread(target=self._announce_loop, daemon=True).start()
        threading.Thread(target=self._listen_loop, daemon=True).start()
        threading.Thread(target=self._cleanup_loop, daemon=True).start()

    def stop(self) -> None:
        self.running = False

    def _announce_loop(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        while self.running:
            manifest = self.state.manifest()
            msg = {
                "magic": DISCOVERY_MAGIC,
                "host": self.state.host_name,
                "http_port": self.http_port,
                "games": list(manifest["games"]),
                "shared_items": list(manifest.get("shared_items", [])),
                "ts": now_ts(),
            }
            data = json.dumps(msg).encode("utf-8")
            try:
                sock.sendto(data, ("255.255.255.255", DISCOVERY_PORT))
            except OSError:
                pass
            time.sleep(ANNOUNCE_INTERVAL)

    def _listen_loop(self) -> None:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            sock.bind(("", DISCOVERY_PORT))
        except OSError:
            return
        while self.running:
            try:
                data, addr = sock.recvfrom(65535)
                msg = json.loads(data.decode("utf-8"))
                if msg.get("magic") != DISCOVERY_MAGIC:
                    continue
                host = msg.get("host", "")
                if host == self.state.host_name:
                    continue

                peer_key = f"{host}@{addr[0]}"
                peer = {
                    "host": host,
                    "ip": addr[0],
                    "http_port": int(msg.get("http_port", HTTP_PORT)),
                    "games": msg.get("games", []),
                    "shared_items": msg.get("shared_items", []),
                    "last_seen": now_ts(),
                }
                self.peers[peer_key] = peer
                self.on_peer_update(self.snapshot())
            except Exception:
                continue

    def _cleanup_loop(self) -> None:
        while self.running:
            stale = []
            for key, peer in self.peers.items():
                if now_ts() - peer["last_seen"] > PEER_TIMEOUT:
                    stale.append(key)
            for key in stale:
                self.peers.pop(key, None)
            if stale:
                self.on_peer_update(self.snapshot())
            time.sleep(3)

    def snapshot(self) -> list[dict]:
        return list(self.peers.values())


class LanGamesApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title(APP_NAME)
        self.root.geometry("1280x820")
        self.root.configure(bg="#0b141f")
        self._apply_window_icon()

        self.state = GameShareState()
        self.ui_queue: queue.Queue = queue.Queue()
        self.remote_peers: list[dict] = []
        self.game_rows: list[GameEntry] = []
        self.filtered_rows: list[GameEntry] = []
        self.banner_image: Optional[tk.PhotoImage] = None
        self.thumb_images: dict[str, tk.PhotoImage] = {}
        self.thumb_size = 31
        self.default_thumb = tk.PhotoImage(width=self.thumb_size, height=self.thumb_size)
        self.empty_thumb = tk.PhotoImage(width=self.thumb_size, height=self.thumb_size)
        self.download_active = False
        self.http_port = HTTP_PORT

        self.search_var = tk.StringVar()
        self.config: dict = load_config()
        self.exe_overrides: dict = self.config.get("exe_overrides", {})
        self.asset_overrides: dict = self.config.get("asset_overrides", {})
        self.manual_game_folders: list[str] = self.config.get("manual_game_folders", [])
        self.shared_files: list[str] = self.config.get("shared_files", [])
        if not self.shared_files:
            legacy = self.config.get("shared_files_by_game", {})
            if isinstance(legacy, dict):
                merged = []
                seen = set()
                for values in legacy.values():
                    if not isinstance(values, list):
                        continue
                    for p in values:
                        rp = str(Path(p).resolve())
                        if rp not in seen:
                            seen.add(rp)
                            merged.append(rp)
                self.shared_files = merged
        self.hidden_entries: list[str] = self.config.get("hidden_entries", [])
        self.auto_asset_attempted: list[str] = self.config.get("auto_asset_attempted", [])
        ASSETS_DIR.mkdir(parents=True, exist_ok=True)
        LAN_ASSET_CACHE_DIR.mkdir(parents=True, exist_ok=True)
        self.http_server: Optional[ThreadingHTTPServer] = None
        self.http_port = 0
        self.lan_share_enabled = False
        self.shared_panel_rows: list[tuple] = []
        self.shared_download_index: dict[str, str] = self.config.get("shared_download_index", {})

        self._setup_styles()
        self._build_ui()
        self.state.set_shared_items(self.shared_files)

        GameHttpHandler.state = self.state
        self.discovery: Optional[LanDiscovery] = None
        try:
            self.http_server, self.http_port = self._start_http_server_with_fallback()
            threading.Thread(target=self.http_server.serve_forever, daemon=True).start()
            self.discovery = LanDiscovery(self.state, self._on_peers_update, http_port=self.http_port)
            self.discovery.start()
            self.lan_share_enabled = True
        except Exception:
            self.lan_share_enabled = False
            self.download_status.config(text="Download: LAN sharing unavailable on this PC (port blocked).")
        self._load_saved_folder()

        self.root.after(250, self._drain_ui_queue)
        self.root.after(350, self._reapply_window_icon)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _start_http_server_with_fallback(self) -> tuple[ThreadingHTTPServer, int]:
        # Try default first, then nearby ports to avoid startup crash on blocked ports.
        ports = [HTTP_PORT] + list(range(HTTP_PORT + 1, HTTP_PORT + 200))
        last_ex: Optional[Exception] = None
        for p in ports:
            try:
                srv = ThreadingHTTPServer(("0.0.0.0", p), GameHttpHandler)
                return srv, p
            except OSError as ex:
                last_ex = ex
                continue
        raise RuntimeError(f"Unable to bind HTTP share server on ports {ports[0]}-{ports[-1]}: {last_ex}")

    def _apply_window_icon(self) -> None:
        if os.name == "nt":
            try:
                ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("Undertaker.LANGamesDeployer.1.1")
            except Exception:
                pass

        runtime_base = Path(getattr(sys, "_MEIPASS", APP_BASE_DIR))
        ico_candidates = [
            runtime_base / "assets" / "app_icon.ico",
            APP_BASE_DIR / "assets" / "app_icon.ico",
        ]
        png_candidates = [
            runtime_base / "assets" / "app_icon.png",
            APP_BASE_DIR / "assets" / "app_icon.png",
        ]

        for ico_path in ico_candidates:
            if ico_path.exists():
                try:
                    self.root.iconbitmap(str(ico_path))
                except Exception:
                    pass
                break

        for png_path in png_candidates:
            if png_path.exists():
                try:
                    pil = Image.open(png_path).convert("RGBA")
                    self.window_icon_photo = ImageTk.PhotoImage(pil)
                    self.root.iconphoto(True, self.window_icon_photo)
                except Exception:
                    pass
                break

    def _reapply_window_icon(self) -> None:
        self._apply_window_icon()

    def _setup_styles(self) -> None:
        style = ttk.Style()
        style.theme_use("clam")
        style.configure(".", background="#0f1a26", foreground="#c7d5e0", fieldbackground="#1b2838")
        style.configure("Body.TFrame", background="#0f1a26")
        style.configure("Card.TLabelframe", background="#16202d", foreground="#9eb4c8", bordercolor="#16202d", relief="flat", borderwidth=0)
        style.configure("Card.TLabelframe.Label", background="#16202d", foreground="#9eb4c8")
        style.configure("Accent.TLabel", background="#16202d", foreground="#66c0f4", font=("Segoe UI", 10, "bold"))
        style.configure("BannerTitle.TLabel", background="#16202d", foreground="#e5f4ff", font=("Segoe UI", 17, "bold"))
        style.configure("Steam.TButton", background="#2a475e", foreground="#e5f4ff", bordercolor="#4b6b88")
        style.map("Steam.TButton", background=[("active", "#365e7e")])
        style.configure("TCombobox", fieldbackground="#1b2838", background="#1b2838", foreground="#c7d5e0")
        style.configure(
            "Steam.TEntry",
            fieldbackground="#1b2838",
            foreground="#c7d5e0",
            bordercolor="#223246",
            lightcolor="#223246",
            darkcolor="#223246",
            insertcolor="#c7d5e0",
            relief="flat",
        )
        style.configure("Horizontal.TProgressbar", background="#66c0f4", troughcolor="#223246", bordercolor="#223246")
        style.configure("Steam.Treeview", background="#1b2838", fieldbackground="#1b2838", foreground="#c7d5e0", borderwidth=0, relief="flat", rowheight=39, indent=0)
        style.map("Steam.Treeview", background=[("selected", "#66c0f4")], foreground=[("selected", "#0b141f")])
        style.layout("Steam.Treeview", [("Treeview.treearea", {"sticky": "nswe"})])
        style.configure(
            "Steam.Vertical.TScrollbar",
            gripcount=0,
            background="#223246",
            darkcolor="#223246",
            lightcolor="#223246",
            troughcolor="#101822",
            bordercolor="#101822",
            arrowcolor="#7f95aa",
            relief="flat",
        )
        style.map("Steam.Vertical.TScrollbar", background=[("active", "#2b3f55")])

    def _build_ui(self) -> None:
        menubar = tk.Menu(self.root)
        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="Choose Shared Folder", command=self.choose_folder)
        file_menu.add_command(label="Add Game...", command=self.add_game_to_list)
        file_menu.add_command(label="Refresh", command=self.refresh_local_games)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self._on_close)
        menubar.add_cascade(label="File", menu=file_menu)
        self.root.config(menu=menubar)

        body = ttk.Frame(self.root, style="Body.TFrame", padding=8)
        body.pack(fill=BOTH, expand=True)

        left = ttk.Frame(body, style="Body.TFrame", padding=0)
        left.pack(side=LEFT, fill=Y)

        controls = ttk.Frame(left)
        controls.pack(fill=X, pady=(0, 6))

        ttk.Label(controls, text="Search", style="Accent.TLabel").pack(anchor="w")
        search_entry = ttk.Entry(controls, textvariable=self.search_var, width=32, style="Steam.TEntry")
        search_entry.pack(fill=X)
        search_entry.bind("<KeyRelease>", lambda _e: self.apply_filters())

        self.games_tree = ttk.Treeview(left, show="tree", style="Steam.Treeview", height=34)
        self.games_tree.column("#0", width=310, stretch=False)
        self.games_tree.pack(side=LEFT, fill=Y, expand=False)
        self.games_tree.bind("<<TreeviewSelect>>", self.on_game_selected)
        self.games_tree.bind("<Button-3>", self.on_list_right_click)

        self.list_menu = tk.Menu(self.root, tearoff=0)
        self.list_menu.configure(
            bg="#16202d",
            fg="#c7d5e0",
            activebackground="#2d6fa8",
            activeforeground="#ffffff",
            relief="flat",
            bd=0,
            font=("Segoe UI", 10),
        )
        self.list_menu.add_command(label="Set Launch Executable...", command=self.set_launch_executable_for_selected)
        self.list_menu.add_command(label="Open Local Folder", command=self.open_local_folder)
        self.list_menu.add_command(label="Set Icon...", command=self.set_icon_for_selected)
        self.list_menu.add_command(label="Set Banner...", command=self.set_banner_for_selected)
        self.list_menu.add_command(label="Remove Entry", command=self.remove_selected_entry)
        self.list_menu.add_separator()
        self.list_menu.add_command(label="Download Assets from SteamGridDB", command=self.download_assets_from_steamgriddb_for_selected)

        scrollbar = ttk.Scrollbar(left, orient=VERTICAL, command=self.games_tree.yview, style="Steam.Vertical.TScrollbar")
        scrollbar.pack(side=RIGHT, fill=Y)
        self.games_tree.configure(yscrollcommand=scrollbar.set)

        right = ttk.Frame(body, style="Body.TFrame")
        right.pack(side=RIGHT, fill=BOTH, expand=True, padx=(8, 0))

        banner = ttk.LabelFrame(right, text="", padding=8, style="Card.TLabelframe")
        banner.pack(fill=X)
        self.banner_canvas = tk.Canvas(banner, height=230, bg="#101822", bd=0, highlightthickness=0, relief="flat")
        self.banner_canvas.pack(fill=X)
        self.banner_canvas.bind("<Configure>", lambda _e: self._position_primary_action_button())
        self.primary_action_button = tk.Button(
            banner,
            text="SELECT A GAME",
            font=("Segoe UI", 16, "bold"),
            height=1,
            relief="flat",
            bd=0,
            bg="#2d3a4a",
            fg="#d7e8f7",
            activebackground="#3a4d62",
            activeforeground="#ffffff",
            command=self.on_primary_action,
            cursor="hand2",
        )
        self.primary_action_button.place(x=14, y=176, width=150, height=42)

        details = ttk.LabelFrame(right, text="GAME DETAILS", padding=8, style="Card.TLabelframe")
        details.pack(fill=BOTH, expand=True, pady=(8, 0))

        shared_bar = ttk.Frame(details, style="Body.TFrame")
        shared_bar.pack(fill=X, pady=(0, 8))
        self.share_file_button = tk.Button(
            shared_bar,
            text="Share file/folder",
            font=("Segoe UI", 10, "bold"),
            relief="flat",
            bd=0,
            bg="#2d6fa8",
            fg="#ffffff",
            activebackground="#3b88ca",
            activeforeground="#ffffff",
            command=self.share_file_or_folder_for_selected_game,
            cursor="hand2",
        )
        self.share_file_button.pack(side=LEFT, ipadx=8, ipady=4)

        self.shared_items_frame = tk.Frame(details, bg="#101822", highlightbackground="#66c0f4", highlightthickness=1)
        self.shared_items_frame.pack_forget()

        self.details_text = tk.Text(details, height=20, wrap="word")
        self.details_text.configure(
            bg="#101822",
            fg="#c7d5e0",
            insertbackground="#c7d5e0",
            relief="flat",
            highlightbackground="#101822",
            highlightthickness=0,
            bd=0,
        )
        self.details_text.pack(fill=BOTH, expand=True)

        actions = ttk.Frame(details)
        actions.pack(fill=X, pady=(8, 0))
        self.download_progress = ttk.Progressbar(details, orient="horizontal", mode="determinate")
        self.download_progress.pack(fill=X, pady=(8, 0))
        self.download_status = ttk.Label(details, text="Download: idle")
        self.download_status.pack(anchor="w")

    def _save_shared_files(self) -> None:
        self.config["shared_files"] = self.shared_files
        self.config["shared_download_index"] = self.shared_download_index
        if "shared_files_by_game" in self.config:
            self.config.pop("shared_files_by_game", None)
        save_config(self.config)
        self.state.set_shared_items(self.shared_files)

    def _ask_share_kind_dialog(self) -> Optional[str]:
        dlg = tk.Toplevel(self.root)
        dlg.title("Share")
        dlg.configure(bg="#16202d")
        dlg.resizable(False, False)
        dlg.transient(self.root)
        dlg.grab_set()

        choice = {"value": None}

        panel = tk.Frame(dlg, bg="#16202d", padx=16, pady=14)
        panel.pack(fill=BOTH, expand=True)
        tk.Label(
            panel,
            text="Choose what you want to share",
            bg="#16202d",
            fg="#d7e8f7",
            font=("Segoe UI", 11, "bold"),
        ).pack(anchor="w", pady=(0, 10))

        row = tk.Frame(panel, bg="#16202d")
        row.pack(fill=X)

        def pick(v: Optional[str]) -> None:
            choice["value"] = v
            dlg.destroy()

        tk.Button(
            row,
            text="File",
            font=("Segoe UI", 10, "bold"),
            relief="flat",
            bd=0,
            bg="#2d6fa8",
            fg="#ffffff",
            activebackground="#3b88ca",
            activeforeground="#ffffff",
            cursor="hand2",
            command=lambda: pick("file"),
        ).pack(side=LEFT, ipadx=14, ipady=5, padx=(0, 8))

        tk.Button(
            row,
            text="Folder",
            font=("Segoe UI", 10, "bold"),
            relief="flat",
            bd=0,
            bg="#3c8f2d",
            fg="#ffffff",
            activebackground="#4ba93a",
            activeforeground="#ffffff",
            cursor="hand2",
            command=lambda: pick("folder"),
        ).pack(side=LEFT, ipadx=14, ipady=5, padx=(0, 8))

        tk.Button(
            row,
            text="Cancel",
            font=("Segoe UI", 10, "bold"),
            relief="flat",
            bd=0,
            bg="#2d3a4a",
            fg="#d7e8f7",
            activebackground="#3a4d62",
            activeforeground="#ffffff",
            cursor="hand2",
            command=lambda: pick(None),
        ).pack(side=LEFT, ipadx=14, ipady=5)

        dlg.protocol("WM_DELETE_WINDOW", lambda: pick(None))
        self.root.update_idletasks()
        x = self.root.winfo_x() + (self.root.winfo_width() // 2) - 170
        y = self.root.winfo_y() + (self.root.winfo_height() // 2) - 70
        dlg.geometry(f"340x120+{x}+{y}")
        dlg.wait_window()
        return choice["value"]

    def share_file_or_folder_for_selected_game(self) -> None:
        kind = self._ask_share_kind_dialog()
        if not kind:
            return
        if kind == "file":
            picked = filedialog.askopenfilename(title="Share file")
        else:
            picked = filedialog.askdirectory(title="Share folder")
        if not picked:
            return
        rp = str(Path(picked).resolve())
        if rp not in self.shared_files:
            self.shared_files.append(rp)
            self._save_shared_files()
            self.refresh_game_list()
            self._refresh_shared_items_ui(self.selected_game())

    def _shared_download_key(self, entry: GameEntry, item: dict) -> str:
        owner = entry.owner.lower()
        iid = str(item.get("id", ""))
        return f"{owner}::{iid}::{item.get('name','').lower()}"

    def _refresh_shared_items_ui(self, entry: Optional[GameEntry]) -> None:
        for child in self.shared_items_frame.winfo_children():
            child.destroy()
        if not entry:
            self.shared_items_frame.pack_forget()
            return
        items = entry.shared_items or []
        if not items:
            self.shared_items_frame.pack_forget()
            return
        if not self.shared_items_frame.winfo_ismapped():
            self.shared_items_frame.pack(fill=X, pady=(0, 8), before=self.details_text)
        for it in items:
            row = tk.Frame(self.shared_items_frame, bg="#101822")
            row.pack(fill=X, padx=6, pady=5)
            key = self._shared_download_key(entry, it)
            local_path = self.shared_download_index.get(key, "")
            have_local = bool(local_path and Path(local_path).exists())
            if entry.source == "local":
                have_local = True
            btn_text = "OPEN" if have_local else "GET"
            btn_bg = "#3c8f2d" if have_local else "#2d6fa8"
            btn_active = "#4ba93a" if have_local else "#3b88ca"
            btn = tk.Button(
                row,
                text=btn_text,
                font=("Segoe UI", 11, "bold"),
                relief="flat",
                bd=0,
                bg=btn_bg,
                fg="#ffffff",
                activebackground=btn_active,
                activeforeground="#ffffff",
                cursor="hand2",
                command=lambda e=entry, i=it: self.on_shared_item_action(e, i),
            )
            btn.pack(side=LEFT, padx=(0, 10), ipadx=12, ipady=3)
            size_mb = float(it.get("size", 0)) / (1024 * 1024)
            text = f"{it.get('name', 'Shared item')} ({'Folder' if it.get('is_dir') else 'File'}, {size_mb:.2f} MB)"
            tk.Label(row, text=text, bg="#101822", fg="#d7e8f7", font=("Segoe UI", 10, "bold")).pack(side=LEFT, anchor="w")

            if entry.source == "local":
                trash_btn = tk.Button(
                    row,
                    text="🗑",
                    font=("Segoe UI", 11, "bold"),
                    relief="flat",
                    bd=0,
                    bg="#101822",
                    fg="#7f95aa",
                    activebackground="#1d2c3d",
                    activeforeground="#d7e8f7",
                    cursor="hand2",
                    command=lambda i=it: self.remove_shared_item(i),
                )
                trash_btn.pack(side=RIGHT, padx=(10, 0), ipadx=6, ipady=2)
                trash_btn.bind("<Enter>", lambda _e, b=trash_btn: b.configure(bg="#1d2c3d", fg="#d7e8f7"))
                trash_btn.bind("<Leave>", lambda _e, b=trash_btn: b.configure(bg="#101822", fg="#7f95aa"))

    def on_shared_item_action(self, entry: GameEntry, item: dict) -> None:
        key = self._shared_download_key(entry, item)
        local_path = self.shared_download_index.get(key, "")
        if entry.source == "local":
            idx = int(item.get("id", -1))
            if 0 <= idx < len(self.shared_files):
                os.startfile(self.shared_files[idx])
            return
        if local_path and Path(local_path).exists():
            os.startfile(local_path)
            return
        item_name = item.get("name", "shared_item")
        is_dir = bool(item.get("is_dir", False))
        base = ""
        file_target = ""
        if is_dir:
            base = filedialog.askdirectory(title=f"Choose destination for {item_name}")
            if not base:
                return
        else:
            save_path = filedialog.asksaveasfilename(
                title=f"Save shared file: {item_name}",
                initialfile=item_name,
                defaultextension=Path(item_name).suffix if Path(item_name).suffix else "",
                filetypes=[("All files", "*.*")],
            )
            if not save_path:
                return
            file_target = str(Path(save_path).resolve())
            base = str(Path(file_target).parent)
        self.download_active = True
        self.download_progress["value"] = 0.0
        self.download_status.config(text="Download: preparing shared item...")
        threading.Thread(
            target=self._download_shared_item_worker,
            args=(entry, item, Path(base), file_target),
            daemon=True,
        ).start()

    def remove_shared_item(self, item: dict) -> None:
        try:
            idx = int(item.get("id", -1))
        except Exception:
            return
        if idx < 0 or idx >= len(self.shared_files):
            return
        self.shared_files.pop(idx)
        self._save_shared_files()
        self.refresh_game_list()
        self._refresh_shared_items_ui(self.selected_game())

    def _download_shared_item_worker(self, entry: GameEntry, item: dict, base_dir: Path, file_target: str = "") -> None:
        try:
            list_url = f"{entry.remote_base_url}/sharedlist"
            with urlopen(list_url, timeout=20) as resp:
                payload = json.loads(resp.read().decode("utf-8"))
            remote_items = payload.get("items", [])
            target = None
            for it in remote_items:
                if str(it.get("id", "")) == str(item.get("id", "")):
                    target = it
                    break
            if not target:
                raise RuntimeError("Shared item not found on peer.")
            target_name = target.get("name", "shared_item")
            target_root = (base_dir / target_name).resolve()
            files = target.get("files", [])
            total_bytes = int(target.get("total_bytes", 0))
            downloaded_total = 0
            is_dir = bool(target.get("is_dir", False))
            if is_dir:
                target_root.mkdir(parents=True, exist_ok=True)
            else:
                base_dir.mkdir(parents=True, exist_ok=True)
            for idx, f in enumerate(files, start=1):
                rel = f.get("path", "")
                if not rel:
                    continue
                rel_path = Path(rel)
                if not is_dir and file_target:
                    dest = Path(file_target).resolve()
                else:
                    dest = (target_root / rel_path).resolve()
                dest.parent.mkdir(parents=True, exist_ok=True)
                file_url = (
                    f"{entry.remote_base_url}/sharedfile?item={quote(str(item.get('id', '')))}"
                    f"&path={quote(str(rel_path).replace(os.sep, '/'))}"
                )
                with urlopen(file_url, timeout=35) as resp:
                    try:
                        out = dest.open("wb")
                    except PermissionError:
                        fallback = Path.home() / "Downloads" / dest.name
                        fallback.parent.mkdir(parents=True, exist_ok=True)
                        out = fallback.open("wb")
                        dest = fallback
                    with out:
                        while True:
                            chunk = resp.read(1024 * 256)
                            if not chunk:
                                break
                            out.write(chunk)
                            downloaded_total += len(chunk)
                            pct = (downloaded_total / total_bytes * 100.0) if total_bytes > 0 else 0.0
                            self.ui_queue.put(
                                (
                                    "download_progress",
                                    {
                                        "percent": pct,
                                        "text": f"Download: {pct:.1f}% - Shared item {idx}/{len(files)}: {rel}",
                                    },
                                )
                            )
            open_path = str((base_dir / target_name).resolve()) if is_dir else str(Path(file_target).resolve() if file_target else (base_dir / target_name).resolve())
            if not Path(open_path).exists():
                open_path = str((Path.home() / "Downloads" / Path(target_name).name).resolve())
            self.ui_queue.put(("shared_item_done", {"entry": entry, "item": item, "path": open_path}))
        except Exception as ex:
            self.ui_queue.put(("download_error", {"text": f"Shared item download failed: {ex}"}))

    def choose_folder(self) -> None:
        folder = filedialog.askdirectory(title="Pick a game root folder")
        if not folder:
            return
        self.state.set_shared_root(folder)
        self._merge_manual_games_into_state()
        self._apply_exe_overrides_to_state()
        self.config["shared_root"] = folder
        save_config(self.config)
        self.refresh_game_list()

    def _build_local_game_meta(self, game_dir: Path, game_name: str = "") -> Optional[dict]:
        if not game_name:
            game_name = game_dir.name
        launch_exe = self.state.pick_launch_exe(game_dir)
        if not launch_exe:
            return None
        rel_launch = GameShareState.rel_to_game_dir(game_dir, Path(launch_exe))
        asset_icon, asset_banner = self.state.get_asset_paths(game_name)
        folder_cover = self.state.pick_cover_image(game_dir)
        return {
            "path": str(game_dir.resolve()),
            "launch_exe": launch_exe,
            "launch_exe_rel": rel_launch,
            "cover_path": asset_banner or folder_cover,
            "icon_path": asset_icon,
        }

    def _merge_manual_games_into_state(self) -> None:
        valid: list[str] = []
        with self.state.lock:
            for folder in self.manual_game_folders:
                p = Path(folder)
                if not p.exists() or not p.is_dir():
                    continue
                meta = self._build_local_game_meta(p, p.name)
                if not meta:
                    continue
                valid.append(str(p.resolve()))
                base_name = p.name
                game_name = base_name
                n = 2
                while game_name in self.state.games and self.state.games[game_name].get("path", "") != meta["path"]:
                    game_name = f"{base_name} ({n})"
                    n += 1
                self.state.games[game_name] = meta
            self.state.games = dict(sorted(self.state.games.items(), key=lambda kv: kv[0].lower()))
        if valid != self.manual_game_folders:
            self.manual_game_folders = valid
            self.config["manual_game_folders"] = self.manual_game_folders
            save_config(self.config)

    def add_game_to_list(self) -> None:
        folder = filedialog.askdirectory(title="Choose game folder to add")
        if not folder:
            return
        p = Path(folder).resolve()
        meta = self._build_local_game_meta(p, p.name)
        if not meta:
            messagebox.showwarning(APP_NAME, "No executable found in that folder.")
            return
        folder_str = str(p)
        if folder_str not in self.manual_game_folders:
            self.manual_game_folders.append(folder_str)
            self.config["manual_game_folders"] = self.manual_game_folders
            save_config(self.config)
        self.refresh_local_games()
        self._start_auto_asset_fetch_for_missing()

    def _load_saved_folder(self) -> None:
        saved = self.config.get("shared_root", "")
        if saved and os.path.isdir(saved):
            self.state.set_shared_root(saved)
        else:
            with self.state.lock:
                self.state.games = {}
        self._merge_manual_games_into_state()
        self._apply_exe_overrides_to_state()
        self.refresh_game_list()
        self._start_auto_asset_fetch_for_missing()

    def refresh_local_games(self) -> None:
        if self.state.shared_root:
            self.state.set_shared_root(self.state.shared_root)
        else:
            with self.state.lock:
                self.state.games = {}
        self._merge_manual_games_into_state()
        self._apply_exe_overrides_to_state()
        self.refresh_game_list()

    def _on_peers_update(self, peers: list[dict]) -> None:
        self.ui_queue.put(("peers", peers))

    def _drain_ui_queue(self) -> None:
        while not self.ui_queue.empty():
            event, payload = self.ui_queue.get_nowait()
            if event == "peers":
                self.remote_peers = payload
                self.refresh_game_list()
            elif event == "download_progress":
                self.download_progress["value"] = payload.get("percent", 0.0)
                self.download_status.config(text=payload.get("text", "Downloading..."))
            elif event == "download_done":
                self.download_active = False
                self.download_progress["value"] = 100.0
                self.download_status.config(text=payload.get("text", "Download complete"))
            elif event == "download_error":
                self.download_active = False
                self.download_progress["value"] = 0.0
                self.download_status.config(text=payload.get("text", "Download failed"))
                messagebox.showerror(APP_NAME, payload.get("text", "Download failed"))
            elif event == "shared_item_done":
                self.download_active = False
                self.download_progress["value"] = 100.0
                p = payload.get("path", "")
                e = payload.get("entry")
                it = payload.get("item")
                if e and it and p:
                    key = self._shared_download_key(e, it)
                    self.shared_download_index[key] = p
                    self._save_shared_files()
                self.download_status.config(text=f"Download complete: {p}")
                self._refresh_shared_items_ui(self.selected_game())
            elif event == "assets_refreshed":
                self.refresh_local_games()
        self.root.after(250, self._drain_ui_queue)

    def refresh_game_list(self) -> None:
        self.game_rows.clear()
        by_name: dict[str, GameEntry] = {}

        with self.state.lock:
            for game_name, meta in self.state.games.items():
                local_entry = GameEntry(
                    name=game_name,
                    owner=self.state.host_name,
                    source="local",
                    local_path=meta["path"],
                    launch_exe=meta["launch_exe"],
                    cover_path=self._resolve_banner_for_game(game_name, meta.get("cover_path", "")),
                    icon_path=self._resolve_icon_for_game(game_name, meta.get("icon_path", "")),
                )
                by_name[game_name.lower()] = local_entry

        for peer in self.remote_peers:
            host = peer.get("host", "Unknown")
            ip = peer.get("ip", "")
            port = peer.get("http_port", HTTP_PORT)
            base_url = f"http://{ip}:{port}"
            for game in peer.get("games", []):
                name = game.get("name", "Unknown Game")
                key = name.lower()
                if key in by_name:
                    continue
                by_name[key] = GameEntry(
                    name=name,
                    owner=host,
                    source="remote",
                    cover_path="",
                    icon_path="",
                    remote_base_url=base_url,
                    remote_game_key=name,
                    remote_launch_exe=game.get("launch_exe_rel", ""),
                    remote_icon_url=f"{base_url}/asset?game={quote(name)}&kind=icon" if game.get("has_icon", False) else "",
                    remote_banner_url=f"{base_url}/asset?game={quote(name)}&kind=banner" if game.get("has_banner", False) else "",
                    shared_items=peer.get("shared_items", []),
                )

        self.game_rows = list(by_name.values())
        hidden = set(self.hidden_entries)
        if hidden:
            self.game_rows = [r for r in self.game_rows if self._entry_hide_key(r) not in hidden]

        self.apply_filters()

        # Intentionally no status text in banner area (clean media-first layout).

    def apply_filters(self) -> None:
        prev_entry = self.selected_game()
        prev_key = prev_entry.name.lower() if prev_entry else ""
        query = self.search_var.get().strip().lower()

        rows = self.game_rows[:]

        if query:
            rows = [
                r
                for r in rows
                if query in r.name.lower()
                or query in r.owner.lower()
                or query in r.local_path.lower()
            ]
        rows.sort(key=lambda r: r.name.lower())

        self.filtered_rows = rows

        for item in self.games_tree.get_children():
            self.games_tree.delete(item)
        for i, row in enumerate(self.filtered_rows):
            text = f" {row.name}"
            thumb = self._get_game_thumb(row)
            self.games_tree.insert("", "end", iid=str(i), text=text, image=thumb)

        restored = False
        if prev_key:
            for i, row in enumerate(self.filtered_rows):
                if row.name.lower() == prev_key:
                    iid = str(i)
                    self.games_tree.selection_set(iid)
                    self.games_tree.focus(iid)
                    self.games_tree.see(iid)
                    restored = True
                    break

        if restored:
            self.on_game_selected()
        else:
            if self.filtered_rows:
                self.games_tree.selection_set("0")
                self.games_tree.focus("0")
                self.games_tree.see("0")
                self.on_game_selected()
            else:
                self.details_text.delete("1.0", END)
                self._clear_banner_image()
                self._refresh_shared_items_ui(None)
                self._update_primary_action_button(None)

    def selected_game(self) -> Optional[GameEntry]:
        selected = self.games_tree.selection()
        if not selected:
            return None
        idx = int(selected[0])
        if idx >= len(self.filtered_rows):
            return None
        return self.filtered_rows[idx]

    def on_list_right_click(self, event) -> None:
        row_id = self.games_tree.identify_row(event.y)
        if not row_id:
            return
        self.games_tree.selection_set(row_id)
        self.games_tree.focus(row_id)
        self.on_game_selected()
        self.list_menu.tk_popup(event.x_root, event.y_root)

    def _entry_hide_key(self, entry: GameEntry) -> str:
        if entry.source == "local":
            return f"local::{entry.local_path.lower()}"
        return f"remote::{entry.owner.lower()}::{entry.name.lower()}"

    def remove_selected_entry(self) -> None:
        entry = self.selected_game()
        if not entry:
            return
        if not messagebox.askyesno(APP_NAME, f"Remove '{entry.name}' from the list?"):
            return

        changed = False
        if entry.source == "local":
            lp = entry.local_path
            if lp:
                new_manual = [p for p in self.manual_game_folders if str(Path(p).resolve()).lower() != str(Path(lp).resolve()).lower()]
                if new_manual != self.manual_game_folders:
                    self.manual_game_folders = new_manual
                    self.config["manual_game_folders"] = self.manual_game_folders
                    changed = True
        key = self._entry_hide_key(entry)
        if key not in self.hidden_entries:
            self.hidden_entries.append(key)
            self.config["hidden_entries"] = self.hidden_entries
            changed = True
        if changed:
            save_config(self.config)
        self.refresh_local_games()

    def _apply_exe_overrides_to_state(self) -> None:
        with self.state.lock:
            for _game_name, meta in self.state.games.items():
                game_path = meta.get("path", "")
                override_exe = self.exe_overrides.get(game_path, "")
                if not override_exe:
                    continue
                if not os.path.isfile(override_exe):
                    continue
                try:
                    game_dir = Path(game_path).resolve()
                    chosen = Path(override_exe).resolve()
                    chosen.relative_to(game_dir)
                except Exception:
                    continue
                meta["launch_exe"] = str(chosen)
                meta["launch_exe_rel"] = GameShareState.rel_to_game_dir(game_dir, chosen)

    def set_launch_executable_for_selected(self) -> None:
        entry = self.selected_game()
        if not entry or entry.source != "local":
            messagebox.showinfo(APP_NAME, "Select a local game first.")
            return
        initial = entry.launch_exe if entry.launch_exe and os.path.isfile(entry.launch_exe) else entry.local_path
        chosen = filedialog.askopenfilename(
            title=f"Choose launch EXE for {entry.name}",
            initialdir=entry.local_path,
            initialfile=os.path.basename(initial) if os.path.isfile(initial) else "",
            filetypes=[("Executable Files", "*.exe")],
        )
        if not chosen:
            return
        chosen_abs = str(Path(chosen).resolve())
        game_dir = Path(entry.local_path).resolve()
        try:
            Path(chosen_abs).relative_to(game_dir)
        except Exception:
            messagebox.showwarning(APP_NAME, "Please choose an executable inside that game's folder.")
            return

        self.exe_overrides[entry.local_path] = chosen_abs
        self.config["exe_overrides"] = self.exe_overrides
        save_config(self.config)

        with self.state.lock:
            meta = self.state.games.get(entry.name)
            if meta and meta.get("path") == entry.local_path:
                meta["launch_exe"] = chosen_abs
                meta["launch_exe_rel"] = GameShareState.rel_to_game_dir(game_dir, Path(chosen_abs))
        self.refresh_game_list()

    def _get_or_create_asset_override(self, game_name: str) -> dict:
        key = slugify_game_name(game_name)
        if key not in self.asset_overrides:
            self.asset_overrides[key] = {}
        return self.asset_overrides[key]

    def _resolve_icon_for_game(self, game_name: str, default_path: str) -> str:
        ov = self.asset_overrides.get(slugify_game_name(game_name), {})
        icon = ov.get("icon_path", "")
        return icon if icon and os.path.isfile(icon) else default_path

    def _resolve_banner_for_game(self, game_name: str, default_path: str) -> str:
        ov = self.asset_overrides.get(slugify_game_name(game_name), {})
        banner = ov.get("banner_path", "")
        return banner if banner and os.path.isfile(banner) else default_path

    def _save_asset_overrides(self) -> None:
        self.config["asset_overrides"] = self.asset_overrides
        save_config(self.config)

    def set_icon_for_selected(self) -> None:
        entry = self.selected_game()
        if not entry:
            messagebox.showinfo(APP_NAME, "Select a game first.")
            return
        selected = filedialog.askopenfilename(
            title=f"Set icon for {entry.name}",
            filetypes=[("Image files", "*.png;*.jpg;*.jpeg;*.gif"), ("All files", "*.*")],
        )
        if not selected:
            return
        ov = self._get_or_create_asset_override(entry.name)
        ov["icon_path"] = str(Path(selected).resolve())
        self._save_asset_overrides()
        self.refresh_game_list()

    def set_banner_for_selected(self) -> None:
        entry = self.selected_game()
        if not entry:
            messagebox.showinfo(APP_NAME, "Select a game first.")
            return
        selected = filedialog.askopenfilename(
            title=f"Set banner for {entry.name}",
            filetypes=[("Image files", "*.png;*.jpg;*.jpeg;*.gif"), ("All files", "*.*")],
        )
        if not selected:
            return
        ov = self._get_or_create_asset_override(entry.name)
        ov["banner_path"] = str(Path(selected).resolve())
        self._save_asset_overrides()
        self.refresh_game_list()

    def _http_get_text(self, url: str) -> str:
        req = Request(
            url,
            headers={
                "User-Agent": HTTP_USER_AGENT,
                "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                "Referer": "https://www.steamgriddb.com/",
            },
        )
        with urlopen(req, timeout=25) as resp:
            return resp.read().decode("utf-8", errors="ignore")

    def _http_get_json(self, url: str, auth_bearer: str = "") -> dict:
        headers = {
            "User-Agent": HTTP_USER_AGENT,
            "Accept": "application/json",
            "Referer": "https://www.steamgriddb.com/",
        }
        if auth_bearer:
            headers["Authorization"] = f"Bearer {auth_bearer}"
        req = Request(url, headers=headers)
        with urlopen(req, timeout=30) as resp:
            return json.loads(resp.read().decode("utf-8", errors="ignore"))

    def _download_file(self, url: str, path: Path) -> None:
        candidates = [url]
        if "/thumb/" in url:
            candidates.append(url.replace("/thumb/", "/"))
        else:
            parts = url.split("steamgriddb.com/")
            if len(parts) == 2:
                candidates.append(parts[0] + "steamgriddb.com/thumb/" + parts[1])
        last_err = None
        for candidate in candidates:
            try:
                req = Request(
                    candidate,
                    headers={
                        "User-Agent": HTTP_USER_AGENT,
                        "Accept": "image/avif,image/webp,image/apng,image/*,*/*;q=0.8",
                        "Referer": "https://www.steamgriddb.com/",
                    },
                )
                with urlopen(req, timeout=45) as resp:
                    data = resp.read()
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                return
            except Exception as ex:
                last_err = ex
        if last_err:
            raise last_err

    def _asset_query_terms_for_entry(self, entry: GameEntry) -> list[str]:
        terms = []
        if entry.local_path:
            folder_name = Path(entry.local_path).name.strip()
            if folder_name:
                terms.append(folder_name)
        terms.append(entry.name)
        # keep unique order
        seen = set()
        out = []
        for t in terms:
            k = t.lower()
            if k not in seen:
                seen.add(k)
                out.append(t)
        return out

    def _find_steamgriddb_game_id(self, game_name: str) -> int:
        if not STEAMGRIDDB_API_KEY:
            return 0
        target = normalize_name(game_name)
        best_id = 0
        best_score = -1
        try:
            endpoint = f"{STEAMGRIDDB_API_BASE}/search/autocomplete/{quote(game_name)}"
            payload = self._http_get_json(endpoint, auth_bearer=STEAMGRIDDB_API_KEY)
            for item in payload.get("data", []):
                gid = int(item.get("id", 0) or 0)
                name = str(item.get("name", ""))
                norm = normalize_name(name)
                score = 1
                if target and target in norm:
                    score += 3
                if norm and norm in target:
                    score += 2
                if score > best_score and gid:
                    best_score = score
                    best_id = gid
        except Exception:
            pass
        return best_id

    def _extract_url_from_sgdb_item(self, item: dict) -> str:
        for k in ("url", "thumb", "thumbnail", "icon", "image"):
            v = item.get(k)
            if isinstance(v, str) and v.startswith("http"):
                return v
        return ""

    def _extract_image_urls_from_steamgriddb_api(self, game_id: int) -> tuple[str, str]:
        if not STEAMGRIDDB_API_KEY or game_id <= 0:
            return "", ""
        icon_url = ""
        banner_url = ""
        try:
            data = self._http_get_json(f"{STEAMGRIDDB_API_BASE}/icons/game/{game_id}", auth_bearer=STEAMGRIDDB_API_KEY).get("data", [])
            if isinstance(data, list) and data:
                icon_url = self._extract_url_from_sgdb_item(data[0])
        except Exception:
            pass
        try:
            data = self._http_get_json(f"{STEAMGRIDDB_API_BASE}/heroes/game/{game_id}", auth_bearer=STEAMGRIDDB_API_KEY).get("data", [])
            if isinstance(data, list) and data:
                banner_url = self._extract_url_from_sgdb_item(data[0])
        except Exception:
            pass
        if not banner_url:
            try:
                data = self._http_get_json(f"{STEAMGRIDDB_API_BASE}/grids/game/{game_id}", auth_bearer=STEAMGRIDDB_API_KEY).get("data", [])
                if isinstance(data, list) and data:
                    banner_url = self._extract_url_from_sgdb_item(data[0])
            except Exception:
                pass
        return icon_url, banner_url

    def _download_assets_for_entry(self, entry: GameEntry, silent: bool = True) -> bool:
        if not STEAMGRIDDB_API_KEY:
            if not silent:
                messagebox.showerror(APP_NAME, "SteamGridDB API key is missing.")
            return False
        game_id = 0
        for term in self._asset_query_terms_for_entry(entry):
            game_id = self._find_steamgriddb_game_id(term)
            if game_id:
                break
        if not game_id:
            if not silent:
                messagebox.showerror(APP_NAME, "SteamGridDB download failed:\nNo matching SteamGridDB game page found.")
            return False
        icon_url, banner_url = self._extract_image_urls_from_steamgriddb_api(game_id)
        if not icon_url and not banner_url:
            if not silent:
                messagebox.showerror(APP_NAME, "SteamGridDB download failed:\nNo usable image links found on SteamGridDB page.")
            return False
        game_dir = ASSETS_DIR / slugify_game_name(entry.name)
        game_dir.mkdir(parents=True, exist_ok=True)
        if icon_url:
            self._download_file(icon_url, game_dir / "icon.png")
        if banner_url:
            self._download_file(banner_url, game_dir / "banner.png")
        if not silent:
            messagebox.showinfo(APP_NAME, f"Assets downloaded to:\n{game_dir}")
        return True

    def _start_auto_asset_fetch_for_missing(self) -> None:
        if not STEAMGRIDDB_API_KEY:
            return
        threading.Thread(target=self._auto_asset_fetch_worker, daemon=True).start()


    def _auto_asset_fetch_worker(self) -> None:
        downloaded_any = False
        with self.state.lock:
            locals_snapshot = [
                GameEntry(
                    name=game_name,
                    owner=self.state.host_name,
                    source="local",
                    local_path=meta.get("path", ""),
                    launch_exe=meta.get("launch_exe", ""),
                    cover_path=meta.get("cover_path", ""),
                    icon_path=meta.get("icon_path", ""),
                )
                for game_name, meta in self.state.games.items()
            ]
        attempted_changed = False
        attempted = set(self.auto_asset_attempted)
        for entry in locals_snapshot:
            key = slugify_game_name(entry.name)
            if key in attempted:
                continue
            icon_path, banner_path = self.state.get_asset_paths(entry.name)
            if icon_path and banner_path:
                attempted.add(key)
                attempted_changed = True
                continue
            try:
                ok = self._download_assets_for_entry(entry, silent=True)
                attempted.add(key)
                attempted_changed = True
                if ok:
                    downloaded_any = True
            except Exception:
                attempted.add(key)
                attempted_changed = True
        if attempted_changed:
            self.auto_asset_attempted = sorted(attempted)
            self.config["auto_asset_attempted"] = self.auto_asset_attempted
            save_config(self.config)
        if downloaded_any:
            self.ui_queue.put(("assets_refreshed", {}))

    def download_assets_from_steamgriddb_for_selected(self) -> None:
        entry = self.selected_game()
        if not entry:
            messagebox.showinfo(APP_NAME, "Select a game first.")
            return
        try:
            if self._download_assets_for_entry(entry, silent=False):
                key = slugify_game_name(entry.name)
                if key not in self.auto_asset_attempted:
                    self.auto_asset_attempted.append(key)
                    self.config["auto_asset_attempted"] = self.auto_asset_attempted
                    save_config(self.config)
                self.refresh_local_games()
        except Exception as ex:
            messagebox.showerror(APP_NAME, f"SteamGridDB download failed:\\n{ex}")

    def _clear_banner_image(self) -> None:
        self.banner_image = None
        self.banner_canvas.delete("all")
        w = self.banner_canvas.winfo_width() or 1200
        h = 230
        self.banner_canvas.create_rectangle(0, 0, w, h, fill="#101822", outline="")

    def _draw_banner_chrome(self) -> None:
        self._position_primary_action_button()

    def _position_primary_action_button(self) -> None:
        if not self.primary_action_button.winfo_ismapped():
            return
        h = self.banner_canvas.winfo_height() or 230
        self.primary_action_button.place_configure(x=14, y=max(8, h - 54), width=150, height=42)

    def _get_game_thumb(self, entry: GameEntry) -> tk.PhotoImage:
        if entry.source == "remote" and not entry.icon_path:
            entry.icon_path = self._cached_remote_asset_path(entry, "icon")
        icon_candidate = entry.icon_path
        if icon_candidate and icon_candidate in self.thumb_images:
            return self.thumb_images[icon_candidate]
        if icon_candidate:
            try:
                pil = Image.open(icon_candidate).convert("RGBA")
                pil.thumbnail((self.thumb_size, self.thumb_size), Image.Resampling.LANCZOS)
                bg = Image.new("RGBA", (self.thumb_size, self.thumb_size), (0, 0, 0, 0))
                x = (self.thumb_size - pil.width) // 2
                y = (self.thumb_size - pil.height) // 2
                bg.paste(pil, (x, y), pil)
                thumb = ImageTk.PhotoImage(bg)
                self.thumb_images[icon_candidate] = thumb
                return thumb
            except Exception:
                pass

        if not entry.cover_path:
            return self.empty_thumb
        if entry.cover_path in self.thumb_images:
            return self.thumb_images[entry.cover_path]
        try:
            pil = Image.open(entry.cover_path).convert("RGBA")
            pil.thumbnail((self.thumb_size, self.thumb_size), Image.Resampling.LANCZOS)
            bg = Image.new("RGBA", (self.thumb_size, self.thumb_size), (0, 0, 0, 0))
            x = (self.thumb_size - pil.width) // 2
            y = (self.thumb_size - pil.height) // 2
            bg.paste(pil, (x, y), pil)
            thumb = ImageTk.PhotoImage(bg)
            self.thumb_images[entry.cover_path] = thumb
            return thumb
        except Exception:
            return self.empty_thumb

    def _draw_banner_overlay(self, title: str) -> None:
        w = self.banner_canvas.winfo_width() or 900
        h = 230
        self.banner_canvas.create_rectangle(0, 0, w, h, fill="#101822", outline="")
        self._draw_banner_chrome()

    def _show_cover(self, cover_path: str) -> None:
        self._clear_banner_image()
        if not cover_path:
            self._draw_banner_overlay("No cover image found in this folder.")
            return
        try:
            pil = Image.open(cover_path).convert("RGB")
            w = self.banner_canvas.winfo_width()
            if w <= 1:
                w = 920
            h = 230
            # Zoom to fill width while preserving aspect ratio.
            scale = w / pil.width if pil.width else 1.0
            new_w = w
            new_h = max(1, int(pil.height * scale))
            resized = pil.resize((new_w, new_h), Image.Resampling.LANCZOS)
            if new_h >= h:
                top = (new_h - h) // 2
                frame = resized.crop((0, top, w, top + h))
            else:
                frame = Image.new("RGB", (w, h), (16, 24, 34))
                y = (h - new_h) // 2
                frame.paste(resized, (0, y))
            self.banner_image = ImageTk.PhotoImage(frame)
            self.banner_canvas.create_image(0, 0, image=self.banner_image, anchor="nw")
            self._draw_banner_chrome()
        except Exception:
            self._draw_banner_overlay("Cover format unsupported for preview")

    def on_game_selected(self, _event=None) -> None:
        entry = self.selected_game()
        self.details_text.delete("1.0", END)
        if not entry:
            self._clear_banner_image()
            self._update_primary_action_button(None)
            return

        lines = [
            f"Game: {entry.name}",
            f"Owner: {entry.owner}",
            f"Source: {entry.source.upper()}",
        ]

        if entry.source == "local":
            lines.append(f"Path: {entry.local_path}")
            lines.append(f"Launch EXE: {entry.launch_exe}")
            lines.append(f"Cover: {entry.cover_path or 'None'}")
            self._show_cover(entry.cover_path)
            with self.state.lock:
                shared_snapshot = list(self.state.shared_items)
            entry.shared_items = [
                {
                    "id": str(i),
                    "name": it.get("name", ""),
                    "is_dir": bool(it.get("is_dir", False)),
                    "size": int(it.get("size", 0)),
                }
                for i, it in enumerate(shared_snapshot)
            ]
        else:
            lines.append(f"Remote: {entry.remote_base_url}")
            lines.append(f"Preferred EXE (shared): {entry.remote_launch_exe or 'Unknown'}")
            lines.append(f"File List URL: {entry.remote_base_url}/filelist/{quote(entry.remote_game_key)}")
            if not entry.cover_path and entry.remote_banner_url:
                entry.cover_path = self._ensure_remote_asset(entry, "banner")
            if entry.cover_path:
                self._show_cover(entry.cover_path)
            else:
                self._clear_banner_image()
                self._draw_banner_overlay("No shared banner asset found for this remote game.")

        self.details_text.insert("1.0", "\n".join(lines))
        self._refresh_shared_items_ui(entry)
        self._update_primary_action_button(entry)

    def _update_primary_action_button(self, entry: Optional[GameEntry]) -> None:
        if not entry:
            self.primary_action_button.config(
                text="SELECT A GAME",
                bg="#2d3a4a",
                activebackground="#3a4d62",
                state="disabled",
            )
            self.primary_action_button.place_forget()
            return
        if not self.primary_action_button.winfo_ismapped():
            self.primary_action_button.place(x=14, y=176, width=150, height=42)
        if entry.source == "local":
            self.primary_action_button.config(
                text="PLAY",
                bg="#3c8f2d",
                activebackground="#4ba93a",
                state="normal",
            )
        else:
            self.primary_action_button.config(
                text="DOWNLOAD",
                bg="#2d6fa8",
                activebackground="#3b88ca",
                state="normal",
            )

    def on_primary_action(self) -> None:
        entry = self.selected_game()
        if not entry:
            return
        if entry.source == "local":
            self.launch_game()
        else:
            self.download_remote_game()

    def _ensure_remote_asset(self, entry: GameEntry, kind: str) -> str:
        if kind not in ("icon", "banner"):
            return ""
        url = entry.remote_icon_url if kind == "icon" else entry.remote_banner_url
        if not url:
            return ""
        owner_slug = slugify_game_name(entry.owner) or "peer"
        game_slug = slugify_game_name(entry.name) or "game"
        cache_dir = LAN_ASSET_CACHE_DIR / owner_slug / game_slug
        cache_dir.mkdir(parents=True, exist_ok=True)
        target = cache_dir / f"{kind}.png"
        if target.exists() and target.stat().st_size > 0:
            return str(target.resolve())
        try:
            with urlopen(url, timeout=20) as resp:
                data = resp.read()
            target.write_bytes(data)
            return str(target.resolve())
        except Exception:
            return ""

    def _cached_remote_asset_path(self, entry: GameEntry, kind: str) -> str:
        if kind not in ("icon", "banner"):
            return ""
        owner_slug = slugify_game_name(entry.owner) or "peer"
        game_slug = slugify_game_name(entry.name) or "game"
        candidate = LAN_ASSET_CACHE_DIR / owner_slug / game_slug / f"{kind}.png"
        if candidate.exists() and candidate.stat().st_size > 0:
            return str(candidate.resolve())
        return ""

    def launch_game(self) -> None:
        entry = self.selected_game()
        if not entry or entry.source != "local":
            messagebox.showinfo(APP_NAME, "Select a local game first.")
            return
        if not entry.launch_exe or not os.path.exists(entry.launch_exe):
            messagebox.showwarning(APP_NAME, "Launch executable not found.")
            return
        os.startfile(entry.launch_exe)

    def open_local_folder(self) -> None:
        entry = self.selected_game()
        if not entry or entry.source != "local":
            messagebox.showinfo(APP_NAME, "Select a local game first.")
            return
        os.startfile(entry.local_path)

    def download_remote_game(self) -> None:
        entry = self.selected_game()
        if not entry or entry.source != "remote":
            messagebox.showinfo(APP_NAME, "Select a LAN game first.")
            return
        if self.download_active:
            messagebox.showinfo(APP_NAME, "A download is already in progress.")
            return

        base_folder = filedialog.askdirectory(title="Choose destination folder for downloaded game")
        if not base_folder:
            return

        target_root = Path(base_folder) / entry.name
        filelist_url = f"{entry.remote_base_url}/filelist/{quote(entry.remote_game_key)}"
        self.download_active = True
        self.download_progress["value"] = 0.0
        self.download_status.config(text="Download: preparing file list...")
        threading.Thread(target=self._download_game_worker, args=(entry.remote_base_url, entry.remote_game_key, target_root, filelist_url), daemon=True).start()

    def _download_game_worker(self, base_url: str, game_key: str, target_root: Path, filelist_url: str) -> None:
        try:
            with urlopen(filelist_url, timeout=20) as resp:
                manifest = json.loads(resp.read().decode("utf-8"))

            files = manifest.get("files", [])
            total_bytes = int(manifest.get("total_bytes", 0))
            downloaded_total = 0
            file_count = len(files)
            target_root.mkdir(parents=True, exist_ok=True)

            for idx, item in enumerate(files, start=1):
                rel = item.get("path", "")
                if not rel:
                    continue
                rel_path = Path(rel)
                destination = (target_root / rel_path).resolve()
                destination.parent.mkdir(parents=True, exist_ok=True)
                file_url = f"{base_url}/file?game={quote(game_key)}&path={quote(str(rel_path).replace(os.sep, '/'))}"
                with urlopen(file_url, timeout=30) as resp:
                    with destination.open("wb") as out:
                        while True:
                            chunk = resp.read(1024 * 256)
                            if not chunk:
                                break
                            out.write(chunk)
                            downloaded_total += len(chunk)
                            percent = (downloaded_total / total_bytes * 100.0) if total_bytes > 0 else 0.0
                            text = (
                                f"Download: {percent:.1f}% ({downloaded_total / (1024 * 1024):.2f} MB / "
                                f"{(total_bytes / (1024 * 1024)) if total_bytes > 0 else 0:.2f} MB) - "
                                f"File {idx}/{file_count}: {rel}"
                            )
                            self.ui_queue.put(("download_progress", {"percent": percent, "text": text}))
            self.ui_queue.put(("download_done", {"text": f"Download complete: {target_root}"}))
        except Exception as ex:
            self.ui_queue.put(("download_error", {"text": f"Download failed: {ex}"}))

    def open_remote_manifest(self) -> None:
        return

    def _on_close(self) -> None:
        try:
            if self.discovery:
                self.discovery.stop()
            if self.http_server:
                self.http_server.shutdown()
        except Exception:
            pass
        self.root.destroy()


def main() -> None:
    handle_setup_firewall_mode()
    if os.name == "nt":
        try:
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("Undertaker.LANGamesDeployer.1.1")
        except Exception:
            pass
    root = tk.Tk()
    ensure_firewall_rules_on_startup(root)
    LanGamesApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
