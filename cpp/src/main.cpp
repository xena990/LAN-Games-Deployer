#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <gdiplus.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <uxtheme.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdio>
#include <unordered_map>
#include <chrono>
#include <memory>
#include "resource_ids.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "uxtheme.lib")
 

static const wchar_t* kAppName = L"LAN Games Deployer C++";
static const int DISCOVERY_PORT = 50055;
static const int HTTP_PORT = 50111;
static const int HTTP_PORT_MAX = 50310;
static const UINT IDM_FILE_CHOOSE_ROOT = 40001;
static const UINT IDM_FILE_ADD_GAME = 40002;
static const UINT IDM_FILE_REFRESH = 40003;
static const UINT IDM_FILE_EXIT = 40004;
static const UINT IDM_FILE_SGDB_SELECTED = 40005;
static const UINT IDM_FILE_SGDB_ALL = 40006;
static const UINT IDM_FILE_REMOVE_ALL = 40007;
static const UINT IDM_FILE_TOGGLE_CACHE = 40008;
static const UINT IDM_FILE_TOGGLE_LAN_SHARE = 40009;
static const UINT IDM_CTX_SET_EXE = 41001;
static const UINT IDM_CTX_OPEN_FOLDER = 41002;
static const UINT IDM_CTX_SET_ICON = 41003;
static const UINT IDM_CTX_SET_BANNER = 41004;
static const UINT IDM_CTX_REMOVE_ENTRY = 41005;
static const UINT IDM_CTX_DOWNLOAD_ASSETS = 41006;
static const UINT IDM_CTX_SET_SGDB_ID = 41007;
static const int IDC_HAMBURGER = 12001;
static const int IDC_BTN_MIN = 12002;
static const int IDC_BTN_MAX = 12003;
static const int IDC_BTN_CLOSE = 12004;
static const int IDC_LISTSCROLL = 12005;
static const int IDC_CHAT_VIEW = 12100;
static const int IDC_CHAT_PEERS = 12101;
static const int IDC_CHAT_INPUT = 12102;
static const int IDC_CHAT_SEND = 12103;
static const int IDC_CHAT_BOLD = 12104;
static const int IDC_CHAT_ITALIC = 12105;
static const int IDC_CHAT_COLOR = 12106;
static const int IDC_CHAT_FONT = 12107;
static const int IDC_CHAT_EMOJI = 12108;
static const int IDC_CHAT_SELF = 12109;
static const int IDC_CHAT_SELF_AVATAR = 12110;
static const int IDC_CHAT_SELF_NAME = 12111;
static const int IDC_CHAT_SELF_STATUS = 12112;
static const int IDC_CHAT_PEERS_SCROLL = 12113;
static const int IDC_CHAT_VIEW_SCROLL = 12114;
static const int CHAT_PORT = 50057;
static const UINT IDM_SELF_SET_NICK = 46001;
static const UINT IDM_SELF_SET_AVATAR = 46006;
static const UINT IDM_EMOJI_BASE = 47000;
static const UINT IDM_EMOJI_LAST = 47031;
static const wchar_t* kEmojiPopupClass = L"LanEmojiGridPopup";
static const UINT IDM_SELF_STATUS_ONLINE = 46002;
static const UINT IDM_SELF_STATUS_BUSY = 46003;
static const UINT IDM_SELF_STATUS_INVISIBLE = 46005;
static const UINT IDT_IDLE_PRESENCE = 48001;
static std::wstring GetSteamGridDbApiKey() {
    wchar_t key[256]{};
    DWORD n = GetEnvironmentVariableW(L"STEAMGRIDDB_API_KEY", key, 255);
    if (n == 0 || n >= 255) return L"";
    return key;
}
static const wchar_t* FIREWALL_RULE_DISCOVERY = L"LAN Games Deployer C++ UDP Discovery";
static const wchar_t* FIREWALL_RULE_CHAT = L"LAN Games Deployer C++ UDP Chat";
static const wchar_t* FIREWALL_RULE_SHARE = L"LAN Games Deployer C++ TCP Share";
static const UINT WM_PEER_UPDATE = WM_APP + 1;
static const UINT WM_ASSETS_UPDATE = WM_APP + 2;
static const UINT WM_CHAT_APPEND = WM_APP + 3;
static const UINT WM_CHAT_PEERS_UPDATE = WM_APP + 4;
static const UINT WM_STATUSBAR_REFRESH = WM_APP + 5;

struct GameEntry {
    std::wstring name;
    std::wstring remoteGameName;
    std::wstring path;
    std::wstring launchExe;
    std::wstring iconPath;
    std::wstring bannerPath;
    std::wstring logoPath;
    bool local = true;
    std::wstring remoteHost;
    std::wstring remoteIp;
    int remotePort = HTTP_PORT;
};

struct Config {
    std::wstring sharedRoot;
    int winX = CW_USEDEFAULT;
    int winY = CW_USEDEFAULT;
    int winW = 1280;
    int winH = 820;
    bool winMax = false;
};
struct ChatLinePayload {
    std::wstring from;
    std::wstring msg;
    bool hasStyle = false;
    bool bold = false;
    bool italic = false;
    COLORREF color = RGB(220,236,248);
    std::wstring face;
};
struct ChatRenderItem {
    std::wstring from;
    std::wstring msg;
    COLORREF color = RGB(220,236,248);
    bool bold = false;
    bool italic = false;
};

static HWND g_main = nullptr, g_hamburger = nullptr, g_search = nullptr, g_list = nullptr, g_playGet = nullptr, g_share = nullptr, g_details = nullptr, g_status = nullptr;
static HWND g_btnMin = nullptr, g_btnMax = nullptr, g_btnClose = nullptr;
static HWND g_listScroll = nullptr;
static HWND g_chatView = nullptr, g_chatPeers = nullptr, g_chatInput = nullptr, g_chatSend = nullptr;
static HWND g_chatBold = nullptr, g_chatItalic = nullptr, g_chatColor = nullptr, g_chatFont = nullptr, g_chatEmoji = nullptr;
static HWND g_chatSelf = nullptr;
static HWND g_chatSelfAvatar = nullptr, g_chatSelfName = nullptr, g_chatSelfStatus = nullptr;
static HWND g_chatPeersScroll = nullptr;
static HWND g_chatViewScroll = nullptr;
static std::vector<GameEntry> g_games;
static std::vector<GameEntry> g_uiGames;
static std::vector<GameEntry> g_remoteGames;
static std::vector<ChatRenderItem> g_chatItems;
static std::atomic<bool> g_running{true};
static std::mutex g_gamesMutex;
static Config g_cfg;
static std::vector<std::wstring> g_manualGameFolders;
static std::vector<std::wstring> g_hiddenEntries;
static std::vector<std::pair<std::wstring, int>> g_manualSgdbIds;
static std::vector<std::pair<std::wstring, std::wstring>> g_nameAliases;
static ULONG_PTR g_gdiplusToken = 0;
static std::wstring g_hostName;
static std::vector<std::pair<std::wstring, std::wstring>> g_peers; // host, ip
static std::unordered_map<std::wstring, std::wstring> g_peerNick;
static std::unordered_map<std::wstring, std::wstring> g_peerPresence;
static std::unordered_map<std::wstring, std::wstring> g_peerAvatarPath;
static std::vector<std::wstring> g_localIps;
static std::unordered_map<std::wstring, ULONGLONG> g_recentChatIds;
static std::mutex g_chatDedupMutex;
static std::atomic<unsigned long long> g_chatMsgSeq{0};
static std::wstring g_myNick;
static std::wstring g_myPresence = L"Online";
static std::wstring g_instanceId;
static std::atomic<ULONGLONG> g_lastInteractionTick{0};
static std::wstring g_lastEffectivePresence = L"";
static std::atomic<int> g_httpPort{0};
static std::wstring g_baseStatus = L"Status: Ready";
static std::vector<std::pair<std::wstring, std::wstring>> g_transferStatusLines;
static std::mutex g_statusMutex;
static std::unordered_map<std::wstring, std::shared_ptr<Gdiplus::Image>> g_imageCache;
static std::mutex g_cacheMutex;
static size_t g_lastStatusLineCount = (size_t)-1;
static bool g_useImageCache = true;
static std::atomic<bool> g_lanShareEnabled{true};
static std::wstring g_lastSelectedGame;
static std::wstring g_lastSelectedPath;
static std::wstring g_nowPlayingGame;
static std::mutex g_playingMutex;
struct DownloadTask {
    GameEntry game;
    std::wstring destBase;
};
static std::deque<DownloadTask> g_downloadQueueTasks;
static std::mutex g_downloadQueueMutex;
static std::condition_variable g_downloadQueueCv;
static std::thread g_downloadQueueThread;
static std::atomic<bool> g_downloadQueueStop{false};
static HWND g_downloadQueueOwner = nullptr;

static HBRUSH g_bgBrush = nullptr, g_panelBrush = nullptr;
static HBRUSH g_searchBrush = nullptr;
static HBRUSH g_scrollBrush = nullptr;
static HFONT g_font = nullptr, g_fontBold = nullptr;
static HFONT g_fontEmoji = nullptr;
static HFONT g_fontSmall = nullptr;
static int g_hotTitleBtn = 0;
static bool g_trackingMouse = false;
static bool g_searchPlaceholder = true;
static const wchar_t* kLanButtonClass = L"LanCustomButton";
static WNDPROC g_oldTitleBtnProc = nullptr;
static WNDPROC g_oldListScrollProc = nullptr;
static WNDPROC g_oldPeersScrollProc = nullptr;
static WNDPROC g_oldChatScrollProc = nullptr;
static WNDPROC g_oldListProc = nullptr;
static WNDPROC g_oldPlayBtnProc = nullptr;
static WNDPROC g_oldChatViewProc = nullptr;
static WNDPROC g_oldChatInputProc = nullptr;
static WNDPROC g_oldChatToolBtnProc = nullptr;
static HWND g_emojiPopup = nullptr;
static bool g_hotPlayBtn = false;
static int g_hotChatToolBtn = 0;
static int g_sgdbPromptResult = -1;
static bool g_dragListThumb = false;
static int g_dragStartY = 0;
static int g_dragStartTop = 0;
static int g_lastDragTop = -1;
static RECT g_listThumbRect{};
static RECT g_peersThumbRect{};
static RECT g_chatThumbRect{};
static bool g_dragPeersThumb = false;
static int g_dragPeersStartY = 0;
static int g_dragPeersStartTop = 0;
static bool g_dragChatThumb = false;
static int g_dragChatStartY = 0;
static int g_dragChatStartTop = 0;
static COLORREF g_chatColorPick = RGB(220,236,248);
static std::wstring g_chatFontName = L"Segoe UI";
static bool g_chatBoldPick = false;
static bool g_chatItalicPick = false;
static const int kChatInputMaxChars = 150;
static const int kChatInputLineH = 20;
static std::wstring g_chatInputText;
static int g_chatInputCaret = 0;
static int g_chatSelStart = 0;
static int g_chatSelEnd = 0;
static bool g_chatSelecting = false;
static int g_chatSelAnchor = 0;
static int g_lastDblClickWordStart = -1;
static int g_lastDblClickWordEnd = -1;
static bool g_chatViewDragSel = false;
static int g_chatViewAnchor = -1;
static bool g_chatTextSelecting = false;
static int g_chatTextSelStartItem = -1;
static int g_chatTextSelStartChar = 0;
static int g_chatTextSelEndItem = -1;
static int g_chatTextSelEndChar = 0;
static int g_chatLastDblItem = -1;
static int g_chatLastDblWordStart = -1;
static int g_chatLastDblWordEnd = -1;
static const wchar_t* kChatInputClass = L"LanChatInputWnd";
struct LanButtonState {
    bool hot = false;
    bool pressed = false;
    bool tracking = false;
    HFONT font = nullptr;
};
struct EmojiItem { UINT id; const wchar_t* glyph; const wchar_t* label; };
static const EmojiItem kEmojiItems[] = {
    { IDM_EMOJI_BASE + 0,  L"\xD83D\xDE00", L"Grinning face" },           // 😀
    { IDM_EMOJI_BASE + 1,  L"\xD83D\xDE04", L"Smiling face" },            // 😄
    { IDM_EMOJI_BASE + 2,  L"\xD83D\xDE02", L"Face with tears of joy" },  // 😂
    { IDM_EMOJI_BASE + 3,  L"\xD83D\xDE09", L"Winking face" },            // 😉
    { IDM_EMOJI_BASE + 4,  L"\xD83D\xDE0A", L"Smiling eyes" },            // 😊
    { IDM_EMOJI_BASE + 5,  L"\xD83E\xDD14", L"Thinking face" },           // 🤔
    { IDM_EMOJI_BASE + 6,  L"\xD83D\xDE0E", L"Sunglasses" },              // 😎
    { IDM_EMOJI_BASE + 7,  L"\xD83E\xDD73", L"Party face" },              // 🥳
    { IDM_EMOJI_BASE + 8,  L"\xD83D\xDC4D", L"Thumbs up" },               // 👍
    { IDM_EMOJI_BASE + 9,  L"\xD83D\xDC4F", L"Clapping hands" },          // 👏
    { IDM_EMOJI_BASE + 10, L"\xD83D\xDE4F", L"Folded hands" },            // 🙏
    { IDM_EMOJI_BASE + 11, L"\xD83D\xDCAA", L"Flexed biceps" },           // 💪
    { IDM_EMOJI_BASE + 12, L"\x2764\xFE0F", L"Red heart" },               // ❤️
    { IDM_EMOJI_BASE + 13, L"\xD83D\xDD25", L"Fire" },                    // 🔥
    { IDM_EMOJI_BASE + 14, L"\xD83C\xDF89", L"Party popper" },            // 🎉
    { IDM_EMOJI_BASE + 15, L"\xD83D\xDE80", L"Rocket" }                   // 🚀
};
static std::wstring EmojiTokenForId(UINT id) {
    if (id < IDM_EMOJI_BASE || id > IDM_EMOJI_LAST) return L"";
    wchar_t buf[24]{};
    swprintf(buf, 24, L":e%u:", (unsigned)(id - IDM_EMOJI_BASE));
    return buf;
}
static std::wstring EmojiPngForId(UINT id) {
    switch (id) {
        case IDM_EMOJI_BASE + 0: return L"grinning_face.png";
        case IDM_EMOJI_BASE + 1: return L"smiling_face.png";
        case IDM_EMOJI_BASE + 2: return L"face_with_tears_of_joy.png";
        case IDM_EMOJI_BASE + 3: return L"winking_face.png";
        case IDM_EMOJI_BASE + 4: return L"smiling_face_with_smiling_eyes.png";
        case IDM_EMOJI_BASE + 5: return L"thinking_face.png";
        case IDM_EMOJI_BASE + 6: return L"smiling_face_with_sunglasses.png";
        case IDM_EMOJI_BASE + 7: return L"partying_face.png";
        case IDM_EMOJI_BASE + 8: return L"thumbs_up.png";
        case IDM_EMOJI_BASE + 9: return L"clapping_hands.png";
        case IDM_EMOJI_BASE + 10: return L"folded_hands.png";
        case IDM_EMOJI_BASE + 11: return L"flexed_biceps.png";
        case IDM_EMOJI_BASE + 12: return L"red_heart.png";
        case IDM_EMOJI_BASE + 13: return L"fire.png";
        case IDM_EMOJI_BASE + 14: return L"party_popper.png";
        case IDM_EMOJI_BASE + 15: return L"rocket.png";
        default: return L"";
    }
}
static std::wstring ResolveEmojiImagePath(const std::wstring& fileName) {
    if (fileName.empty()) return L"";
    auto hasFile = [](const std::wstring& p) -> bool {
        DWORD a = GetFileAttributesW(p.c_str());
        return (a != INVALID_FILE_ATTRIBUTES) && ((a & FILE_ATTRIBUTE_DIRECTORY) == 0);
    };
    wchar_t mod[MAX_PATH]{};
    GetModuleFileNameW(nullptr, mod, MAX_PATH);
    std::wstring exeDir = mod;
    size_t cut = exeDir.find_last_of(L"\\/");
    if (cut != std::wstring::npos) exeDir = exeDir.substr(0, cut);
    std::vector<std::wstring> roots;
    roots.push_back(exeDir + L"\\data\\emoji\\fluent\\");
    roots.push_back(exeDir + L"\\..\\data\\emoji\\fluent\\");
    roots.push_back(exeDir + L"\\..\\..\\data\\emoji\\fluent\\");
    roots.push_back(exeDir + L"\\..\\..\\..\\data\\emoji\\fluent\\");
    wchar_t cwd[MAX_PATH]{};
    if (GetCurrentDirectoryW(MAX_PATH, cwd) > 0) roots.push_back(std::wstring(cwd) + L"\\data\\emoji\\fluent\\");
    for (const auto& r : roots) {
        std::wstring p = r + fileName;
        if (hasFile(p)) return p;
    }
    return L"";
}
static const wchar_t* EmojiAsciiForId(UINT id) {
    switch (id) {
        case IDM_EMOJI_BASE + 0: return L":D";
        case IDM_EMOJI_BASE + 1: return L":)";
        case IDM_EMOJI_BASE + 2: return L":,)";
        case IDM_EMOJI_BASE + 3: return L";)";
        case IDM_EMOJI_BASE + 4: return L"^^";
        case IDM_EMOJI_BASE + 5: return L":?";
        case IDM_EMOJI_BASE + 6: return L"8)";
        case IDM_EMOJI_BASE + 7: return L"<o/";
        case IDM_EMOJI_BASE + 8: return L"+1";
        case IDM_EMOJI_BASE + 9: return L"clap";
        case IDM_EMOJI_BASE + 10: return L"pray";
        case IDM_EMOJI_BASE + 11: return L"flex";
        case IDM_EMOJI_BASE + 12: return L"<3";
        case IDM_EMOJI_BASE + 13: return L"fire";
        case IDM_EMOJI_BASE + 14: return L"party";
        case IDM_EMOJI_BASE + 15: return L"go!";
    }
    return L":)";
}

static void EnsureBundledEmojiAssets() {
    auto getExeDir = []() -> std::wstring {
        wchar_t mod[MAX_PATH]{};
        GetModuleFileNameW(nullptr, mod, MAX_PATH);
        std::wstring p = mod;
        size_t cut = p.find_last_of(L"\\/");
        return (cut == std::wstring::npos) ? p : p.substr(0, cut);
    };
    auto ensureDir = [](const std::wstring& p) {
        size_t start = 0;
        if (p.size() > 2 && p[1] == L':') start = 3;
        for (size_t i = start; i < p.size(); ++i) {
            if (p[i] == L'\\' || p[i] == L'/') {
                std::wstring sub = p.substr(0, i);
                if (!sub.empty()) CreateDirectoryW(sub.c_str(), nullptr);
            }
        }
        CreateDirectoryW(p.c_str(), nullptr);
    };
    auto fileExists = [](const std::wstring& p) -> bool {
        DWORD a = GetFileAttributesW(p.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
    };

    std::wstring exeDir = getExeDir();
    ensureDir(exeDir + L"\\data");
    ensureDir(exeDir + L"\\data\\emoji");
    ensureDir(exeDir + L"\\data\\emoji\\fluent");
    struct EmojiRes {
        int id;
        const wchar_t* file;
    };
    const EmojiRes items[] = {
        { IDR_EMOJI_GRINNING_FACE, L"grinning_face.png" },
        { IDR_EMOJI_SMILING_FACE, L"smiling_face.png" },
        { IDR_EMOJI_TEAR_OF_JOY, L"face_with_tears_of_joy.png" },
        { IDR_EMOJI_WINKING_FACE, L"winking_face.png" },
        { IDR_EMOJI_SMILING_EYES, L"smiling_face_with_smiling_eyes.png" },
        { IDR_EMOJI_THINKING_FACE, L"thinking_face.png" },
        { IDR_EMOJI_SUNGLASSES, L"smiling_face_with_sunglasses.png" },
        { IDR_EMOJI_PARTYING_FACE, L"partying_face.png" },
        { IDR_EMOJI_THUMBS_UP, L"thumbs_up.png" },
        { IDR_EMOJI_CLAPPING_HANDS, L"clapping_hands.png" },
        { IDR_EMOJI_FOLDED_HANDS, L"folded_hands.png" },
        { IDR_EMOJI_FLEXED_BICEPS, L"flexed_biceps.png" },
        { IDR_EMOJI_RED_HEART, L"red_heart.png" },
        { IDR_EMOJI_FIRE, L"fire.png" },
        { IDR_EMOJI_PARTY_POPPER, L"party_popper.png" },
        { IDR_EMOJI_ROCKET, L"rocket.png" },
    };
    HMODULE hm = GetModuleHandleW(nullptr);
    for (const auto& it : items) {
        std::wstring dst = exeDir + L"\\data\\emoji\\fluent\\" + it.file;
        if (fileExists(dst)) continue;
        HRSRC r = FindResourceW(hm, MAKEINTRESOURCEW(it.id), RT_RCDATA);
        if (!r) continue;
        HGLOBAL hg = LoadResource(hm, r);
        if (!hg) continue;
        DWORD sz = SizeofResource(hm, r);
        const void* data = LockResource(hg);
        if (!data || sz == 0) continue;
        std::ofstream out(dst.c_str(), std::ios::binary);
        if (!out) continue;
        out.write((const char*)data, sz);
        out.close();
    }
}
static void SetStatus(const wchar_t* t);
static void SetTransferStatusLine(const std::wstring& key, const std::wstring& line);
static void RemoveTransferStatusLine(const std::wstring& key);
static void RefreshChatPeersUI();
static void AppendChatLine(const std::wstring& from, const std::wstring& msg);
static void AppendChatLineStyled(const std::wstring& from, const std::wstring& msg, const CHARFORMAT2W* fmt);
static void SendChatToSelectedPeer();
static void ChatListenThread();
static std::wstring PromptText(HWND owner, const wchar_t* title, const wchar_t* label, const std::wstring& current);
static std::wstring SelfAvatarPath();
static std::wstring ResolveSelfAvatarPath();
static std::wstring PeerAvatarCachePath(const std::wstring& host);
static void EnsurePeerAvatarCached(const std::wstring& host, const std::wstring& ip, int port);
static std::string HttpGetRaw(const std::string& host, int port, const std::string& path, int timeoutMs);
static bool IsLocalIp(const std::wstring& ip);
static bool ShouldAcceptChatPacket(const std::wstring& packetKey);
static std::wstring EffectiveMyPresence();
static std::wstring CurrentPlayingGame();
static std::wstring NormalizeDisplayGameName(const std::wstring& in);
static void MarkUserInteraction();
static void SyncPeersScrollbar();
static LRESULT CALLBACK ChatPeersScrollProc(HWND h, UINT m, WPARAM w, LPARAM l);
static void SyncChatScrollbar();
static LRESULT CALLBACK ChatViewScrollProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK ChatInputCustomProc(HWND h, UINT m, WPARAM w, LPARAM l);
static std::wstring PickFolder(HWND owner, const wchar_t* title);
static void ScanLocalGames();
static bool DownloadSteamGridForGameName(const std::wstring& game);
static void DownloadSteamGridForAll();
static std::wstring EnsureRemoteAssetCache(const GameEntry& e, const wchar_t* kind);
static std::wstring PickFile(HWND owner, const wchar_t* title, const wchar_t* filter, const wchar_t* initialDir = nullptr);
static void EnsureDir(const std::wstring& p);
static bool ExistsDir(const std::wstring& p);
static bool ExistsFile(const std::wstring& p);
static std::wstring ExeDir();
static std::wstring FindLaunchExeFast(const std::wstring& dir);
static std::wstring FindLaunchExeDeep(const std::wstring& dir);
static void InvalidateBannerOnly();
static void InvalidateTopBarOnly();
static void SyncListScrollbar();
static std::wstring CurrentSearchText();
static std::shared_ptr<Gdiplus::Image> GetCachedImage(const std::wstring& path);
static bool DrawImageSmart(HDC hdc, const std::wstring& path, int x, int y, int w, int h);
static void CleanupGdiplusCaches();
static bool IsWindowsXpFamily();
static void PreloadVisibleBannerImages();
static std::wstring JsonFindFirstName(const std::wstring& json);
static void ApplyStatusBarTextAndLayout();
static void LayoutControls(HWND hwnd);
static void EnqueueRemoteGameDownload(const GameEntry& e, HWND owner);
static void EnsureBundledEmojiAssets();
static bool MatchEmojiAt(const std::wstring& text, size_t pos, UINT* outId, size_t* outLen);
static void DrawChatTextWithEmoji(HDC hdc, const std::wstring& text, int x, int y, int rightLimit);
static void ClampChatInputState();
static void SetChatInputCaretPos(HWND h);
static int ChatInputIndexFromXY(HWND h, HDC hdc, int x, int y);
static int ChatTextIndexFromX(HDC hdc, const std::wstring& s, int x);
static bool IsWordChar(wchar_t ch);
static void SetAliasForGame(const std::wstring& gameName, const std::wstring& alias);
static std::wstring GetAliasForGame(const std::wstring& gameName);
static LRESULT CALLBACK ListScrollProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK PlayBtnProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK ChatViewProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK ChatInputProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK ChatToolBtnProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK LanButtonProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK EmojiPopupProc(HWND h, UINT m, WPARAM w, LPARAM l);
static LRESULT CALLBACK SgdbPromptProc(HWND h, UINT m, WPARAM w, LPARAM l);
static void AddDroppedPathAsGameFolder(const std::wstring& droppedPath);
static int PromptSteamGridId(HWND owner, int currentId);
static void RegisterLanButtonClass(HINSTANCE hInst);
static const int TOP_BAR_H = 34;
static const int RESIZE_BORDER = 6;
static void InvalidateTitleButtons() {
    if (g_btnMin) InvalidateRect(g_btnMin, nullptr, FALSE);
    if (g_btnMax) InvalidateRect(g_btnMax, nullptr, FALSE);
    if (g_btnClose) InvalidateRect(g_btnClose, nullptr, FALSE);
    if (g_hamburger) InvalidateRect(g_hamburger, nullptr, FALSE);
}
static void DrawCogIcon(HDC hdc, const RECT& r, COLORREF color) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color c(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::SolidBrush brush(c);
    Gdiplus::Pen pen(c, 2.0f);
    float cx = (float)(r.left + r.right) * 0.5f;
    float cy = (float)(r.top + r.bottom) * 0.5f;
    float outer = 8.0f;
    float inner = 4.2f;
    float hub = 2.1f;
    for (int i = 0; i < 8; ++i) {
        float a = (float)i * 3.1415926f / 4.0f;
        float tx = cosf(a), ty = sinf(a);
        Gdiplus::RectF tooth(cx + tx * (outer - 1.5f) - 1.3f, cy + ty * (outer - 1.5f) - 1.3f, 2.6f, 2.6f);
        g.FillRectangle(&brush, tooth);
    }
    g.DrawEllipse(&pen, cx - outer + 1.0f, cy - outer + 1.0f, (outer - 1.0f) * 2.0f, (outer - 1.0f) * 2.0f);
    g.DrawEllipse(&pen, cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);
    g.FillEllipse(&brush, cx - hub, cy - hub, hub * 2.0f, hub * 2.0f);
}
static void RegisterLanButtonClass(HINSTANCE hInst) {
    static bool registered = false;
    if (registered) return;
    WNDCLASSW wc{};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = LanButtonProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kLanButtonClass;
    RegisterClassW(&wc);
    registered = true;
}
static void DrawLanButtonFace(HWND h, HDC hdc, const RECT& r, bool hot, bool pressed) {
    int id = GetDlgCtrlID(h);
    bool enabled = IsWindowEnabled(h) != FALSE;
    COLORREF bg = RGB(30, 62, 92);
    COLORREF border = RGB(72, 130, 178);
    COLORREF fg = enabled ? RGB(235, 245, 252) : RGB(155, 170, 185);
    bool drawBorder = true;
    if (id == IDC_BTN_MIN || id == IDC_BTN_MAX || id == IDC_BTN_CLOSE || id == IDC_HAMBURGER) {
        bg = RGB(15, 26, 38);
        border = RGB(15, 26, 38);
        fg = RGB(210, 228, 244);
        if (id == IDC_BTN_CLOSE && (hot || pressed)) bg = RGB(180, 45, 45);
        else if (hot || pressed) bg = RGB(36, 61, 84);
    } else if (id == IDC_PLAYGET) {
        wchar_t txt[32] = {};
        GetWindowTextW(h, txt, 31);
        if (_wcsicmp(txt, L"PLAY") == 0) bg = RGB(60, 143, 45);
        else bg = RGB(45, 111, 168);
        if (hot) bg = RGB(min(255, GetRValue(bg) + 20), min(255, GetGValue(bg) + 20), min(255, GetBValue(bg) + 20));
        if (pressed) bg = RGB(min(255, GetRValue(bg) + 15), min(255, GetGValue(bg) + 15), min(255, GetBValue(bg) + 15));
        drawBorder = false;
    } else if (id == IDC_CHAT_SEND) {
        bg = RGB(45, 111, 168);
        if (hot) bg = RGB(min(255, GetRValue(bg) + 20), min(255, GetGValue(bg) + 20), min(255, GetBValue(bg) + 20));
        if (pressed) bg = RGB(min(255, GetRValue(bg) + 18), min(255, GetGValue(bg) + 18), min(255, GetBValue(bg) + 18));
    } else if (id == IDC_CHAT_BOLD || id == IDC_CHAT_ITALIC || id == IDC_CHAT_COLOR || id == IDC_CHAT_FONT || id == IDC_CHAT_EMOJI) {
        bg = RGB(30, 62, 92);
        if (hot) bg = RGB(50, 82, 112);
        if (pressed) bg = RGB(58, 96, 130);
    } else if (id == IDC_CHAT_SELF || id == IDC_CHAT_SELF_AVATAR) {
        bg = RGB(22, 32, 45);
        if (hot) bg = RGB(28, 41, 58);
        if (pressed) bg = RGB(34, 52, 72);
        drawBorder = false;
    }

    RECT fill = r;
    HBRUSH b = CreateSolidBrush(bg);
    FillRect(hdc, &fill, b);
    DeleteObject(b);

    if (drawBorder) {
        HPEN p = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldP = SelectObject(hdc, p);
        HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(p);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    if (id == IDC_HAMBURGER) {
        DrawCogIcon(hdc, r, fg);
        return;
    }
    if (id == IDC_BTN_MIN) {
        RECT t = r;
        t.top = (r.top + r.bottom) / 2;
        t.bottom = t.top + 1;
        t.left += 14; t.right -= 14;
        FillRect(hdc, &t, (HBRUSH)GetStockObject(WHITE_BRUSH));
        return;
    }
    if (id == IDC_BTN_MAX) {
        RECT t = r;
        int x1 = (r.left + r.right) / 2 - 7;
        int y1 = (r.top + r.bottom) / 2 - 7;
        Rectangle(hdc, x1, y1, x1 + 14, y1 + 14);
        return;
    }
    if (id == IDC_BTN_CLOSE) {
        HPEN xp = CreatePen(PS_SOLID, 2, fg);
        HGDIOBJ op = SelectObject(hdc, xp);
        MoveToEx(hdc, r.left + 14, r.top + 10, nullptr);
        LineTo(hdc, r.right - 14, r.bottom - 10);
        MoveToEx(hdc, r.left + 14, r.bottom - 10, nullptr);
        LineTo(hdc, r.right - 14, r.top + 10);
        SelectObject(hdc, op);
        DeleteObject(xp);
        return;
    }
    if (id == IDC_CHAT_EMOJI) {
        std::wstring imgPath = ResolveEmojiImagePath(L"grinning_face.png");
        if (!imgPath.empty() && ExistsFile(imgPath)) {
            Gdiplus::Graphics g(hdc);
            Gdiplus::Image img(imgPath.c_str());
            if (img.GetLastStatus() == Gdiplus::Ok) {
                int s = min((r.right - r.left) - 8, (r.bottom - r.top) - 8);
                int dx = r.left + ((r.right - r.left) - s) / 2;
                int dy = r.top + ((r.bottom - r.top) - s) / 2;
                g.DrawImage(&img, dx, dy, s, s);
                return;
            }
        }
        if (g_fontEmoji) SelectObject(hdc, g_fontEmoji);
        DrawTextW(hdc, L":)", -1, (RECT*)&r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        return;
    }
    if (id == IDC_CHAT_SELF || id == IDC_CHAT_SELF_AVATAR) {
        RECT av = r;
        if (id == IDC_CHAT_SELF) {
            av.left += 4;
            av.top += 2;
            av.right = av.left + 42;
            av.bottom = av.top + 42;
        } else {
            av.left += 1;
            av.top += 1;
            av.right -= 1;
            av.bottom -= 1;
        }
        std::wstring avp = ResolveSelfAvatarPath();
        if (!avp.empty() && ExistsFile(avp)) {
            DrawImageSmart(hdc, avp, av.left, av.top, av.right - av.left, av.bottom - av.top);
        } else {
            HBRUSH avb = CreateSolidBrush(RGB(88, 174, 240));
            FillRect(hdc, &av, avb);
            DeleteObject(avb);
        }
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(45, 78, 108));
        HGDIOBJ oldP2 = SelectObject(hdc, pen);
        HGDIOBJ oldB2 = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, av.left, av.top, av.right, av.bottom);
        SelectObject(hdc, oldB2);
        SelectObject(hdc, oldP2);
        DeleteObject(pen);
        if (id == IDC_CHAT_SELF) {
            RECT nr{r.left + 52, r.top + 4, r.right - 6, r.top + 24};
            RECT sr{r.left + 52, r.top + 22, r.right - 6, r.bottom - 2};
            std::wstring nowPlaying;
            {
                std::lock_guard<std::mutex> lk(g_playingMutex);
                nowPlaying = g_nowPlayingGame;
            }
            bool isPlaying = !nowPlaying.empty();
            SetTextColor(hdc, isPlaying ? RGB(138, 255, 132) : RGB(126, 194, 255));
            DrawTextW(hdc, g_myNick.empty() ? L"You" : g_myNick.c_str(), -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            COLORREF sc = RGB(110, 190, 255);
            std::wstring ep = EffectiveMyPresence();
            if (isPlaying) {
                ep = L"Playing " + nowPlaying;
                sc = RGB(138, 255, 132);
            } else if (_wcsicmp(ep.c_str(), L"Busy") == 0) sc = RGB(220, 75, 75);
            else if (_wcsicmp(ep.c_str(), L"Idle") == 0) sc = RGB(255, 175, 70);
            else if (_wcsicmp(ep.c_str(), L"Invisible") == 0) sc = RGB(120, 130, 140);
            SetTextColor(hdc, sc);
            DrawTextW(hdc, ep.c_str(), -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        return;
    }
    wchar_t txt[128] = {};
    GetWindowTextW(h, txt, 127);
    if (txt[0] == 0) {
        switch (id) {
            case IDC_CHAT_SEND: wcscpy_s(txt, _countof(txt), L"SEND"); break;
            case IDC_CHAT_BOLD: wcscpy_s(txt, _countof(txt), L"B"); break;
            case IDC_CHAT_ITALIC: wcscpy_s(txt, _countof(txt), L"I"); break;
            case IDC_CHAT_COLOR: wcscpy_s(txt, _countof(txt), L"Color"); break;
            case IDC_CHAT_FONT: wcscpy_s(txt, _countof(txt), L"Font"); break;
            case IDC_CHAT_EMOJI: wcscpy_s(txt, _countof(txt), L":)"); break;
            case IDC_PLAYGET: {
                wchar_t playTxt[16]{};
                GetWindowTextW(h, playTxt, 15);
                if (playTxt[0] == 0) wcscpy_s(txt, _countof(txt), L"PLAY");
                else wcscpy_s(txt, _countof(txt), playTxt);
                break;
            }
            default: break;
        }
    }
    HFONT oldF = nullptr;
    if (id == IDC_CHAT_SEND || id == IDC_PLAYGET || id == IDC_HAMBURGER || id == IDC_BTN_MIN || id == IDC_BTN_MAX || id == IDC_BTN_CLOSE || id == IDC_CHAT_SELF) {
        oldF = (HFONT)SelectObject(hdc, g_fontBold ? g_fontBold : g_font);
    } else if (id == IDC_CHAT_BOLD || id == IDC_CHAT_ITALIC || id == IDC_CHAT_COLOR || id == IDC_CHAT_FONT) {
        oldF = (HFONT)SelectObject(hdc, g_font ? g_font : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
    }
    DrawTextW(hdc, txt, -1, const_cast<RECT*>(&r), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (oldF) SelectObject(hdc, oldF);
}
static LRESULT CALLBACK LanButtonProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    LanButtonState* st = (LanButtonState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_NCCREATE: {
            LanButtonState* ns = new LanButtonState();
            ns->font = g_font;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)ns);
            return TRUE;
        }
        case WM_NCDESTROY:
            if (st) {
                delete st;
                SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            }
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(h, nullptr, FALSE);
            break;
        case WM_ENABLE:
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_SETTEXT:
            InvalidateRect(h, nullptr, FALSE);
            return DefWindowProcW(h, m, w, l);
        case WM_SETFONT:
            if (st) st->font = (HFONT)w;
            InvalidateRect(h, nullptr, FALSE);
            if (l) UpdateWindow(h);
            return 0;
        case WM_GETFONT:
            return st && st->font ? (LRESULT)st->font : (LRESULT)g_font;
        case WM_MOUSEMOVE: {
            if (st && !st->hot) {
                st->hot = true;
                InvalidateRect(h, nullptr, FALSE);
            }
            if (st && !st->tracking) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = h;
                TrackMouseEvent(&tme);
                st->tracking = true;
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            if (st) {
                st->tracking = false;
                if (st->hot) {
                    st->hot = false;
                    InvalidateRect(h, nullptr, FALSE);
                }
            }
            return 0;
        case WM_LBUTTONDOWN:
            SetFocus(h);
            if (st) {
                st->pressed = true;
                SetCapture(h);
                if (!st->hot) st->hot = true;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP: {
            bool click = false;
            if (st && st->pressed) {
                st->pressed = false;
                ReleaseCapture();
                POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
                RECT rc{};
                GetClientRect(h, &rc);
                click = PtInRect(&rc, pt) != 0;
                InvalidateRect(h, nullptr, FALSE);
            }
            if (click) {
                HWND parent = GetParent(h);
                if (parent) SendMessageW(parent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
            }
            return 0;
        }
        case WM_CAPTURECHANGED:
            if (st && st->pressed) {
                st->pressed = false;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_KEYDOWN:
            if (w == VK_SPACE || w == VK_RETURN) {
                HWND parent = GetParent(h);
                if (parent) SendMessageW(parent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
                return 0;
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(h, &ps);
            RECT rc{}; GetClientRect(h, &rc);
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, max(1, rc.right - rc.left), max(1, rc.bottom - rc.top));
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
            HBRUSH bg = CreateSolidBrush(RGB(15, 26, 38));
            FillRect(mem, &rc, bg);
            DeleteObject(bg);
            if (st && st->font) SelectObject(mem, st->font);
            DrawLanButtonFace(h, mem, rc, st && st->hot, st && st->pressed);
            BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK TitleBtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            int id = GetDlgCtrlID(h);
            if (g_hotTitleBtn != id) {
                int prev = g_hotTitleBtn;
                g_hotTitleBtn = id;
                InvalidateRect(h, nullptr, FALSE);
                if (prev == IDC_BTN_MIN && g_btnMin) InvalidateRect(g_btnMin, nullptr, FALSE);
                else if (prev == IDC_BTN_MAX && g_btnMax) InvalidateRect(g_btnMax, nullptr, FALSE);
                else if (prev == IDC_BTN_CLOSE && g_btnClose) InvalidateRect(g_btnClose, nullptr, FALSE);
                else if (prev == IDC_HAMBURGER && g_hamburger) InvalidateRect(g_hamburger, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = h;
            TrackMouseEvent(&tme);
            break;
        }
        case WM_MOUSELEAVE:
            if (g_hotTitleBtn == GetDlgCtrlID(h)) {
                g_hotTitleBtn = 0;
                InvalidateRect(h, nullptr, FALSE);
            }
            break;
    }
    return CallWindowProcW(g_oldTitleBtnProc, h, m, w, l);
}
static HMENU BuildStyledContextMenu() {
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* label) {
        MENUITEMINFOW mi{};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA;
        mi.fType = MFT_OWNERDRAW;
        mi.wID = id;
        mi.dwItemData = (ULONG_PTR)label;
        InsertMenuItemW(m, (UINT)-1, TRUE, &mi);
    };
    add(IDM_CTX_SET_EXE, L"Set Launch Executable...");
    add(IDM_CTX_OPEN_FOLDER, L"Open Local Folder");
    add(IDM_CTX_SET_ICON, L"Set Icon...");
    add(IDM_CTX_SET_BANNER, L"Set Banner...");
    add(IDM_CTX_SET_SGDB_ID, L"Set SteamGridDB ID...");
    add(IDM_CTX_REMOVE_ENTRY, L"Remove Entry");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    add(IDM_CTX_DOWNLOAD_ASSETS, L"Download Assets from SteamGridDB");
    MENUINFO mi{};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_BACKGROUND;
    mi.dwStyle = MNS_NOCHECK;
    mi.hbrBack = g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_MENU + 1);
    SetMenuInfo(m, &mi);
    return m;
}
static HMENU BuildStyledSettingsMenu() {
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* label) {
        MENUITEMINFOW mi{};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA;
        mi.fType = MFT_OWNERDRAW;
        mi.wID = id;
        mi.dwItemData = (ULONG_PTR)label;
        InsertMenuItemW(m, (UINT)-1, TRUE, &mi);
    };
    add(IDM_FILE_ADD_GAME, L"Add game(s)");
    add(IDM_FILE_REFRESH, L"Refresh");
    add(IDM_FILE_SGDB_ALL, L"Download Assets (SteamGridDB) for All");
    add(IDM_FILE_REMOVE_ALL, L"Remove all entries");
    add(IDM_FILE_TOGGLE_CACHE, g_useImageCache ? L"[x] Use image cache" : L"[ ] Use image cache");
    add(IDM_FILE_TOGGLE_LAN_SHARE, g_lanShareEnabled.load() ? L"[x] Share list on LAN" : L"[ ] Share list on LAN");
    add(IDM_FILE_EXIT, L"Exit");
    MENUINFO mi{};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_BACKGROUND;
    mi.dwStyle = MNS_NOCHECK;
    mi.hbrBack = g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_MENU + 1);
    SetMenuInfo(m, &mi);
    return m;
}
static HMENU BuildStyledSelfMenu() {
    HMENU m = CreatePopupMenu();
    auto add = [&](UINT id, const wchar_t* label) {
        MENUITEMINFOW mi{};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA;
        mi.fType = MFT_OWNERDRAW;
        mi.wID = id;
        mi.dwItemData = (ULONG_PTR)label;
        InsertMenuItemW(m, (UINT)-1, TRUE, &mi);
    };
    add(IDM_SELF_SET_NICK, L"Set Nickname...");
    add(IDM_SELF_SET_AVATAR, L"Set Avatar...");
    add(IDM_SELF_STATUS_ONLINE, L"Online");
    add(IDM_SELF_STATUS_BUSY, L"Busy");
    add(IDM_SELF_STATUS_INVISIBLE, L"Invisible");
    MENUINFO mi{};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_BACKGROUND;
    mi.dwStyle = MNS_NOCHECK;
    mi.hbrBack = g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_MENU + 1);
    SetMenuInfo(m, &mi);
    return m;
}
static void ShowEmojiGridPopup(HWND owner, const RECT& anchor) {
    if (g_emojiPopup && IsWindow(g_emojiPopup)) {
        DestroyWindow(g_emojiPopup);
        g_emojiPopup = nullptr;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = EmojiPopupProc;
    wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    wc.lpszClassName = kEmojiPopupClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    const int cols = 4;
    const int cellW = 52;
    const int cellH = 44;
    const int pad = 8;
    int rows = ((int)(sizeof(kEmojiItems) / sizeof(kEmojiItems[0])) + cols - 1) / cols;
    int w = pad * 2 + cols * cellW;
    int h = pad * 2 + rows * cellH;
    int x = anchor.left;
    int y = anchor.top - h - 4;
    g_emojiPopup = CreateWindowExW(WS_EX_TOOLWINDOW, kEmojiPopupClass, L"",
        WS_POPUP | WS_VISIBLE, x, y, w, h, owner, nullptr, wc.hInstance, nullptr);
    if (!g_emojiPopup) return;
    for (int i = 0; i < (int)(sizeof(kEmojiItems) / sizeof(kEmojiItems[0])); ++i) {
        int r = i / cols, c = i % cols;
        int bx = pad + c * cellW;
        int by = pad + r * cellH;
        HWND b = CreateWindowW(L"BUTTON", EmojiAsciiForId(kEmojiItems[i].id), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            bx, by, cellW - 4, cellH - 4, g_emojiPopup, (HMENU)kEmojiItems[i].id, nullptr, nullptr);
        if (g_fontEmoji) SendMessageW(b, WM_SETFONT, (WPARAM)g_fontEmoji, TRUE);
    }
    SetForegroundWindow(g_emojiPopup);
}
static LRESULT CALLBACK ListScrollProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_LBUTTONDOWN: {
            SetFocus(g_list);
            SetCapture(h);
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            if (PtInRect(&g_listThumbRect, pt)) {
                g_dragListThumb = true;
                g_dragStartY = pt.y;
                g_dragStartTop = (int)SendMessageW(g_list, LB_GETTOPINDEX, 0, 0);
                g_lastDragTop = g_dragStartTop;
            } else {
                int count = (int)SendMessageW(g_list, LB_GETCOUNT, 0, 0);
                int itemH = (int)SendMessageW(g_list, LB_GETITEMHEIGHT, 0, 0);
                RECT lr{}; GetClientRect(g_list, &lr);
                int page = max(1, (lr.bottom - lr.top) / max(1, itemH));
                int top = (int)SendMessageW(g_list, LB_GETTOPINDEX, 0, 0);
                if (pt.y < g_listThumbRect.top) top -= page;
                else top += page;
                if (top < 0) top = 0;
                if (top > count - page) top = max(0, count - page);
                SendMessageW(g_list, LB_SETTOPINDEX, top, 0);
                SyncListScrollbar();
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g_dragListThumb) {
                POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                RECT sr{}; GetClientRect(h, &sr);
                int trackH = (sr.bottom - sr.top) - 4;
                int thumbH = g_listThumbRect.bottom - g_listThumbRect.top;
                int travel = max(1, trackH - thumbH);
                int dy = pt.y - g_dragStartY;
                int count = (int)SendMessageW(g_list, LB_GETCOUNT, 0, 0);
                int itemH = (int)SendMessageW(g_list, LB_GETITEMHEIGHT, 0, 0);
                RECT lr{}; GetClientRect(g_list, &lr);
                int page = max(1, (lr.bottom - lr.top) / max(1, itemH));
                int maxTop = max(1, count - page);
                int top = g_dragStartTop + (dy * maxTop) / travel;
                if (top < 0) top = 0;
                if (top > maxTop) top = maxTop;
                if (top != g_lastDragTop) {
                    SendMessageW(g_list, LB_SETTOPINDEX, top, 0);
                    g_lastDragTop = top;
                    SyncListScrollbar();
                }
            }
            return 0;
        case WM_LBUTTONUP:
            g_dragListThumb = false;
            g_lastDragTop = -1;
            ReleaseCapture();
            return 0;
        case WM_MOUSEWHEEL:
            SendMessageW(g_list, WM_MOUSEWHEEL, w, l);
            SyncListScrollbar();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC hdc = BeginPaint(h, &ps);
            RECT rc{}; GetClientRect(h, &rc);
            HBRUSH tr = CreateSolidBrush(RGB(18,30,43));
            FillRect(hdc, &rc, tr);
            DeleteObject(tr);
            HBRUSH th = CreateSolidBrush(RGB(53,122,186));
            FillRect(hdc, &g_listThumbRect, th);
            DeleteObject(th);
            EndPaint(h, &ps);
            return 0;
        }
    }
    return CallWindowProcW(g_oldListScrollProc, h, m, w, l);
}

static LRESULT CALLBACK ListProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_MOUSEWHEEL) {
        int delta = GET_WHEEL_DELTA_WPARAM(w);
        int top = (int)SendMessageW(h, LB_GETTOPINDEX, 0, 0);
        int itemH = (int)SendMessageW(h, LB_GETITEMHEIGHT, 0, 0);
        RECT cr{}; GetClientRect(h, &cr);
        int page = max(1, (cr.bottom - cr.top) / max(1, itemH));
        int count = (int)SendMessageW(h, LB_GETCOUNT, 0, 0);
        int maxTop = max(0, count - page);
        int step = max(1, page / 4);
        top += (delta > 0) ? -step : step;
        if (top < 0) top = 0;
        if (top > maxTop) top = maxTop;
        SendMessageW(h, LB_SETTOPINDEX, top, 0);
        SyncListScrollbar();
        return 0;
    }
    LRESULT r = CallWindowProcW(g_oldListProc, h, m, w, l);
    if (m == WM_MOUSEWHEEL || m == WM_KEYDOWN || m == WM_VSCROLL || m == WM_LBUTTONUP) {
        SyncListScrollbar();
    }
    return r;
}
static LRESULT CALLBACK PlayBtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            if (!g_hotPlayBtn) {
                g_hotPlayBtn = true;
                InvalidateRect(h, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = h;
            TrackMouseEvent(&tme);
            break;
        }
        case WM_MOUSELEAVE:
            g_hotPlayBtn = false;
            InvalidateRect(h, nullptr, FALSE);
            break;
    }
    return CallWindowProcW(g_oldPlayBtnProc, h, m, w, l);
}

static LRESULT CALLBACK ChatViewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) && (w == 'C')) {
        std::wstring clip;
        if (g_chatTextSelStartItem >= 0 && g_chatTextSelEndItem >= 0 && !g_chatItems.empty()) {
            int i0 = g_chatTextSelStartItem, i1 = g_chatTextSelEndItem;
            int c0 = g_chatTextSelStartChar, c1 = g_chatTextSelEndChar;
            if (i0 > i1 || (i0 == i1 && c0 > c1)) { std::swap(i0, i1); std::swap(c0, c1); }
            for (int i = i0; i <= i1; ++i) {
                if (i < 0 || i >= (int)g_chatItems.size()) continue;
                const auto& it = g_chatItems[(size_t)i];
                int a = 0, b = (int)it.msg.size();
                if (i == i0) a = max(0, min(c0, (int)it.msg.size()));
                if (i == i1) b = max(0, min(c1, (int)it.msg.size()));
                if (b < a) std::swap(a, b);
                if (i > i0) clip += L"\r\n";
                clip += it.msg.substr((size_t)a, (size_t)(b - a));
            }
        } else {
            int cur = (int)SendMessageW(h, LB_GETCURSEL, 0, 0);
            if (cur >= 0 && cur < (int)g_chatItems.size()) clip = L"[" + g_chatItems[cur].from + L"] " + g_chatItems[cur].msg;
        }
        if (!clip.empty() && OpenClipboard(h)) {
            EmptyClipboard();
            size_t bytes = (clip.size() + 1) * sizeof(wchar_t);
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (hg) {
                void* p = GlobalLock(hg);
                if (p) { memcpy(p, clip.c_str(), bytes); GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); }
            }
            CloseClipboard();
        }
        return 0;
    }
    if (m == WM_MOUSEWHEEL) {
        int delta = GET_WHEEL_DELTA_WPARAM(w);
        int top = (int)SendMessageW(h, LB_GETTOPINDEX, 0, 0);
        int itemH = (int)SendMessageW(h, LB_GETITEMHEIGHT, 0, 0);
        RECT cr{}; GetClientRect(h, &cr);
        int page = max(1, (cr.bottom - cr.top) / max(1, itemH));
        int count = (int)SendMessageW(h, LB_GETCOUNT, 0, 0);
        int maxTop = max(0, count - page);
        int step = max(1, page / 4);
        top += (delta > 0) ? -step : step;
        if (top < 0) top = 0;
        if (top > maxTop) top = maxTop;
        SendMessageW(h, LB_SETTOPINDEX, top, 0);
        SyncChatScrollbar();
        return 0;
    }
    if (m == WM_LBUTTONDOWN) {
        SetFocus(h);
        int idx = (int)SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        if (HIWORD(idx) == 0) {
            idx = LOWORD(idx);
            int cnt = (int)SendMessageW(h, LB_GETCOUNT, 0, 0);
            if (idx >= 0 && idx < cnt) {
                const auto& it = g_chatItems[(size_t)idx];
                RECT ir{}; SendMessageW(h, LB_GETITEMRECT, idx, (LPARAM)&ir);
                HDC hdc = GetDC(h);
                HFONT oldF = (HFONT)SelectObject(hdc, g_font);
                std::wstring prefix = L"[" + it.from + L"] ";
                SIZE preSz{}; GetTextExtentPoint32W(hdc, prefix.c_str(), (int)prefix.size(), &preSz);
                int localX = GET_X_LPARAM(l) - (ir.left + 6 + preSz.cx);
                int charIdx = ChatTextIndexFromX(hdc, it.msg, max(0, localX));
                SelectObject(hdc, oldF);
                ReleaseDC(h, hdc);
                g_chatTextSelStartItem = idx;
                g_chatTextSelStartChar = charIdx;
                g_chatTextSelEndItem = idx;
                g_chatTextSelEndChar = charIdx;
                g_chatTextSelecting = true;
                SetCapture(h);
                InvalidateRect(h, nullptr, FALSE);
            }
        }
        SyncChatScrollbar();
        return 0;
    }
    if (m == WM_MOUSEMOVE && g_chatTextSelecting) {
        int idx = (int)SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        if (HIWORD(idx) == 0 && LOWORD(idx) >= 0 && LOWORD(idx) < (int)g_chatItems.size()) {
            idx = LOWORD(idx);
            const auto& it = g_chatItems[(size_t)idx];
            RECT ir{}; SendMessageW(h, LB_GETITEMRECT, idx, (LPARAM)&ir);
            HDC hdc = GetDC(h);
            HFONT oldF = (HFONT)SelectObject(hdc, g_font);
            std::wstring prefix = L"[" + it.from + L"] ";
            SIZE preSz{}; GetTextExtentPoint32W(hdc, prefix.c_str(), (int)prefix.size(), &preSz);
            int localX = GET_X_LPARAM(l) - (ir.left + 6 + preSz.cx);
            int charIdx = ChatTextIndexFromX(hdc, it.msg, max(0, localX));
            SelectObject(hdc, oldF);
            ReleaseDC(h, hdc);
            g_chatTextSelEndItem = idx;
            g_chatTextSelEndChar = charIdx;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    }
    if (m == WM_LBUTTONUP && g_chatTextSelecting) {
        g_chatTextSelecting = false;
        ReleaseCapture();
        SyncChatScrollbar();
        return 0;
    }
    if (m == WM_LBUTTONDBLCLK) {
        SetFocus(h);
        int idx = (int)SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        if (HIWORD(idx) == 0) {
            idx = LOWORD(idx);
            int cnt = (int)SendMessageW(h, LB_GETCOUNT, 0, 0);
            if (idx >= 0 && idx < cnt && idx < (int)g_chatItems.size()) {
                const auto& it = g_chatItems[(size_t)idx];
                RECT ir{}; SendMessageW(h, LB_GETITEMRECT, idx, (LPARAM)&ir);
                HDC hdc = GetDC(h);
                HFONT oldF = (HFONT)SelectObject(hdc, g_font);
                std::wstring prefix = L"[" + it.from + L"] ";
                SIZE preSz{}; GetTextExtentPoint32W(hdc, prefix.c_str(), (int)prefix.size(), &preSz);
                int localX = GET_X_LPARAM(l) - (ir.left + 6 + preSz.cx);
                int charIdx = ChatTextIndexFromX(hdc, it.msg, max(0, localX));
                SelectObject(hdc, oldF);
                ReleaseDC(h, hdc);

                int a = max(0, min(charIdx, (int)it.msg.size()));
                int b = a;
                while (a > 0 && IsWordChar(it.msg[(size_t)a - 1])) a--;
                while (b < (int)it.msg.size() && IsWordChar(it.msg[(size_t)b])) b++;

                bool sameWordSecondDbl =
                    (g_chatLastDblItem == idx) &&
                    (a == g_chatLastDblWordStart) &&
                    (b == g_chatLastDblWordEnd);

                if (sameWordSecondDbl) {
                    g_chatTextSelStartItem = idx;
                    g_chatTextSelStartChar = 0;
                    g_chatTextSelEndItem = idx;
                    g_chatTextSelEndChar = (int)it.msg.size();
                    g_chatLastDblItem = -1;
                    g_chatLastDblWordStart = -1;
                    g_chatLastDblWordEnd = -1;
                } else {
                    g_chatTextSelStartItem = idx;
                    g_chatTextSelStartChar = a;
                    g_chatTextSelEndItem = idx;
                    g_chatTextSelEndChar = b;
                    g_chatLastDblItem = idx;
                    g_chatLastDblWordStart = a;
                    g_chatLastDblWordEnd = b;
                }
                InvalidateRect(h, nullptr, FALSE);
            }
        }
        return 0;
    }
    if (m == WM_VSCROLL || m == WM_KEYDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDOWN) {
        LRESULT r = CallWindowProcW(g_oldChatViewProc, h, m, w, l);
        SyncChatScrollbar();
        return r;
    }
    return CallWindowProcW(g_oldChatViewProc, h, m, w, l);
}

static LRESULT CALLBACK ChatInputProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!shiftDown) {
            HWND parent = GetParent(h);
            if (parent) SendMessageW(parent, WM_COMMAND, MAKEWPARAM(IDC_CHAT_SEND, BN_CLICKED), (LPARAM)g_chatSend);
            return 0;
        }
    }
    return CallWindowProcW(g_oldChatInputProc, h, m, w, l);
}

static LRESULT CALLBACK ChatInputCustomProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SETFOCUS:
            CreateCaret(h, nullptr, 2, 18);
            ShowCaret(h);
            SetChatInputCaretPos(h);
            return 0;
        case WM_KILLFOCUS:
            HideCaret(h);
            DestroyCaret();
            return 0;
        case WM_LBUTTONDOWN:
            SetFocus(h);
            {
                HDC hdc = GetDC(h);
                HFONT old = (HFONT)SelectObject(hdc, g_font);
                int idx = ChatInputIndexFromXY(h, hdc, GET_X_LPARAM(l), GET_Y_LPARAM(l));
                SelectObject(hdc, old);
                ReleaseDC(h, hdc);
                g_chatInputCaret = idx;
                g_chatSelStart = g_chatSelEnd = idx;
                g_chatSelAnchor = idx;
                g_chatSelecting = true;
                SetCapture(h);
                g_lastDblClickWordStart = -1;
                g_lastDblClickWordEnd = -1;
            }
            SetChatInputCaretPos(h);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDBLCLK:
            SetFocus(h);
            {
                HDC hdc = GetDC(h);
                HFONT old = (HFONT)SelectObject(hdc, g_font);
                int idx = ChatInputIndexFromXY(h, hdc, GET_X_LPARAM(l), GET_Y_LPARAM(l));
                SelectObject(hdc, old);
                ReleaseDC(h, hdc);
                idx = max(0, min(idx, (int)g_chatInputText.size()));
                if (g_lastDblClickWordStart >= 0 && g_lastDblClickWordEnd > g_lastDblClickWordStart &&
                    idx >= g_lastDblClickWordStart && idx <= g_lastDblClickWordEnd) {
                    // Second double-click in same word -> select full line.
                    g_chatSelStart = 0;
                    g_chatSelEnd = (int)g_chatInputText.size();
                    g_chatInputCaret = g_chatSelEnd;
                    g_lastDblClickWordStart = -1;
                    g_lastDblClickWordEnd = -1;
                } else {
                    int a = idx;
                    int b = idx;
                    while (a > 0 && IsWordChar(g_chatInputText[(size_t)a - 1])) a--;
                    while (b < (int)g_chatInputText.size() && IsWordChar(g_chatInputText[(size_t)b])) b++;
                    g_chatSelStart = a;
                    g_chatSelEnd = b;
                    g_chatInputCaret = b;
                    g_lastDblClickWordStart = a;
                    g_lastDblClickWordEnd = b;
                }
            }
            SetChatInputCaretPos(h);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            if (g_chatSelecting) {
                HDC hdc = GetDC(h);
                HFONT old = (HFONT)SelectObject(hdc, g_font);
                int idx = ChatInputIndexFromXY(h, hdc, GET_X_LPARAM(l), GET_Y_LPARAM(l));
                SelectObject(hdc, old);
                ReleaseDC(h, hdc);
                idx = max(0, min(idx, (int)g_chatInputText.size()));
                g_chatInputCaret = idx;
                g_chatSelStart = min(g_chatSelAnchor, idx);
                g_chatSelEnd = max(g_chatSelAnchor, idx);
                SetChatInputCaretPos(h);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            return 0;
        case WM_LBUTTONUP:
            if (g_chatSelecting) {
                g_chatSelecting = false;
                ReleaseCapture();
            }
            return 0;
        case WM_GETTEXTLENGTH:
            return (LRESULT)g_chatInputText.size();
        case WM_GETTEXT: {
            wchar_t* dst = (wchar_t*)l;
            int cap = (int)w;
            if (!dst || cap <= 0) return 0;
            int n = (int)g_chatInputText.size();
            int copyN = min(n, cap - 1);
            if (copyN > 0) wmemcpy(dst, g_chatInputText.c_str(), copyN);
            dst[copyN] = 0;
            return copyN;
        }
        case WM_SETTEXT: {
            const wchar_t* s = (const wchar_t*)l;
            g_chatInputText = s ? s : L"";
            g_chatInputCaret = (int)g_chatInputText.size();
            g_chatSelStart = g_chatSelEnd = g_chatInputCaret;
            InvalidateRect(h, nullptr, FALSE);
            SetChatInputCaretPos(h);
            return TRUE;
        }
        case EM_SETSEL: {
            int a = (int)w;
            int b = (int)l;
            if (a == -1 && b == -1) g_chatInputCaret = (int)g_chatInputText.size();
            else g_chatInputCaret = max(0, b);
            ClampChatInputState();
            g_chatSelStart = g_chatSelEnd = g_chatInputCaret;
            SetChatInputCaretPos(h);
            return 0;
        }
        case EM_REPLACESEL: {
            const wchar_t* s = (const wchar_t*)l;
            std::wstring ins = s ? s : L"";
            ClampChatInputState();
            int selLen = max(0, g_chatSelEnd - g_chatSelStart);
            int curLen = (int)g_chatInputText.size();
            int remaining = kChatInputMaxChars - (curLen - selLen);
            if (remaining <= 0) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            if ((int)ins.size() > remaining) {
                ins.resize((size_t)remaining);
                MessageBeep(MB_ICONWARNING);
            }
            if (g_chatSelEnd > g_chatSelStart) {
                g_chatInputText.replace((size_t)g_chatSelStart, (size_t)(g_chatSelEnd - g_chatSelStart), ins);
                g_chatInputCaret = g_chatSelStart + (int)ins.size();
            } else {
                g_chatInputText.insert((size_t)g_chatInputCaret, ins);
                g_chatInputCaret += (int)ins.size();
            }
            g_chatSelStart = g_chatSelEnd = g_chatInputCaret;
            InvalidateRect(h, nullptr, FALSE);
            SetChatInputCaretPos(h);
            return 0;
        }
        case WM_PASTE: {
            if (!OpenClipboard(h)) return 0;
            HANDLE hd = GetClipboardData(CF_UNICODETEXT);
            if (hd) {
                const wchar_t* clip = (const wchar_t*)GlobalLock(hd);
                if (clip) {
                    SendMessageW(h, EM_REPLACESEL, FALSE, (LPARAM)clip);
                    GlobalUnlock(hd);
                }
            }
            CloseClipboard();
            return 0;
        }
        case WM_KEYDOWN:
            if (w == VK_RETURN) {
                HWND parent = GetParent(h);
                if (parent) SendMessageW(parent, WM_COMMAND, MAKEWPARAM(IDC_CHAT_SEND, BN_CLICKED), (LPARAM)g_chatSend);
                return 0;
            }
            if (w == VK_BACK) {
                if (g_chatSelEnd > g_chatSelStart) {
                    g_chatInputText.erase((size_t)g_chatSelStart, (size_t)(g_chatSelEnd - g_chatSelStart));
                    g_chatInputCaret = g_chatSelStart;
                    g_chatSelStart = g_chatSelEnd = g_chatInputCaret;
                    InvalidateRect(h, nullptr, FALSE);
                    SetChatInputCaretPos(h);
                } else if (!g_chatInputText.empty() && g_chatInputCaret > 0) {
                    g_chatInputText.erase((size_t)g_chatInputCaret - 1, 1);
                    g_chatInputCaret--;
                    g_chatSelStart = g_chatSelEnd = g_chatInputCaret;
                    InvalidateRect(h, nullptr, FALSE);
                    SetChatInputCaretPos(h);
                }
                return 0;
            }
            if (w == VK_DELETE) {
                if (g_chatSelEnd > g_chatSelStart) {
                    g_chatInputText.erase((size_t)g_chatSelStart, (size_t)(g_chatSelEnd - g_chatSelStart));
                    g_chatInputCaret = g_chatSelStart;
                } else if (g_chatInputCaret < (int)g_chatInputText.size()) {
                    g_chatInputText.erase((size_t)g_chatInputCaret, 1);
                }
                g_chatSelStart = g_chatSelEnd = g_chatInputCaret;
                InvalidateRect(h, nullptr, FALSE);
                SetChatInputCaretPos(h);
                return 0;
            }
            if (w == VK_LEFT) { g_chatInputCaret = max(0, g_chatInputCaret - 1); g_chatSelStart = g_chatSelEnd = g_chatInputCaret; SetChatInputCaretPos(h); return 0; }
            if (w == VK_RIGHT) { g_chatInputCaret = min((int)g_chatInputText.size(), g_chatInputCaret + 1); g_chatSelStart = g_chatSelEnd = g_chatInputCaret; SetChatInputCaretPos(h); return 0; }
            if (w == VK_HOME) { g_chatInputCaret = 0; g_chatSelStart = g_chatSelEnd = 0; SetChatInputCaretPos(h); return 0; }
            if (w == VK_END) { g_chatInputCaret = (int)g_chatInputText.size(); g_chatSelStart = g_chatSelEnd = g_chatInputCaret; SetChatInputCaretPos(h); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (w == 'V')) { SendMessageW(h, WM_PASTE, 0, 0); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (w == 'A')) { g_chatSelStart = 0; g_chatSelEnd = (int)g_chatInputText.size(); g_chatInputCaret = g_chatSelEnd; InvalidateRect(h, nullptr, FALSE); SetChatInputCaretPos(h); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && (w == 'C')) {
                if (g_chatSelEnd > g_chatSelStart && OpenClipboard(h)) {
                    EmptyClipboard();
                    std::wstring sub = g_chatInputText.substr((size_t)g_chatSelStart, (size_t)(g_chatSelEnd - g_chatSelStart));
                    size_t bytes = (sub.size() + 1) * sizeof(wchar_t);
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (hg) {
                        void* p = GlobalLock(hg);
                        if (p) { memcpy(p, sub.c_str(), bytes); GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); }
                    }
                    CloseClipboard();
                }
                return 0;
            }
            return 0;
        case WM_CHAR:
            if (w >= 32) {
                wchar_t ch[2]{ (wchar_t)w, 0 };
                SendMessageW(h, EM_REPLACESEL, FALSE, (LPARAM)ch);
                return 0;
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC hdc = BeginPaint(h, &ps);
            RECT r{}; GetClientRect(h, &r);
            HBRUSH bg = CreateSolidBrush(RGB(22,32,45));
            FillRect(hdc, &r, bg);
            DeleteObject(bg);
            HPEN p = CreatePen(PS_SOLID, 1, RGB(45, 78, 108));
            HGDIOBJ oldP = SelectObject(hdc, p);
            HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, oldB);
            SelectObject(hdc, oldP);
            DeleteObject(p);
            HFONT oldF = (HFONT)SelectObject(hdc, g_font);
            SetBkMode(hdc, TRANSPARENT);
            int x = 8;
            int y = 8;
            int wrapW = max(24, (r.right - r.left) - 16);
            int a = min(g_chatSelStart, g_chatSelEnd);
            int b = max(g_chatSelStart, g_chatSelEnd);
            size_t i = 0;
            int idx = 0;
            while (i < g_chatInputText.size()) {
                UINT emojiId = 0; size_t emojiLen = 0;
                bool isEmoji = MatchEmojiAt(g_chatInputText, i, &emojiId, &emojiLen);
                int len = isEmoji ? (int)emojiLen : 1;
                int wpx = 0;
                if (isEmoji) {
                    wpx = 20;
                } else {
                    wchar_t ch[2]{ g_chatInputText[i], 0 };
                    SIZE sz{}; GetTextExtentPoint32W(hdc, ch, 1, &sz);
                    wpx = max(6, (int)sz.cx);
                }
                if ((x - 8) + wpx > wrapW && x > 8) {
                    x = 8;
                    y += kChatInputLineH;
                }
                bool sel = (max(idx, a) < min(idx + len, b));
                if (sel) {
                    RECT sr{ x, y - 2, x + wpx, y + 16 };
                    HBRUSH sb = CreateSolidBrush(RGB(70,130,190));
                    FillRect(hdc, &sr, sb);
                    DeleteObject(sb);
                }
                if (isEmoji) {
                    std::wstring png = ResolveEmojiImagePath(EmojiPngForId(emojiId));
                    if (!png.empty()) {
                        Gdiplus::Graphics g(hdc);
                        Gdiplus::Image img(png.c_str());
                        if (img.GetLastStatus() == Gdiplus::Ok) {
                            g.DrawImage(&img, x, y - 1, 18, 18);
                        } else {
                            SetTextColor(hdc, sel ? RGB(255,255,255) : RGB(220,236,248));
                            TextOutW(hdc, x, y, g_chatInputText.c_str() + i, (int)emojiLen);
                        }
                    }
                    i += emojiLen;
                    idx += (int)emojiLen;
                } else {
                    SetTextColor(hdc, sel ? RGB(255,255,255) : RGB(220,236,248));
                    TextOutW(hdc, x, y, g_chatInputText.c_str() + i, 1);
                    i++;
                    idx++;
                }
                x += wpx;
            }
            int remaining = kChatInputMaxChars - (int)g_chatInputText.size();
            std::wstring cnt = std::to_wstring(max(0, remaining));
            SIZE csz{};
            GetTextExtentPoint32W(hdc, cnt.c_str(), (int)cnt.size(), &csz);
            SetTextColor(hdc, remaining <= 5 ? RGB(255,120,120) : (remaining <= 15 ? RGB(255,210,120) : RGB(138,166,192)));
            TextOutW(hdc, r.right - 8 - csz.cx, r.bottom - 22, cnt.c_str(), (int)cnt.size());
            SelectObject(hdc, oldF);
            EndPaint(h, &ps);
            SetChatInputCaretPos(h);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK ChatToolBtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            int id = GetDlgCtrlID(h);
            if (g_hotChatToolBtn != id) {
                int prev = g_hotChatToolBtn;
                g_hotChatToolBtn = id;
                InvalidateRect(h, nullptr, FALSE);
                if (prev == IDC_CHAT_BOLD && g_chatBold) InvalidateRect(g_chatBold, nullptr, FALSE);
                else if (prev == IDC_CHAT_ITALIC && g_chatItalic) InvalidateRect(g_chatItalic, nullptr, FALSE);
                else if (prev == IDC_CHAT_COLOR && g_chatColor) InvalidateRect(g_chatColor, nullptr, FALSE);
                else if (prev == IDC_CHAT_FONT && g_chatFont) InvalidateRect(g_chatFont, nullptr, FALSE);
                else if (prev == IDC_CHAT_EMOJI && g_chatEmoji) InvalidateRect(g_chatEmoji, nullptr, FALSE);
            }
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = h;
            TrackMouseEvent(&tme);
            break;
        }
        case WM_MOUSELEAVE:
            if (g_hotChatToolBtn == GetDlgCtrlID(h)) {
                g_hotChatToolBtn = 0;
                InvalidateRect(h, nullptr, FALSE);
            }
            break;
    }
    return CallWindowProcW(g_oldChatToolBtnProc, h, m, w, l);
}

static LRESULT CALLBACK EmojiPopupProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)l;
            if (!dis) return FALSE;
            if (dis->CtlID >= IDM_EMOJI_BASE && dis->CtlID <= IDM_EMOJI_LAST) {
                RECT r = dis->rcItem;
                bool pushed = (dis->itemState & ODS_SELECTED) != 0;
                HBRUSH b = CreateSolidBrush(pushed ? RGB(55, 110, 165) : RGB(26, 44, 62));
                FillRect(dis->hDC, &r, b);
                DeleteObject(b);
                HPEN p = CreatePen(PS_SOLID, 1, RGB(72,130,178));
                HGDIOBJ oldP = SelectObject(dis->hDC, p);
                HGDIOBJ oldB = SelectObject(dis->hDC, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dis->hDC, r.left, r.top, r.right, r.bottom);
                SelectObject(dis->hDC, oldB);
                SelectObject(dis->hDC, oldP);
                DeleteObject(p);
                std::wstring fileName = EmojiPngForId((UINT)dis->CtlID);
                std::wstring imgPath = ResolveEmojiImagePath(fileName);
                bool drewImage = false;
                if (!imgPath.empty() && ExistsFile(imgPath)) {
                    Gdiplus::Graphics g(dis->hDC);
                    Gdiplus::Image img(imgPath.c_str());
                    if (img.GetLastStatus() == Gdiplus::Ok) {
                        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                        int iw = (int)img.GetWidth(), ih = (int)img.GetHeight();
                        if (iw > 0 && ih > 0) {
                            int maxW = (r.right - r.left) - 10;
                            int maxH = (r.bottom - r.top) - 10;
                            double s = min((double)maxW / (double)iw, (double)maxH / (double)ih);
                            int dw = max(16, (int)(iw * s));
                            int dh = max(16, (int)(ih * s));
                            int dx = r.left + ((r.right - r.left) - dw) / 2;
                            int dy = r.top + ((r.bottom - r.top) - dh) / 2;
                            g.DrawImage(&img, dx, dy, dw, dh);
                            drewImage = true;
                        }
                    }
                }
                if (!drewImage) {
                    if (g_fontEmoji) SelectObject(dis->hDC, g_fontEmoji);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, RGB(235,245,252));
                    DrawTextW(dis->hDC, EmojiAsciiForId((UINT)dis->CtlID), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
                return TRUE;
            }
            return FALSE;
        }
        case WM_KILLFOCUS: {
            HWND next = (HWND)w;
            if (next && (next == h || IsChild(h, next))) return 0;
            DestroyWindow(h);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id >= (int)IDM_EMOJI_BASE && id <= (int)IDM_EMOJI_LAST) {
                // Insert ASCII token for XP-safe network transfer; renderer maps token to PNG.
                std::wstring tok = EmojiTokenForId((UINT)id);
                if (!tok.empty() && g_chatInput) {
                    SetFocus(g_chatInput);
                    SendMessageW(g_chatInput, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
                    SendMessageW(g_chatInput, EM_REPLACESEL, FALSE, (LPARAM)tok.c_str());
                } else {
                    HWND owner = GetWindow(h, GW_OWNER);
                    if (!owner) owner = g_main;
                    if (owner) SendMessageW(owner, WM_COMMAND, MAKEWPARAM(id, 0), 0);
                }
                DestroyWindow(h);
                return 0;
            }
            break;
        }
        case WM_DESTROY:
            if (g_emojiPopup == h) g_emojiPopup = nullptr;
            return 0;
        case WM_ERASEBKGND: {
            RECT rc{}; GetClientRect(h, &rc);
            FillRect((HDC)w, &rc, g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_WINDOW + 1));
            return 1;
        }
    }
    return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK SgdbPromptProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == 2) { // OK
                HWND edit = GetDlgItem(h, 1);
                wchar_t b[64]{}; GetWindowTextW(edit, b, 63);
                int v = _wtoi(b);
                g_sgdbPromptResult = (v > 0 ? v : -1);
                DestroyWindow(h);
                return 0;
            }
            if (id == 3) { // Cancel
                g_sgdbPromptResult = -1;
                DestroyWindow(h);
                return 0;
            }
            break;
        }
        case WM_KEYDOWN:
            if (w == VK_ESCAPE) { g_sgdbPromptResult = -1; DestroyWindow(h); return 0; }
            if (w == VK_RETURN) {
                SendMessageW(h, WM_COMMAND, MAKEWPARAM(2, BN_CLICKED), (LPARAM)GetDlgItem(h, 2));
                return 0;
            }
            break;
        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)w;
            SetTextColor(hdc, RGB(220,236,248));
            SetBkColor(hdc, RGB(22,32,45));
            return (LRESULT)g_panelBrush;
        }
        case WM_CTLCOLOREDIT: {
            static HBRUSH s_editBrush = CreateSolidBrush(RGB(34, 52, 74));
            HDC hdc = (HDC)w;
            SetTextColor(hdc, RGB(242, 250, 255));
            SetBkColor(hdc, RGB(34, 52, 74));
            return (LRESULT)s_editBrush;
        }
        case WM_ERASEBKGND: {
            RECT rc{}; GetClientRect(h, &rc);
            FillRect((HDC)w, &rc, g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_WINDOW+1));
            return 1;
        }
        case WM_CLOSE:
            g_sgdbPromptResult = -1;
            DestroyWindow(h);
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
static void AddDroppedPathAsGameFolder(const std::wstring& droppedPath) {
    if (!ExistsDir(droppedPath)) return;
    // Drag rule: if folder has EXE in its own root, treat it as exactly one game entry.
    if (!FindLaunchExeFast(droppedPath).empty()) {
        bool exists = std::find_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
            [&](const std::wstring& p) { return _wcsicmp(p.c_str(), droppedPath.c_str()) == 0; }) != g_manualGameFolders.end();
        if (!exists) g_manualGameFolders.push_back(droppedPath);
        return;
    }
    auto folderLooksLikeGameRoot = [&](const std::wstring& folder) {
        // 1) EXE directly in folder.
        if (!FindLaunchExeFast(folder).empty()) return true;
        // 2) EXE directly in immediate child folder (x86/x64/bin/etc layouts).
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW((folder + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return false;
        bool ok = false;
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                std::wstring sub = folder + L"\\" + fd.cFileName;
                if (!FindLaunchExeFast(sub).empty()) { ok = true; break; }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return ok;
    };
    auto tryAddFolder = [&](const std::wstring& folder) {
        if (!folderLooksLikeGameRoot(folder)) return false;
        bool exists = std::find_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
            [&](const std::wstring& p) { return _wcsicmp(p.c_str(), folder.c_str()) == 0; }) != g_manualGameFolders.end();
        if (exists) return false;
        g_manualGameFolders.push_back(folder);
        return true;
    };

    bool addedRoot = tryAddFolder(droppedPath);

    bool addedAnySub = addedRoot;
    bool addedChildGame = false;
    // Depth-bounded fallback scan: +1/+2/+3 folder levels from selected root.
    std::vector<std::pair<std::wstring, int>> q;
    q.push_back({droppedPath, 0});
    for (size_t qi = 0; qi < q.size(); ++qi) {
        const std::wstring cur = q[qi].first;
        const int depth = q[qi].second;
        if (depth >= 3) continue;
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW((cur + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                std::wstring sub = cur + L"\\" + fd.cFileName;
                bool added = tryAddFolder(sub);
                if (added) {
                    addedAnySub = true;
                    addedChildGame = true;
                    // Do not dive into this game root; avoids adding technical children (x86/x64) as separate entries.
                } else {
                    q.push_back({sub, depth + 1});
                }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (addedRoot && addedChildGame) {
        auto it = std::remove_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
            [&](const std::wstring& p) { return _wcsicmp(p.c_str(), droppedPath.c_str()) == 0; });
        if (it != g_manualGameFolders.end()) g_manualGameFolders.erase(it, g_manualGameFolders.end());
    }
    if (!addedAnySub) {
        bool exists = std::find_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
            [&](const std::wstring& p) { return _wcsicmp(p.c_str(), droppedPath.c_str()) == 0; }) != g_manualGameFolders.end();
        if (!exists) g_manualGameFolders.push_back(droppedPath);
    }
}
static void SyncListScrollbar() {
    if (!g_list || !g_listScroll) return;
    int count = (int)SendMessageW(g_list, LB_GETCOUNT, 0, 0);
    int top = (int)SendMessageW(g_list, LB_GETTOPINDEX, 0, 0);
    int itemH = (int)SendMessageW(g_list, LB_GETITEMHEIGHT, 0, 0);
    if (itemH <= 0) itemH = 36;
    RECT lr{}; GetClientRect(g_list, &lr);
    int page = (lr.bottom - lr.top) / itemH;
    if (page < 1) page = 1;
    RECT sr{}; GetClientRect(g_listScroll, &sr);
    int trackH = (sr.bottom - sr.top) - 4;
    if (trackH < 20) trackH = 20;
    if (count <= page) {
        ShowWindow(g_listScroll, SW_HIDE);
        g_listThumbRect = {2, 2, sr.right - 2, sr.bottom - 2};
    } else {
        ShowWindow(g_listScroll, SW_SHOW);
        int thumbH = max(26, (page * trackH) / count);
        int maxTop = max(1, count - page);
        int travel = max(1, trackH - thumbH);
        int thumbY = 2 + (top * travel) / maxTop;
        g_listThumbRect = {2, thumbY, sr.right - 2, thumbY + thumbH};
    }
    InvalidateRect(g_listScroll, nullptr, TRUE);
}

static std::string W2A(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring A2W(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::wstring ExeDir() {
    wchar_t p[MAX_PATH]{};
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s = p;
    size_t pos = s.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : s.substr(0, pos);
}

static void SetControlFont(HWND h, bool bold = false) { SendMessageW(h, WM_SETFONT, (WPARAM)(bold ? g_fontBold : g_font), TRUE); }
static std::wstring NormalizePathKey(std::wstring p) {
    std::replace(p.begin(), p.end(), L'/', L'\\');
    while (!p.empty() && (p.back() == L'\\' || p.back() == L' ')) p.pop_back();
    std::transform(p.begin(), p.end(), p.begin(), towlower);
    return p;
}

static std::wstring QuoteArg(const std::wstring& s) {
    return L"\"" + s + L"\"";
}
static std::shared_ptr<Gdiplus::Image> GetCachedImage(const std::wstring& path) {
    if (path.empty()) return nullptr;
    if (!g_useImageCache) {
        auto img = std::make_shared<Gdiplus::Image>(path.c_str());
        if (img->GetLastStatus() != Gdiplus::Ok) return nullptr;
        return img;
    }
    auto it = g_imageCache.find(path);
    if (it != g_imageCache.end()) return it->second;
    auto img = std::make_shared<Gdiplus::Image>(path.c_str());
    if (img->GetLastStatus() != Gdiplus::Ok) return nullptr;
    g_imageCache[path] = img;
    return img;
}
static void PreloadVisibleBannerImages() {
    for (const auto& g : g_uiGames) {
        if (!g.bannerPath.empty()) (void)GetCachedImage(g.bannerPath);
        if (!g.logoPath.empty()) (void)GetCachedImage(g.logoPath);
    }
}
static std::wstring CurrentSearchText() {
    if (!g_search) return L"";
    if (g_searchPlaceholder) return L"";
    wchar_t buf[512]{};
    GetWindowTextW(g_search, buf, 511);
    std::wstring q = buf;
    std::transform(q.begin(), q.end(), q.begin(), towlower);
    return q;
}

static int RunHidden(const std::wstring& cmdLine) {
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    std::wstring mutableCmd = cmdLine;
    BOOL ok = CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
}

static bool FirewallRuleExists(const std::wstring& ruleName) {
    std::wstring cmd = L"netsh advfirewall firewall show rule name=" + QuoteArg(ruleName) + L" >nul 2>&1";
    int rc = RunHidden(L"cmd /c " + QuoteArg(cmd));
    return rc == 0;
}

static void EnsureFirewallRules() {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring prog = exePath;

    if (!FirewallRuleExists(FIREWALL_RULE_DISCOVERY)) {
        std::wstring add = L"netsh advfirewall firewall add rule name=" + QuoteArg(FIREWALL_RULE_DISCOVERY) +
            L" dir=in action=allow program=" + QuoteArg(prog) + L" protocol=UDP localport=50055 profile=any";
        RunHidden(L"cmd /c " + QuoteArg(add));
    }
    if (!FirewallRuleExists(FIREWALL_RULE_CHAT)) {
        std::wstring add = L"netsh advfirewall firewall add rule name=" + QuoteArg(FIREWALL_RULE_CHAT) +
            L" dir=in action=allow program=" + QuoteArg(prog) + L" protocol=UDP localport=50057 profile=any";
        RunHidden(L"cmd /c " + QuoteArg(add));
    }
    if (!FirewallRuleExists(FIREWALL_RULE_SHARE)) {
        std::wstring add = L"netsh advfirewall firewall add rule name=" + QuoteArg(FIREWALL_RULE_SHARE) +
            L" dir=in action=allow program=" + QuoteArg(prog) + L" protocol=TCP localport=50111-50310 profile=any";
        RunHidden(L"cmd /c " + QuoteArg(add));
    }
}

static void LoadConfig() {
    g_manualGameFolders.clear();
    g_cfg.sharedRoot.clear();
    g_nameAliases.clear();
    std::wstring p = ExeDir() + L"\\data\\config.ini";
    std::wifstream in(p.c_str());
    if (!in) return;
    std::wstring line;
    while (std::getline(in, line)) {
        if (line.rfind(L"shared_root=", 0) == 0) g_cfg.sharedRoot = line.substr(12);
        else if (line.rfind(L"manual_game=", 0) == 0) {
            std::wstring mg = line.substr(12);
            if (!mg.empty()) g_manualGameFolders.push_back(mg);
        }
        else if (line.rfind(L"hidden_entry=", 0) == 0) {
            std::wstring he = line.substr(13);
            if (!he.empty()) g_hiddenEntries.push_back(he);
        }
        else if (line.rfind(L"sgdb_id=", 0) == 0) {
            std::wstring kv = line.substr(8);
            size_t sep = kv.find(L'|');
            if (sep != std::wstring::npos) {
                std::wstring key = kv.substr(0, sep);
                int id = _wtoi(kv.substr(sep + 1).c_str());
                if (!key.empty() && id > 0) g_manualSgdbIds.push_back({key, id});
            }
        }
        else if (line.rfind(L"sgdb_name=", 0) == 0) {
            std::wstring kv = line.substr(10);
            size_t sep = kv.find(L'|');
            if (sep != std::wstring::npos) {
                std::wstring key = kv.substr(0, sep);
                std::wstring val = kv.substr(sep + 1);
                if (!key.empty() && !val.empty()) g_nameAliases.push_back({key, val});
            }
        }
        else if (line.rfind(L"win_x=", 0) == 0) g_cfg.winX = _wtoi(line.substr(6).c_str());
        else if (line.rfind(L"win_y=", 0) == 0) g_cfg.winY = _wtoi(line.substr(6).c_str());
        else if (line.rfind(L"win_w=", 0) == 0) g_cfg.winW = max(800, _wtoi(line.substr(6).c_str()));
        else if (line.rfind(L"win_h=", 0) == 0) g_cfg.winH = max(600, _wtoi(line.substr(6).c_str()));
        else if (line.rfind(L"win_max=", 0) == 0) g_cfg.winMax = (_wtoi(line.substr(8).c_str()) != 0);
        else if (line.rfind(L"image_cache=", 0) == 0) g_useImageCache = (_wtoi(line.substr(12).c_str()) != 0);
        else if (line.rfind(L"lan_share=", 0) == 0) g_lanShareEnabled = (_wtoi(line.substr(10).c_str()) != 0);
        else if (line.rfind(L"last_selected_game=", 0) == 0) g_lastSelectedGame = line.substr(19);
        else if (line.rfind(L"last_selected_path=", 0) == 0) g_lastSelectedPath = line.substr(19);
        else if (line.rfind(L"nick=", 0) == 0) g_myNick = line.substr(5);
        else if (line.rfind(L"presence=", 0) == 0) g_myPresence = line.substr(9);
        else if (line.rfind(L"last_selected=", 0) == 0) g_lastSelectedGame = line.substr(14); // backward compatibility
    }
    std::vector<std::wstring> uniq;
    for (const auto& m : g_manualGameFolders) {
        bool ex = false;
        for (const auto& u : uniq) if (_wcsicmp(u.c_str(), m.c_str()) == 0) { ex = true; break; }
        if (!ex) uniq.push_back(m);
    }
    g_manualGameFolders.swap(uniq);
}

static void SaveConfig() {
    EnsureDir(ExeDir() + L"\\data");
    std::wstring p = ExeDir() + L"\\data\\config.ini";
    std::wofstream out(p.c_str(), std::ios::trunc);
    if (!out) return;
    out << L"shared_root=" << g_cfg.sharedRoot << L"\n";
    for (const auto& m : g_manualGameFolders) {
        out << L"manual_game=" << m << L"\n";
    }
    for (const auto& h : g_hiddenEntries) {
        out << L"hidden_entry=" << h << L"\n";
    }
    for (const auto& p : g_manualSgdbIds) {
        out << L"sgdb_id=" << p.first << L"|" << p.second << L"\n";
    }
    for (const auto& p : g_nameAliases) {
        out << L"sgdb_name=" << p.first << L"|" << p.second << L"\n";
    }
    out << L"win_x=" << g_cfg.winX << L"\n";
    out << L"win_y=" << g_cfg.winY << L"\n";
    out << L"win_w=" << g_cfg.winW << L"\n";
    out << L"win_h=" << g_cfg.winH << L"\n";
    out << L"win_max=" << (g_cfg.winMax ? 1 : 0) << L"\n";
    out << L"image_cache=" << (g_useImageCache ? 1 : 0) << L"\n";
    out << L"lan_share=" << (g_lanShareEnabled.load() ? 1 : 0) << L"\n";
    out << L"last_selected_game=" << g_lastSelectedGame << L"\n";
    out << L"last_selected_path=" << g_lastSelectedPath << L"\n";
    out << L"nick=" << g_myNick << L"\n";
    out << L"presence=" << g_myPresence << L"\n";
}

static std::wstring EntryHideKey(const GameEntry& e) {
    if (e.local) return L"local::" + e.path;
    return L"remote::" + e.remoteHost + L"::" + e.name;
}
static void CleanupGdiplusCaches() {
    std::lock_guard<std::mutex> lk(g_cacheMutex);
    g_imageCache.clear();
}
static bool IsWindowsXpFamily() {
    OSVERSIONINFOEXW vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);
#pragma warning(push)
#pragma warning(disable:4996)
    if (!GetVersionExW((OSVERSIONINFOW*)&vi)) return false;
#pragma warning(pop)
    return (vi.dwMajorVersion < 6);
}
static bool DrawImageSmart(HDC hdc, const std::wstring& path, int x, int y, int w, int h) {
    if (path.empty() || !ExistsFile(path) || w <= 0 || h <= 0) return false;
    std::ifstream in(path.c_str(), std::ios::binary);
    bool looksIco = false;
    if (in) {
        unsigned char sig[4]{};
        in.read((char*)sig, 4);
        looksIco = (in.gcount() == 4 && sig[0] == 0x00 && sig[1] == 0x00 && sig[2] == 0x01 && sig[3] == 0x00);
    }
    if (looksIco) {
        HICON hi = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, w, h, LR_LOADFROMFILE);
        if (!hi) hi = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
        if (hi) {
            DrawIconEx(hdc, x, y, hi, w, h, 0, nullptr, DI_NORMAL);
            DestroyIcon(hi);
            return true;
        }
    }
    Gdiplus::Graphics g(hdc);
    Gdiplus::Image img(path.c_str());
    if (img.GetLastStatus() == Gdiplus::Ok) {
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(&img, x, y, w, h);
        return true;
    }
    if (!looksIco) {
        HICON hi = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, w, h, LR_LOADFROMFILE);
        if (!hi) hi = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
        if (hi) {
            DrawIconEx(hdc, x, y, hi, w, h, 0, nullptr, DI_NORMAL);
            DestroyIcon(hi);
            return true;
        }
    }
    return false;
}

static bool ParseIPv4Address(const std::string& ip, in_addr* out) {
    if (!out) return false;
    unsigned long a = inet_addr(ip.c_str());
    if (a == INADDR_NONE && ip != "255.255.255.255") return false;
    out->s_addr = a;
    return true;
}

static std::string IPv4ToString(const in_addr& addr) {
    const char* p = inet_ntoa(*(in_addr*)&addr);
    return p ? std::string(p) : std::string();
}

static void TryDwmSetWindowAttribute(HWND hwnd, DWORD attr, const void* value, DWORD valueSize) {
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    typedef HRESULT (WINAPI *PFNDWMSETWINDOWATTRIBUTE)(HWND, DWORD, LPCVOID, DWORD);
    PFNDWMSETWINDOWATTRIBUTE pSet = (PFNDWMSETWINDOWATTRIBUTE)GetProcAddress(hDwm, "DwmSetWindowAttribute");
    if (pSet) pSet(hwnd, attr, value, valueSize);
    FreeLibrary(hDwm);
}

static ULONGLONG AppTickCount64() {
    typedef ULONGLONG (WINAPI *PFNGETTICKCOUNT64)(void);
    static PFNGETTICKCOUNT64 pGetTick64 = (PFNGETTICKCOUNT64)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTickCount64");
    if (pGetTick64) return pGetTick64();
    static DWORD last = 0;
    static ULONGLONG high = 0;
    DWORD now = GetTickCount();
    if (now < last) high += (1ULL << 32); // rollover
    last = now;
    return high + now;
}

static std::wstring ToLongPath(const std::wstring& p) {
    if (p.empty()) return p;
    if (p.rfind(L"\\\\?\\", 0) == 0) return p;
    if (p.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    if (p.size() >= 2 && p[1] == L':') return L"\\\\?\\" + p;
    return p;
}

static bool WriteAllBytes(const std::wstring& p, const std::string& bytes) {
    std::wstring lp = ToLongPath(p);
    HANDLE h = CreateFileW(lp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    if (!bytes.empty()) {
        const char* data = bytes.data();
        size_t left = bytes.size();
        while (left > 0) {
            DWORD chunk = (left > 1u * 1024u * 1024u) ? (1u * 1024u * 1024u) : (DWORD)left;
            DWORD wr = 0;
            if (!WriteFile(h, data, chunk, &wr, nullptr) || wr == 0) { ok = false; break; }
            data += wr;
            left -= wr;
        }
    }
    CloseHandle(h);
    return ok;
}

static bool ReadAllBytes(const std::wstring& p, std::string& out) {
    out.clear();
    std::wstring lp = ToLongPath(p);
    HANDLE h = CreateFileW(lp.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0) { CloseHandle(h); return false; }
    if (sz.QuadPart > 0) {
        out.resize((size_t)sz.QuadPart);
        char* dst = out.data();
        size_t left = (size_t)sz.QuadPart;
        while (left > 0) {
            DWORD chunk = (left > 1u * 1024u * 1024u) ? (1u * 1024u * 1024u) : (DWORD)left;
            DWORD rd = 0;
            if (!ReadFile(h, dst, chunk, &rd, nullptr) || rd == 0) { CloseHandle(h); out.clear(); return false; }
            dst += rd;
            left -= rd;
        }
    }
    CloseHandle(h);
    return true;
}

static bool ExistsFile(const std::wstring& p) { DWORD a = GetFileAttributesW(ToLongPath(p).c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
static bool ExistsDir(const std::wstring& p) { DWORD a = GetFileAttributesW(ToLongPath(p).c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
static void EnsureDir(const std::wstring& p) { SHCreateDirectoryExW(nullptr, ToLongPath(p).c_str(), nullptr); }
static bool DeleteDirTree(const std::wstring& path) {
    if (path.empty()) return false;
    std::wstring from = path;
    from.push_back(L'\0'); // double-null terminated for SHFileOperationW
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    int r = SHFileOperationW(&op);
    return r == 0 && !op.fAnyOperationsAborted;
}

static std::wstring Slugify(const std::wstring& in) {
    std::wstring out;
    bool dash = false;
    for (wchar_t c : in) {
        wchar_t lc = (wchar_t)towlower(c);
        if ((lc >= L'a' && lc <= L'z') || (lc >= L'0' && lc <= L'9')) {
            out.push_back(lc);
            dash = false;
        } else if (!dash) {
            out.push_back(L'-');
            dash = true;
        }
    }
    while (!out.empty() && out.front() == L'-') out.erase(out.begin());
    while (!out.empty() && out.back() == L'-') out.pop_back();
    return out.empty() ? L"game" : out;
}

static std::wstring MakeSafeFsName(const std::wstring& in) {
    std::wstring out;
    out.reserve(in.size());
    for (wchar_t c : in) {
        switch (c) {
        case L'<': case L'>': case L':': case L'"': case L'/': case L'\\': case L'|': case L'?': case L'*':
            out.push_back((c == L':') ? L'-' : L'_');
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
    while (!out.empty() && (out.front() == L' ')) out.erase(out.begin());
    return out.empty() ? L"game" : out;
}

static int GetManualSgdbIdForGame(const std::wstring& gameName) {
    std::wstring key = Slugify(gameName);
    for (const auto& p : g_manualSgdbIds) {
        if (_wcsicmp(p.first.c_str(), key.c_str()) == 0) return p.second;
    }
    return 0;
}

static void SetManualSgdbIdForGame(const std::wstring& gameName, int id) {
    std::wstring key = Slugify(gameName);
    for (auto& p : g_manualSgdbIds) {
        if (_wcsicmp(p.first.c_str(), key.c_str()) == 0) {
            p.second = id;
            return;
        }
    }
    g_manualSgdbIds.push_back({key, id});
}

static std::wstring HttpGetText(const std::wstring& host, INTERNET_PORT port, const std::wstring& path, const std::wstring& auth = L"") {
    std::wstring result;
    HINTERNET hSession = WinHttpOpen(L"LANGamesDeployerCpp/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return result; }
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return result; }
    std::wstring headers = L"User-Agent: Mozilla/5.0\r\nAccept: application/json\r\n";
    if (!auth.empty()) headers += L"Authorization: Bearer " + auth + L"\r\n";
    BOOL ok = WinHttpSendRequest(hReq, headers.c_str(), (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hReq, nullptr);
    if (ok) {
        DWORD size = 0;
        do {
            size = 0;
            if (!WinHttpQueryDataAvailable(hReq, &size) || size == 0) break;
            std::string chunk(size, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(hReq, &chunk[0], size, &read) || read == 0) break;
            chunk.resize(read);
            result += A2W(chunk);
        } while (size > 0);
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return result;
}

static bool HttpDownloadFile(const std::wstring& url, const std::wstring& dest, const std::wstring& auth = L"") {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2047;
    uc.dwSchemeLength = 1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    std::wstring h(host, uc.dwHostNameLength);
    std::wstring p(path, uc.dwUrlPathLength);
    INTERNET_PORT port = uc.nPort;
    bool https = uc.nScheme == INTERNET_SCHEME_HTTPS;

    HINTERNET s = WinHttpOpen(L"LANGamesDeployerCpp/1.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;
    HINTERNET c = WinHttpConnect(s, h.c_str(), port, 0);
    if (!c) { WinHttpCloseHandle(s); return false; }
    HINTERNET r = WinHttpOpenRequest(c, L"GET", p.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }
    std::wstring headers = L"User-Agent: Mozilla/5.0\r\n";
    if (!auth.empty()) headers += L"Authorization: Bearer " + auth + L"\r\n";
    BOOL ok = WinHttpSendRequest(r, headers.c_str(), (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(r, nullptr);
    if (!ok) { WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }
    std::ofstream out(dest.c_str(), std::ios::binary);
    if (!out) { WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); return false; }
    DWORD size = 0;
    do {
        size = 0;
        if (!WinHttpQueryDataAvailable(r, &size) || size == 0) break;
        std::vector<char> buf(size);
        DWORD read = 0;
        if (!WinHttpReadData(r, buf.data(), size, &read) || read == 0) break;
        out.write(buf.data(), read);
    } while (size > 0);
    out.close();
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return true;
}

static int JsonFindFirstId(const std::wstring& json) {
    std::wregex re(L"\"id\"\\s*:\\s*([0-9]+)");
    std::wsmatch m;
    if (std::regex_search(json, m, re)) return _wtoi(m[1].str().c_str());
    return 0;
}

static std::wstring JsonFindFirstUrl(const std::wstring& json) {
    std::wregex re(L"\"url\"\\s*:\\s*\"([^\"]+)\"");
    std::wsmatch m;
    if (std::regex_search(json, m, re)) {
        std::wstring u = m[1].str();
        size_t pos = 0;
        while ((pos = u.find(L"\\/", pos)) != std::wstring::npos) {
            u.replace(pos, 2, L"/");
            pos += 1;
        }
        return u;
    }
    return L"";
}

static std::wstring UrlEncodeSimple(const std::wstring& s) {
    std::string utf8 = W2A(s);
    std::wstring out;
    for (unsigned char b : utf8) {
        if ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') ||
            b == '-' || b == '_' || b == '.' || b == '/' || b == '~') {
            out.push_back((wchar_t)b);
        } else if (b == ' ') {
            out += L"%20";
        } else {
            wchar_t hex[4]{};
            swprintf(hex, 4, L"%%%02X", (unsigned int)b);
            out += hex;
        }
    }
    return out;
}

static std::wstring UrlDecodeSimple(const std::wstring& s) {
    std::string bytes;
    bytes.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'%' && i + 2 < s.size()) {
            wchar_t h1 = s[i + 1], h2 = s[i + 2];
            int v1 = (h1 >= L'0' && h1 <= L'9') ? h1 - L'0' : ((h1 >= L'a' && h1 <= L'f') ? h1 - L'a' + 10 : (h1 >= L'A' && h1 <= L'F' ? h1 - L'A' + 10 : -1));
            int v2 = (h2 >= L'0' && h2 <= L'9') ? h2 - L'0' : ((h2 >= L'a' && h2 <= L'f') ? h2 - L'a' + 10 : (h2 >= L'A' && h2 <= L'F' ? h2 - L'A' + 10 : -1));
            if (v1 >= 0 && v2 >= 0) { bytes.push_back((char)((v1 << 4) | v2)); i += 2; continue; }
        }
        wchar_t ch = (s[i] == L'+') ? L' ' : s[i];
        if (ch >= 0 && ch <= 0x7F) bytes.push_back((char)ch);
    }
    return A2W(bytes);
}

static void DownloadSteamGridForSelected() {
    int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= (int)g_uiGames.size()) return;
    std::wstring game = g_uiGames[idx].name;
    SetStatus(L"Status: SteamGridDB search...");
    bool okAny = DownloadSteamGridForGameName(game);
    if (okAny) SetStatus(L"Status: SteamGrid assets downloaded");
    else SetStatus(L"Status: SteamGrid download failed");
    ScanLocalGames();
    PostMessageW(g_main, WM_ASSETS_UPDATE, 0, 0);
}

static bool DownloadSteamGridForGameName(const std::wstring& game) {
    int gameId = GetManualSgdbIdForGame(game);
    std::wstring apiKey = GetSteamGridDbApiKey();
    if (gameId <= 0) {
        std::wstring searchPath = L"/api/v2/search/autocomplete/" + UrlEncodeSimple(game);
        std::wstring searchJson = HttpGetText(L"www.steamgriddb.com", INTERNET_DEFAULT_HTTPS_PORT, searchPath, apiKey);
        gameId = JsonFindFirstId(searchJson);
        std::wstring nameHit = JsonFindFirstName(searchJson);
        if (!nameHit.empty()) SetAliasForGame(game, nameHit);
    }
    if (gameId <= 0) return false;
    {
        std::wstringstream gp; gp << L"/api/v2/games/id/" << gameId;
        std::wstring gameJson = HttpGetText(L"www.steamgriddb.com", INTERNET_DEFAULT_HTTPS_PORT, gp.str(), apiKey);
        std::wstring canonical = JsonFindFirstName(gameJson);
        if (!canonical.empty()) SetAliasForGame(game, canonical);
    }
    std::wstringstream p1; p1 << L"/api/v2/icons/game/" << gameId;
    std::wstringstream p2; p2 << L"/api/v2/heroes/game/" << gameId;
    std::wstringstream p3; p3 << L"/api/v2/grids/game/" << gameId;
    std::wstringstream p4; p4 << L"/api/v2/logos/game/" << gameId;
    std::wstring iconJson = HttpGetText(L"www.steamgriddb.com", INTERNET_DEFAULT_HTTPS_PORT, p1.str(), apiKey);
    std::wstring bannerJson = HttpGetText(L"www.steamgriddb.com", INTERNET_DEFAULT_HTTPS_PORT, p2.str(), apiKey);
    std::wstring logoJson = HttpGetText(L"www.steamgriddb.com", INTERNET_DEFAULT_HTTPS_PORT, p4.str(), apiKey);
    std::wstring iconUrl = JsonFindFirstUrl(iconJson);
    std::wstring bannerUrl = JsonFindFirstUrl(bannerJson);
    std::wstring logoUrl = JsonFindFirstUrl(logoJson);
    if (bannerUrl.empty()) bannerUrl = JsonFindFirstUrl(HttpGetText(L"www.steamgriddb.com", INTERNET_DEFAULT_HTTPS_PORT, p3.str(), apiKey));
    std::wstring dir = ExeDir() + L"\\data\\assets\\games\\" + Slugify(game);
    EnsureDir(ExeDir() + L"\\data");
    EnsureDir(ExeDir() + L"\\data\\assets");
    EnsureDir(ExeDir() + L"\\data\\assets\\games");
    EnsureDir(dir);
    bool okAny = false;
    if (!iconUrl.empty()) okAny = HttpDownloadFile(iconUrl, dir + L"\\icon.png");
    if (!bannerUrl.empty()) okAny = HttpDownloadFile(bannerUrl, dir + L"\\banner.png") || okAny;
    if (!logoUrl.empty()) okAny = HttpDownloadFile(logoUrl, dir + L"\\logo.png") || okAny;
    SaveConfig();
    return okAny;
}

static void ChatWrapSelection(const wchar_t* openTag, const wchar_t* closeTag) {
    if (!g_chatInput) return;
    SetFocus(g_chatInput);
    SendMessageW(g_chatInput, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g_chatInput, EM_REPLACESEL, FALSE, (LPARAM)openTag);
    if (closeTag && *closeTag) SendMessageW(g_chatInput, EM_REPLACESEL, FALSE, (LPARAM)closeTag);
}

static std::wstring ChatStripTags(const std::wstring& in) {
    return std::regex_replace(in, std::wregex(L"\\[(\\/)?(b|i|color(=[^\\]]+)?|font(=[^\\]]+)?)\\]"), L"");
}
static bool MatchEmojiAt(const std::wstring& text, size_t pos, UINT* outId, size_t* outLen) {
    if (!outId || !outLen || pos >= text.size()) return false;
    // ASCII wire token format for cross-OS reliability: :eN:
    if (text[pos] == L':' && (pos + 3) < text.size() && text[pos + 1] == L'e') {
        size_t j = pos + 2;
        unsigned v = 0;
        bool hasDigit = false;
        while (j < text.size() && text[j] >= L'0' && text[j] <= L'9') {
            hasDigit = true;
            v = v * 10 + (unsigned)(text[j] - L'0');
            ++j;
        }
        if (hasDigit && j < text.size() && text[j] == L':') {
            UINT id = IDM_EMOJI_BASE + v;
            if (id >= IDM_EMOJI_BASE && id <= IDM_EMOJI_LAST) {
                *outId = id;
                *outLen = (j - pos + 1);
                return true;
            }
        }
    }
    for (const auto& e : kEmojiItems) {
        size_t n = wcslen(e.glyph);
        if (n == 0 || pos + n > text.size()) continue;
        if (text.compare(pos, n, e.glyph) == 0) {
            *outId = e.id;
            *outLen = n;
            return true;
        }
    }
    return false;
}
static void DrawChatTextWithEmoji(HDC hdc, const std::wstring& text, int x, int y, int rightLimit) {
    SetBkMode(hdc, TRANSPARENT);
    int cx = x;
    size_t i = 0;
    while (i < text.size()) {
        UINT emojiId = 0;
        size_t emojiLen = 0;
        if (MatchEmojiAt(text, i, &emojiId, &emojiLen)) {
            std::wstring png = ResolveEmojiImagePath(EmojiPngForId(emojiId));
            if (!png.empty()) {
                Gdiplus::Graphics g(hdc);
                Gdiplus::Image img(png.c_str());
                if (img.GetLastStatus() == Gdiplus::Ok) {
                    int sz = 18;
                    if (cx + sz > rightLimit) break;
                    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    g.DrawImage(&img, cx, y - 1, sz, sz);
                    cx += sz + 2;
                    i += emojiLen;
                    continue;
                }
            }
        }
        size_t start = i;
        while (i < text.size()) {
            UINT nextId = 0; size_t nextLen = 0;
            if (MatchEmojiAt(text, i, &nextId, &nextLen)) break;
            i++;
        }
        std::wstring seg = text.substr(start, i - start);
        if (!seg.empty()) {
            SIZE sz{};
            GetTextExtentPoint32W(hdc, seg.c_str(), (int)seg.size(), &sz);
            if (cx + sz.cx > rightLimit) break;
            TextOutW(hdc, cx, y, seg.c_str(), (int)seg.size());
            cx += sz.cx;
        }
    }
}
static int MeasureChatTextWithEmoji(HDC hdc, const std::wstring& text) {
    int w = 0;
    size_t i = 0;
    while (i < text.size()) {
        UINT emojiId = 0;
        size_t emojiLen = 0;
        if (MatchEmojiAt(text, i, &emojiId, &emojiLen)) {
            w += 20;
            i += emojiLen;
            continue;
        }
        size_t start = i;
        while (i < text.size()) {
            UINT nextId = 0; size_t nextLen = 0;
            if (MatchEmojiAt(text, i, &nextId, &nextLen)) break;
            i++;
        }
        std::wstring seg = text.substr(start, i - start);
        if (!seg.empty()) {
            SIZE sz{};
            GetTextExtentPoint32W(hdc, seg.c_str(), (int)seg.size(), &sz);
            w += sz.cx;
        }
    }
    return w;
}
static int MeasureChatRangeWithEmoji(HDC hdc, const std::wstring& text, size_t start, size_t end) {
    if (start >= end || start >= text.size()) return 0;
    end = min(end, text.size());
    int w = 0;
    size_t i = start;
    while (i < end) {
        UINT emojiId = 0;
        size_t emojiLen = 0;
        if (MatchEmojiAt(text, i, &emojiId, &emojiLen) && i + emojiLen <= end) {
            w += 20;
            i += emojiLen;
            continue;
        }
        wchar_t ch[2]{ text[i], 0 };
        SIZE sz{};
        GetTextExtentPoint32W(hdc, ch, 1, &sz);
        w += sz.cx;
        ++i;
    }
    return w;
}
static void DrawWrappedPlainText(HDC hdc, const std::wstring& text, RECT r, COLORREF color) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    int y = r.top;
    int lineH = 18;
    size_t pos = 0;
    while (pos < text.size() && y + lineH <= r.bottom) {
        int maxW = max(1, r.right - r.left);
        size_t bestEnd = pos;
        size_t lastBreak = pos;
        int usedW = 0;
        size_t i = pos;
        while (i < text.size()) {
            UINT emojiId = 0;
            size_t emojiLen = 0;
            if (MatchEmojiAt(text, i, &emojiId, &emojiLen)) {
                int tokW = 20;
                if (usedW + tokW > maxW && i > pos) break;
                usedW += tokW;
                i += emojiLen;
                bestEnd = i;
                continue;
            }
            wchar_t ch = text[i];
            wchar_t buf[2]{ ch, 0 };
            SIZE sz{};
            GetTextExtentPoint32W(hdc, buf, 1, &sz);
            int tokW = max(1, (int)sz.cx);
            if (usedW + tokW > maxW && i > pos) break;
            usedW += tokW;
            ++i;
            bestEnd = i;
            if (ch == L' ' || ch == L'\t') lastBreak = i;
        }
        if (bestEnd <= pos) bestEnd = min(text.size(), pos + 1);
        if (bestEnd < text.size() && lastBreak > pos) bestEnd = lastBreak;

        std::wstring line = text.substr(pos, bestEnd - pos);
        while (!line.empty() && (line.back() == L' ' || line.back() == L'\t')) line.pop_back();
        if (!line.empty()) DrawChatTextWithEmoji(hdc, line, r.left, y, r.right);

        pos = bestEnd;
        while (pos < text.size() && (text[pos] == L' ' || text[pos] == L'\t')) ++pos;
        y += lineH;
    }
}
static int MeasureChatInputPrefixPx(HDC hdc, const std::wstring& text, int charCount) {
    int w = 0;
    int consumed = 0;
    size_t i = 0;
    while (i < text.size() && consumed < charCount) {
        UINT emojiId = 0;
        size_t emojiLen = 0;
        if (MatchEmojiAt(text, i, &emojiId, &emojiLen)) {
            int take = min((int)emojiLen, charCount - consumed);
            if (take <= 0) break;
            // Emoji token behaves as a single rendered glyph box.
            w += 20;
            i += (size_t)take;
            consumed += take;
            continue;
        }
        wchar_t ch[2]{ text[i], 0 };
        SIZE sz{};
        GetTextExtentPoint32W(hdc, ch, 1, &sz);
        w += max(6, (int)sz.cx);
        i++;
        consumed++;
    }
    return w;
}
static POINT ChatInputPointFromIndex(HWND h, HDC hdc, int charIndex) {
    RECT r{}; GetClientRect(h, &r);
    int wrapW = max(24, (r.right - r.left) - 16);
    int x = 8;
    int y = 8;
    int idx = 0;
    size_t i = 0;
    while (i < g_chatInputText.size() && idx < charIndex) {
        UINT emojiId = 0;
        size_t emojiLen = 0;
        bool isEmoji = MatchEmojiAt(g_chatInputText, i, &emojiId, &emojiLen);
        int len = isEmoji ? (int)emojiLen : 1;
        int wpx = 0;
        if (isEmoji) {
            wpx = 20;
        } else {
            wchar_t ch[2]{ g_chatInputText[i], 0 };
            SIZE sz{};
            GetTextExtentPoint32W(hdc, ch, 1, &sz);
            wpx = max(6, (int)sz.cx);
        }
        if ((x - 8) + wpx > wrapW && x > 8) {
            x = 8;
            y += kChatInputLineH;
        }
        x += wpx;
        i += (size_t)len;
        idx += len;
    }
    POINT pt{ x, y };
    return pt;
}
static void ClampChatInputState() {
    int n = (int)g_chatInputText.size();
    if (g_chatInputCaret < 0) g_chatInputCaret = 0;
    if (g_chatInputCaret > n) g_chatInputCaret = n;
    if (g_chatSelStart < 0) g_chatSelStart = 0;
    if (g_chatSelEnd < 0) g_chatSelEnd = 0;
    if (g_chatSelStart > n) g_chatSelStart = n;
    if (g_chatSelEnd > n) g_chatSelEnd = n;
}
static void SetChatInputCaretPos(HWND h) {
    if (!h || GetFocus() != h) return;
    HDC hdc = GetDC(h);
    HFONT old = (HFONT)SelectObject(hdc, g_font);
    POINT pt = ChatInputPointFromIndex(h, hdc, g_chatInputCaret);
    RECT r{}; GetClientRect(h, &r);
    int x = pt.x;
    int y = pt.y;
    if (x < 8) x = 8;
    if (x > r.right - 8) x = r.right - 8;
    if (y < 8) y = 8;
    if (y > r.bottom - 12) y = r.bottom - 12;
    SelectObject(hdc, old);
    ReleaseDC(h, hdc);
    SetCaretPos(x, y);
}
static int ChatInputIndexFromXY(HWND h, HDC hdc, int x, int y) {
    RECT r{}; GetClientRect(h, &r);
    int wrapW = max(24, (r.right - r.left) - 16);
    int targetRow = max(0, (y - 8) / kChatInputLineH);
    int cx = 8;
    int row = 0;
    int idx = 0;
    size_t i = 0;
    while (i < g_chatInputText.size()) {
        UINT emojiId = 0;
        size_t emojiLen = 0;
        bool isEmoji = MatchEmojiAt(g_chatInputText, i, &emojiId, &emojiLen);
        int len = isEmoji ? (int)emojiLen : 1;
        int w = 0;
        if (isEmoji) w = 20;
        else {
            wchar_t ch[2]{ g_chatInputText[i], 0 };
            SIZE sz{}; GetTextExtentPoint32W(hdc, ch, 1, &sz);
            w = max(6, (int)sz.cx);
        }
        if ((cx - 8) + w > wrapW && cx > 8) {
            if (targetRow <= row) return idx;
            cx = 8;
            row++;
        }
        if (targetRow == row && x < cx + w / 2) return idx;
        cx += w;
        i += (size_t)len;
        idx += len;
    }
    return (int)g_chatInputText.size();
}
static bool IsWordChar(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || ch == L'_';
}
static int ChatTextIndexFromX(HDC hdc, const std::wstring& s, int x) {
    int cx = 0;
    for (int i = 0; i < (int)s.size(); ++i) {
        wchar_t ch[2]{ s[(size_t)i], 0 };
        SIZE sz{}; GetTextExtentPoint32W(hdc, ch, 1, &sz);
        int w = max(6, (int)sz.cx);
        if (x < cx + w / 2) return i;
        cx += w;
    }
    return (int)s.size();
}
static std::wstring SelfAvatarPath() {
    EnsureDir(ExeDir() + L"\\data");
    EnsureDir(ExeDir() + L"\\data\\avatars");
    return ExeDir() + L"\\data\\avatars\\self.png";
}
static std::wstring ResolveSelfAvatarPath() {
    std::wstring base = ExeDir() + L"\\data\\avatars\\self";
    const wchar_t* exts[] = { L".png", L".jpg", L".jpeg", L".bmp" };
    for (const auto* ext : exts) {
        std::wstring p = base + ext;
        if (ExistsFile(p)) return p;
    }
    return base + L".png";
}
static std::wstring PeerAvatarCachePath(const std::wstring& host) {
    EnsureDir(ExeDir() + L"\\data");
    EnsureDir(ExeDir() + L"\\data\\avatars");
    return ExeDir() + L"\\data\\avatars\\peer-" + Slugify(host) + L".png";
}
static void EnsurePeerAvatarCached(const std::wstring& host, const std::wstring& ip, int port) {
    std::wstring dst = PeerAvatarCachePath(host);
    if (ExistsFile(dst)) { g_peerAvatarPath[host] = dst; return; }
    std::string bytes = HttpGetRaw(W2A(ip), port, "/avatar", 2000);
    if (bytes.empty()) return;
    std::ofstream out(dst.c_str(), std::ios::binary);
    if (!out) return;
    out.write(bytes.data(), (std::streamsize)bytes.size());
    out.close();
    if (ExistsFile(dst)) g_peerAvatarPath[host] = dst;
}

static void AppendChatLine(const std::wstring& from, const std::wstring& msg) {
    AppendChatLineStyled(from, msg, nullptr);
}

static void AppendChatLineStyled(const std::wstring& from, const std::wstring& msg, const CHARFORMAT2W* fmt) {
    if (!g_chatView) return;
    int countBefore = (int)SendMessageW(g_chatView, LB_GETCOUNT, 0, 0);
    int topBefore = (int)SendMessageW(g_chatView, LB_GETTOPINDEX, 0, 0);
    int itemH = (int)SendMessageW(g_chatView, LB_GETITEMHEIGHT, 0, 0);
    if (itemH <= 0) itemH = 28;
    RECT vr{}; GetClientRect(g_chatView, &vr);
    int page = max(1, (vr.bottom - vr.top) / itemH);
    bool stickBottom = (countBefore <= page) || (topBefore >= max(0, countBefore - page - 1));

    ChatRenderItem it{};
    it.from = from;
    it.msg = msg;
    if (fmt) {
        it.bold = (fmt->dwEffects & CFE_BOLD) != 0;
        it.italic = (fmt->dwEffects & CFE_ITALIC) != 0;
        it.color = fmt->crTextColor;
    }
    g_chatItems.push_back(it);
    SendMessageW(g_chatView, LB_ADDSTRING, 0, (LPARAM)L"");
    int count = (int)SendMessageW(g_chatView, LB_GETCOUNT, 0, 0);
    if (stickBottom && count > 0) {
        SendMessageW(g_chatView, LB_SETTOPINDEX, max(0, count - page), 0);
    }
    SyncChatScrollbar();
}
static bool IsLocalIp(const std::wstring& ip) {
    for (const auto& l : g_localIps) {
        if (_wcsicmp(l.c_str(), ip.c_str()) == 0) return true;
    }
    return false;
}
static bool ShouldAcceptChatPacket(const std::wstring& packetKey) {
    if (packetKey.empty()) return true;
    ULONGLONG now = AppTickCount64();
    std::lock_guard<std::mutex> lk(g_chatDedupMutex);
    for (auto it = g_recentChatIds.begin(); it != g_recentChatIds.end();) {
        if (now - it->second > 15000) it = g_recentChatIds.erase(it);
        else ++it;
    }
    auto f = g_recentChatIds.find(packetKey);
    if (f != g_recentChatIds.end()) return false;
    g_recentChatIds[packetKey] = now;
    return true;
}

static std::wstring EffectiveMyPresence() {
    if (_wcsicmp(g_myPresence.c_str(), L"Invisible") == 0) return L"Invisible";
    std::wstring playing = CurrentPlayingGame();
    if (!playing.empty()) return L"Playing " + playing;
    if (_wcsicmp(g_myPresence.c_str(), L"Busy") == 0) return L"Busy";
    ULONGLONG now = AppTickCount64();
    ULONGLONG last = g_lastInteractionTick.load();
    if (last == 0) return L"Online";
    if (now > last && (now - last) >= 120000) return L"Idle";
    return L"Online";
}

static std::wstring BaseNameLower(const std::wstring& p) {
    size_t pos = p.find_last_of(L"\\/");
    std::wstring b = (pos == std::wstring::npos) ? p : p.substr(pos + 1);
    std::transform(b.begin(), b.end(), b.begin(), towlower);
    return b;
}

static std::wstring CurrentPlayingGame() {
    std::lock_guard<std::mutex> lk(g_playingMutex);
    return g_nowPlayingGame;
}

static void SetCurrentPlayingGame(const std::wstring& game) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(g_playingMutex);
        if (_wcsicmp(g_nowPlayingGame.c_str(), game.c_str()) != 0) {
            g_nowPlayingGame = game;
            changed = true;
        }
    }
    if (changed && g_main) PostMessageW(g_main, WM_CHAT_PEERS_UPDATE, 0, 0);
}

static void PlayingDetectThread() {
    while (g_running) {
        std::unordered_map<std::wstring, std::wstring> exeToGame;
        {
            std::lock_guard<std::mutex> lk(g_gamesMutex);
            for (const auto& g : g_games) {
                if (!g.local || g.launchExe.empty()) continue;
                exeToGame[BaseNameLower(g.launchExe)] = NormalizeDisplayGameName(g.name);
            }
        }
        std::wstring foundGame;
        if (!exeToGame.empty()) {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe{};
                pe.dwSize = sizeof(pe);
                if (Process32FirstW(snap, &pe)) {
                    do {
                        std::wstring exe = pe.szExeFile ? pe.szExeFile : L"";
                        std::transform(exe.begin(), exe.end(), exe.begin(), towlower);
                        auto it = exeToGame.find(exe);
                        if (it != exeToGame.end()) {
                            foundGame = it->second;
                            break;
                        }
                    } while (Process32NextW(snap, &pe));
                }
                CloseHandle(snap);
            }
        }
        SetCurrentPlayingGame(foundGame);
        for (int i = 0; i < 20 && g_running; ++i) Sleep(100);
    }
}

static void MarkUserInteraction() {
    g_lastInteractionTick = AppTickCount64();
}

static void RefreshChatPeersUI() {
    if (!g_chatPeers) return;
    SendMessageW(g_chatPeers, LB_RESETCONTENT, 0, 0);
    if (g_myNick.empty()) g_myNick = g_hostName.empty() ? L"You" : g_hostName;
    std::wstring myEff = EffectiveMyPresence();
    for (const auto& p : g_peers) {
        std::wstring nick = g_peerNick.count(p.first) ? g_peerNick[p.first] : p.first;
        std::wstring pres = L"Online";
        auto itp = g_peerPresence.find(p.first);
        if (itp != g_peerPresence.end() && !itp->second.empty()) pres = itp->second;
        if (_wcsicmp(pres.c_str(), L"Invisible") == 0) pres = L"Offline";
        std::wstring line = nick;
        SendMessageW(g_chatPeers, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
    if (g_peers.empty()) {
        SendMessageW(g_chatPeers, LB_ADDSTRING, 0, (LPARAM)L"No peers online");
    }
    if (g_chatSelfName) SetWindowTextW(g_chatSelfName, g_myNick.c_str());
    if (g_chatSelfStatus) SetWindowTextW(g_chatSelfStatus, myEff.c_str());
    if (g_chatSelfAvatar) InvalidateRect(g_chatSelfAvatar, nullptr, TRUE);
    if (g_chatSelfName) InvalidateRect(g_chatSelfName, nullptr, TRUE);
    if (g_chatSelfStatus) InvalidateRect(g_chatSelfStatus, nullptr, TRUE);
    InvalidateRect(g_chatPeers, nullptr, TRUE);
    SyncPeersScrollbar();
}

static void SyncPeersScrollbar() {
    if (!g_chatPeers || !g_chatPeersScroll) return;
    int count = (int)SendMessageW(g_chatPeers, LB_GETCOUNT, 0, 0);
    int top = (int)SendMessageW(g_chatPeers, LB_GETTOPINDEX, 0, 0);
    int itemH = (int)SendMessageW(g_chatPeers, LB_GETITEMHEIGHT, 0, 0);
    if (itemH <= 0) itemH = 34;
    RECT lr{}; GetClientRect(g_chatPeers, &lr);
    int page = (lr.bottom - lr.top) / itemH;
    if (page < 1) page = 1;
    RECT sr{}; GetClientRect(g_chatPeersScroll, &sr);
    int trackH = (sr.bottom - sr.top) - 4;
    if (trackH < 20) trackH = 20;
    if (count <= page) {
        ShowWindow(g_chatPeersScroll, SW_HIDE);
        g_peersThumbRect = {2, 2, sr.right - 2, sr.bottom - 2};
    } else {
        ShowWindow(g_chatPeersScroll, SW_SHOW);
        int thumbH = max(16, trackH * page / count);
        int maxTop = max(1, count - page);
        int y = 2 + (trackH - thumbH) * top / maxTop;
        g_peersThumbRect = {2, y, sr.right - 2, y + thumbH};
    }
    InvalidateRect(g_chatPeersScroll, nullptr, TRUE);
}

static void SyncChatScrollbar() {
    if (!g_chatView || !g_chatViewScroll) return;
    int lines = (int)SendMessageW(g_chatView, LB_GETCOUNT, 0, 0);
    int first = (int)SendMessageW(g_chatView, LB_GETTOPINDEX, 0, 0);
    int lineH = (int)SendMessageW(g_chatView, LB_GETITEMHEIGHT, 0, 0);
    if (lineH <= 0) lineH = 28;
    RECT vr{}; GetClientRect(g_chatView, &vr);
    int page = max(1, (vr.bottom - vr.top) / lineH);
    RECT sr{}; GetClientRect(g_chatViewScroll, &sr);
    int trackH = (sr.bottom - sr.top) - 4;
    if (trackH < 20) trackH = 20;
    if (lines <= page) {
        ShowWindow(g_chatViewScroll, SW_HIDE);
        g_chatThumbRect = {2, 2, sr.right - 2, sr.bottom - 2};
    } else {
        ShowWindow(g_chatViewScroll, SW_SHOW);
        int thumbH = max(16, trackH * page / lines);
        int maxTop = max(1, lines - page);
        int y = 2 + (trackH - thumbH) * first / maxTop;
        g_chatThumbRect = {2, y, sr.right - 2, y + thumbH};
    }
    InvalidateRect(g_chatViewScroll, nullptr, TRUE);
}

static LRESULT CALLBACK ChatViewScrollProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_LBUTTONDOWN: {
            SetFocus(g_chatView); SetCapture(h);
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            if (PtInRect(&g_chatThumbRect, pt)) {
                g_dragChatThumb = true;
                g_dragChatStartY = pt.y;
                g_dragChatStartTop = (int)SendMessageW(g_chatView, LB_GETTOPINDEX, 0, 0);
            } else {
                int count = (int)SendMessageW(g_chatView, LB_GETCOUNT, 0, 0);
                int itemH = (int)SendMessageW(g_chatView, LB_GETITEMHEIGHT, 0, 0);
                RECT lr{}; GetClientRect(g_chatView, &lr);
                int page = max(1, (lr.bottom - lr.top) / max(1, itemH));
                int top = (int)SendMessageW(g_chatView, LB_GETTOPINDEX, 0, 0);
                if (pt.y < g_chatThumbRect.top) top -= page; else top += page;
                if (top < 0) top = 0;
                if (top > count - page) top = max(0, count - page);
                SendMessageW(g_chatView, LB_SETTOPINDEX, top, 0);
                SyncChatScrollbar();
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g_dragChatThumb) {
                POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                RECT sr{}; GetClientRect(h, &sr);
                int trackH = (sr.bottom - sr.top) - 4;
                int thumbH = g_chatThumbRect.bottom - g_chatThumbRect.top;
                int lines = (int)SendMessageW(g_chatView, LB_GETCOUNT, 0, 0);
                int itemH = (int)SendMessageW(g_chatView, LB_GETITEMHEIGHT, 0, 0);
                RECT vr{}; GetClientRect(g_chatView, &vr);
                int page = max(1, (vr.bottom - vr.top) / max(1, itemH));
                int maxTop = max(0, lines - page);
                int maxMove = max(1, trackH - thumbH);
                int dy = pt.y - g_dragChatStartY;
                int top = g_dragChatStartTop + (dy * maxTop) / maxMove;
                if (top < 0) top = 0; if (top > maxTop) top = maxTop;
                SendMessageW(g_chatView, LB_SETTOPINDEX, top, 0);
                SyncChatScrollbar();
            }
            return 0;
        case WM_LBUTTONUP:
            g_dragChatThumb = false; ReleaseCapture(); return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC hdc = BeginPaint(h, &ps);
            RECT rc{}; GetClientRect(h, &rc);
            HBRUSH tr = CreateSolidBrush(RGB(20,31,44)); FillRect(hdc, &rc, tr); DeleteObject(tr);
            HBRUSH th = CreateSolidBrush(RGB(57,93,126)); FillRect(hdc, &g_chatThumbRect, th); DeleteObject(th);
            EndPaint(h, &ps); return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK ChatPeersScrollProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_LBUTTONDOWN: {
            SetFocus(g_chatPeers);
            SetCapture(h);
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            if (PtInRect(&g_peersThumbRect, pt)) {
                g_dragPeersThumb = true;
                g_dragPeersStartY = pt.y;
                g_dragPeersStartTop = (int)SendMessageW(g_chatPeers, LB_GETTOPINDEX, 0, 0);
            } else {
                int count = (int)SendMessageW(g_chatPeers, LB_GETCOUNT, 0, 0);
                int itemH = (int)SendMessageW(g_chatPeers, LB_GETITEMHEIGHT, 0, 0);
                RECT lr{}; GetClientRect(g_chatPeers, &lr);
                int page = max(1, (lr.bottom - lr.top) / max(1, itemH));
                int top = (int)SendMessageW(g_chatPeers, LB_GETTOPINDEX, 0, 0);
                if (pt.y < g_peersThumbRect.top) top -= page; else top += page;
                if (top < 0) top = 0;
                if (top > count - page) top = max(0, count - page);
                SendMessageW(g_chatPeers, LB_SETTOPINDEX, top, 0);
                SyncPeersScrollbar();
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g_dragPeersThumb) {
                POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                RECT sr{}; GetClientRect(h, &sr);
                int trackH = (sr.bottom - sr.top) - 4;
                int thumbH = g_peersThumbRect.bottom - g_peersThumbRect.top;
                int count = (int)SendMessageW(g_chatPeers, LB_GETCOUNT, 0, 0);
                int itemH = (int)SendMessageW(g_chatPeers, LB_GETITEMHEIGHT, 0, 0);
                RECT lr{}; GetClientRect(g_chatPeers, &lr);
                int page = max(1, (lr.bottom - lr.top) / max(1, itemH));
                int maxTop = max(0, count - page);
                int maxMove = max(1, trackH - thumbH);
                int dy = pt.y - g_dragPeersStartY;
                int top = g_dragPeersStartTop + (dy * maxTop) / maxMove;
                if (top < 0) top = 0;
                if (top > maxTop) top = maxTop;
                SendMessageW(g_chatPeers, LB_SETTOPINDEX, top, 0);
                SyncPeersScrollbar();
            }
            return 0;
        case WM_LBUTTONUP:
            g_dragPeersThumb = false;
            ReleaseCapture();
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC hdc = BeginPaint(h, &ps);
            RECT rc{}; GetClientRect(h, &rc);
            HBRUSH tr = CreateSolidBrush(RGB(20,31,44)); FillRect(hdc, &rc, tr); DeleteObject(tr);
            HBRUSH th = CreateSolidBrush(RGB(57,93,126)); FillRect(hdc, &g_peersThumbRect, th); DeleteObject(th);
            EndPaint(h, &ps);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}
static std::wstring JsonFindFirstName(const std::wstring& json) {
    std::wregex re(L"\"name\"\\s*:\\s*\"([^\"]+)\"");
    std::wsmatch m;
    if (std::regex_search(json, m, re)) return m[1].str();
    return L"";
}
static std::wstring NormalizeDisplayGameName(const std::wstring& in) {
    std::wstring out = in;
    // Normalize common separator artifacts from folder/API names.
    std::replace(out.begin(), out.end(), L'_', L' ');
    out = std::regex_replace(out, std::wregex(L"\\s*-\\s*"), L" - ");
    out = std::regex_replace(out, std::wregex(L"\\s+"), L" ");
    // Normalize Command & Conquer naming.
    out = std::regex_replace(out, std::wregex(L"\\bCommand\\s+Conquer\\b", std::regex_constants::icase), L"Command & Conquer");
    // Trim
    while (!out.empty() && iswspace(out.front())) out.erase(out.begin());
    while (!out.empty() && iswspace(out.back())) out.pop_back();
    return out;
}
static std::wstring GetAliasForGame(const std::wstring& gameName) {
    std::wstring key = Slugify(gameName);
    for (const auto& p : g_nameAliases) {
        if (_wcsicmp(p.first.c_str(), key.c_str()) == 0) return NormalizeDisplayGameName(p.second);
    }
    return L"";
}
static void SetAliasForGame(const std::wstring& gameName, const std::wstring& alias) {
    if (alias.empty()) return;
    std::wstring normalized = NormalizeDisplayGameName(alias);
    std::wstring key = Slugify(gameName);
    for (auto& p : g_nameAliases) {
        if (_wcsicmp(p.first.c_str(), key.c_str()) == 0) { p.second = normalized; return; }
    }
    g_nameAliases.push_back({key, normalized});
}
static void DownloadSteamGridForAll() {
    std::vector<std::wstring> names;
    {
        std::lock_guard<std::mutex> lk(g_gamesMutex);
        for (const auto& g : g_games) names.push_back(g.name);
        for (const auto& g : g_remoteGames) names.push_back(g.name);
    }
    std::sort(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    names.erase(std::unique(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }), names.end());
    if (names.empty()) { SetStatus(L"Status: no games for asset download"); return; }
    int ok = 0;
    for (size_t i = 0; i < names.size(); ++i) {
        std::wstringstream st;
        st << L"Status: assets " << (i + 1) << L"/" << names.size() << L"...";
        SetStatus(st.str().c_str());
        if (DownloadSteamGridForGameName(names[i])) ok++;
    }
    ScanLocalGames();
    PostMessageW(g_main, WM_ASSETS_UPDATE, 0, 0);
    std::wstringstream done;
    done << L"Status: assets complete (" << ok << L"/" << names.size() << L")";
    SetStatus(done.str().c_str());
}

static void AutoDownloadAssetsForLocalGames() {
    std::vector<GameEntry> locals;
    {
        std::lock_guard<std::mutex> lk(g_gamesMutex);
        locals = g_games;
    }
    bool changed = false;
    for (const auto& g : locals) {
        if (!g.iconPath.empty() && !g.bannerPath.empty()) continue;
        if (DownloadSteamGridForGameName(g.name)) changed = true;
    }
    if (changed) {
        ScanLocalGames();
        PostMessageW(g_main, WM_ASSETS_UPDATE, 0, 0);
    }
}

static void CacheRemoteAssetsInBackground() {
    std::vector<GameEntry> remotes;
    {
        std::lock_guard<std::mutex> lk(g_gamesMutex);
        remotes = g_remoteGames;
    }
    bool changed = false;
    for (auto& e : remotes) {
        std::wstring icon = EnsureRemoteAssetCache(e, L"icon");
        std::wstring banner = EnsureRemoteAssetCache(e, L"banner");
        std::wstring logo = EnsureRemoteAssetCache(e, L"logo");

        // If local SteamGrid assets are missing, copy LAN assets into local assets folder at startup/peer update.
        std::wstring slug = Slugify(e.name);
        std::wstring localDir = ExeDir() + L"\\data\\assets\\games\\" + slug;
        EnsureDir(ExeDir() + L"\\data");
        EnsureDir(ExeDir() + L"\\data\\assets");
        EnsureDir(ExeDir() + L"\\data\\assets\\games");
        EnsureDir(localDir);
        auto copyIfMissing = [&](const std::wstring& src, const wchar_t* kind) {
            if (src.empty() || !ExistsFile(src)) return;
            std::wstring dst = localDir + L"\\" + kind + L".png";
            if (!ExistsFile(dst)) CopyFileW(src.c_str(), dst.c_str(), TRUE);
        };
        copyIfMissing(icon, L"icon");
        copyIfMissing(banner, L"banner");
        copyIfMissing(logo, L"logo");

        std::wstring localIcon = localDir + L"\\icon.png";
        std::wstring localBanner = localDir + L"\\banner.png";
        std::wstring localLogo = localDir + L"\\logo.png";
        if (ExistsFile(localIcon)) icon = localIcon;
        if (ExistsFile(localBanner)) banner = localBanner;
        if (ExistsFile(localLogo)) logo = localLogo;

        std::lock_guard<std::mutex> lk(g_gamesMutex);
        for (auto& rg : g_remoteGames) {
            if (_wcsicmp(rg.name.c_str(), e.name.c_str()) == 0 && _wcsicmp(rg.remoteHost.c_str(), e.remoteHost.c_str()) == 0) {
                if (rg.iconPath != icon || rg.bannerPath != banner || rg.logoPath != logo) {
                    rg.iconPath = icon;
                    rg.bannerPath = banner;
                    rg.logoPath = logo;
                    changed = true;
                }
            }
        }
    }
    if (changed) PostMessageW(g_main, WM_ASSETS_UPDATE, 0, 0);
}

static std::wstring FindLaunchExeFast(const std::wstring& dir) {
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((dir + L"\\*.exe").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return L"";
    std::wstring best;
    ULONGLONG bestSz = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            if (sz > bestSz) { bestSz = sz; best = dir + L"\\" + fd.cFileName; }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return best;
}

static std::wstring FindLaunchExeDeep(const std::wstring& dir) {
    std::wstring best;
    ULONGLONG bestSz = 0;
    std::vector<std::wstring> stack{dir};
    while (!stack.empty()) {
        std::wstring cur = stack.back();
        stack.pop_back();
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW((cur + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring full = cur + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                stack.push_back(full);
            } else {
                size_t len = wcslen(fd.cFileName);
                if (len >= 4 && _wcsicmp(fd.cFileName + len - 4, L".exe") == 0) {
                    ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                    if (sz > bestSz) { bestSz = sz; best = full; }
                }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return best;
}

static void ScanLocalGames() {
    std::vector<GameEntry> locals;
    if (ExistsDir(g_cfg.sharedRoot)) {
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW((g_cfg.sharedRoot + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                    std::wstring gameDir = g_cfg.sharedRoot + L"\\" + fd.cFileName;
                    std::wstring exe = FindLaunchExeFast(gameDir);
                    if (!exe.empty()) {
                        GameEntry e;
                        e.name = fd.cFileName;
                        {
                            std::wstring alias = GetAliasForGame(e.name);
                            if (!alias.empty()) e.name = alias;
                        }
                        e.path = gameDir;
                        e.launchExe = exe;
                        std::wstring slug = Slugify(e.name);
                        std::wstring aroot = ExeDir() + L"\\data\\assets\\games\\" + slug;
                        std::wstring icon = aroot + L"\\icon.png";
                        std::wstring banner = aroot + L"\\banner.png";
                        std::wstring logo = aroot + L"\\logo.png";
                        e.iconPath = ExistsFile(icon) ? icon : L"";
                        e.bannerPath = ExistsFile(banner) ? banner : L"";
                        e.logoPath = ExistsFile(logo) ? logo : L"";
                        e.local = true;
                        locals.push_back(e);
                    }
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    for (const auto& manual : g_manualGameFolders) {
        if (!ExistsDir(manual)) continue;
        std::wstring exe = FindLaunchExeFast(manual);
        if (exe.empty()) exe = FindLaunchExeDeep(manual);
        GameEntry e;
        size_t pos = manual.find_last_of(L"\\/");
        e.name = pos == std::wstring::npos ? manual : manual.substr(pos + 1);
        {
            std::wstring alias = GetAliasForGame(e.name);
            if (!alias.empty()) e.name = alias;
        }
        e.path = manual;
        e.launchExe = exe;
        std::wstring slug = Slugify(e.name);
        std::wstring aroot = ExeDir() + L"\\data\\assets\\games\\" + slug;
        std::wstring icon = aroot + L"\\icon.png";
        std::wstring banner = aroot + L"\\banner.png";
        std::wstring logo = aroot + L"\\logo.png";
        e.iconPath = ExistsFile(icon) ? icon : L"";
        e.bannerPath = ExistsFile(banner) ? banner : L"";
        e.logoPath = ExistsFile(logo) ? logo : L"";
        e.local = true;
        locals.push_back(e);
    }

    // Keep one entry per path (manual add should be exactly one visible entry).
    std::vector<GameEntry> unique;
    for (const auto& e : locals) {
        bool exists = false;
        for (const auto& u : unique) {
            if (_wcsicmp(u.path.c_str(), e.path.c_str()) == 0) { exists = true; break; }
        }
        if (!exists) unique.push_back(e);
    }

    std::sort(unique.begin(), unique.end(), [](const GameEntry& a, const GameEntry& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
    std::lock_guard<std::mutex> lk(g_gamesMutex);
    g_games = unique;
}

static std::wstring ExtractJsonString(const std::string& s, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    if (std::regex_search(s, m, re)) return A2W(m[1].str());
    return L"";
}

static std::vector<std::wstring> ExtractGameNames(const std::string& s) {
    std::vector<std::wstring> out;
    std::regex re("\"name\"\\s*:\\s*\"([^\"]+)\"");
    for (auto it = std::sregex_iterator(s.begin(), s.end(), re); it != std::sregex_iterator(); ++it) {
        std::wstring n = A2W((*it)[1].str());
        if (!n.empty()) out.push_back(n);
    }
    return out;
}

static int ExtractJsonInt(const std::string& s, const std::string& key, int fallback = 0) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch m;
    if (std::regex_search(s, m, re)) return atoi(m[1].str().c_str());
    return fallback;
}

static std::string HttpGetRaw(const std::string& host, int port, const std::string& path, int timeoutMs = 2500) {
    std::string out;
    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) return out;
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); return out; }
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    int c = connect(s, res->ai_addr, (int)res->ai_addrlen);
    if (c != 0) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(s, &wfds);
        timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 200000;
        int sel = select(0, nullptr, &wfds, nullptr, &tv);
        if (sel <= 0) { closesocket(s); freeaddrinfo(res); return out; }
        int soerr = 0;
        int slen = sizeof(soerr);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &slen);
        if (soerr != 0) { closesocket(s); freeaddrinfo(res); return out; }
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    DWORD to = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, sizeof(to));
    freeaddrinfo(res);
    std::ostringstream req;
    req << "GET " << path << " HTTP/1.1\r\nHost: " << host << "\r\nConnection: close\r\n\r\n";
    std::string r = req.str();
    send(s, r.c_str(), (int)r.size(), 0);
    char buf[4096];
    int n = 0;
    while ((n = recv(s, buf, sizeof(buf), 0)) > 0) out.append(buf, buf + n);
    closesocket(s);
    // Only treat HTTP 200 as success.
    if (!(out.rfind("HTTP/1.1 200", 0) == 0 || out.rfind("HTTP/1.0 200", 0) == 0)) return std::string();
    size_t p = out.find("\r\n\r\n");
    if (p != std::string::npos) return out.substr(p + 4);
    return out;
}

static bool HttpDownloadToFile(const std::string& host, int port, const std::string& path, const std::wstring& dst, int timeoutMs, unsigned long long* outBytes) {
    if (outBytes) *outBytes = 0;
    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) return false;
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); return false; }

    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    int c = connect(s, res->ai_addr, (int)res->ai_addrlen);
    if (c != 0) {
        fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
        timeval tv{}; tv.tv_sec = 2; tv.tv_usec = 0;
        int sel = select(0, nullptr, &wfds, nullptr, &tv);
        if (sel <= 0) { closesocket(s); freeaddrinfo(res); return false; }
        int soerr = 0; int slen = sizeof(soerr);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &slen);
        if (soerr != 0) { closesocket(s); freeaddrinfo(res); return false; }
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    DWORD to = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, sizeof(to));
    freeaddrinfo(res);

    std::ostringstream req;
    req << "GET " << path << " HTTP/1.1\r\nHost: " << host << "\r\nConnection: close\r\n\r\n";
    std::string rq = req.str();
    if (send(s, rq.c_str(), (int)rq.size(), 0) <= 0) { closesocket(s); return false; }

    std::wstring lp = ToLongPath(dst);
    std::wstring tmp = lp + L".part";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { closesocket(s); return false; }

    std::string header;
    bool headerDone = false;
    bool statusOk = false;
    long long contentLen = -1;
    unsigned long long written = 0;
    char buf[32 * 1024];
    bool ok = true;
    while (true) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) { ok = false; break; }
        if (!headerDone) {
            header.append(buf, buf + n);
            size_t p = header.find("\r\n\r\n");
            if (p == std::string::npos) {
                if (header.size() > 256 * 1024) { ok = false; break; }
                continue;
            }
            std::string htxt = header.substr(0, p + 4);
            statusOk = (htxt.rfind("HTTP/1.1 200", 0) == 0 || htxt.rfind("HTTP/1.0 200", 0) == 0);
            size_t cl = htxt.find("Content-Length:");
            if (cl != std::string::npos) {
                cl += 15;
                while (cl < htxt.size() && (htxt[cl] == ' ' || htxt[cl] == '\t')) cl++;
                size_t ce = cl;
                while (ce < htxt.size() && htxt[ce] >= '0' && htxt[ce] <= '9') ce++;
                if (ce > cl) contentLen = _strtoi64(htxt.substr(cl, ce - cl).c_str(), nullptr, 10);
            }
            if (!statusOk) { ok = false; break; }
            size_t bodyStart = p + 4;
            size_t bodySz = header.size() - bodyStart;
            if (bodySz > 0) {
                DWORD wr = 0;
                if (!WriteFile(h, header.data() + bodyStart, (DWORD)bodySz, &wr, nullptr) || wr != bodySz) { ok = false; break; }
                written += wr;
            }
            headerDone = true;
            header.clear();
        } else {
            DWORD wr = 0;
            if (!WriteFile(h, buf, (DWORD)n, &wr, nullptr) || wr != (DWORD)n) { ok = false; break; }
            written += wr;
        }
    }
    CloseHandle(h);
    closesocket(s);

    if (!ok) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (contentLen >= 0 && written != (unsigned long long)contentLen) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), lp.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    if (outBytes) *outBytes = written;
    return true;
}

static std::wstring EnsureRemoteAssetCache(const GameEntry& e, const wchar_t* kind) {
    std::wstring owner = Slugify(e.remoteHost.empty() ? L"peer" : e.remoteHost);
    std::wstring game = Slugify(e.name);
    std::wstring dir = ExeDir() + L"\\data\\lan_assets\\" + owner + L"\\" + game;
    EnsureDir(ExeDir() + L"\\data");
    EnsureDir(ExeDir() + L"\\data\\lan_assets");
    EnsureDir(ExeDir() + L"\\data\\lan_assets\\" + owner);
    EnsureDir(dir);
    std::wstring dst = dir + L"\\" + kind + L".png";
    if (ExistsFile(dst)) return dst;
    std::string host = W2A(e.remoteIp.empty() ? e.remoteHost : e.remoteIp);
    std::string gameEnc = W2A(UrlEncodeSimple(e.name));
    std::string kindStr = "banner";
    if (wcscmp(kind, L"icon") == 0) kindStr = "icon";
    else if (wcscmp(kind, L"logo") == 0) kindStr = "logo";
    std::string path = std::string("/asset?game=") + gameEnc + "&kind=" + kindStr;
    std::string bytes = HttpGetRaw(host, e.remotePort, path);
    if (bytes.empty()) return L"";
    if (!WriteAllBytes(dst, bytes)) return L"";
    return ExistsFile(dst) ? dst : L"";
}

static void RebuildMergedGames() {
    std::vector<GameEntry> locals;
    std::vector<GameEntry> remotes;
    {
        std::lock_guard<std::mutex> lk(g_gamesMutex);
        locals = g_games;
        remotes = g_remoteGames;
    }
    std::vector<GameEntry> merged = locals;
    for (auto& m : merged) m.name = NormalizeDisplayGameName(m.name);
    for (const auto& rg : remotes) {
        GameEntry rgNorm = rg;
        rgNorm.name = NormalizeDisplayGameName(rgNorm.name);
        bool existsLocal = false;
        for (const auto& lg : locals) {
            if (_wcsicmp(NormalizeDisplayGameName(lg.name).c_str(), rgNorm.name.c_str()) == 0) {
                existsLocal = true;
                break;
            }
        }
        if (!existsLocal) merged.push_back(rgNorm);
    }
    std::sort(merged.begin(), merged.end(), [](const GameEntry& a, const GameEntry& b) { return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });

    auto applyLocalAssetsBySlug = [&](GameEntry& m, const std::wstring& slug) {
        if (slug.empty()) return;
        std::wstring dir = ExeDir() + L"\\data\\assets\\games\\" + slug;
        std::wstring icon = dir + L"\\icon.png";
        std::wstring banner = dir + L"\\banner.png";
        std::wstring logo = dir + L"\\logo.png";
        if (m.iconPath.empty() && ExistsFile(icon)) m.iconPath = icon;
        if (m.bannerPath.empty() && ExistsFile(banner)) m.bannerPath = banner;
        if (m.logoPath.empty() && ExistsFile(logo)) m.logoPath = logo;
    };

    // Any entry (LOCAL or LAN) can use already-downloaded local assets by slug.
    for (auto& m : merged) {
        applyLocalAssetsBySlug(m, Slugify(m.name));
        if (!m.remoteGameName.empty()) applyLocalAssetsBySlug(m, Slugify(m.remoteGameName));
        if (m.iconPath.empty() || m.bannerPath.empty() || m.logoPath.empty()) {
            std::wstring norm = NormalizeDisplayGameName(m.name);
            if (_wcsicmp(norm.c_str(), m.name.c_str()) != 0) applyLocalAssetsBySlug(m, Slugify(norm));
        }
    }

    // Local entries can reuse LAN assets (same game) when local assets are missing.
    for (auto& m : merged) {
        if (!m.local) continue;
        if (!m.iconPath.empty() && !m.bannerPath.empty() && !m.logoPath.empty()) continue;
        for (const auto& r : remotes) {
            if (_wcsicmp(r.name.c_str(), m.name.c_str()) == 0) {
                GameEntry tmp = r;
                if (m.iconPath.empty()) m.iconPath = EnsureRemoteAssetCache(tmp, L"icon");
                if (m.bannerPath.empty()) m.bannerPath = EnsureRemoteAssetCache(tmp, L"banner");
                if (m.logoPath.empty()) m.logoPath = EnsureRemoteAssetCache(tmp, L"logo");
                break;
            }
        }
    }
    std::wstring q = CurrentSearchText();
    std::vector<GameEntry> filtered;
    for (const auto& e : merged) {
        GameEntry e2 = e;
        e2.name = NormalizeDisplayGameName(e2.name);
        std::wstring k = EntryHideKey(e);
        bool hidden = false;
        for (const auto& h : g_hiddenEntries) {
            if (_wcsicmp(h.c_str(), k.c_str()) == 0) { hidden = true; break; }
        }
        if (hidden) continue;
        if (!q.empty()) {
            std::wstring n = e2.name;
            std::transform(n.begin(), n.end(), n.begin(), towlower);
            if (n.find(q) == std::wstring::npos) continue;
        }
        filtered.push_back(e2);
    }
    g_uiGames = filtered;
}

static void RefreshList() {
    int prevSel = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    std::wstring prevName;
    std::wstring prevPath;
    if (prevSel >= 0 && prevSel < (int)g_uiGames.size()) prevName = g_uiGames[prevSel].name;
    if (prevSel >= 0 && prevSel < (int)g_uiGames.size()) prevPath = g_uiGames[prevSel].path;
    if (prevName.empty()) prevName = g_lastSelectedGame;
    if (prevPath.empty()) prevPath = g_lastSelectedPath;
    RebuildMergedGames();
    SendMessageW(g_list, LB_RESETCONTENT, 0, 0);
    for (const auto& g : g_uiGames) SendMessageW(g_list, LB_ADDSTRING, 0, (LPARAM)g.name.c_str());
    if (!g_uiGames.empty()) {
        int sel = -1;
        if (!prevPath.empty()) {
            for (int i = 0; i < (int)g_uiGames.size(); ++i) {
                if (_wcsicmp(g_uiGames[i].path.c_str(), prevPath.c_str()) == 0) { sel = i; break; }
            }
        }
        if (!prevName.empty()) {
            if (sel < 0) {
                for (int i = 0; i < (int)g_uiGames.size(); ++i) {
                    if (_wcsicmp(g_uiGames[i].name.c_str(), prevName.c_str()) == 0) { sel = i; break; }
                }
            }
            if (sel < 0) {
                std::wstring prevSlug = Slugify(prevName);
                for (int i = 0; i < (int)g_uiGames.size(); ++i) {
                    if (Slugify(g_uiGames[i].name) == prevSlug) { sel = i; break; }
                }
            }
        }
        if (sel >= 0) SendMessageW(g_list, LB_SETCURSEL, sel, 0);
        else SendMessageW(g_list, LB_SETCURSEL, 0, 0);
    }
    PreloadVisibleBannerImages();
    SyncListScrollbar();
}

static void UpdateSelectionUI() {
    int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= (int)g_uiGames.size()) { SetWindowTextW(g_playGet, L""); SetWindowTextW(g_details, L""); return; }
    const auto& e = g_uiGames[idx];
    if (_wcsicmp(g_lastSelectedGame.c_str(), e.name.c_str()) != 0 || _wcsicmp(g_lastSelectedPath.c_str(), e.path.c_str()) != 0) {
        g_lastSelectedGame = e.name;
        g_lastSelectedPath = e.path;
        SaveConfig();
    }
    SetWindowTextW(g_playGet, e.local ? (ExistsFile(e.launchExe) ? L"PLAY" : L"SET EXE") : L"GET");
    std::wstring details = L"Game: " + e.name + L"\r\nSource: " + (e.local ? std::wstring(L"LOCAL") : std::wstring(L"LAN")) + L"\r\nPath: " + e.path + L"\r\nLaunch EXE: " + e.launchExe;
    SetWindowTextW(g_details, details.c_str());
    InvalidateBannerOnly();
    SyncListScrollbar();
}
static void SaveCurrentSelectionState() {
    if (!g_list) return;
    int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= (int)g_uiGames.size()) return;
    const auto& e = g_uiGames[idx];
    g_lastSelectedGame = e.name;
    g_lastSelectedPath = e.path;
}

static std::string BuildManifestJson() {
    std::lock_guard<std::mutex> lk(g_gamesMutex);
    std::ostringstream o;
    o << "{\"app\":\"LAN Games Deployer C++\",\"games\":[";
    for (size_t i = 0; i < g_games.size(); ++i) {
        if (i) o << ",";
        o << "{\"name\":\"" << W2A(g_games[i].name) << "\",\"has_icon\":" << (g_games[i].iconPath.empty() ? "false" : "true")
          << ",\"has_banner\":" << (g_games[i].bannerPath.empty() ? "false" : "true")
          << ",\"has_logo\":" << (g_games[i].logoPath.empty() ? "false" : "true") << "}";
    }
    o << "]}";
    return o.str();
}

static void HttpServerThread() {
    SOCKET s = INVALID_SOCKET;
    int boundPort = 0;
    for (int p = HTTP_PORT; p <= HTTP_PORT_MAX; ++p) {
        SOCKET t = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (t == INVALID_SOCKET) continue;
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((u_short)p); a.sin_addr.s_addr = INADDR_ANY;
        if (bind(t, (sockaddr*)&a, sizeof(a)) == 0) {
            s = t;
            boundPort = p;
            break;
        }
        closesocket(t);
    }
    if (s == INVALID_SOCKET) {
        g_httpPort = 0;
        return;
    }
    g_httpPort = boundPort;
    listen(s, 8);
    while (g_running) {
        SOCKET c = accept(s, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        std::string r;
        {
            char req[8192]{};
            int n = 0;
            while ((n = recv(c, req, sizeof(req), 0)) > 0) {
                r.append(req, req + n);
                if (r.find("\r\n\r\n") != std::string::npos) break;
                if (r.size() >= 65536) break;
            }
        }
        if (!r.empty()) {
            std::string body;
            if (r.find("GET /manifest") == 0) {
                body = BuildManifestJson();
                std::ostringstream resp;
                resp << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
                auto sresp = resp.str();
                send(c, sresp.c_str(), (int)sresp.size(), 0);
            } else if (r.find("GET /filelist/") == 0) {
                size_t p1 = r.find("GET /filelist/") + 14;
                size_t p2 = r.find(" HTTP/");
                std::string enc = r.substr(p1, p2 - p1);
                std::wstring game = UrlDecodeSimple(A2W(enc));
                std::wstring root;
                {
                    std::lock_guard<std::mutex> lk(g_gamesMutex);
                    for (const auto& g : g_games) if (_wcsicmp(g.name.c_str(), game.c_str()) == 0) { root = g.path; break; }
                }
                if (root.empty() || !ExistsDir(root)) {
                    const char* nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(c, nf, (int)strlen(nf), 0);
                } else {
                    std::ostringstream j;
                    j << "{\"game\":\"" << enc << "\",\"files\":[";
                    ULONGLONG total = 0;
                    bool first = true;
                    std::vector<std::wstring> stack{root};
                    while (!stack.empty()) {
                        std::wstring cur = stack.back(); stack.pop_back();
                        WIN32_FIND_DATAW fd{};
                        HANDLE h = FindFirstFileW((cur + L"\\*").c_str(), &fd);
                        if (h == INVALID_HANDLE_VALUE) continue;
                        do {
                            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                            std::wstring full = cur + L"\\" + fd.cFileName;
                            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) stack.push_back(full);
                            else {
                                std::wstring rel = full.substr(root.size() + 1);
                                std::replace(rel.begin(), rel.end(), L'\\', L'/');
                                ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                                total += sz;
                                if (!first) j << ",";
                                first = false;
                                j << "{\"path\":\"" << W2A(rel) << "\",\"size\":" << sz << "}";
                            }
                        } while (FindNextFileW(h, &fd));
                        FindClose(h);
                    }
                    j << "],\"total_bytes\":" << total << "}";
                    body = j.str();
                    std::ostringstream resp;
                    resp << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n" << body;
                    auto sresp = resp.str();
                    send(c, sresp.c_str(), (int)sresp.size(), 0);
                }
            } else if (r.find("GET /asset?") == 0) {
                size_t p1 = r.find("/asset?") + 7;
                size_t p2 = r.find(" HTTP/");
                std::string qs = r.substr(p1, p2 - p1);
                std::string gameEnc, kind;
                for (const auto& part : {std::string("game="), std::string("kind=")}) {
                    size_t pos = qs.find(part);
                    if (pos != std::string::npos) {
                        size_t end = qs.find('&', pos);
                        std::string val = qs.substr(pos + part.size(), (end == std::string::npos ? qs.size() : end) - (pos + part.size()));
                        if (part == "game=") gameEnc = val; else kind = val;
                    }
                }
                std::wstring game = UrlDecodeSimple(A2W(gameEnc));
                std::wstring file;
                {
                    std::lock_guard<std::mutex> lk(g_gamesMutex);
                    for (const auto& g : g_games) if (_wcsicmp(g.name.c_str(), game.c_str()) == 0) {
                        if (kind == "icon") file = g.iconPath;
                        else if (kind == "logo") file = g.logoPath;
                        else file = g.bannerPath;
                        break;
                    }
                }
                if (!file.empty() && ExistsFile(file)) {
                    std::string bytes;
                    if (!ReadAllBytes(file, bytes)) bytes.clear();
                    std::ostringstream resp;
                    resp << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " << bytes.size() << "\r\nConnection: close\r\n\r\n";
                    auto head = resp.str();
                    send(c, head.c_str(), (int)head.size(), 0);
                    if (!bytes.empty()) send(c, bytes.data(), (int)bytes.size(), 0);
                } else {
                    const char* nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(c, nf, (int)strlen(nf), 0);
                }
            } else if (r.find("GET /avatar") == 0) {
                std::wstring av = ResolveSelfAvatarPath();
                if (ExistsFile(av)) {
                    std::string bytes;
                    if (!ReadAllBytes(av, bytes)) bytes.clear();
                    std::ostringstream resp;
                    resp << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " << bytes.size() << "\r\nConnection: close\r\n\r\n";
                    auto head = resp.str();
                    send(c, head.c_str(), (int)head.size(), 0);
                    if (!bytes.empty()) send(c, bytes.data(), (int)bytes.size(), 0);
                } else {
                    const char* nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(c, nf, (int)strlen(nf), 0);
                }
            } else if (r.find("GET /file?") == 0) {
                size_t p1 = r.find("/file?") + 6;
                size_t p2 = r.find(" HTTP/");
                std::string qs = r.substr(p1, p2 - p1);
                std::string gameEnc, pathEnc;
                size_t pg = qs.find("game="); if (pg != std::string::npos) { size_t e = qs.find('&', pg); gameEnc = qs.substr(pg + 5, (e == std::string::npos ? qs.size() : e) - (pg + 5)); }
                size_t pp = qs.find("path="); if (pp != std::string::npos) { size_t e = qs.find('&', pp); pathEnc = qs.substr(pp + 5, (e == std::string::npos ? qs.size() : e) - (pp + 5)); }
                std::wstring game = UrlDecodeSimple(A2W(gameEnc));
                std::wstring rel = UrlDecodeSimple(A2W(pathEnc));
                std::wstring root;
                {
                    std::lock_guard<std::mutex> lk(g_gamesMutex);
                    for (const auto& g : g_games) if (_wcsicmp(g.name.c_str(), game.c_str()) == 0) { root = g.path; break; }
                }
                std::wstring full = root.empty() ? L"" : (root + L"\\" + rel);
                std::replace(full.begin(), full.end(), L'/', L'\\');
                if (!full.empty() && ExistsFile(full)) {
                    std::string bytes;
                    if (!ReadAllBytes(full, bytes)) bytes.clear();
                    std::ostringstream resp;
                    resp << "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " << bytes.size() << "\r\nConnection: close\r\n\r\n";
                    auto head = resp.str();
                    send(c, head.c_str(), (int)head.size(), 0);
                    if (!bytes.empty()) send(c, bytes.data(), (int)bytes.size(), 0);
                } else {
                    const char* nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(c, nf, (int)strlen(nf), 0);
                }
            } else {
                const char* nf = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(c, nf, (int)strlen(nf), 0);
            }
        }
        closesocket(c);
    }
    closesocket(s);
}

static void LanBroadcastThread() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return;
    BOOL on = TRUE; setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof(on));
    sockaddr_in b{}; b.sin_family = AF_INET; b.sin_port = htons(DISCOVERY_PORT); b.sin_addr.s_addr = INADDR_BROADCAST;
    char host[256]{}; gethostname(host, sizeof(host)-1);

    while (g_running) {
        int hp = g_httpPort.load();
        if (hp <= 0) { Sleep(1500); continue; }
        std::wstring effPresence = EffectiveMyPresence();
        bool invisible = (_wcsicmp(effPresence.c_str(), L"Invisible") == 0);
        if (!g_lanShareEnabled.load() || invisible) {
            std::ostringstream off;
            off << "{\"magic\":\"LAN_GAMES_DEPLOYER_V1\",\"host\":\"" << host << "\",\"nick\":\"" << W2A(g_myNick)
                << "\",\"presence\":\"" << W2A(effPresence) << "\",\"http_port\":" << hp << ",\"games\":[]}";
            auto msgOff = off.str();
            sendto(s, msgOff.c_str(), (int)msgOff.size(), 0, (sockaddr*)&b, sizeof(b));
            Sleep(1500);
            continue;
        }
        std::vector<GameEntry> locals;
        {
            std::lock_guard<std::mutex> lk(g_gamesMutex);
            locals = g_games;
        }
        std::ostringstream o;
        o << "{\"magic\":\"LAN_GAMES_DEPLOYER_V1\",\"host\":\"" << host << "\",\"nick\":\"" << W2A(g_myNick)
          << "\",\"presence\":\"" << W2A(effPresence) << "\",\"http_port\":" << hp << ",\"games\":[";
        bool first = true;
        size_t added = 0;
        for (const auto& g : locals) {
            if (added >= 40) break; // compact fallback payload
            if (!first) o << ",";
            first = false;
            o << "{\"name\":\"" << W2A(g.name) << "\"}";
            added++;
        }
        o << "]}";
        auto msg = o.str();
        sendto(s, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&b, sizeof(b));
        Sleep(5000);
    }
    closesocket(s);
}

static void LanListenThread() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return;
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(DISCOVERY_PORT);
    a.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&a, sizeof(a)) != 0) { closesocket(s); return; }

    char buf[65535];
    while (g_running) {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        int n = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromLen);
        if (n <= 0) continue;
        buf[n] = 0;
        std::string msg(buf, n);
        if (msg.find("\"magic\":\"LAN_GAMES_DEPLOYER_V1\"") == std::string::npos) continue;
        std::wstring host = ExtractJsonString(msg, "host");
        std::wstring nick = ExtractJsonString(msg, "nick");
        std::wstring presence = ExtractJsonString(msg, "presence");
        if (!g_hostName.empty() && _wcsicmp(host.c_str(), g_hostName.c_str()) == 0) continue;

        int peerPort = ExtractJsonInt(msg, "http_port", HTTP_PORT);
        std::wstring peerIp = A2W(IPv4ToString(from.sin_addr));

        bool peerChanged = false;
        {
            bool found = false;
            for (auto& p : g_peers) {
                if (_wcsicmp(p.first.c_str(), host.c_str()) == 0) {
                    if (_wcsicmp(p.second.c_str(), peerIp.c_str()) != 0) { p.second = peerIp; peerChanged = true; }
                    found = true;
                    break;
                }
            }
            if (!found) { g_peers.push_back({host, peerIp}); peerChanged = true; }
            if (!nick.empty()) {
                auto old = g_peerNick[host];
                if (_wcsicmp(old.c_str(), nick.c_str()) != 0) { g_peerNick[host] = nick; peerChanged = true; }
            }
            if (!presence.empty()) {
                auto oldp = g_peerPresence[host];
                if (_wcsicmp(oldp.c_str(), presence.c_str()) != 0) { g_peerPresence[host] = presence; peerChanged = true; }
            }
        }
        if (peerChanged) {
            std::thread([host, peerIp, peerPort]() {
                EnsurePeerAvatarCached(host, peerIp, peerPort);
                PostMessageW(g_main, WM_CHAT_PEERS_UPDATE, 0, 0);
            }).detach();
        }

        std::vector<std::wstring> names = ExtractGameNames(msg); // immediate fallback path
        if (names.empty()) {
            bool changed = false;
            {
                std::lock_guard<std::mutex> lk(g_gamesMutex);
                std::vector<GameEntry> kept;
                for (const auto& e : g_remoteGames) {
                    if (_wcsicmp(e.remoteHost.c_str(), host.c_str()) != 0) kept.push_back(e);
                    else changed = true;
                }
                g_remoteGames.swap(kept);
            }
            if (changed) PostMessageW(g_main, WM_PEER_UPDATE, 0, 0);
            continue;
        }
        std::vector<GameEntry> incoming;
        for (const auto& nm : names) {
            GameEntry e;
            std::wstring alias = GetAliasForGame(nm);
            e.name = alias.empty() ? nm : alias;
            e.remoteGameName = nm;
            e.local = false;
            e.remoteHost = host;
            e.remoteIp = peerIp;
            e.remotePort = peerPort;
            incoming.push_back(e);
        }
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(g_gamesMutex);
            std::vector<GameEntry> previousHost;
            for (const auto& e : g_remoteGames) if (_wcsicmp(e.remoteHost.c_str(), host.c_str()) == 0) previousHost.push_back(e);
            std::vector<GameEntry> kept;
            for (const auto& e : g_remoteGames) {
                if (_wcsicmp(e.remoteHost.c_str(), host.c_str()) != 0) kept.push_back(e);
            }
            kept.insert(kept.end(), incoming.begin(), incoming.end());
            if (previousHost.size() != incoming.size()) changed = true;
            if (!changed) {
                for (size_t i = 0; i < incoming.size(); ++i) {
                    if (_wcsicmp(previousHost[i].name.c_str(), incoming[i].name.c_str()) != 0 ||
                        previousHost[i].remotePort != incoming[i].remotePort ||
                        _wcsicmp(previousHost[i].remoteIp.c_str(), incoming[i].remoteIp.c_str()) != 0) {
                        changed = true;
                        break;
                    }
                }
            }
            g_remoteGames.swap(kept);
        }
        if (changed) PostMessageW(g_main, WM_PEER_UPDATE, 0, 0);

        // Async enrich: fetch full manifest without blocking listener.
        std::thread([peerIp, peerPort, host]() {
            std::string manifest = HttpGetRaw(W2A(peerIp), peerPort, "/manifest");
            if (manifest.empty()) return;
            std::vector<std::wstring> fullNames = ExtractGameNames(manifest);
            if (fullNames.empty()) return;
            std::vector<GameEntry> fullIncoming;
            for (const auto& nm2 : fullNames) {
                GameEntry e2;
                std::wstring alias = GetAliasForGame(nm2);
                e2.name = alias.empty() ? nm2 : alias;
                e2.remoteGameName = nm2;
                e2.local = false;
                e2.remoteHost = host;
                e2.remoteIp = peerIp;
                e2.remotePort = peerPort;
                fullIncoming.push_back(e2);
            }
            bool localChanged = false;
            {
                std::lock_guard<std::mutex> lk(g_gamesMutex);
                std::vector<GameEntry> prev;
                for (const auto& e : g_remoteGames) if (_wcsicmp(e.remoteHost.c_str(), host.c_str()) == 0) prev.push_back(e);
                if (prev.size() != fullIncoming.size()) localChanged = true;
                std::vector<GameEntry> kept;
                for (const auto& e : g_remoteGames) {
                    if (_wcsicmp(e.remoteHost.c_str(), host.c_str()) != 0) kept.push_back(e);
                }
                kept.insert(kept.end(), fullIncoming.begin(), fullIncoming.end());
                g_remoteGames.swap(kept);
            }
            if (localChanged) PostMessageW(g_main, WM_PEER_UPDATE, 0, 0);
        }).detach();
    }
    closesocket(s);
}

static void SendChatToSelectedPeer() {
    if (!g_chatInput) return;
    int len = GetWindowTextLengthW(g_chatInput);
    if (len <= 0) return;
    std::wstring msg;
    msg.resize(len);
    GetWindowTextW(g_chatInput, &msg[0], len + 1);
    if (msg.empty()) return;
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_COLOR | CFM_FACE;
    cf.dwEffects = (g_chatBoldPick ? CFE_BOLD : 0) | (g_chatItalicPick ? CFE_ITALIC : 0);
    cf.crTextColor = g_chatColorPick;
    wcsncpy_s(cf.szFaceName, LF_FACESIZE, g_chatFontName.c_str(), _TRUNCATE);
    int r = GetRValue(cf.crTextColor), g = GetGValue(cf.crTextColor), b = GetBValue(cf.crTextColor);
    wchar_t colorHex[16]{};
    swprintf(colorHex, 16, L"%02X%02X%02X", r, g, b);
    std::wstring enc = UrlEncodeSimple(msg);
    ULONGLONG ts = AppTickCount64();
    DWORD pid = GetCurrentProcessId();
    unsigned long long seq = g_chatMsgSeq.fetch_add(1) + 1;
    std::wstring msgId = std::to_wstring(ts) + L"-" + std::to_wstring((unsigned long long)pid) + L"-" + std::to_wstring(seq);
    std::wstring displayName = g_myNick.empty() ? g_hostName : g_myNick;
    std::wstring pkt = L"LANCHAT|from=" + g_hostName + L"|nick=" + UrlEncodeSimple(displayName) + L"|msg=" + enc
        + L"|b=" + std::to_wstring((cf.dwEffects & CFE_BOLD) ? 1 : 0)
        + L"|i=" + std::to_wstring((cf.dwEffects & CFE_ITALIC) ? 1 : 0)
        + L"|c=" + colorHex
        + L"|f=" + UrlEncodeSimple(cf.szFaceName)
        + L"|sid=" + g_instanceId
        + L"|id=" + msgId;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return;
    BOOL on = TRUE; setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&on, sizeof(on));
    std::string out = W2A(pkt);
    // Some Windows editions/net profiles may drop LAN broadcast in one direction.
    // Send both broadcast and direct unicast to discovered peers for reliability.
    sockaddr_in bcast{}; bcast.sin_family = AF_INET; bcast.sin_port = htons(CHAT_PORT); bcast.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(s, out.c_str(), (int)out.size(), 0, (sockaddr*)&bcast, sizeof(bcast));
    {
        std::lock_guard<std::mutex> lk(g_gamesMutex);
        for (const auto& p : g_peers) {
            const std::wstring& ipW = p.second;
            if (ipW.empty()) continue;
            std::string ip = W2A(ipW);
            sockaddr_in u{}; u.sin_family = AF_INET; u.sin_port = htons(CHAT_PORT);
            if (ParseIPv4Address(ip, &u.sin_addr)) {
                sendto(s, out.c_str(), (int)out.size(), 0, (sockaddr*)&u, sizeof(u));
            }
        }
    }
    closesocket(s);
    std::wstring selfName = g_myNick.empty() ? g_hostName : g_myNick;
    if (selfName.empty()) selfName = L"You";
    AppendChatLineStyled(selfName, msg, &cf);
    SetWindowTextW(g_chatInput, L"");
}

static void ChatListenThread() {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return;
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(CHAT_PORT); a.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (sockaddr*)&a, sizeof(a)) != 0) { closesocket(s); return; }
    char buf[8192];
    while (g_running) {
        sockaddr_in from{}; int fromLen = sizeof(from);
        int n = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr*)&from, &fromLen);
        if (n <= 0) continue;
        buf[n] = 0;
        std::string m(buf, n);
        if (m.find("LANCHAT|") != 0) continue;
        auto getField = [&](const std::string& k) {
            size_t p = 0;
            while (p < m.size()) {
                size_t e = m.find('|', p);
                if (e == std::string::npos) e = m.size();
                std::string token = m.substr(p, e - p);
                size_t eq = token.find('=');
                if (eq != std::string::npos) {
                    std::string tk = token.substr(0, eq);
                    if (_stricmp(tk.c_str(), k.c_str()) == 0) {
                        return token.substr(eq + 1);
                    }
                }
                if (e >= m.size()) break;
                p = e + 1;
            }
            return std::string();
        };
        std::wstring fromHost = A2W(getField("from"));
        std::wstring fromNick = UrlDecodeSimple(A2W(getField("nick")));
        std::wstring msg = UrlDecodeSimple(A2W(getField("msg")));
        std::wstring sid = A2W(getField("sid"));
        std::wstring id = A2W(getField("id"));
        std::wstring b = A2W(getField("b"));
        std::wstring i = A2W(getField("i"));
        std::wstring c = A2W(getField("c"));
        std::wstring f = UrlDecodeSimple(A2W(getField("f")));
        std::wstring fromIp = A2W(IPv4ToString(from.sin_addr));
        if (!id.empty()) {
            std::wstring key = fromIp + L"|" + id;
            if (!ShouldAcceptChatPacket(key)) continue;
        } else {
            std::wstring key = fromIp + L"|" + fromHost + L"|" + msg + L"|" + b + L"|" + i + L"|" + c;
            if (!ShouldAcceptChatPacket(key)) continue;
        }
        bool isSelf = false;
        if (!sid.empty() && !g_instanceId.empty() && _wcsicmp(sid.c_str(), g_instanceId.c_str()) == 0) {
            isSelf = true;
        } else if (sid.empty() && !fromHost.empty() && _wcsicmp(fromHost.c_str(), g_hostName.c_str()) == 0 && IsLocalIp(fromIp)) {
            // Backward-compat for older clients that don't send sid.
            isSelf = true;
        }
        if (!fromHost.empty() && !isSelf && g_main) {
            ChatLinePayload* p = new ChatLinePayload();
            p->from = fromNick.empty() ? fromHost : fromNick;
            p->msg = msg;
            p->hasStyle = true;
            p->bold = (b == L"1");
            p->italic = (i == L"1");
            if (c.size() >= 6) {
                int rv = 0, gv = 0, bv = 0;
                swscanf_s(c.c_str(), L"%02x%02x%02x", &rv, &gv, &bv);
                p->color = RGB(rv, gv, bv);
            }
            p->face = f;
            PostMessageW(g_main, WM_CHAT_APPEND, (WPARAM)p, 0);
        }
    }
    closesocket(s);
}

static void RefreshStatusBarTextLocked() {
    if (!g_status) return;
    bool hasTransferLines = false;
    for (const auto& ln : g_transferStatusLines) {
        if (!ln.second.empty()) { hasTransferLines = true; break; }
    }
    std::wstring text;
    if (!hasTransferLines || _wcsicmp((g_baseStatus.empty() ? L"Status: Ready" : g_baseStatus.c_str()), L"Status: Ready") != 0) {
        text = g_baseStatus.empty() ? L"Status: Ready" : g_baseStatus;
    }
    for (const auto& ln : g_transferStatusLines) {
        if (ln.second.empty()) continue;
        if (!text.empty()) text += L"\r\n";
        text += ln.second;
    }
    SetWindowTextW(g_status, text.c_str());
}

static void ApplyStatusBarTextAndLayout() {
    size_t visibleLines = 1;
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        int transferLines = 0;
        for (const auto& it : g_transferStatusLines) if (!it.second.empty()) transferLines++;
        bool baseVisible = true;
        std::wstring base = g_baseStatus.empty() ? L"Status: Ready" : g_baseStatus;
        if (transferLines > 0 && _wcsicmp(base.c_str(), L"Status: Ready") == 0) baseVisible = false;
        int v = transferLines + (baseVisible ? 1 : 0);
        if (v < 1) v = 1;
        visibleLines = (size_t)v;
        RefreshStatusBarTextLocked();
    }
    if (g_main && visibleLines != g_lastStatusLineCount) {
        g_lastStatusLineCount = visibleLines;
        LayoutControls(g_main);
    }
}

static void SetStatus(const wchar_t* t) {
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        g_baseStatus = (t && *t) ? t : L"Status: Ready";
    }
    if (g_main) PostMessageW(g_main, WM_STATUSBAR_REFRESH, 0, 0);
}

static void SetTransferStatusLine(const std::wstring& key, const std::wstring& line) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        bool found = false;
        for (auto& it : g_transferStatusLines) {
            if (_wcsicmp(it.first.c_str(), key.c_str()) == 0) {
                if (it.second != line) {
                    it.second = line;
                    changed = true;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            g_transferStatusLines.push_back({ key, line });
            changed = true;
        }
    }
    if (changed && g_main) PostMessageW(g_main, WM_STATUSBAR_REFRESH, 0, 0);
}

static void RemoveTransferStatusLine(const std::wstring& key) {
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        g_transferStatusLines.erase(
            std::remove_if(g_transferStatusLines.begin(), g_transferStatusLines.end(),
                [&](const std::pair<std::wstring, std::wstring>& it) {
                    return _wcsicmp(it.first.c_str(), key.c_str()) == 0;
                }),
            g_transferStatusLines.end());
    }
    if (g_main) PostMessageW(g_main, WM_STATUSBAR_REFRESH, 0, 0);
}

static void InvalidateBannerOnly() {
    if (!g_main) return;
    RECT rc{};
    GetClientRect(g_main, &rc);
    RECT banner{342, 6 + TOP_BAR_H, rc.right - 8, 268 + TOP_BAR_H};
    RedrawWindow(g_main, &banner, nullptr, RDW_INVALIDATE);
}
static void InvalidateTopBarOnly() {
    if (!g_main) return;
    RECT rc{};
    GetClientRect(g_main, &rc);
    RECT top{0, 0, rc.right, TOP_BAR_H};
    RedrawWindow(g_main, &top, nullptr, RDW_INVALIDATE | RDW_NOERASE);
}

static void DownloadRemoteGame(const GameEntry& e, HWND owner, const std::wstring& base) {
    if (base.empty()) return;
    std::string host = W2A(e.remoteIp.empty() ? e.remoteHost : e.remoteIp);
    std::wstring remoteKey = e.remoteGameName.empty() ? e.name : e.remoteGameName;
    std::string gameEnc = W2A(UrlEncodeSimple(remoteKey));
    std::string listJson;
    for (int attempt = 0; attempt < 5; ++attempt) {
        listJson = HttpGetRaw(host, e.remotePort, "/filelist/" + gameEnc, 7000);
        if (!listJson.empty()) break;
        Sleep(150 * (attempt + 1));
    }
    if (listJson.empty()) { SetStatus(L"Status: GET failed (file list)"); return; }
    std::regex rePath("\"path\"\\s*:\\s*\"([^\"]+)\"");
    std::regex reFile("\"path\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*\"size\"\\s*:\\s*([0-9]+)");
    std::regex reTotal("\"total_bytes\"\\s*:\\s*([0-9]+)");
    std::vector<std::string> files;
    std::unordered_map<std::string, unsigned long long> fileSizes;
    for (auto it = std::sregex_iterator(listJson.begin(), listJson.end(), reFile); it != std::sregex_iterator(); ++it) {
        std::string p = (*it)[1].str();
        unsigned long long sz = _strtoui64((*it)[2].str().c_str(), nullptr, 10);
        files.push_back(p);
        fileSizes[p] = sz;
    }
    if (files.empty()) {
        for (auto it = std::sregex_iterator(listJson.begin(), listJson.end(), rePath); it != std::sregex_iterator(); ++it) files.push_back((*it)[1].str());
    }
    if (files.empty()) { SetStatus(L"Status: GET nothing to download"); return; }
    unsigned long long totalBytes = 0;
    std::smatch tm;
    if (std::regex_search(listJson, tm, reTotal)) totalBytes = _strtoui64(tm[1].str().c_str(), nullptr, 10);
    std::wstring gameDir = base + L"\\" + MakeSafeFsName(e.name);
    EnsureDir(gameDir);

    const int workerCount = 2;
    std::atomic<size_t> nextIndex{0};
    std::atomic<int> done{0};
    std::atomic<int> failed{0};
    std::atomic<int> okCount{0};
    std::atomic<unsigned long long> downloadedBytes{0};
    std::atomic<bool> abortAll{false};
    std::mutex firstFailMutex;
    std::wstring firstFailPath;
    auto t0 = std::chrono::steady_clock::now();
    const std::wstring transferKey = L"GET|" + e.name + L"|" + e.remoteHost + L"|" + e.path;
    SetTransferStatusLine(transferKey, L"Status: Copying " + e.name + L" 0/" + std::to_wstring((unsigned long long)files.size()) + L" files 0.0 mbps ETA --");

    auto worker = [&]() {
        while (true) {
            if (abortAll.load()) break;
            size_t i = nextIndex.fetch_add(1);
            if (i >= files.size()) break;
            if (abortAll.load()) break;
            const std::string& f = files[i];
            std::wstring rel = UrlDecodeSimple(A2W(f));
            std::wstring dst = gameDir + L"\\" + rel;
            std::replace(dst.begin(), dst.end(), L'/', L'\\');
            size_t cut = dst.find_last_of(L"\\/");
            if (cut != std::wstring::npos) EnsureDir(dst.substr(0, cut));
            std::string pathEnc = W2A(UrlEncodeSimple(rel));
            bool ok = false;
            unsigned long long gotBytes = 0;
            for (int attempt = 0; attempt < 3; ++attempt) {
                if (abortAll.load()) break;
                unsigned long long expected = 0;
                auto fsIt = fileSizes.find(f);
                if (fsIt != fileSizes.end()) expected = fsIt->second;
                gotBytes = 0;
                if (HttpDownloadToFile(host, e.remotePort, "/file?game=" + gameEnc + "&path=" + pathEnc, dst, 7000, &gotBytes)) {
                    const bool expectedKnown = (fsIt != fileSizes.end());
                    if (!expectedKnown || expected == gotBytes) {
                        downloadedBytes.fetch_add(gotBytes);
                        okCount.fetch_add(1);
                        ok = true;
                        break;
                    }
                }
                Sleep(150 * (attempt + 1));
            }
            if (!ok) {
                failed.fetch_add(1);
                abortAll.store(true);
                std::lock_guard<std::mutex> lk(firstFailMutex);
                if (firstFailPath.empty()) firstFailPath = rel;
            }
            int d = done.fetch_add(1) + 1;
            if ((d % 10) == 0 || d == (int)files.size()) {
                auto now = std::chrono::steady_clock::now();
                double secs = std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count() / 1000.0;
                if (secs <= 0.0) secs = 0.001;
                double mbps = (downloadedBytes.load() / (1024.0 * 1024.0)) / secs;
                unsigned long long leftBytes = 0;
                if (totalBytes > downloadedBytes.load()) leftBytes = totalBytes - downloadedBytes.load();
                int etaSec = (mbps > 0.01) ? (int)(leftBytes / (mbps * 1024.0 * 1024.0)) : -1;
                std::wstringstream ss;
                ss.setf(std::ios::fixed);
                ss.precision(1);
                ss << L"Status: Copying " << e.name << L" " << d << L"/" << files.size() << L" files " << mbps << L" mbps";
                if (etaSec >= 0) {
                    ss << L" ETA ";
                    if (etaSec >= 60) ss << (etaSec / 60) << L"m";
                    else ss << etaSec << L"s";
                } else {
                    ss << L" ETA --";
                }
                SetTransferStatusLine(transferKey, ss.str());
            }
        }
    };

    std::vector<std::thread> threads;
    int launchN = (int)files.size() < workerCount ? (int)files.size() : workerCount;
    for (int i = 0; i < launchN; ++i) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    bool completedAll = (failed.load() == 0 && done.load() == (int)files.size());
    if (okCount.load() == 0 && failed.load() > 0) {
        std::wstringstream ss;
        ss << L"Status: GET failed (0 files copied). First failed: ";
        ss << (firstFailPath.empty() ? L"(unknown)" : firstFailPath);
        SetStatus(ss.str().c_str());
    } else if (failed.load() > 0) {
        std::wstringstream ss; ss << L"Status: GET complete with errors (" << failed.load() << L" failed)";
        SetStatus(ss.str().c_str());
    } else {
        SetStatus(L"Status: GET complete");
    }
    RemoveTransferStatusLine(transferKey);
    if (!completedAll) {
        DeleteDirTree(gameDir);
        SetStatus((L"Status: GET failed/incomplete, removed partial folder: " + e.name).c_str());
        ScanLocalGames();
        PostMessageW(g_main, WM_PEER_UPDATE, 0, 0);
        return;
    }
    bool exists = std::find_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
        [&](const std::wstring& p) { return _wcsicmp(p.c_str(), gameDir.c_str()) == 0; }) != g_manualGameFolders.end();
    if (!exists) g_manualGameFolders.push_back(gameDir);
    // Preserve original LAN display name for the local downloaded folder name.
    size_t basePos = gameDir.find_last_of(L"\\/");
    std::wstring folderName = (basePos == std::wstring::npos) ? gameDir : gameDir.substr(basePos + 1);
    SetAliasForGame(folderName, e.name);
    SaveConfig();
    ScanLocalGames();
    PostMessageW(g_main, WM_PEER_UPDATE, 0, 0);
}

static void EnqueueRemoteGameDownload(const GameEntry& e, HWND owner) {
    std::wstring base = PickFolder(owner, L"Choose destination folder");
    if (base.empty()) return;
    size_t pos = 0;
    size_t qCountAfter = 0;
    bool hasActiveTransfer = false;
    {
        std::lock_guard<std::mutex> lk(g_downloadQueueMutex);
        g_downloadQueueTasks.push_back({ e, base });
        if (owner) g_downloadQueueOwner = owner;
        pos = g_downloadQueueTasks.size();
        qCountAfter = g_downloadQueueTasks.size();
    }
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        for (const auto& it : g_transferStatusLines) {
            if (!it.second.empty()) { hasActiveTransfer = true; break; }
        }
    }
    if (qCountAfter > 1 || pos > 1 || hasActiveTransfer) {
        std::wstringstream ss;
        ss << L"Status: queued " << e.name << L" (position " << pos << L")";
        SetStatus(ss.str().c_str());
    } else {
        // For a single queued download, avoid an extra queue-position status line.
        SetStatus(L"Status: Ready");
    }
    g_downloadQueueCv.notify_one();
}

static std::wstring PickFolder(HWND owner, const wchar_t* title) {
    BROWSEINFOW bi{};
    bi.hwndOwner = owner;
    bi.lpszTitle = title;
    // Keep compatibility with older Windows shells; avoid newer dialog-only flags.
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";
    wchar_t path[MAX_PATH]{};
    std::wstring out;
    if (SHGetPathFromIDListW(pidl, path)) out = path;
    CoTaskMemFree(pidl);
    return out;
}

static std::wstring PickFile(HWND owner, const wchar_t* title, const wchar_t* filter, const wchar_t* initialDir) {
    OPENFILENAMEW ofn{};
    wchar_t fileBuf[MAX_PATH]{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrInitialDir = initialDir;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return L"";
    return std::wstring(fileBuf);
}

static int PromptSteamGridId(HWND owner, int currentId) {
    const wchar_t* cls = L"SgdbIdPromptWnd";
    WNDCLASSW wc{};
    wc.lpfnWndProc = SgdbPromptProc;
    wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);
    RECT orc{}; GetWindowRect(owner, &orc);
    int w = 340, h = 150;
    int x = orc.left + ((orc.right - orc.left) - w) / 2;
    int y = orc.top + ((orc.bottom - orc.top) - h) / 2;
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, cls, L"Set SteamGridDB ID",
        WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        x, y, w, h, owner, nullptr, wc.hInstance, nullptr);
    if (!dlg) return -1;
    EnableWindow(owner, FALSE);
    HWND lbl = CreateWindowW(L"STATIC", L"SteamGridDB game ID:", WS_CHILD | WS_VISIBLE, 16, 16, 220, 18, dlg, nullptr, nullptr, nullptr);
    HWND edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, 42, 304, 24, dlg, (HMENU)1, nullptr, nullptr);
    HWND ok = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 160, 84, 76, 28, dlg, (HMENU)2, nullptr, nullptr);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 244, 84, 76, 28, dlg, (HMENU)3, nullptr, nullptr);
    SetControlFont(lbl); SetControlFont(edit); SetControlFont(ok, true); SetControlFont(cancel);
    if (currentId > 0) {
        wchar_t b[32]; _snwprintf_s(b, _TRUNCATE, L"%d", currentId);
        SetWindowTextW(edit, b);
    }
    SetFocus(edit);
    g_sgdbPromptResult = -1;
    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    int result = g_sgdbPromptResult;
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    return result;
}

static std::wstring PromptText(HWND owner, const wchar_t* title, const wchar_t* label, const std::wstring& current) {
    std::wstring out;
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    wc.lpszClassName = L"LanPromptTextWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_panelBrush ? g_panelBrush : (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&wc);
    RECT orc{}; GetWindowRect(owner, &orc);
    int w = 380, h = 160;
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, title, WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_VISIBLE,
        orc.left + 40, orc.top + 40, w, h, owner, nullptr, wc.hInstance, nullptr);
    if (!dlg) return out;
    EnableWindow(owner, FALSE);
    HWND lbl = CreateWindowW(L"STATIC", label, WS_CHILD | WS_VISIBLE, 14, 14, 340, 20, dlg, nullptr, nullptr, nullptr);
    HWND edit = CreateWindowW(L"EDIT", current.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 14, 40, 350, 24, dlg, (HMENU)1, nullptr, nullptr);
    HWND ok = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 206, 88, 74, 30, dlg, (HMENU)2, nullptr, nullptr);
    HWND cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 290, 88, 74, 30, dlg, (HMENU)3, nullptr, nullptr);
    SetControlFont(lbl); SetControlFont(edit); SetControlFont(ok, true); SetControlFont(cancel);
    SetFocus(edit);
    bool done = false, accepted = false;
    MSG msg{};
    while (!done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.hwnd == ok && msg.message == WM_LBUTTONUP) { accepted = true; done = true; }
        else if (msg.hwnd == cancel && msg.message == WM_LBUTTONUP) { done = true; }
        else if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { accepted = true; done = true; }
        else if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { done = true; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(dlg)) break;
    }
    if (accepted && IsWindow(edit)) {
        int len = GetWindowTextLengthW(edit);
        out.resize(len);
        if (len > 0) GetWindowTextW(edit, &out[0], len + 1);
    }
    if (IsWindow(dlg)) DestroyWindow(dlg);
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    return out;
}

static void LayoutControls(HWND hwnd) {
    RECT rc{}; GetClientRect(hwnd, &rc);
    const int pad=8,leftW=330,bannerH=230,topH=TOP_BAR_H;
    int transferLines = 0;
    bool baseVisible = true;
    {
        std::lock_guard<std::mutex> lk(g_statusMutex);
        for (const auto& it : g_transferStatusLines) if (!it.second.empty()) transferLines++;
        std::wstring base = g_baseStatus.empty() ? L"Status: Ready" : g_baseStatus;
        if (transferLines > 0 && _wcsicmp(base.c_str(), L"Status: Ready") == 0) baseVisible = false;
    }
    int visibleLines = transferLines + (baseVisible ? 1 : 0);
    if (visibleLines < 1) visibleLines = 1;
    const int statusH = 26 + (visibleLines - 1) * 22;
    const int titleBtnW = 44;
    MoveWindow(g_btnClose, rc.right - titleBtnW, 0, titleBtnW, TOP_BAR_H, TRUE);
    MoveWindow(g_btnMax, rc.right - titleBtnW * 2, 0, titleBtnW, TOP_BAR_H, TRUE);
    MoveWindow(g_btnMin, rc.right - titleBtnW * 3, 0, titleBtnW, TOP_BAR_H, TRUE);
    MoveWindow(g_hamburger, rc.right - titleBtnW * 4, 0, titleBtnW, TOP_BAR_H, TRUE);
    MoveWindow(g_search,pad,pad+topH,leftW-2*pad,24,TRUE);
    MoveWindow(g_list,pad,pad+topH+28,leftW-2*pad-12,rc.bottom-statusH-topH-38,TRUE);
    MoveWindow(g_listScroll,leftW-pad-12,pad+topH+28,12,rc.bottom-statusH-topH-38,TRUE);
    int rightX=leftW,rightW=rc.right-leftW-pad;
    const int btnW = 188, btnH = 45, btnPad = 0;
    const int bannerLeft = 342;
    const int bannerBottom = 268 + topH;
    MoveWindow(g_playGet, bannerLeft + btnPad, bannerBottom - btnH - btnPad, btnW, btnH, TRUE);
    int chatTop = pad + topH + bannerH + 50;
    int chatH = rc.bottom - chatTop - statusH - pad;
    int peersW = 280;
    int selfH = 44;
    int toolbarH = 28;
    int inputH = 70;
    MoveWindow(g_chatView, rightX + pad, chatTop, rightW - 2 * pad - peersW - 20, chatH - inputH - toolbarH - 8, TRUE);
    MoveWindow(g_chatViewScroll, rightX + rightW - pad - peersW - 12, chatTop, 12, chatH - inputH - toolbarH - 8, TRUE);
    MoveWindow(g_chatSelf, rightX + rightW - pad - peersW, chatTop, peersW - 12, selfH, TRUE);
    MoveWindow(g_chatSelfAvatar, rightX + rightW - pad - peersW + 4, chatTop + 1, 42, 42, TRUE);
    MoveWindow(g_chatSelfName, rightX + rightW - pad - peersW + 52, chatTop + 4, peersW - 70, 20, TRUE);
    MoveWindow(g_chatSelfStatus, rightX + rightW - pad - peersW + 52, chatTop + 22, peersW - 70, 18, TRUE);
    MoveWindow(g_chatPeers, rightX + rightW - pad - peersW, chatTop + selfH + 6, peersW - 12, chatH - selfH - 10, TRUE);
    MoveWindow(g_chatPeersScroll, rightX + rightW - pad - 12, chatTop + selfH + 6, 12, chatH - selfH - 10, TRUE);
    MoveWindow(g_chatBold, rightX + pad, chatTop + chatH - inputH - toolbarH, 30, toolbarH, TRUE);
    MoveWindow(g_chatItalic, rightX + pad + 34, chatTop + chatH - inputH - toolbarH, 30, toolbarH, TRUE);
    MoveWindow(g_chatColor, rightX + pad + 68, chatTop + chatH - inputH - toolbarH, 56, toolbarH, TRUE);
    MoveWindow(g_chatFont, rightX + pad + 128, chatTop + chatH - inputH - toolbarH, 56, toolbarH, TRUE);
    MoveWindow(g_chatEmoji, rightX + pad + 188, chatTop + chatH - inputH - toolbarH, 40, toolbarH, TRUE);
    MoveWindow(g_chatInput, rightX + pad, chatTop + chatH - inputH, rightW - 2 * pad - peersW - 86, inputH, TRUE);
    MoveWindow(g_chatSend, rightX + rightW - pad - peersW - 78, chatTop + chatH - inputH, 78, inputH, TRUE);
    MoveWindow(g_details, rightX + pad, chatTop, 1, 1, TRUE);
    MoveWindow(g_status,pad,rc.bottom-statusH,rc.right-2*pad,statusH-8,TRUE);
}
static BOOL CALLBACK RedrawChildProc(HWND child, LPARAM) {
    RedrawWindow(child, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME);
    return TRUE;
}
static void ForceFullRepaint(HWND hwnd) {
    if (!hwnd) return;
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME | RDW_ALLCHILDREN);
    EnumChildWindows(hwnd, RedrawChildProc, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        MarkUserInteraction();
    }
    switch (msg) {
        case WM_CREATE: {
            g_bgBrush = CreateSolidBrush(RGB(15,26,38));
            g_panelBrush = CreateSolidBrush(RGB(22,32,45));
            g_searchBrush = CreateSolidBrush(RGB(28,41,58));
            g_scrollBrush = CreateSolidBrush(RGB(20,31,44));
            g_font = CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            g_fontBold = CreateFontW(18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            g_fontEmoji = CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Emoji");
            if (!g_fontEmoji) {
                g_fontEmoji = CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Symbol");
            }
            g_fontSmall = CreateFontW(14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");

            RegisterLanButtonClass((HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            g_hamburger = CreateWindowExW(0, kLanButtonClass, L"\x2699", WS_VISIBLE|WS_CHILD, 8, 28, 28, 24, hwnd, (HMENU)IDC_HAMBURGER, nullptr, nullptr);
            g_search = CreateWindowExW(0,L"EDIT",L"Search",WS_VISIBLE|WS_CHILD|ES_AUTOHSCROLL,40,28,268,24,hwnd,(HMENU)IDC_SEARCH,nullptr,nullptr);
            g_list = CreateWindowExW(0,L"LISTBOX",L"",WS_VISIBLE|WS_CHILD|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,8,56,300,600,hwnd,(HMENU)IDC_GAMELIST,nullptr,nullptr);
            g_listScroll = CreateWindowExW(0, L"STATIC", L"", WS_VISIBLE|WS_CHILD|SS_NOTIFY, 0,0,12,120, hwnd, (HMENU)IDC_LISTSCROLL, nullptr, nullptr);
            g_playGet = CreateWindowExW(0, kLanButtonClass, L"", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 360,210,150,36,hwnd,(HMENU)IDC_PLAYGET,nullptr,nullptr);
            g_details = CreateWindowExW(0,L"EDIT",L"",WS_VISIBLE|WS_CHILD|ES_MULTILINE|ES_READONLY,350,290,700,360,hwnd,(HMENU)IDC_DETAILS,nullptr,nullptr);
            g_chatView = CreateWindowExW(0, L"LISTBOX", L"", WS_VISIBLE|WS_CHILD|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOTIFY, 350,290,600,300, hwnd, (HMENU)IDC_CHAT_VIEW, nullptr, nullptr);
            g_chatViewScroll = CreateWindowExW(0, L"STATIC", L"", WS_VISIBLE|WS_CHILD|SS_NOTIFY, 0,0,12,120, hwnd, (HMENU)IDC_CHAT_VIEW_SCROLL, nullptr, nullptr);
            g_chatSelf = CreateWindowExW(0, kLanButtonClass, L"", WS_VISIBLE|WS_CHILD, 960,290,220,44, hwnd, (HMENU)IDC_CHAT_SELF, nullptr, nullptr);
            g_chatSelfAvatar = CreateWindowExW(0, kLanButtonClass, L"", WS_VISIBLE|WS_CHILD, 966,293,38,38, hwnd, (HMENU)IDC_CHAT_SELF_AVATAR, nullptr, nullptr);
            g_chatSelfName = CreateWindowW(L"STATIC", L"", WS_VISIBLE|WS_CHILD|SS_NOTIFY, 1010,294,160,20, hwnd, (HMENU)IDC_CHAT_SELF_NAME, nullptr, nullptr);
            g_chatSelfStatus = CreateWindowW(L"STATIC", L"", WS_VISIBLE|WS_CHILD|SS_NOTIFY, 1010,312,160,18, hwnd, (HMENU)IDC_CHAT_SELF_STATUS, nullptr, nullptr);
            g_chatPeers = CreateWindowExW(0, L"LISTBOX", L"", WS_VISIBLE|WS_CHILD|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS, 960,340,220,250, hwnd, (HMENU)IDC_CHAT_PEERS, nullptr, nullptr);
            g_chatPeersScroll = CreateWindowExW(0, L"STATIC", L"", WS_VISIBLE|WS_CHILD|SS_NOTIFY, 0,0,12,120, hwnd, (HMENU)IDC_CHAT_PEERS_SCROLL, nullptr, nullptr);
            g_chatInput = CreateWindowExW(0, kChatInputClass, L"", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 350,600,520,70, hwnd, (HMENU)IDC_CHAT_INPUT, nullptr, nullptr);
            g_chatSend = CreateWindowExW(0, kLanButtonClass, L"SEND", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 880,600,70,70, hwnd, (HMENU)IDC_CHAT_SEND, nullptr, nullptr);
            g_chatBold = CreateWindowExW(0, kLanButtonClass, L"B", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 350,572,30,28, hwnd, (HMENU)IDC_CHAT_BOLD, nullptr, nullptr);
            g_chatItalic = CreateWindowExW(0, kLanButtonClass, L"I", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 384,572,30,28, hwnd, (HMENU)IDC_CHAT_ITALIC, nullptr, nullptr);
            g_chatColor = CreateWindowExW(0, kLanButtonClass, L"Color", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 418,572,56,28, hwnd, (HMENU)IDC_CHAT_COLOR, nullptr, nullptr);
            g_chatFont = CreateWindowExW(0, kLanButtonClass, L"Font", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 478,572,56,28, hwnd, (HMENU)IDC_CHAT_FONT, nullptr, nullptr);
            g_chatEmoji = CreateWindowExW(0, kLanButtonClass, L":)", WS_VISIBLE|WS_CHILD|WS_TABSTOP, 538,572,40,28, hwnd, (HMENU)IDC_CHAT_EMOJI, nullptr, nullptr);
            g_status = CreateWindowW(L"STATIC",L"Status: Ready",WS_VISIBLE|WS_CHILD|SS_LEFT,8,760,1000,64,hwnd,(HMENU)IDC_STATUS,nullptr,nullptr);
            g_btnMin = CreateWindowExW(0, kLanButtonClass, L"\x2013", WS_VISIBLE | WS_CHILD, 0, 0, 44, TOP_BAR_H, hwnd, (HMENU)IDC_BTN_MIN, nullptr, nullptr);
            g_btnMax = CreateWindowExW(0, kLanButtonClass, L"\x25A1", WS_VISIBLE | WS_CHILD, 0, 0, 44, TOP_BAR_H, hwnd, (HMENU)IDC_BTN_MAX, nullptr, nullptr);
            g_btnClose = CreateWindowExW(0, kLanButtonClass, L"\x00D7", WS_VISIBLE | WS_CHILD, 0, 0, 44, TOP_BAR_H, hwnd, (HMENU)IDC_BTN_CLOSE, nullptr, nullptr);
            SetControlFont(g_hamburger, true); SetControlFont(g_search); SetControlFont(g_list); SetControlFont(g_playGet,true); SetControlFont(g_details); SetControlFont(g_status);
            SetControlFont(g_chatView); SetControlFont(g_chatPeers); SetControlFont(g_chatInput); SetControlFont(g_chatSend, true);
            SetControlFont(g_chatBold, true); SetControlFont(g_chatItalic, true); SetControlFont(g_chatColor); SetControlFont(g_chatFont); SetControlFont(g_chatEmoji);
            SetControlFont(g_chatSelf, true);
            SetControlFont(g_chatSelfAvatar);
            SetControlFont(g_chatSelfName, true);
            SetControlFont(g_chatSelfStatus);
            SendMessageW(g_chatPeers, LB_SETITEMHEIGHT, 0, 44);
            ShowWindow(g_chatSelfAvatar, SW_SHOW);
            ShowWindow(g_chatSelfName, SW_SHOW);
            ShowWindow(g_chatSelfStatus, SW_SHOW);
            if (g_fontEmoji) {
                SendMessageW(g_chatView, WM_SETFONT, (WPARAM)g_fontEmoji, TRUE);
                SendMessageW(g_chatInput, WM_SETFONT, (WPARAM)g_fontEmoji, TRUE);
                SendMessageW(g_chatEmoji, WM_SETFONT, (WPARAM)g_fontEmoji, TRUE);
            }
            ShowScrollBar(g_chatPeers, SB_VERT, FALSE);
            ShowScrollBar(g_chatInput, SB_VERT, FALSE);
            ShowScrollBar(g_list, SB_VERT, FALSE);
            g_oldListScrollProc = (WNDPROC)SetWindowLongPtrW(g_listScroll, GWLP_WNDPROC, (LONG_PTR)ListScrollProc);
            g_oldPeersScrollProc = (WNDPROC)SetWindowLongPtrW(g_chatPeersScroll, GWLP_WNDPROC, (LONG_PTR)ChatPeersScrollProc);
            g_oldChatScrollProc = (WNDPROC)SetWindowLongPtrW(g_chatViewScroll, GWLP_WNDPROC, (LONG_PTR)ChatViewScrollProc);
            g_oldListProc = (WNDPROC)SetWindowLongPtrW(g_list, GWLP_WNDPROC, (LONG_PTR)ListProc);
            g_oldChatViewProc = (WNDPROC)SetWindowLongPtrW(g_chatView, GWLP_WNDPROC, (LONG_PTR)ChatViewProc);
            DragAcceptFiles(hwnd, TRUE);
            // FlatSB + modern manifests is inconsistent on Win10/11 and caused invisible bars.
            // Keep native visible vertical list scrollbar and remove details scrollbar artifacts.
            ShowScrollBar(g_list, SB_HORZ, FALSE);
            ShowScrollBar(g_details, SB_HORZ, FALSE);
            ShowScrollBar(g_chatPeers, SB_HORZ, FALSE);
            ShowScrollBar(g_chatView, SB_HORZ, FALSE);
            ShowScrollBar(g_chatView, SB_VERT, FALSE);
            SetMenu(hwnd, nullptr);
            BOOL dark = TRUE;
            TryDwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
            COLORREF border = RGB(12, 20, 31);
            TryDwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
            COLORREF caption = RGB(12, 20, 31);
            TryDwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
            COLORREF txt = RGB(210, 228, 244);
            TryDwmSetWindowAttribute(hwnd, 36, &txt, sizeof(txt));

            MarkUserInteraction();
            SetTimer(hwnd, IDT_IDLE_PRESENCE, 1000, nullptr);
            LoadConfig();
            if (g_myNick.empty()) g_myNick = g_hostName;
            if (g_myPresence.empty()) g_myPresence = L"Online";
            g_lastEffectivePresence = EffectiveMyPresence();
            if (g_cfg.winW > 0 && g_cfg.winH > 0) {
                UINT swpFlags = SWP_NOZORDER;
                int x = g_cfg.winX;
                int y = g_cfg.winY;
                if (x == CW_USEDEFAULT || y == CW_USEDEFAULT) swpFlags |= SWP_NOMOVE;
                SetWindowPos(hwnd, nullptr, x, y, g_cfg.winW, g_cfg.winH, swpFlags);
                if (g_cfg.winMax) ShowWindow(hwnd, SW_MAXIMIZE);
            }
            LayoutControls(hwnd);
            ScanLocalGames();
            RefreshList();
            UpdateSelectionUI();
            RefreshChatPeersUI();
            InvalidateRect(g_chatSelf, nullptr, TRUE);
            InvalidateRect(g_chatSelfAvatar, nullptr, TRUE);
            InvalidateRect(g_chatSelfName, nullptr, TRUE);
            InvalidateRect(g_chatSelfStatus, nullptr, TRUE);
            InvalidateRect(g_chatPeers, nullptr, TRUE);
            SyncChatScrollbar();

            std::thread(HttpServerThread).detach();
            std::thread(LanBroadcastThread).detach();
            std::thread(LanListenThread).detach();
            std::thread(ChatListenThread).detach();
            std::thread(PlayingDetectThread).detach();
            g_downloadQueueStop = false;
            g_downloadQueueThread = std::thread([]() {
                while (!g_downloadQueueStop.load()) {
                    DownloadTask next{};
                    bool hasItem = false;
                    {
                        std::unique_lock<std::mutex> lk(g_downloadQueueMutex);
                        g_downloadQueueCv.wait(lk, []() { return g_downloadQueueStop.load() || !g_downloadQueueTasks.empty(); });
                        if (g_downloadQueueStop.load()) break;
                        if (!g_downloadQueueTasks.empty()) {
                            next = g_downloadQueueTasks.front();
                            g_downloadQueueTasks.pop_front();
                            hasItem = true;
                        }
                    }
                    if (hasItem) {
                        DownloadRemoteGame(next.game, g_downloadQueueOwner ? g_downloadQueueOwner : g_main, next.destBase);
                    }
                }
            });
            SetStatus(L"Status: LAN services running");
            return 0;
        }
        case WM_TIMER:
            if (wParam == IDT_IDLE_PRESENCE) {
                std::wstring eff = EffectiveMyPresence();
                if (_wcsicmp(eff.c_str(), g_lastEffectivePresence.c_str()) != 0) {
                    g_lastEffectivePresence = eff;
                    RefreshChatPeersUI();
                }
                return 0;
            }
            break;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            if (mmi) {
                HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(mon, &mi)) {
                    RECT wr = mi.rcWork;
                    RECT mr = mi.rcMonitor;
                    mmi->ptMaxPosition.x = wr.left - mr.left;
                    mmi->ptMaxPosition.y = wr.top - mr.top;
                    mmi->ptMaxSize.x = wr.right - wr.left;
                    mmi->ptMaxSize.y = wr.bottom - wr.top;
                    mmi->ptMaxTrackSize = mmi->ptMaxSize;
                }
            }
            return 0;
        }
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) {
                return 0;
            }
            LayoutControls(hwnd);
            ShowScrollBar(g_list, SB_HORZ, FALSE);
            ShowScrollBar(g_details, SB_HORZ, FALSE);
            ShowScrollBar(g_list, SB_VERT, FALSE);
            ShowScrollBar(g_chatPeers, SB_VERT, FALSE);
            ShowScrollBar(g_chatView, SB_VERT, FALSE);
            ShowScrollBar(g_chatInput, SB_VERT, FALSE);
            SyncListScrollbar();
            SyncPeersScrollbar();
            SyncChatScrollbar();
            if (g_chatPeers) InvalidateRect(g_chatPeers, nullptr, FALSE);
            if (g_chatInput) InvalidateRect(g_chatInput, nullptr, FALSE);
            if (g_chatView) InvalidateRect(g_chatView, nullptr, FALSE);
            if (g_chatBold) InvalidateRect(g_chatBold, nullptr, FALSE);
            if (g_chatItalic) InvalidateRect(g_chatItalic, nullptr, FALSE);
            if (g_chatColor) InvalidateRect(g_chatColor, nullptr, FALSE);
            if (g_chatFont) InvalidateRect(g_chatFont, nullptr, FALSE);
            if (g_chatEmoji) InvalidateRect(g_chatEmoji, nullptr, FALSE);
            if (g_chatSend) InvalidateRect(g_chatSend, nullptr, FALSE);
            InvalidateRect(hwnd, nullptr, FALSE);
            ForceFullRepaint(hwnd);
            if (g_chatInput) UpdateWindow(g_chatInput);
            if (g_chatBold) UpdateWindow(g_chatBold);
            if (g_chatItalic) UpdateWindow(g_chatItalic);
            if (g_chatColor) UpdateWindow(g_chatColor);
            if (g_chatFont) UpdateWindow(g_chatFont);
            if (g_chatEmoji) UpdateWindow(g_chatEmoji);
            if (g_chatSend) UpdateWindow(g_chatSend);
            return 0;
        case WM_WINDOWPOSCHANGED: {
            WINDOWPOS* wp = (WINDOWPOS*)lParam;
            if (wp && !(wp->flags & SWP_NOSIZE)) {
                ForceFullRepaint(hwnd);
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            POINT sp{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT lr{}, sr{};
            GetWindowRect(g_list, &lr);
            GetWindowRect(g_listScroll, &sr);
            if (PtInRect(&lr, sp) || PtInRect(&sr, sp)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int top = (int)SendMessageW(g_list, LB_GETTOPINDEX, 0, 0);
                int itemH = (int)SendMessageW(g_list, LB_GETITEMHEIGHT, 0, 0);
                RECT cr{}; GetClientRect(g_list, &cr);
                int page = max(1, (cr.bottom - cr.top) / max(1, itemH));
                int count = (int)SendMessageW(g_list, LB_GETCOUNT, 0, 0);
                int maxTop = max(0, count - page);
                int step = max(1, page / 4);
                top += (delta > 0) ? -step : step;
                if (top < 0) top = 0;
                if (top > maxTop) top = maxTop;
                SendMessageW(g_list, LB_SETTOPINDEX, top, 0);
                SyncListScrollbar();
                return 0;
            }
            RECT pr{}, psr{};
            GetWindowRect(g_chatPeers, &pr);
            GetWindowRect(g_chatPeersScroll, &psr);
            if (PtInRect(&pr, sp) || PtInRect(&psr, sp)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int top = (int)SendMessageW(g_chatPeers, LB_GETTOPINDEX, 0, 0);
                int itemH = (int)SendMessageW(g_chatPeers, LB_GETITEMHEIGHT, 0, 0);
                RECT cr{}; GetClientRect(g_chatPeers, &cr);
                int page = max(1, (cr.bottom - cr.top) / max(1, itemH));
                int count = (int)SendMessageW(g_chatPeers, LB_GETCOUNT, 0, 0);
                int maxTop = max(0, count - page);
                int step = max(1, page / 4);
                top += (delta > 0) ? -step : step;
                if (top < 0) top = 0;
                if (top > maxTop) top = maxTop;
                SendMessageW(g_chatPeers, LB_SETTOPINDEX, top, 0);
                SyncPeersScrollbar();
                return 0;
            }
            RECT cvr{}, cvsr{};
            GetWindowRect(g_chatView, &cvr);
            GetWindowRect(g_chatViewScroll, &cvsr);
            if (PtInRect(&cvr, sp) || PtInRect(&cvsr, sp)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int top = (int)SendMessageW(g_chatView, LB_GETTOPINDEX, 0, 0);
                int itemH = (int)SendMessageW(g_chatView, LB_GETITEMHEIGHT, 0, 0);
                RECT cr{}; GetClientRect(g_chatView, &cr);
                int page = max(1, (cr.bottom - cr.top) / max(1, itemH));
                int count = (int)SendMessageW(g_chatView, LB_GETCOUNT, 0, 0);
                int maxTop = max(0, count - page);
                int step = max(1, page / 4);
                top += (delta > 0) ? -step : step;
                if (top < 0) top = 0;
                if (top > maxTop) top = maxTop;
                SendMessageW(g_chatView, LB_SETTOPINDEX, top, 0);
                SyncChatScrollbar();
                return 0;
            }
            break;
        }
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            size_t before = g_manualGameFolders.size();
            UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t path[MAX_PATH]{};
                DragQueryFileW(hDrop, i, path, MAX_PATH);
                std::wstring p = path;
                DWORD a = GetFileAttributesW(p.c_str());
                if (a != INVALID_FILE_ATTRIBUTES) {
                    if (a & FILE_ATTRIBUTE_DIRECTORY) {
                        AddDroppedPathAsGameFolder(p);
                    } else {
                        size_t pos = p.find_last_of(L"\\/");
                        if (pos != std::wstring::npos) AddDroppedPathAsGameFolder(p.substr(0, pos));
                    }
                }
            }
            size_t after = g_manualGameFolders.size();
            SaveConfig();
            ScanLocalGames();
            RefreshList();
            UpdateSelectionUI();
            if (after > before) {
                std::thread(AutoDownloadAssetsForLocalGames).detach();
                SetStatus(L"Status: dropped game(s) added");
            } else {
                SetStatus(L"Status: no games found in dropped folder(s)");
            }
            DragFinish(hDrop);
            return 0;
        }
        case WM_PEER_UPDATE:
            RefreshList();
            UpdateSelectionUI();
            RefreshChatPeersUI();
            SetStatus(L"Status: LAN peer update received");
            std::thread(CacheRemoteAssetsInBackground).detach();
            return 0;
        case WM_ASSETS_UPDATE:
            g_imageCache.clear();
            RefreshList();
            UpdateSelectionUI();
            SetStatus(L"Status: assets updated");
            return 0;
        case WM_CHAT_APPEND: {
            ChatLinePayload* p = (ChatLinePayload*)wParam;
            if (p) {
                if (p->hasStyle) {
                    CHARFORMAT2W cf{};
                    cf.cbSize = sizeof(cf);
                    cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_FACE;
                    cf.dwEffects = 0;
                    if (p->bold) cf.dwEffects |= CFE_BOLD;
                    if (p->italic) cf.dwEffects |= CFE_ITALIC;
                    cf.crTextColor = p->color;
                    if (!p->face.empty()) wcsncpy_s(cf.szFaceName, LF_FACESIZE, p->face.c_str(), _TRUNCATE);
                    AppendChatLineStyled(p->from, p->msg, &cf);
                } else {
                    AppendChatLine(p->from, p->msg);
                }
                delete p;
            }
            return 0;
        }
        case WM_CHAT_PEERS_UPDATE:
            RefreshChatPeersUI();
            return 0;
        case WM_STATUSBAR_REFRESH:
            ApplyStatusBarTextAndLayout();
            return 0;
        case WM_COMMAND: {
            int id=LOWORD(wParam), code=HIWORD(wParam);
            if (id >= IDM_EMOJI_BASE && id <= IDM_EMOJI_LAST) {
                for (const auto& it : kEmojiItems) {
                    if (it.id == (UINT)id) {
                        ChatWrapSelection(it.glyph, L"");
                        break;
                    }
                }
                return 0;
            }
            if (id == IDC_SEARCH && code == EN_SETFOCUS) {
                if (g_searchPlaceholder) {
                    SetWindowTextW(g_search, L"");
                    g_searchPlaceholder = false;
                }
                return 0;
            }
            if (id == IDC_SEARCH && code == EN_KILLFOCUS) {
                wchar_t b[4]{};
                GetWindowTextW(g_search, b, 3);
                if (wcslen(b) == 0) {
                    SetWindowTextW(g_search, L"Search");
                    g_searchPlaceholder = true;
                    RefreshList();
                    UpdateSelectionUI();
                }
                return 0;
            }
            if (id == IDC_SEARCH && code == EN_CHANGE) {
                RefreshList();
                UpdateSelectionUI();
                return 0;
            }
            if (id == IDM_FILE_CHOOSE_ROOT) {
                std::wstring folder = PickFolder(hwnd, L"Choose shared games root");
                if (!folder.empty()) {
                    g_cfg.sharedRoot = folder;
                    SaveConfig();
                    ScanLocalGames();
                    RefreshList();
                    UpdateSelectionUI();
                    std::thread(AutoDownloadAssetsForLocalGames).detach();
                    SetStatus(L"Status: shared root updated");
                }
                return 0;
            }
            if (id == IDC_HAMBURGER && code == BN_CLICKED) {
                RECT br{};
                GetWindowRect(g_hamburger, &br);
                HMENU m = BuildStyledSettingsMenu();
                TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, br.left, br.bottom + 2, 0, hwnd, nullptr);
                DestroyMenu(m);
                return 0;
            }
            if (id == IDM_SELF_SET_NICK) {
                std::wstring n = PromptText(hwnd, L"Set Nickname", L"Nickname:", g_myNick);
                if (!n.empty()) { g_myNick = n; SaveConfig(); RefreshChatPeersUI(); }
                return 0;
            }
            if (id == IDM_SELF_SET_AVATAR) {
                std::wstring chosen = PickFile(hwnd, L"Choose avatar image", L"Images\0*.png;*.jpg;*.jpeg;*.bmp\0All files\0*.*\0");
                if (!chosen.empty()) {
                    std::wstring avatarsDir = ExeDir() + L"\\data\\avatars";
                    EnsureDir(avatarsDir);
                    DeleteFileW((avatarsDir + L"\\self.png").c_str());
                    DeleteFileW((avatarsDir + L"\\self.jpg").c_str());
                    DeleteFileW((avatarsDir + L"\\self.jpeg").c_str());
                    DeleteFileW((avatarsDir + L"\\self.bmp").c_str());
                    std::wstring ext = L".png";
                    size_t dot = chosen.find_last_of(L'.');
                    if (dot != std::wstring::npos) {
                        std::wstring e = chosen.substr(dot);
                        std::transform(e.begin(), e.end(), e.begin(), towlower);
                        if (e == L".png" || e == L".jpg" || e == L".jpeg" || e == L".bmp") ext = e;
                    }
                    std::wstring dst = avatarsDir + L"\\self" + ext;
                    CopyFileW(chosen.c_str(), dst.c_str(), FALSE);
                    SetStatus(L"Status: avatar updated");
                    if (g_chatSelfAvatar) { InvalidateRect(g_chatSelfAvatar, nullptr, FALSE); UpdateWindow(g_chatSelfAvatar); }
                    if (g_chatSelf) InvalidateRect(g_chatSelf, nullptr, FALSE);
                    if (g_chatPeers) InvalidateRect(g_chatPeers, nullptr, FALSE);
                    RefreshChatPeersUI();
                    InvalidateRect(g_chatPeers, nullptr, FALSE);
                }
                return 0;
            }
            if (id == IDM_SELF_STATUS_ONLINE || id == IDM_SELF_STATUS_BUSY || id == IDM_SELF_STATUS_INVISIBLE) {
                if (id == IDM_SELF_STATUS_ONLINE) g_myPresence = L"Online";
                else if (id == IDM_SELF_STATUS_BUSY) g_myPresence = L"Busy";
                else g_myPresence = L"Invisible";
                SaveConfig();
                RefreshChatPeersUI();
                SetStatus((_wcsicmp(g_myPresence.c_str(), L"Invisible") == 0) ? L"Status: Invisible (games hidden on LAN)" : L"Status: presence updated");
                return 0;
            }
            if (id == IDM_FILE_ADD_GAME) {
                std::wstring folder = PickFolder(hwnd, L"Choose game folder to add");
                if (!folder.empty()) {
                    auto existsFolder = [&](const std::wstring& pth) {
                        return std::find_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
                            [&](const std::wstring& p) { return _wcsicmp(p.c_str(), pth.c_str()) == 0; }) != g_manualGameFolders.end();
                    };
                    bool existedBefore = existsFolder(folder);
                    size_t before = g_manualGameFolders.size();
                    AddDroppedPathAsGameFolder(folder);
                    size_t after = g_manualGameFolders.size();
                    SaveConfig();
                    ScanLocalGames();
                    RefreshList();
                    UpdateSelectionUI();
                    std::thread(AutoDownloadAssetsForLocalGames).detach();
                    if (after > before) {
                        SetStatus(L"Status: game(s) added");
                    } else if (existedBefore || existsFolder(folder)) {
                        SetStatus(L"Status: folder already added");
                    } else {
                        MessageBoxW(hwnd, L"No game executables were detected in this folder or direct subfolders.\nYou can still add the folder and set EXE manually.", kAppName, MB_OK | MB_ICONWARNING);
                        SetStatus(L"Status: no games found in selected folder");
                    }
                }
                return 0;
            }
            if (id == IDM_FILE_REFRESH) {
                ScanLocalGames();
                RefreshList();
                UpdateSelectionUI();
                std::thread(AutoDownloadAssetsForLocalGames).detach();
                SetStatus(L"Status: refreshed");
                return 0;
            }
            if (id == IDM_FILE_SGDB_SELECTED) {
                std::thread(DownloadSteamGridForSelected).detach();
                return 0;
            }
            if (id == IDM_FILE_SGDB_ALL) {
                std::thread(DownloadSteamGridForAll).detach();
                return 0;
            }
            if (id == IDM_FILE_REMOVE_ALL) {
                g_manualGameFolders.clear();
                g_hiddenEntries.clear();
                g_manualSgdbIds.clear();
                g_nameAliases.clear();
                g_cfg.sharedRoot.clear();
                g_lastSelectedGame.clear();
                g_lastSelectedPath.clear();
                {
                    std::lock_guard<std::mutex> lk(g_gamesMutex);
                    g_games.clear();
                    g_remoteGames.clear();
                }
                SaveConfig();
                ScanLocalGames();
                RefreshList();
                UpdateSelectionUI();
                SetStatus(L"Status: all entries removed and folder list cleaned");
                return 0;
            }
            if (id == IDM_FILE_TOGGLE_CACHE) {
                g_useImageCache = !g_useImageCache;
                if (!g_useImageCache) g_imageCache.clear();
                SaveConfig();
                InvalidateBannerOnly();
                SetStatus(g_useImageCache ? L"Status: image cache enabled" : L"Status: image cache disabled");
                return 0;
            }
            if (id == IDM_FILE_TOGGLE_LAN_SHARE) {
                g_lanShareEnabled = !g_lanShareEnabled.load();
                SaveConfig();
                SetStatus(g_lanShareEnabled.load() ? L"Status: LAN list sharing enabled" : L"Status: LAN list sharing disabled");
                return 0;
            }
            if (id == IDM_CTX_SET_EXE) {
                int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < (int)g_uiGames.size() && g_uiGames[idx].local) {
                    std::wstring chosen = PickFile(hwnd, L"Choose launch executable", L"Executable (*.exe)\0*.exe\0All files\0*.*\0", g_uiGames[idx].path.c_str());
                    if (!chosen.empty()) {
                        {
                            std::lock_guard<std::mutex> lk(g_gamesMutex);
                            for (auto& g : g_games) if (_wcsicmp(g.path.c_str(), g_uiGames[idx].path.c_str()) == 0) g.launchExe = chosen;
                        }
                        SaveConfig();
                        ScanLocalGames();
                        RefreshList();
                        UpdateSelectionUI();
                        SetStatus(L"Status: launch EXE set");
                    }
                }
                return 0;
            }
            if (id == IDM_CTX_OPEN_FOLDER) {
                int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < (int)g_uiGames.size()) {
                    const auto& e = g_uiGames[idx];
                    std::wstring p = e.path.empty() ? (ExeDir() + L"\\data\\assets\\games\\" + Slugify(e.name)) : e.path;
                    ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                return 0;
            }
            if (id == IDM_CTX_SET_ICON || id == IDM_CTX_SET_BANNER) {
                int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < (int)g_uiGames.size()) {
                    std::wstring chosen = PickFile(hwnd, id == IDM_CTX_SET_ICON ? L"Choose icon image" : L"Choose banner image", L"Images\0*.png;*.jpg;*.jpeg;*.bmp\0All files\0*.*\0");
                    if (!chosen.empty()) {
                        std::wstring dir = ExeDir() + L"\\data\\assets\\games\\" + Slugify(g_uiGames[idx].name);
                        EnsureDir(ExeDir() + L"\\data");
                        EnsureDir(ExeDir() + L"\\data\\assets");
                        EnsureDir(ExeDir() + L"\\data\\assets\\games");
                        EnsureDir(dir);
                        std::wstring dst = dir + (id == IDM_CTX_SET_ICON ? L"\\icon.png" : L"\\banner.png");
                        CopyFileW(chosen.c_str(), dst.c_str(), FALSE);
                        ScanLocalGames();
                        RefreshList();
                        UpdateSelectionUI();
                    }
                }
                return 0;
            }
            if (id == IDM_CTX_REMOVE_ENTRY) {
                int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < (int)g_uiGames.size() && g_uiGames[idx].local) {
                    std::wstring target = NormalizePathKey(g_uiGames[idx].path);
                    auto it = std::remove_if(g_manualGameFolders.begin(), g_manualGameFolders.end(),
                        [&](const std::wstring& p) { return NormalizePathKey(p) == target; });
                    bool removedFromManual = (it != g_manualGameFolders.end());
                    if (removedFromManual) {
                        g_manualGameFolders.erase(it, g_manualGameFolders.end());
                    } else {
                        // Shared-root item: keep it hidden persistently.
                        std::wstring hk = EntryHideKey(g_uiGames[idx]);
                        bool has = false;
                        for (const auto& h : g_hiddenEntries) if (_wcsicmp(h.c_str(), hk.c_str()) == 0) { has = true; break; }
                        if (!has) g_hiddenEntries.push_back(hk);
                    }
                    SaveConfig();
                    ScanLocalGames();
                    RefreshList();
                    UpdateSelectionUI();
                }
                return 0;
            }
            if (id == IDM_CTX_DOWNLOAD_ASSETS) {
                std::thread(DownloadSteamGridForSelected).detach();
                return 0;
            }
            if (id == IDM_CTX_SET_SGDB_ID) {
                int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < (int)g_uiGames.size()) {
                    int cur = GetManualSgdbIdForGame(g_uiGames[idx].name);
                    int v = PromptSteamGridId(hwnd, cur);
                    if (v > 0) {
                        SetManualSgdbIdForGame(g_uiGames[idx].name, v);
                        SaveConfig();
                        SetStatus(L"Status: SteamGridDB ID saved");
                    }
                }
                return 0;
            }
            if (id == IDM_FILE_EXIT) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_BTN_MIN && code == BN_CLICKED) { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
            if (id == IDC_BTN_MAX && code == BN_CLICKED) { ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE); return 0; }
            if (id == IDC_BTN_CLOSE && code == BN_CLICKED) { PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0; }
            if (id==IDC_GAMELIST && code==LBN_SELCHANGE) UpdateSelectionUI();
            if (id==IDC_CHAT_SEND && code==BN_CLICKED) { SendChatToSelectedPeer(); return 0; }
            if (id==IDC_CHAT_INPUT && code==EN_MAXTEXT) return 0;
            if (id==IDC_CHAT_BOLD && code==BN_CLICKED) {
                g_chatBoldPick = !g_chatBoldPick;
                InvalidateRect(g_chatBold, nullptr, TRUE);
                return 0;
            }
            if (id==IDC_CHAT_ITALIC && code==BN_CLICKED) {
                g_chatItalicPick = !g_chatItalicPick;
                InvalidateRect(g_chatItalic, nullptr, TRUE);
                return 0;
            }
            if (id==IDC_CHAT_EMOJI && code==BN_CLICKED) {
                RECT br{}; GetWindowRect(g_chatEmoji, &br);
                ShowEmojiGridPopup(hwnd, br);
                return 0;
            }
            if (id==IDC_CHAT_COLOR && code==BN_CLICKED) {
                CHOOSECOLORW cc{}; COLORREF cust[16]{};
                cc.lStructSize = sizeof(cc); cc.hwndOwner = hwnd; cc.lpCustColors = cust; cc.rgbResult = g_chatColorPick; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    g_chatColorPick = cc.rgbResult;
                }
                return 0;
            }
            if (id==IDC_CHAT_FONT && code==BN_CLICKED) {
                LOGFONTW lf{}; wcscpy_s(lf.lfFaceName, g_chatFontName.c_str());
                CHOOSEFONTW cf{}; cf.lStructSize = sizeof(cf); cf.hwndOwner = hwnd; cf.lpLogFont = &lf; cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
                if (ChooseFontW(&cf)) {
                    g_chatFontName = lf.lfFaceName;
                }
                return 0;
            }
            if (id==IDC_PLAYGET && code==BN_CLICKED) {
                int idx=(int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (idx>=0 && idx<(int)g_uiGames.size()) {
                    GameEntry e = g_uiGames[idx];
                    if (e.local) {
                        if (ExistsFile(e.launchExe)) {
                            ShellExecuteW(nullptr,L"open",e.launchExe.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                        } else {
                            std::wstring chosen = PickFile(hwnd, L"Choose launch executable", L"Executable (*.exe)\0*.exe\0All files\0*.*\0", e.path.c_str());
                            if (!chosen.empty()) {
                                {
                                    std::lock_guard<std::mutex> lk(g_gamesMutex);
                                    for (auto& g : g_games) if (_wcsicmp(g.path.c_str(), e.path.c_str()) == 0) g.launchExe = chosen;
                                }
                                SaveConfig();
                                ScanLocalGames();
                                RefreshList();
                                UpdateSelectionUI();
                                SetStatus(L"Status: launch EXE set");
                            }
                        }
                    } else {
                        EnqueueRemoteGameDownload(e, hwnd);
                    }
                }
            }
            return 0;
        }
        case WM_CONTEXTMENU: {
            HWND src = (HWND)wParam;
            if (src == g_chatSelfAvatar) {
                POINT pt{ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
                HMENU m = BuildStyledSelfMenu();
                TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(m);
                return 0;
            }
            if (src == g_chatSelfName || src == g_chatSelfStatus || src == g_chatSelf) {
                POINT pt{ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
                HMENU m = BuildStyledSelfMenu();
                TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(m);
                return 0;
            }
            if (src == g_list) {
                POINT pt{ (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
                POINT cpt = pt;
                ScreenToClient(g_list, &cpt);
                DWORD hit = (DWORD)SendMessageW(g_list, LB_ITEMFROMPOINT, 0, MAKELPARAM(cpt.x, cpt.y));
                int idx = LOWORD(hit);
                BOOL outside = HIWORD(hit);
                if (!outside && idx >= 0 && idx < (int)g_uiGames.size()) {
                    SendMessageW(g_list, LB_SETCURSEL, idx, 0);
                    UpdateSelectionUI();
                    HMENU m = BuildStyledContextMenu();
                    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                    DestroyMenu(m);
                }
                return 0;
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc=(HDC)wParam;
            HWND ctl = (HWND)lParam;
            if (ctl == g_chatSelfName) {
                std::wstring ep = EffectiveMyPresence();
                bool playing = (ep.rfind(L"Playing ", 0) == 0);
                SetTextColor(hdc, playing ? RGB(108, 214, 98) : RGB(126, 194, 255));
                SetBkColor(hdc, RGB(22,32,45));
                return (LRESULT)g_panelBrush;
            }
            if (ctl == g_chatSelfStatus) {
                COLORREF sc = RGB(110, 190, 255);
                std::wstring ep = EffectiveMyPresence();
                if (ep.rfind(L"Playing ", 0) == 0) sc = RGB(108, 214, 98);
                else if (_wcsicmp(ep.c_str(), L"Busy") == 0) sc = RGB(220, 75, 75);
                else if (_wcsicmp(ep.c_str(), L"Idle") == 0) sc = RGB(255, 175, 70);
                else if (_wcsicmp(ep.c_str(), L"Invisible") == 0) sc = RGB(120, 130, 140);
                SetTextColor(hdc, sc);
                SetBkColor(hdc, RGB(22,32,45));
                return (LRESULT)g_panelBrush;
            }
            if (ctl == g_search) {
                SetTextColor(hdc, g_searchPlaceholder ? RGB(138, 166, 192) : RGB(220, 236, 248));
                SetBkColor(hdc, RGB(28,41,58));
                return (LRESULT)g_searchBrush;
            }
            SetTextColor(hdc,RGB(199,213,224)); SetBkColor(hdc,RGB(22,32,45)); return (LRESULT)g_panelBrush;
        }
        case WM_CTLCOLORSCROLLBAR: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(20,31,44));
            return (LRESULT)g_scrollBrush;
        }
        case WM_MEASUREITEM: {
            LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
            if (mis && mis->CtlType == ODT_MENU) {
                mis->itemHeight = 26;
                mis->itemWidth = 270;
                return TRUE;
            }
            if (mis && mis->CtlID == IDC_GAMELIST) {
                mis->itemHeight = 36;
                return TRUE;
            }
            if (mis && mis->CtlID == IDC_CHAT_PEERS) {
                mis->itemHeight = 34;
                return TRUE;
            }
            if (mis && mis->CtlID == IDC_CHAT_VIEW) {
                mis->itemHeight = 52;
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (!dis) break;
            if (dis->CtlType == ODT_MENU) {
                RECT r = dis->rcItem;
                bool sel = (dis->itemState & ODS_SELECTED) != 0;
                HBRUSH b = CreateSolidBrush(sel ? RGB(55, 110, 165) : RGB(18, 30, 43));
                FillRect(dis->hDC, &r, b);
                DeleteObject(b);
                SetBkMode(dis->hDC, TRANSPARENT);
                const wchar_t* txt = (const wchar_t*)dis->itemData;
                RECT tr = r;
                tr.left += 10;
                UINT id = dis->itemID;
                bool isStatusItem = (id == IDM_SELF_STATUS_ONLINE || id == IDM_SELF_STATUS_BUSY || id == IDM_SELF_STATUS_INVISIBLE);
                if (isStatusItem) {
                    COLORREF dot = RGB(110, 190, 255); // online
                    if (id == IDM_SELF_STATUS_BUSY) dot = RGB(220, 75, 75);
                    else if (id == IDM_SELF_STATUS_INVISIBLE) dot = RGB(150, 160, 170);
                    RECT dr{ r.left + 8, r.top + 8, r.left + 20, r.bottom - 8 };
                    HBRUSH db = CreateSolidBrush(dot);
                    FillRect(dis->hDC, &dr, db);
                    DeleteObject(db);
                    tr.left += 16;
                    bool current =
                        (id == IDM_SELF_STATUS_ONLINE && _wcsicmp(g_myPresence.c_str(), L"Online") == 0) ||
                        (id == IDM_SELF_STATUS_BUSY && _wcsicmp(g_myPresence.c_str(), L"Busy") == 0) ||
                        (id == IDM_SELF_STATUS_INVISIBLE && _wcsicmp(g_myPresence.c_str(), L"Invisible") == 0);
                    SetTextColor(dis->hDC, sel ? RGB(255,255,255) : (current ? RGB(170, 220, 255) : RGB(205, 220, 235)));
                    DrawTextW(dis->hDC, txt ? txt : L"", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    if (current) {
                        RECT rr = r;
                        rr.right -= 8;
                        SetTextColor(dis->hDC, sel ? RGB(255,255,255) : RGB(170,220,255));
                        DrawTextW(dis->hDC, L"\x2022", -1, &rr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                    }
                } else {
                    SetTextColor(dis->hDC, sel ? RGB(255,255,255) : RGB(205, 220, 235));
                    DrawTextW(dis->hDC, txt ? txt : L"", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                return TRUE;
            }
            if (dis->CtlID >= IDM_EMOJI_BASE && dis->CtlID <= IDM_EMOJI_LAST) {
                RECT r = dis->rcItem;
                bool pushed = (dis->itemState & ODS_SELECTED) != 0;
                HBRUSH b = CreateSolidBrush(pushed ? RGB(55, 110, 165) : RGB(26, 44, 62));
                FillRect(dis->hDC, &r, b);
                DeleteObject(b);
                HPEN p = CreatePen(PS_SOLID, 1, RGB(72,130,178));
                HGDIOBJ oldP = SelectObject(dis->hDC, p);
                HGDIOBJ oldB = SelectObject(dis->hDC, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dis->hDC, r.left, r.top, r.right, r.bottom);
                SelectObject(dis->hDC, oldB);
                SelectObject(dis->hDC, oldP);
                DeleteObject(p);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, RGB(235,245,252));
                DrawTextW(dis->hDC, EmojiAsciiForId((UINT)dis->CtlID), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                return TRUE;
            }
            if (dis->CtlID == IDC_GAMELIST) {
                if (dis->itemID == (UINT)-1) return TRUE;
                RECT r = dis->rcItem;
                bool sel = (dis->itemState & ODS_SELECTED) != 0;
                HBRUSH bg = CreateSolidBrush(sel ? RGB(102,192,244) : RGB(22,32,45));
                FillRect(dis->hDC, &r, bg);
                DeleteObject(bg);
                if (dis->itemID < g_uiGames.size()) {
                    GameEntry& ge = g_uiGames[dis->itemID];
                    int x = r.left + 6;
                    int y = r.top + 4;
                    if (!ge.iconPath.empty()) {
                        DrawImageSmart(dis->hDC, ge.iconPath, x, y, 26, 26);
                    }
                    RECT tr{r.left + 38, r.top, r.right - 6, r.bottom};
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, sel ? RGB(11,20,31) : RGB(230,240,248));
                    DrawTextW(dis->hDC, ge.name.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                }
                return TRUE;
            }
            if (dis->CtlID == IDC_CHAT_PEERS) {
                if (dis->itemID == (UINT)-1) return TRUE;
                RECT r = dis->rcItem;
                bool sel = (dis->itemState & ODS_SELECTED) != 0;
                HBRUSH bg = CreateSolidBrush(sel ? RGB(55, 110, 165) : RGB(22,32,45));
                FillRect(dis->hDC, &r, bg);
                DeleteObject(bg);
                wchar_t txt[256]{}; SendMessageW(g_chatPeers, LB_GETTEXT, dis->itemID, (LPARAM)txt);
                bool placeholder = (_wcsicmp(txt, L"No peers online") == 0);
                if (placeholder) {
                    RECT tr{r.left + 10, r.top, r.right - 6, r.bottom};
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, RGB(130,150,170));
                    DrawTextW(dis->hDC, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                    return TRUE;
                }
                RECT av{r.left + 6, r.top + 6, r.left + 36, r.top + 36};
                std::wstring host = (dis->itemID < g_peers.size()) ? g_peers[dis->itemID].first : L"";
                std::wstring nick = host;
                auto itNick = g_peerNick.find(host);
                if (itNick != g_peerNick.end() && !itNick->second.empty()) nick = itNick->second;
                std::wstring status = L"Online";
                auto itPres = g_peerPresence.find(host);
                if (itPres != g_peerPresence.end() && !itPres->second.empty()) status = itPres->second;
                if (_wcsicmp(status.c_str(), L"Invisible") == 0) status = L"Offline";
                bool peerPlaying = (status.rfind(L"Playing ", 0) == 0);
                if (peerPlaying) {
                    HBRUSH pbg = CreateSolidBrush(sel ? RGB(36, 84, 40) : RGB(24, 54, 28));
                    FillRect(dis->hDC, &r, pbg);
                    DeleteObject(pbg);
                }
                std::wstring avPath;
                auto itAv = g_peerAvatarPath.find(host);
                if (itAv != g_peerAvatarPath.end()) avPath = itAv->second;
                if (!avPath.empty() && ExistsFile(avPath)) {
                    if (!DrawImageSmart(dis->hDC, avPath, av.left, av.top, av.right - av.left, av.bottom - av.top)) {
                        HBRUSH avb = CreateSolidBrush(RGB(88, 174, 240)); FillRect(dis->hDC, &av, avb); DeleteObject(avb);
                    }
                } else {
                    HBRUSH avb = CreateSolidBrush(RGB(88, 174, 240)); FillRect(dis->hDC, &av, avb); DeleteObject(avb);
                }
                RECT tr{r.left + 42, r.top, r.right - 6, r.bottom};
                SetBkMode(dis->hDC, TRANSPARENT);
                HFONT oldF = (HFONT)SelectObject(dis->hDC, g_fontBold ? g_fontBold : g_font);
                RECT nr = tr;
                nr.top += 3;
                nr.bottom = nr.top + 18;
                SetTextColor(dis->hDC, peerPlaying ? RGB(138, 255, 132) : RGB(230,240,248));
                DrawTextW(dis->hDC, nick.c_str(), -1, &nr, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                SelectObject(dis->hDC, g_fontSmall ? g_fontSmall : g_font);
                RECT sr = tr;
                sr.top += 21;
                sr.bottom = sr.top + 16;
                COLORREF sc = RGB(110,190,255); // Online
                if (peerPlaying) sc = RGB(138, 255, 132);
                else if (_wcsicmp(status.c_str(), L"Busy") == 0) sc = RGB(220,75,75);
                else if (_wcsicmp(status.c_str(), L"Idle") == 0) sc = RGB(255,175,70);
                else if (_wcsicmp(status.c_str(), L"Offline") == 0) sc = RGB(150,160,170);
                SetTextColor(dis->hDC, sc);
                DrawTextW(dis->hDC, status.c_str(), -1, &sr, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                SelectObject(dis->hDC, oldF);
                return TRUE;
            }
            if (dis->CtlID == IDC_CHAT_VIEW) {
                if (dis->itemID == (UINT)-1 || dis->itemID >= g_chatItems.size()) return TRUE;
                RECT r = dis->rcItem;
                RECT bgR = r;
                InflateRect(&bgR, -1, -1);
                HBRUSH bg = CreateSolidBrush(RGB(22,32,45));
                FillRect(dis->hDC, &bgR, bg);
                DeleteObject(bg);

                const ChatRenderItem& it = g_chatItems[dis->itemID];
                std::wstring prefix = L"[" + it.from + L"] ";
                SetTextColor(dis->hDC, RGB(220, 236, 248));
                SetBkMode(dis->hDC, TRANSPARENT);
                int y = r.top + 9;
                int x = r.left + 10;
                TextOutW(dis->hDC, x, y, prefix.c_str(), (int)prefix.size());
                SIZE preSz{};
                GetTextExtentPoint32W(dis->hDC, prefix.c_str(), (int)prefix.size(), &preSz);
                HFONT oldF = (HFONT)SelectObject(dis->hDC, it.bold ? g_fontBold : g_font);
                int textX = x + preSz.cx;
                int a = 0, b = 0;
                bool hasTextSel = false;
                if (g_chatTextSelStartItem >= 0 && g_chatTextSelEndItem >= 0) {
                    int i0 = g_chatTextSelStartItem, i1 = g_chatTextSelEndItem;
                    int c0 = g_chatTextSelStartChar, c1 = g_chatTextSelEndChar;
                    if (i0 > i1 || (i0 == i1 && c0 > c1)) { std::swap(i0, i1); std::swap(c0, c1); }
                    int ii = (int)dis->itemID;
                    if (ii >= i0 && ii <= i1) {
                        hasTextSel = true;
                        a = (ii == i0) ? c0 : 0;
                        b = (ii == i1) ? c1 : (int)it.msg.size();
                        a = max(0, min(a, (int)it.msg.size()));
                        b = max(0, min(b, (int)it.msg.size()));
                    }
                }
                if (hasTextSel && b > a) {
                    std::wstring left = it.msg.substr(0, (size_t)a);
                    std::wstring mid = it.msg.substr((size_t)a, (size_t)(b - a));
                    int leftW = MeasureChatTextWithEmoji(dis->hDC, left);
                    int midW = max(1, MeasureChatTextWithEmoji(dis->hDC, mid));
                    RECT sr{textX + leftW, y - 1, textX + leftW + midW, y + 19};
                    HBRUSH sb = CreateSolidBrush(RGB(82, 137, 196));
                    FillRect(dis->hDC, &sr, sb);
                    DeleteObject(sb);
                }
                RECT mr{ textX + 1, y + 1, r.right - 14, r.bottom - 8 };
                DrawWrappedPlainText(dis->hDC, it.msg, mr, it.color);
                SelectObject(dis->hDC, oldF);
                return TRUE;
            }
            if (dis->CtlID == IDC_CHAT_SELF_AVATAR) {
                RECT r = dis->rcItem;
                HBRUSH b = CreateSolidBrush(RGB(18,30,43));
                FillRect(dis->hDC, &r, b);
                DeleteObject(b);
                std::wstring av = ResolveSelfAvatarPath();
                if (!av.empty() && ExistsFile(av)) {
                    DrawImageSmart(dis->hDC, av, r.left + 1, r.top + 1, (r.right - r.left) - 2, (r.bottom - r.top) - 2);
                } else {
                    HBRUSH avb = CreateSolidBrush(RGB(88, 174, 240));
                    RECT rr{r.left + 4, r.top + 4, r.right - 4, r.bottom - 4};
                    FillRect(dis->hDC, &rr, avb);
                    DeleteObject(avb);
                }
                HPEN p = CreatePen(PS_SOLID, 1, RGB(45, 78, 108));
                HGDIOBJ oldP = SelectObject(dis->hDC, p);
                HGDIOBJ oldB = SelectObject(dis->hDC, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dis->hDC, r.left, r.top, r.right, r.bottom);
                SelectObject(dis->hDC, oldB);
                SelectObject(dis->hDC, oldP);
                DeleteObject(p);
                return TRUE;
            }
            if (dis->CtlID == IDC_CHAT_SELF || dis->CtlID == IDC_HAMBURGER || dis->CtlID == IDC_BTN_MIN || dis->CtlID == IDC_BTN_MAX || dis->CtlID == IDC_BTN_CLOSE) {
                HDC hdc = dis->hDC;
                RECT r = dis->rcItem;
                bool pushed = (dis->itemState & ODS_SELECTED) != 0;
                bool hot = g_hotTitleBtn == (int)dis->CtlID;
                COLORREF bg = (dis->CtlID == IDC_CHAT_SELF) ? RGB(26, 44, 62) : RGB(15,26,38);
                if (dis->CtlID == IDC_BTN_CLOSE && (pushed || hot)) bg = RGB(180, 45, 45);
                else if (dis->CtlID != IDC_CHAT_SELF && (pushed || hot)) bg = RGB(36, 61, 84);
                HBRUSH b = CreateSolidBrush(bg);
                FillRect(hdc, &r, b);
                DeleteObject(b);
                SetBkMode(hdc, TRANSPARENT);
                COLORREF fg = RGB(210, 228, 244);
                if (dis->CtlID == IDC_CHAT_SELF) {
                    RECT av{r.left + 4, r.top + 2, r.left + 46, r.top + 44};
                    std::wstring avp = ResolveSelfAvatarPath();
                    if (!avp.empty() && ExistsFile(avp)) {
                        DrawImageSmart(hdc, avp, av.left, av.top, av.right - av.left, av.bottom - av.top);
                    } else {
                        HBRUSH avb = CreateSolidBrush(RGB(88, 174, 240));
                        FillRect(hdc, &av, avb);
                        DeleteObject(avb);
                    }
                    HPEN pen = CreatePen(PS_SOLID, 1, RGB(45, 78, 108));
                    HGDIOBJ oldP = SelectObject(hdc, pen);
                    HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                    Rectangle(hdc, av.left, av.top, av.right, av.bottom);
                    SelectObject(hdc, oldB);
                    SelectObject(hdc, oldP);
                    DeleteObject(pen);
                    RECT nr{r.left + 52, r.top + 4, r.right - 6, r.top + 24};
                    RECT sr{r.left + 52, r.top + 22, r.right - 6, r.bottom - 2};
                    SetTextColor(hdc, RGB(126, 194, 255));
                    DrawTextW(hdc, g_myNick.empty() ? L"You" : g_myNick.c_str(), -1, &nr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                    COLORREF sc = RGB(110, 190, 255);
                    std::wstring ep = EffectiveMyPresence();
                    if (_wcsicmp(ep.c_str(), L"Busy") == 0) sc = RGB(220, 75, 75);
                    else if (_wcsicmp(ep.c_str(), L"Idle") == 0) sc = RGB(255, 175, 70);
                    else if (_wcsicmp(ep.c_str(), L"Invisible") == 0) sc = RGB(120, 130, 140);
                    SetTextColor(hdc, sc);
                    DrawTextW(hdc, ep.c_str(), -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                    return TRUE;
                } else if (dis->CtlID == IDC_HAMBURGER) {
                    DrawCogIcon(hdc, r, fg);
                } else {
                    SetTextColor(hdc, fg);
                    wchar_t txt[8] = {};
                    GetWindowTextW(dis->hwndItem, txt, 7);
                    DrawTextW(hdc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                return TRUE;
            }
            if (dis->CtlID == IDC_CHAT_SEND || dis->CtlID == IDC_CHAT_BOLD || dis->CtlID == IDC_CHAT_ITALIC ||
                dis->CtlID == IDC_CHAT_COLOR || dis->CtlID == IDC_CHAT_FONT || dis->CtlID == IDC_CHAT_EMOJI) {
                HDC hdc = dis->hDC;
                RECT r = dis->rcItem;
                bool pushed = (dis->itemState & ODS_SELECTED) != 0;
                COLORREF bg = (dis->CtlID == IDC_CHAT_SEND) ? RGB(45,111,168) : RGB(30,62,92);
                if (g_hotChatToolBtn == (int)dis->CtlID) bg = RGB(min(255, GetRValue(bg)+20), min(255, GetGValue(bg)+20), min(255, GetBValue(bg)+20));
                if (pushed) bg = RGB(min(255, GetRValue(bg)+18), min(255, GetGValue(bg)+18), min(255, GetBValue(bg)+18));
                HBRUSH b = CreateSolidBrush(bg);
                FillRect(hdc, &r, b);
                DeleteObject(b);
                HPEN p = CreatePen(PS_SOLID, 1, RGB(72,130,178));
                HGDIOBJ oldP = SelectObject(hdc, p);
                HGDIOBJ oldB = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(hdc, r.left, r.top, r.right, r.bottom);
                SelectObject(hdc, oldB);
                SelectObject(hdc, oldP);
                DeleteObject(p);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(235,245,252));
                if (dis->CtlID == IDC_CHAT_EMOJI) {
                    std::wstring imgPath = ResolveEmojiImagePath(L"grinning_face.png");
                    if (!imgPath.empty()) {
                        Gdiplus::Graphics g(hdc);
                        Gdiplus::Image img(imgPath.c_str());
                        if (img.GetLastStatus() == Gdiplus::Ok) {
                            int s = min((r.right - r.left) - 8, (r.bottom - r.top) - 8);
                            int dx = r.left + ((r.right - r.left) - s) / 2;
                            int dy = r.top + ((r.bottom - r.top) - s) / 2;
                            g.DrawImage(&img, dx, dy, s, s);
                        } else {
                            DrawTextW(hdc, L":)", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                        }
                    } else {
                        DrawTextW(hdc, L":)", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    }
                } else {
                    wchar_t txt[128] = {};
                    GetWindowTextW(dis->hwndItem, txt, 127);
                    DrawTextW(hdc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
                return TRUE;
            }
            if (dis->CtlID != IDC_PLAYGET) break;
            HDC hdc = dis->hDC;
            RECT r = dis->rcItem;
            bool pushed = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bg = RGB(45,111,168);
            if (dis->CtlID == IDC_PLAYGET) {
                wchar_t txt[32] = {};
                GetWindowTextW(g_playGet, txt, 31);
                if (wcscmp(txt, L"PLAY") == 0) bg = RGB(60,143,45);
                else bg = RGB(45,111,168);
            }
            if (g_hotPlayBtn) bg = RGB(min(255, GetRValue(bg)+20), min(255, GetGValue(bg)+20), min(255, GetBValue(bg)+20));
            if (pushed) bg = RGB(min(255, GetRValue(bg)+15), min(255, GetGValue(bg)+15), min(255, GetBValue(bg)+15));
            HBRUSH b = CreateSolidBrush(bg);
            FillRect(hdc, &r, b);
            DeleteObject(b);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255,255,255));
            wchar_t txt[128] = {};
            GetWindowTextW(dis->hwndItem, txt, 127);
            DrawTextW(hdc, txt, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        case WM_ERASEBKGND: { RECT rc{}; GetClientRect(hwnd,&rc); FillRect((HDC)wParam,&rc,g_bgBrush); return 1; }
        case WM_NCHITTEST: {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            RECT wr{}; GetWindowRect(hwnd, &wr);
            int x = pt.x - wr.left;
            int y = pt.y - wr.top;
            int w = wr.right - wr.left;
            int h = wr.bottom - wr.top;
            bool left = x < RESIZE_BORDER;
            bool right = x >= w - RESIZE_BORDER;
            bool top = y < RESIZE_BORDER;
            bool bottom = y >= h - RESIZE_BORDER;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            POINT cpt = pt;
            ScreenToClient(hwnd, &cpt);
            RECT minR{}, maxR{}, closeR{};
            RECT cogR{};
            GetWindowRect(g_btnMin, &minR); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&minR, 2);
            GetWindowRect(g_btnMax, &maxR); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&maxR, 2);
            GetWindowRect(g_btnClose, &closeR); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&closeR, 2);
            GetWindowRect(g_hamburger, &cogR); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&cogR, 2);
            if (PtInRect(&minR, cpt) || PtInRect(&maxR, cpt) || PtInRect(&closeR, cpt) || PtInRect(&cogR, cpt)) return HTCLIENT;
            if (cpt.y < TOP_BAR_H) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_LBUTTONDBLCLK: {
            POINT cpt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (cpt.y < TOP_BAR_H) {
                ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                return 0;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{}; HDC hdc=BeginPaint(hwnd,&ps); RECT rc{}; GetClientRect(hwnd,&rc); FillRect(hdc,&rc,g_bgBrush);
            RECT topBar{0,0,rc.right,TOP_BAR_H};
            HBRUSH topBrush = CreateSolidBrush(RGB(12,20,31));
            FillRect(hdc, &topBar, topBrush);
            DeleteObject(topBrush);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(170, 195, 215));
            RECT titleRc{10, 0, 360, TOP_BAR_H};
            DrawTextW(hdc, L"LAN Games Deployer", -1, &titleRc, DT_VCENTER | DT_SINGLELINE | DT_LEFT);
            RECT leftPanel{6,6+TOP_BAR_H,336,rc.bottom-30}; FillRect(hdc,&leftPanel,g_panelBrush);
            RECT rightBanner{342,6+TOP_BAR_H,rc.right-8,268+TOP_BAR_H}; FillRect(hdc,&rightBanner,g_panelBrush);
            int idx = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
            {
                if (idx >= 0 && idx < (int)g_uiGames.size()) {
                    GameEntry& ge = g_uiGames[idx];
                    int bw = rightBanner.right - rightBanner.left;
                    int bh = rightBanner.bottom - rightBanner.top;
                    HDC mem = CreateCompatibleDC(hdc);
                    HBITMAP bmp = CreateCompatibleBitmap(hdc, bw, bh);
                    HGDIOBJ oldBmp = SelectObject(mem, bmp);
                    RECT zr{0,0,bw,bh};
                    FillRect(mem, &zr, g_panelBrush);
                    Gdiplus::Graphics g(mem);
                    if (!ge.bannerPath.empty()) {
                        auto img = GetCachedImage(ge.bannerPath);
                        if (img && img->GetLastStatus() == Gdiplus::Ok) {
                            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                            g.DrawImage(img.get(), 0, 0, bw, bh);
                            if (!ge.logoPath.empty()) {
                                Gdiplus::SolidBrush dimBrush(Gdiplus::Color(128, 0, 0, 0)); // ~50% black overlay
                                g.FillRectangle(&dimBrush, 0, 0, bw, bh);
                                auto logo = GetCachedImage(ge.logoPath);
                                if (logo && logo->GetLastStatus() == Gdiplus::Ok) {
                                    UINT lw = logo->GetWidth(), lh = logo->GetHeight();
                                    if (lw > 0 && lh > 0) {
                                        int maxW = bw * 90 / 100;
                                        int maxH = bh * 70 / 100;
                                        double sx = (double)maxW / (double)lw;
                                        double sy = (double)maxH / (double)lh;
                                        double s = min(1.0, min(sx, sy));
                                        int dw = max(1, (int)(lw * s));
                                        int dh = max(1, (int)(lh * s));
                                        int dx = (bw - dw) / 2;
                                        int dy = (bh - dh) / 2 + (bh * 5 / 100);
                                        if (dy + dh > bh) dy = bh - dh;
                                        g.DrawImage(logo.get(), dx, dy, dw, dh);
                                    }
                                }
                            }
                        }
                    }
                    BitBlt(hdc, rightBanner.left, rightBanner.top, bw, bh, mem, 0, 0, SRCCOPY);
                    SelectObject(mem, oldBmp);
                    DeleteObject(bmp);
                    DeleteDC(mem);
                    RECT titleBand{rightBanner.left, rightBanner.top, rightBanner.right, rightBanner.top + 36};
                    HBRUSH tb = CreateSolidBrush(RGB(10, 18, 28));
                    FillRect(hdc, &titleBand, tb);
                    DeleteObject(tb);
                    SetTextColor(hdc, RGB(235, 244, 252));
                    RECT tr = titleBand;
                    tr.left += 10;
                    DrawTextW(hdc, ge.name.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                }
            }
            RECT sr{}, lr{}, dr{}, cvr{}, cir{}, cpr{}, csr{}, chatPanel{};
            GetWindowRect(g_search, &sr); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&sr, 2);
            GetWindowRect(g_list, &lr); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&lr, 2);
            GetWindowRect(g_details, &dr); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&dr, 2);
            GetWindowRect(g_chatView, &cvr); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&cvr, 2);
            GetWindowRect(g_chatInput, &cir); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&cir, 2);
            GetWindowRect(g_chatPeers, &cpr); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&cpr, 2);
            GetWindowRect(g_chatSelf, &csr); MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&csr, 2);
            chatPanel.left = min(cvr.left, cir.left) - 1;
            chatPanel.top = min(cvr.top, cpr.top) - 1;
            chatPanel.right = max(cir.right, cpr.right) + 1;
            chatPanel.bottom = max(cir.bottom, cpr.bottom) + 1;
            HBRUSH border = CreateSolidBrush(RGB(43, 83, 120));
            FrameRect(hdc, &sr, border);
            FrameRect(hdc, &lr, border);
            FrameRect(hdc, &chatPanel, border);
            FrameRect(hdc, &cvr, border);
            FrameRect(hdc, &cir, border);
            FrameRect(hdc, &cpr, border);
            FrameRect(hdc, &csr, border);
            DeleteObject(border);
            EndPaint(hwnd,&ps); return 0;
        }
        case WM_DESTROY:
            g_running=false;
            KillTimer(hwnd, IDT_IDLE_PRESENCE);
            g_downloadQueueStop = true;
            g_downloadQueueCv.notify_all();
            if (g_downloadQueueThread.joinable()) g_downloadQueueThread.join();
            {
                SaveCurrentSelectionState();
                WINDOWPLACEMENT wp{}; wp.length = sizeof(wp);
                if (GetWindowPlacement(hwnd, &wp)) {
                    g_cfg.winMax = (wp.showCmd == SW_MAXIMIZE);
                    RECT r = wp.rcNormalPosition;
                    g_cfg.winX = r.left; g_cfg.winY = r.top;
                    g_cfg.winW = max(800, r.right - r.left);
                    g_cfg.winH = max(600, r.bottom - r.top);
                } else {
                    RECT r{};
                    if (GetWindowRect(hwnd, &r)) {
                        g_cfg.winX = r.left; g_cfg.winY = r.top;
                        g_cfg.winW = max(800, r.right - r.left);
                        g_cfg.winH = max(600, r.bottom - r.top);
                    }
                }
                SaveConfig();
            }
            if (g_font) DeleteObject(g_font); if (g_fontBold) DeleteObject(g_fontBold); if (g_fontEmoji) DeleteObject(g_fontEmoji); if (g_fontSmall) DeleteObject(g_fontSmall); if (g_bgBrush) DeleteObject(g_bgBrush); if (g_panelBrush) DeleteObject(g_panelBrush); if (g_searchBrush) DeleteObject(g_searchBrush); if (g_scrollBrush) DeleteObject(g_scrollBrush);
            PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wParam,lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LoadLibraryW(L"Msftedit.dll");
    EnsureBundledEmojiAssets();
    EnsureFirewallRules();
    WSADATA wsa{}; WSAStartup(MAKEWORD(2,2), &wsa);
    {
        char host[256]{};
        gethostname(host, sizeof(host) - 1);
        g_hostName = A2W(std::string(host));
        if (g_myNick.empty()) g_myNick = g_hostName;
        g_instanceId = g_hostName + L"-" + std::to_wstring((unsigned long long)GetCurrentProcessId()) + L"-" + std::to_wstring((unsigned long long)AppTickCount64());
        addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) == 0) {
            for (addrinfo* p = res; p; p = p->ai_next) {
                sockaddr_in* a = (sockaddr_in*)p->ai_addr;
                std::wstring ip = A2W(IPv4ToString(a->sin_addr));
                if (!ip.empty()) g_localIps.push_back(ip);
            }
            freeaddrinfo(res);
        }
        g_localIps.push_back(L"127.0.0.1");
    }
    Gdiplus::GdiplusStartupInput gdsi;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdsi, nullptr);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS}; InitCommonControlsEx(&icc);
    WNDCLASSW ci{};
    ci.style = CS_DBLCLKS;
    ci.lpfnWndProc = ChatInputCustomProc;
    ci.hInstance = hInst;
    ci.lpszClassName = kChatInputClass;
    ci.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    ci.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassW(&ci);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"LanGamesDeployerCppWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!RegisterClassExW(&wc)) return 1;
    HWND hwnd=CreateWindowExW(WS_EX_COMPOSITED,wc.lpszClassName,kAppName,
        WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,CW_USEDEFAULT,1280,820,nullptr,nullptr,hInst,nullptr);
    if (!hwnd) return 1;
    HICON hBig = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    HICON hSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (hBig) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
    if (hSmall) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
    g_main=hwnd; ShowWindow(hwnd,nCmdShow); UpdateWindow(hwnd);
    MSG msg{}; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (!IsWindowsXpFamily()) {
        CleanupGdiplusCaches();
        if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
    }
    WSACleanup();
    CoUninitialize();
    return 0;
}
