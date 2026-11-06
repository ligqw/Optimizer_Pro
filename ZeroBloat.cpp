#pragma execution_character_set("utf-8")
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wincrypt.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <wbemidl.h>
#include <comdef.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <intrin.h>
#include <srrestoreptapi.h>
#include <taskschd.h>
#include <wintrust.h>
#include <softpub.h>

#include <cstdio>
#include <cwchar>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <algorithm>
#include <memory>
#include <random>
#include <array>
#include <unordered_map>
#include <future>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "SrClient.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")

#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")
// Принудительная статическая линковка CRT для работы без VCRUNTIME140.dll
#pragma comment(linker, "/NODEFAULTLIB:MSVCRT")
#pragma comment(linker, "/NODEFAULTLIB:MSVCRTD")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

// Программное создание иконки молнии (чтобы не требовать внешних файлов)
static HICON CreateLightningIcon(int size) {
    HDC hdc = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateCompatibleBitmap(GetDC(nullptr), size, size);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
    RECT rc = { 0, 0, size, size };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    // Рисуем желтую молнию
    POINT pts[] = {
        { size * 50 / 100, size * 10 / 100 },{ size * 20 / 100, size * 55 / 100 },
        { size * 45 / 100, size * 55 / 100 },{ size * 30 / 100, size * 90 / 100 },
        { size * 80 / 100, size * 40 / 100 },{ size * 55 / 100, size * 40 / 100 },
        { size * 70 / 100, size * 10 / 100 }
    };
    HBRUSH yellow = CreateSolidBrush(RGB(255, 200, 0));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 200, 0));
    HGDIOBJ oldBr = SelectObject(hdc, yellow);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Polygon(hdc, pts, 7);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(yellow);
    DeleteObject(pen);
    SelectObject(hdc, oldBmp);
    DeleteDC(hdc);

    HBITMAP hMask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO ii = { 0 };
    ii.fIcon = TRUE;
    ii.hbmMask = hMask;
    ii.hbmColor = hBmp;
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hBmp);
    DeleteObject(hMask);
    return hIcon;
}

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE h) noexcept : h_(h) {}
    UniqueHandle(UniqueHandle&& o) noexcept : h_(o.release()) {}
    UniqueHandle& operator=(UniqueHandle&& o) noexcept { if (this != &o) reset(o.release()); return *this; }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    ~UniqueHandle() { reset(); }
    void reset(HANDLE h = nullptr) noexcept { if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); h_ = h; }
    HANDLE get() const noexcept { return h_; }
    HANDLE release() noexcept { HANDLE t = h_; h_ = nullptr; return t; }
    bool valid() const noexcept { return h_ && h_ != INVALID_HANDLE_VALUE; }
    explicit operator bool() const noexcept { return valid(); }
private: HANDLE h_ = nullptr;
};

struct RegKeyCloser { void operator()(HKEY k) const noexcept { if (k) RegCloseKey(k); } };
using UniqueRegKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegKeyCloser>;

struct ScHandle {
    SC_HANDLE h = nullptr;
    ScHandle() = default;
    explicit ScHandle(SC_HANDLE v) noexcept : h(v) {}
    ScHandle(ScHandle&& o) noexcept : h(o.h) { o.h = nullptr; }
    ScHandle& operator=(ScHandle&& o) noexcept { if (this != &o) { reset(); h = o.h; o.h = nullptr; } return *this; }
    ScHandle(const ScHandle&) = delete;
    ScHandle& operator=(const ScHandle&) = delete;
    ~ScHandle() { reset(); }
    void reset() noexcept { if (h) { CloseServiceHandle(h); h = nullptr; } }
    SC_HANDLE get() const noexcept { return h; }
    explicit operator bool() const noexcept { return h != nullptr; }
};

struct WinHttpHandle {
    HINTERNET h = nullptr;
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET v) noexcept : h(v) {}
    WinHttpHandle(WinHttpHandle&& o) noexcept : h(o.h) { o.h = nullptr; }
    WinHttpHandle& operator=(WinHttpHandle&& o) noexcept { if (this != &o) { reset(); h = o.h; o.h = nullptr; } return *this; }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    ~WinHttpHandle() { reset(); }
    void reset() noexcept { if (h) { WinHttpCloseHandle(h); h = nullptr; } }
    HINTERNET get() const noexcept { return h; }
    explicit operator bool() const noexcept { return h != nullptr; }
};

struct PdhQueryGuard {
    PDH_HQUERY q = nullptr;
    PdhQueryGuard() = default;
    explicit PdhQueryGuard(PDH_HQUERY v) noexcept : q(v) {}
    ~PdhQueryGuard() { if (q) PdhCloseQuery(q); }
    PdhQueryGuard(const PdhQueryGuard&) = delete;
    PdhQueryGuard& operator=(const PdhQueryGuard&) = delete;
};

class AlignedBuffer {
public:
    explicit AlignedBuffer(size_t bytes) : w_((bytes + 7) / 8, 0) {}
    void* data() noexcept { return w_.data(); }
    template <typename T> T* as() noexcept { return reinterpret_cast<T*>(w_.data()); }
    size_t size_bytes() const noexcept { return w_.size() * 8; }
    void resize_bytes(size_t bytes) { w_.resize((bytes + 7) / 8, 0); }
private: std::vector<std::uint64_t> w_;
};

struct TempFile {
    wchar_t path[MAX_PATH] = {};
    bool created = false;
    TempFile() = default;
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    ~TempFile() { if (created && path[0]) DeleteFileW(path); }
    bool Create(const wchar_t* prefix) {
        wchar_t dir[MAX_PATH] = {};
        if (!GetTempPathW(MAX_PATH, dir)) return false;
        if (!GetTempFileNameW(dir, prefix, 0, path)) return false;
        created = true;
        return true;
    }
};

static const wchar_t* ULTIMATE_GUID = L"e9a42b02-d5df-448d-aa00-03f14749eb61";
static const wchar_t* HIGH_PERF_GUID = L"8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
static const wchar_t* BALANCED_GUID = L"381b4222-f694-41f0-9685-ff5bb260df2e";
static const size_t LOG_MAX_SIZE = 1ull * 1024 * 1024;
static const size_t CMD_OUTPUT_LIMIT = 4ull * 1024 * 1024;

constexpr UINT WM_TRAYICON = WM_APP + 100;
constexpr UINT WM_APP_RUNUI = WM_APP + 42;
constexpr UINT WM_BOOT_DONE = WM_APP + 43;
constexpr UINT WM_APP_MSG = WM_APP + 44;

struct Paths { std::wstring SystemRoot, BackupDir, WebView2Dir, ThemesDir, AppDir, LogFile, SystemDrive; };
static Paths g_paths;
static std::mutex g_logMutex;
static std::mutex g_cleanMutex;
static std::mutex g_boostMutex;

static std::atomic<float> g_cpuUsage{ 0.0f };
static std::atomic<float> g_gpuUsage{ 0.0f };
static std::atomic<float> g_cpuTemp{ -1.0f };
static std::atomic<float> g_gpuTemp{ -1.0f };
static std::atomic<float> g_gpuFan{ -1.0f };
static std::atomic<float> g_cpuFan{ -1.0f };
static std::atomic<uint64_t> g_vramUsedMB{ 0 };
static std::atomic<uint64_t> g_vramTotalMB{ 0 };
static std::atomic<bool>   g_throttling{ false };
static std::atomic<DWORD>  g_procCount{ 0 };
static std::atomic<bool>   g_stopMonitor{ false };
static std::thread         g_monThread;

static std::atomic<float> g_appCpuUsage{ 0.0f };
static std::atomic<float> g_appGpuUsage{ 0.0f };
static std::atomic<uint64_t> g_appRamMB{ 0 };

static std::atomic<float> g_netDownMbps{ 0.0f };
static std::atomic<float> g_netUpMbps{ 0.0f };
static std::atomic<int>    g_pingMs{ -1 };
static std::atomic<float> g_batteryPercent{ -1.0f };
static std::atomic<float> g_dpcIsrTime{ 0.0f };

static std::atomic<bool>   g_portableMode{ false };
static std::atomic<bool>   g_cacheTimerEnabled{ false };

static std::wstring g_pcInfoJson = L"{\"cpu\":\"Loading...\",\"gpu\":\"Loading...\",\"ram\":\"Loading...\"}";
static std::mutex g_pcInfoMutex;

static std::wstring g_diskHealthJson = L"[]";
static std::mutex g_diskHealthMutex;

static std::atomic<wchar_t> g_activeDrive{ L'C' };
static wchar_t g_systemDriveLetter = L'C';

static std::atomic<int> g_activeAsyncTasks = 0;
static std::condition_variable g_asyncCv;
static std::mutex g_asyncMutex;

static const std::vector<std::wstring> kCriticalProcesses = {
    L"system", L"idle", L"registry", L"memory compression",
    L"csrss.exe", L"smss.exe", L"wininit.exe", L"services.exe",
    L"lsass.exe", L"winlogon.exe", L"svchost.exe", L"fontdrvhost.exe",
    L"dwm.exe", L"explorer.exe", L"audiodg.exe"
};

static bool IsCriticalProcess(const std::wstring& name) {
    std::wstring lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& c : kCriticalProcesses) if (lower == c) return true;
    return false;
}

struct UserSettings {
    bool cleanTemp = true;
    bool cleanPrefetch = true;
    bool cleanOldPrefetch = false;
    int oldPrefetchDays = 30;
    bool cleanRecycle = true;
    bool cleanThumbs = true;
    bool cleanShader = true;
    bool cleanDns = true;
    bool cleanDO = true;
    bool cleanFontCache = false;
    bool cleanStoreCache = false;
    bool cleanUpdateCache = false;
    bool clean0ByteFiles = false;
    bool cleanLaunchers = true;
    bool cleanBrowsers = true;
    bool cleanChats = true;
    bool cleanDumps = false;
    int ramAggressiveness = 3;
    bool ramClearStandby = true;
    bool ramFlushModified = true;
    bool ramClearWorkingSets = true;
    bool ramClearFileCache = true;
    bool autoBoost = true;
    bool disableFSO = false;
    bool overlayDarkTheme = true;
    bool hotkeysEnabled = true;
    bool ramAutoCleanEnabled = false;
    int ramAutoCleanThresholdPct = 90;
    int ramAutoCleanIntervalMin = 30;
    bool ramAutoCleanOnIdle = true;

    bool s50_telemetry = false;
    bool s50_cortana = false;
    bool s50_adid = false;
    bool s50_actfeed = false;
    bool s50_smartscreen = false;
    bool s50_uac = false;
    bool s50_defender = false;
    bool s50_vbs = false;
    bool s50_etw = false;
    bool s50_winupd = false;
    bool s50_gamemode = false;
    bool s50_gamedvr = false;
    bool s50_prio = false;
    bool s50_netthrottle = false;
    bool s50_nagle = false;
    bool s50_dynamictick = false;
    bool s50_hags = false;
    bool s50_mouseaccel = false;
    bool s50_power = false;
    bool s50_zero = false;
    bool s50_minecraft = false;
    bool s50_print = false;
    bool s50_icons = false;
    bool s50_pools = false;
    bool s50_paging = false;
    bool s50_memcomp = false;
    bool s50_fastdns = false;
    bool s50_rss = false;
    bool s50_resetnet = false;
    bool s50_ping = false;
    bool s50_services = false;
    bool s50_ctxmenu = false;
    bool s50_usb = false;
    bool s50_pcie = false;
    bool s50_nvapi = false;
};
static UserSettings g_userSettings;
static std::mutex g_settingsMutex;

template <typename... Args>
static std::wstring Fmt(const wchar_t* fmt, Args... args) {
    wchar_t buf[1024] = {};
    int n = _snwprintf_s(buf, _TRUNCATE, fmt, args...);
    if (n < 0) return std::wstring(buf);
    return std::wstring(buf, static_cast<size_t>(n));
}

static std::wstring FloatToJsonStr(float v) {
    if (std::isnan(v) || std::isinf(v)) return L"0.0";
    wchar_t buf[64] = {};
    int n = swprintf_s(buf, L"%.2f", v);
    if (n < 0) return L"0.0";
    for (int i = 0; i < n; ++i) if (buf[i] == L',') buf[i] = L'.';
    return std::wstring(buf, n);
}

static std::wstring GetEnvW(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (!n) return L"";
    std::wstring s(n, L'\0');
    DWORD got = GetEnvironmentVariableW(name, &s[0], n);
    if (!got || got >= n) return L"";
    s.resize(got);
    return s;
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::string OemToUtf8(const std::string& oem) {
    if (oem.empty()) return "";
    int wlen = MultiByteToWideChar(CP_OEMCP, 0, oem.data(), (int)oem.size(), nullptr, 0);
    if (wlen <= 0) return "";
    std::wstring w(wlen, 0);
    MultiByteToWideChar(CP_OEMCP, 0, oem.data(), (int)oem.size(), &w[0], wlen);
    return WideToUtf8(w);
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static int SafeStoi(const std::wstring& s, int def = 0) {
    try { return std::stoi(s); }
    catch (...) { return def; }
}

static std::wstring JsonEscape(const std::wstring& s) {
    std::wstring o; o.reserve(s.size() + 8);
    for (wchar_t c : s) {
        switch (c) {
        case L'"':  o += L"\\\""; break;
        case L'\\': o += L"\\\\"; break;
        case L'\n': o += L"\\n";  break;
        case L'\r': o += L"\\r";  break;
        case L'\t': o += L"\\t";  break;
        default: if (c >= 0x20) o += c; else o += Fmt(L"\\u%04x", (unsigned)c);
        }
    }
    return o;
}
static std::wstring JsonStr(const std::wstring& s) { return L"\"" + JsonEscape(s) + L"\""; }

struct CpuFeatures {
    bool sse41 = false, sse42 = false, popcnt = false;
    bool avx = false, fma3 = false, avx2 = false;
    bool bmi1 = false, bmi2 = false, avx512 = false;
};

static CpuFeatures GetCpuFeatures() {
    CpuFeatures f; int info[4];
    __cpuid(info, 0);
    int nIds = info[0];
    if (nIds >= 1) {
        __cpuidex(info, 1, 0);
        f.sse41 = (info[2] & (1 << 19)) != 0;
        f.sse42 = (info[2] & (1 << 20)) != 0;
        f.popcnt = (info[2] & (1 << 23)) != 0;
        f.fma3 = (info[2] & (1 << 12)) != 0;
        bool osxsave = (info[2] & (1 << 27)) != 0;
        bool avxHw = (info[2] & (1 << 28)) != 0;
        if (osxsave && avxHw) {
            unsigned long long xcr0 = _xgetbv(0);
            if ((xcr0 & 0x6) == 0x6) f.avx = true;
        }
    }
    if (nIds >= 7) {
        __cpuidex(info, 7, 0);
        f.bmi1 = (info[1] & (1 << 3)) != 0;
        f.avx2 = (info[1] & (1 << 5)) != 0;
        f.bmi2 = (info[1] & (1 << 8)) != 0;
        bool avx512fHw = (info[1] & (1 << 16)) != 0;
        if (avx512fHw && f.avx) {
            unsigned long long xcr0 = _xgetbv(0);
            if ((xcr0 & 0xE0) == 0xE0) f.avx512 = true;
        }
    }
    return f;
}

static void InitPaths() {
    g_paths.SystemRoot = GetEnvW(L"SystemRoot");
    if (g_paths.SystemRoot.empty()) g_paths.SystemRoot = L"C:\\Windows";
    g_paths.SystemDrive = GetEnvW(L"SystemDrive");
    if (g_paths.SystemDrive.empty()) g_paths.SystemDrive = L"C:";
    if (!g_paths.SystemDrive.empty()) g_systemDriveLetter = g_paths.SystemDrive[0];
    g_activeDrive = g_systemDriveLetter;
    g_paths.SystemDrive += L"\\";

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::error_code ec{};
    g_paths.AppDir = fs::path(exePath).parent_path().wstring();
    g_paths.ThemesDir = g_paths.AppDir + L"\\themes";
    fs::create_directories(g_paths.ThemesDir, ec);

    if (g_portableMode.load()) {
        g_paths.BackupDir = g_paths.AppDir + L"\\data";
        g_paths.WebView2Dir = g_paths.AppDir + L"\\WebView2Data";
    }
    else {
        std::wstring local = GetEnvW(L"LOCALAPPDATA");
        if (local.empty()) {
            wchar_t b[MAX_PATH] = {};
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, b))) local = b;
            else local = g_paths.SystemRoot + L"\\Temp";
        }
        g_paths.BackupDir = local + L"\\OptimizerPro";
        g_paths.WebView2Dir = g_paths.BackupDir + L"\\WebView2";
    }
    fs::create_directories(g_paths.BackupDir, ec);
    fs::create_directories(g_paths.WebView2Dir, ec);
    g_paths.LogFile = g_paths.BackupDir + L"\\optimization_log.txt";
}

static std::mutex g_sinkMutex;
static std::function<void(const std::wstring&)> g_uiLogSink;
static std::function<void(int, const std::wstring&)> g_uiProgressSink;

static void WriteLogFile(const std::wstring& text) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    try {
        std::error_code ec{};
        auto sz = fs::file_size(g_paths.LogFile, ec);
        if (!ec && sz > LOG_MAX_SIZE) {
            fs::remove(g_paths.LogFile + L".old", ec);
            fs::rename(g_paths.LogFile, g_paths.LogFile + L".old", ec);
        }
        std::ofstream f(g_paths.LogFile.c_str(), std::ios::app | std::ios::binary);
        if (!f) return;
        std::string s = WideToUtf8(text) + "\r\n";
        f.write(s.data(), (std::streamsize)s.size());
    }
    catch (...) {}
}

static void Print(const std::wstring& text) {
    WriteLogFile(text);
    std::function<void(const std::wstring&)> sink;
    { std::lock_guard<std::mutex> lk(g_sinkMutex); sink = g_uiLogSink; }
    if (sink) { try { sink(text); } catch (...) {} }
}

static void SendProgress(int percent, const std::wstring& msg) {
    std::function<void(int, const std::wstring&)> sink;
    { std::lock_guard<std::mutex> lk(g_sinkMutex); sink = g_uiProgressSink; }
    if (sink) { try { sink(percent, msg); } catch (...) {} }
}

struct CmdResult {
    DWORD exitCode = (DWORD)-1;
    std::string out, err;
    bool timedOut = false;
};

static std::string ReadFileCapped(const wchar_t* path, size_t maxBytes) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string s(maxBytes, '\0');
    f.read(&s[0], (std::streamsize)maxBytes);
    s.resize((size_t)f.gcount());
    return s;
}

static CmdResult RunCmdImpl(const std::wstring& cmd, int timeoutSec) {
    CmdResult res;
    TempFile outFile, errFile;
    if (!outFile.Create(L"op")) return res;
    if (!errFile.Create(L"op")) return res;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    UniqueHandle hOut(CreateFileW(outFile.path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    UniqueHandle hErr(CreateFileW(errFile.path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    UniqueHandle hNul(CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr));
    if (!hOut || !hErr || !hNul) return res;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hNul.get();
    si.hStdOutput = hOut.get();
    si.hStdError = hErr.get();
    UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jli{};
        jli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &jli, sizeof(jli));
    }

    std::wstring cl = L"cmd.exe /d /s /c \"" + cmd + L"\"";

    std::vector<wchar_t> clbuf(cl.begin(), cl.end());
    clbuf.push_back(L'\0');
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, clbuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi);
    UniqueHandle hProcess(pi.hProcess);
    UniqueHandle hThread(pi.hThread);
    if (!ok) return res;
    if (job) AssignProcessToJobObject(job.get(), hProcess.get());
    ResumeThread(hThread.get());
    hThread.reset(); hOut.reset(); hErr.reset(); hNul.reset();
    if (timeoutSec <= 0) timeoutSec = 1;
    DWORD waitMs = (DWORD)timeoutSec * 1000u;
    DWORD wr = WaitForSingleObject(hProcess.get(), waitMs);
    if (wr == WAIT_TIMEOUT) {
        res.timedOut = true;
        if (job) TerminateJobObject(job.get(), 1); else TerminateProcess(hProcess.get(), 1);
        WaitForSingleObject(hProcess.get(), 5000);
    }
    GetExitCodeProcess(hProcess.get(), &res.exitCode);
    res.out = OemToUtf8(ReadFileCapped(outFile.path, CMD_OUTPUT_LIMIT));
    res.err = OemToUtf8(ReadFileCapped(errFile.path, CMD_OUTPUT_LIMIT));
    return res;
}

static bool RunCmd(const std::wstring& cmd, const std::wstring& text = L"", bool check = true, int timeoutSec = 300, std::initializer_list<DWORD> allowed = { 0 }) {
    if (!text.empty()) Print(L"  [+] " + text + L"...");
    CmdResult r = RunCmdImpl(cmd, timeoutSec);
    if (r.timedOut) { Print(L"      └─ [TIMEOUT]"); return false; }
    for (DWORD a : allowed) {
        if (r.exitCode == a) { if (!text.empty()) Print(L"      └─ [OK]"); return true; }
    }
    std::string d = r.err.empty() ? r.out : r.err;
    if (d.empty()) d = "Code " + std::to_string(r.exitCode);
    if (d.size() > 200) d.resize(200);
    Print(std::wstring(L"      └─ [") + (check ? L"ERROR" : L"WARN") + L"] " + Utf8ToWide(d));
    return false;
}

static DWORD CountProcesses() {
    DWORD c = 0;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) { do { ++c; } while (Process32NextW(snap.get(), &pe)); }
    return c;
}

static bool EnablePrivilege(const wchar_t* priv) {
    UniqueHandle tok; HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &raw)) return false;
    tok.reset(raw);
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueW(nullptr, priv, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    return AdjustTokenPrivileges(tok.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr) != FALSE;
}

static void ClearKernelPools() {
    EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME);
    using NtSetSystemInformation_t = NTSTATUS(WINAPI*)(INT, PVOID, ULONG);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    auto NtSetSystemInformation = (NtSetSystemInformation_t)GetProcAddress(ntdll, "NtSetSystemInformation");
    if (!NtSetSystemInformation) return;
    int command = 2;
    NtSetSystemInformation(0x50, &command, sizeof(command));
}

static int DefragmentMemory(int& freedMB) {
    freedMB = 0; EnablePrivilege(SE_DEBUG_NAME);
    MEMORYSTATUSEX before{}; before.dwLength = sizeof(before); GlobalMemoryStatusEx(&before);
    int count = 0;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (pe.th32ProcessID <= 4 || IsCriticalProcess(pe.szExeFile)) continue;
            UniqueHandle h(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pe.th32ProcessID));
            if (h) {
                SetProcessWorkingSetSizeEx(h.get(), (SIZE_T)-1, (SIZE_T)-1, 0);
                if (EmptyWorkingSet(h.get())) ++count;
            }
        } while (Process32NextW(snap.get(), &pe));
    }
    MEMORYSTATUSEX after{}; after.dwLength = sizeof(after); GlobalMemoryStatusEx(&after);
    freedMB = (int)(((int64_t)after.ullAvailPhys - (int64_t)before.ullAvailPhys) / (1024 * 1024));
    return count;
}

static uint64_t PurgeStandbyEnhanced() {
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }

    MEMORYSTATUSEX before{ sizeof(before) };
    GlobalMemoryStatusEx(&before);

    EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME);
    EnablePrivilege(SE_INCREASE_QUOTA_NAME);
    EnablePrivilege(SE_DEBUG_NAME);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        using NtSetSysInfo_t = NTSTATUS(WINAPI*)(INT, PVOID, ULONG);
        auto fn = (NtSetSysInfo_t)GetProcAddress(ntdll, "NtSetSystemInformation");
        if (fn) {
            if (s.ramFlushModified) {
                int cmd = 3;
                fn(0x50, &cmd, sizeof(cmd));
            }
            if (s.ramClearStandby) {
                int cmd = 4;
                fn(0x50, &cmd, sizeof(cmd));
            }
        }
    }

    if (s.ramClearFileCache) {
        SetSystemFileCacheSize((SIZE_T)-1, (SIZE_T)-1, 0);
    }

    if (s.ramClearWorkingSets) {
        UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (snap) {
            PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap.get(), &pe)) {
                do {
                    if (pe.th32ProcessID <= 4 || IsCriticalProcess(pe.szExeFile)) continue;
                    UniqueHandle h(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_QUOTA, FALSE, pe.th32ProcessID));
                    if (h) EmptyWorkingSet(h.get());
                } while (Process32NextW(snap.get(), &pe));
            }
        }
    }

    ClearKernelPools();

    MEMORYSTATUSEX after{ sizeof(after) };
    GlobalMemoryStatusEx(&after);
    return after.ullAvailPhys > before.ullAvailPhys ? after.ullAvailPhys - before.ullAvailPhys : 0;
}

static void UpdateSelfMetrics() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        g_appRamMB = pmc.WorkingSetSize / (1024 * 1024);

    static ULONGLONG lastProcTime = 0, lastSysTime = 0;
    FILETIME create, exit, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
        ULONGLONG procTime = k.QuadPart + u.QuadPart;
        ULONGLONG sysTime = GetTickCount64() * 10000;
        if (lastSysTime > 0 && sysTime > lastSysTime) {
            ULONGLONG procDiff = procTime - lastProcTime;
            ULONGLONG sysDiff = sysTime - lastSysTime;
            SYSTEM_INFO si; GetSystemInfo(&si);
            float val = (float)((double)procDiff / (double)sysDiff / si.dwNumberOfProcessors * 100.0);
            g_appCpuUsage = std::clamp(val, 0.0f, 100.0f);
        }
        lastSysTime = sysTime; lastProcTime = procTime;
    }
}

static const long kConnectMaxWait = 0x80;
class WmiConnection {
public:
    bool Open(const wchar_t* ns) {
        ComPtr<IWbemLocator> loc;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&loc));
        if (FAILED(hr)) return false;
        ComPtr<IWbemServices> svc;
        hr = loc->ConnectServer(_bstr_t(ns), nullptr, nullptr, nullptr, kConnectMaxWait, nullptr, nullptr, &svc);
        if (FAILED(hr)) return false;
        hr = CoSetProxyBlanket(svc.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) return false;
        svc_ = svc;
        return true;
    }
    IWbemServices* get() const noexcept { return svc_.Get(); }
private: ComPtr<IWbemServices> svc_;
};

static bool WmiQueryFirstString(IWbemServices* svc, const wchar_t* wql, const wchar_t* prop, std::wstring& out) {
    if (!svc) return false;
    ComPtr<IEnumWbemClassObject> en;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
    if (FAILED(hr) || !en) return false;
    ComPtr<IWbemClassObject> obj; ULONG ret = 0;
    hr = en->Next(4000, 1, &obj, &ret);
    if (FAILED(hr) || ret == 0 || !obj) return false;
    VARIANT v; VariantInit(&v);
    bool ok = false;
    if (SUCCEEDED(obj->Get(prop, 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR && v.bstrVal) {
        out = v.bstrVal; ok = true;
    }
    VariantClear(&v);
    obj.Reset();
    return ok;
}

static std::wstring GetDiskHealthJson(IWbemServices* svc) {
    std::wstring result = L"[";
    if (svc) {
        ComPtr<IEnumWbemClassObject> en;
        HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"SELECT FriendlyName, HealthStatus, MediaType, Size FROM MSFT_PhysicalDisk"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
        if (SUCCEEDED(hr) && en) {
            bool first = true;
            for (;;) {
                ComPtr<IWbemClassObject> obj; ULONG ret = 0;
                hr = en->Next(4000, 1, &obj, &ret);
                if (FAILED(hr) || ret == 0 || !obj) break;
                VARIANT vName, vHealth, vMedia, vSize;
                VariantInit(&vName); VariantInit(&vHealth); VariantInit(&vMedia); VariantInit(&vSize);
                std::wstring name = L"Unknown Disk", media = L"Unknown";
                int health = 0; uint64_t sz = 0;
                if (SUCCEEDED(obj->Get(L"FriendlyName", 0, &vName, nullptr, nullptr)) && vName.vt == VT_BSTR) name = vName.bstrVal;
                if (SUCCEEDED(obj->Get(L"HealthStatus", 0, &vHealth, nullptr, nullptr))) {
                    if (vHealth.vt == VT_I4) health = vHealth.lVal;
                    else if (vHealth.vt == VT_UI4) health = vHealth.ulVal;
                }
                if (SUCCEEDED(obj->Get(L"MediaType", 0, &vMedia, nullptr, nullptr))) {
                    int mt = (vMedia.vt == VT_I4) ? vMedia.lVal : (vMedia.vt == VT_UI4 ? vMedia.ulVal : 0);
                    media = (mt == 3) ? L"HDD" : (mt == 4) ? L"SSD" : (mt == 5) ? L"SCM" : L"Unspecified";
                }
                if (SUCCEEDED(obj->Get(L"Size", 0, &vSize, nullptr, nullptr)) && vSize.vt == VT_UI8) sz = vSize.ullVal;

                if (!first) result += L",";
                result += L"{\"model\":" + JsonStr(name) + L",\"media\":" + JsonStr(media) + L",\"status\":" + std::to_wstring(health) +
                    L",\"size\":" + std::to_wstring(sz) + L",\"temp\":-1,\"wear\":-1}";
                first = false;
                VariantClear(&vName); VariantClear(&vHealth); VariantClear(&vMedia); VariantClear(&vSize);
                obj.Reset();
            }
        }
    }
    result += L"]";
    return result;
}

static bool WmiQueryMaxThermalZone(IWbemServices* svc, float& celsius) {
    celsius = -1;
    if (!svc) return false;
    ComPtr<IEnumWbemClassObject> en;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
    if (FAILED(hr) || !en) return false;
    float maxT = 0;
    for (;;) {
        if (g_stopMonitor) break;
        ComPtr<IWbemClassObject> obj; ULONG ret = 0;
        hr = en->Next(4000, 1, &obj, &ret);
        if (FAILED(hr) || ret == 0 || !obj) break;
        VARIANT v{}; VariantInit(&v);
        if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &v, nullptr, nullptr))) {
            float c = -1;
            if (v.vt == VT_I4) c = (float)((v.lVal / 10.0) - 273.15);
            else if (v.vt == VT_UI4) c = (float)((v.ulVal / 10.0) - 273.15);
            if (c > maxT && c > 0 && c < 150) maxT = c;
        }
        VariantClear(&v);
        obj.Reset();
    }
    if (maxT > 0) { celsius = maxT; return true; }
    return false;
}

static ULONGLONG FileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

class CpuSampler {
public:
    float Sample() {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0f;
        ULONGLONG i = FileTimeToULL(idle), k = FileTimeToULL(kernel), u = FileTimeToULL(user);
        if (first_) { prevIdle_ = i; prevKernel_ = k; prevUser_ = u; first_ = false; return 0.0f; }
        ULONGLONG total = (k - prevKernel_) + (u - prevUser_);
        ULONGLONG idleDiff = i - prevIdle_;
        prevIdle_ = i; prevKernel_ = k; prevUser_ = u;
        if (total == 0) return 0.0f;
        float val = (float)((double)(total - idleDiff) * 100.0 / (double)total);
        return std::clamp(val, 0.0f, 100.0f);
    }
private:
    ULONGLONG prevIdle_ = 0, prevKernel_ = 0, prevUser_ = 0;
    bool first_ = true;
};

static float GetGpuUsage() {
    struct State { PdhQueryGuard guard; PDH_HCOUNTER counter = nullptr; bool ready = false; };
    static State st;
    static std::once_flag once;
    std::call_once(once, [] {
        PDH_HQUERY q = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS || !q) return;
        st.guard.q = q;
        if (PdhAddEnglishCounterW(q, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &st.counter) != ERROR_SUCCESS)
            st.counter = nullptr;
        if (!st.counter) { PdhCloseQuery(st.guard.q); st.guard.q = nullptr; return; }
        PdhCollectQueryData(q);
        st.ready = true;
        });
    if (!st.ready) return -1.0f;
    if (PdhCollectQueryData(st.guard.q) != ERROR_SUCCESS) return -1.0f;
    DWORD bufSize = 0, itemCount = 0;
    PDH_STATUS s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
    if (s == ERROR_SUCCESS) return 0.0f;
    if (s != (PDH_STATUS)0x800007D2L || bufSize == 0) return -1.0f; // PDH_MORE_DATA
    for (int attempt = 0; attempt < 3; ++attempt) {
        AlignedBuffer buf(bufSize); DWORD bytes = bufSize, count = itemCount;
        s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bytes, &count, buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>());
        if (s == (PDH_STATUS)0x800007D2L) { bufSize = bytes; continue; } // PDH_MORE_DATA
        if (s != ERROR_SUCCESS) return -1.0f;
        auto* items = buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>();
        double sum = 0.0;
        for (DWORD i = 0; i < count; ++i) {
            PDH_STATUS cs = items[i].FmtValue.CStatus;
            if (cs == (PDH_STATUS)0x00000000L || cs == (PDH_STATUS)0x00000001L) sum += items[i].FmtValue.doubleValue; // VALID / NEW
        }
        return (float)std::clamp(sum, 0.0, 100.0);
    }
    return -1.0f;
}

static void UpdateGpuMetrics() {
    static bool checked = false;
    static bool nvSmiAvailable = false;
    static std::wstring nvSmiPath;
    static ULONGLONG lastQuery = 0;

    if (!checked) {
        wchar_t path[MAX_PATH] = {};
        if (SearchPathW(nullptr, L"nvidia-smi.exe", nullptr, MAX_PATH, path, nullptr) > 0) {
            nvSmiAvailable = true;
            nvSmiPath = path;
        }
        checked = true;
    }

    ULONGLONG now = GetTickCount64();
    if (nvSmiAvailable && !nvSmiPath.empty() && (now - lastQuery > 5000)) {
        lastQuery = now;
        std::wstring cmd = L"\"" + nvSmiPath + L"\" --query-gpu=temperature.gpu,fan.speed,utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits";
        CmdResult r = RunCmdImpl(cmd, 5);
        if (r.exitCode == 0 && !r.out.empty()) {
            std::istringstream iss(r.out);
            std::string line;
            double maxTemp = -1, maxFan = -1, maxUtil = -1;
            uint64_t maxUsed = 0, maxTotal = 0;
            while (std::getline(iss, line)) {
                if (line.empty()) continue;
                std::vector<std::string> parts;
                std::string part;
                std::istringstream lss(line);
                while (std::getline(lss, part, ',')) {
                    while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) part.erase(0, 1);
                    while (!part.empty() && (part.back() == ' ' || part.back() == '\r' || part.back() == '\t')) part.pop_back();
                    parts.push_back(part);
                }
                if (parts.size() >= 5) {
                    try {
                        double t = std::stod(parts[0]);
                        double f = std::stod(parts[1]);
                        double u = std::stod(parts[2]);
                        uint64_t used = (uint64_t)std::stoll(parts[3]);
                        uint64_t total = (uint64_t)std::stoll(parts[4]);
                        if (t > maxTemp) maxTemp = t;
                        if (f > maxFan) maxFan = f;
                        if (u > maxUtil) maxUtil = u;
                        maxUsed += used;
                        maxTotal += total;
                    }
                    catch (...) {}
                }
            }
            if (maxTemp > 0) g_gpuTemp = (float)maxTemp;
            if (maxFan >= 0) g_gpuFan = (float)maxFan;
            if (maxUtil >= 0) g_gpuUsage = (float)maxUtil;
            if (maxTotal > 0) { g_vramUsedMB = maxUsed; g_vramTotalMB = maxTotal; }
        }
    }
    if (g_batteryPercent.load() < 0) {
        SYSTEM_POWER_STATUS sps{};
        if (GetSystemPowerStatus(&sps) && sps.BatteryLifePercent != 255) {
            g_batteryPercent = (float)sps.BatteryLifePercent;
        }
    }
}

static void UpdatePingMonitor() {
    static ULONGLONG lastPing = 0;
    ULONGLONG now = GetTickCount64();
    if (now - lastPing < 5000) return;
    lastPing = now;

    std::thread([]() {
        HANDLE hIcmp = IcmpCreateFile();
        if (hIcmp == INVALID_HANDLE_VALUE) return;

        IPAddr ip = 0;
        InetPtonA(AF_INET, "8.8.8.8", &ip);

        char SendData[32] = "DataBuffer";
        DWORD replySize = sizeof(ICMP_ECHO_REPLY) + 32;
        LPVOID ReplyBuffer = malloc(replySize);
        if (ReplyBuffer) {
            DWORD dwRet = IcmpSendEcho(hIcmp, ip, SendData, sizeof(SendData), NULL, ReplyBuffer, replySize, 1000);
            if (dwRet != 0) {
                PICMP_ECHO_REPLY pIpe = (PICMP_ECHO_REPLY)ReplyBuffer;
                g_pingMs = pIpe->RoundTripTime;
            }
            else {
                g_pingMs = -1;
            }
            free(ReplyBuffer);
        }
        IcmpCloseHandle(hIcmp);
        }).detach();
}

static void UpdateThrottleDetection() {
    struct State { PdhQueryGuard q; PDH_HCOUNTER counter = nullptr; bool ready = false; };
    static State st;
    static std::once_flag once;
    std::call_once(once, [] {
        PDH_HQUERY q = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) == ERROR_SUCCESS) {
            st.q.q = q;
            if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% Processor Performance", 0, &st.counter) == ERROR_SUCCESS) {
                PdhCollectQueryData(q);
                st.ready = true;
            }
        }
        });
    if (!st.ready) return;
    PdhCollectQueryData(st.q.q);
    PDH_FMT_COUNTERVALUE val;
    if (PdhGetFormattedCounterValue(st.counter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
        if (val.doubleValue < 60.0 && g_cpuUsage.load() > 50.0f) g_throttling = true;
        else g_throttling = false;
    }
}

static void UpdateDpcIsrMetrics() {
    struct State { PdhQueryGuard q; PDH_HCOUNTER dpc = nullptr; PDH_HCOUNTER isr = nullptr; bool ready = false; };
    static State st;
    static std::once_flag once;
    std::call_once(once, [] {
        PDH_HQUERY q = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) return;
        st.q.q = q;
        if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% DPC Time", 0, &st.dpc) != ERROR_SUCCESS) st.dpc = nullptr;
        if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% Interrupt Time", 0, &st.isr) != ERROR_SUCCESS) st.isr = nullptr;
        if (st.dpc || st.isr) {
            PdhCollectQueryData(q);
            st.ready = true;
        }
        });
    if (!st.ready) return;
    PdhCollectQueryData(st.q.q);
    PDH_FMT_COUNTERVALUE valDpc, valIsr;
    double total = 0;
    if (st.dpc && PdhGetFormattedCounterValue(st.dpc, PDH_FMT_DOUBLE, nullptr, &valDpc) == ERROR_SUCCESS) total += valDpc.doubleValue;
    if (st.isr && PdhGetFormattedCounterValue(st.isr, PDH_FMT_DOUBLE, nullptr, &valIsr) == ERROR_SUCCESS) total += valIsr.doubleValue;
    g_dpcIsrTime = (float)total;
}

static std::wstring GetPcInfo(IWbemServices* svc) {
    std::wstring cpuName = L"Unknown CPU", gpuName = L"Unknown GPU", ramTotal = L"0 GB";
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
    ramTotal = Fmt(L"%.1f GB", (double)ms.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
    try {
        if (svc) {
            WmiQueryFirstString(svc, L"SELECT Name FROM Win32_Processor", L"Name", cpuName);
            WmiQueryFirstString(svc, L"SELECT Name FROM Win32_VideoController WHERE Name IS NOT NULL", L"Name", gpuName);
        }
    }
    catch (...) {}
    return L"{\"cpu\":" + JsonStr(cpuName) + L",\"gpu\":" + JsonStr(gpuName) + L",\"ram\":" + JsonStr(ramTotal) + L"}";
}

static bool GetTemperatures(IWbemServices* svc, float& cpuTemp, float& gpuTemp) {
    cpuTemp = -1.0f; gpuTemp = g_gpuTemp.load();
    try {
        if (svc) {
            float t = -1;
            if (WmiQueryMaxThermalZone(svc, t)) { cpuTemp = t; return true; }
        }
    }
    catch (...) {}
    return false;
}

static void UpdateNetworkSpeed() {
    static ULONGLONG lastTick = 0;
    static ULONGLONG lastIn = 0, lastOut = 0;
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR) {
        ULONGLONG totalIn = 0, totalOut = 0;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            if (table->Table[i].Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (table->Table[i].Type == IF_TYPE_ETHERNET_CSMACD || table->Table[i].Type == IF_TYPE_IEEE80211) {
                totalIn += table->Table[i].InOctets;
                totalOut += table->Table[i].OutOctets;
            }
        }
        ULONGLONG now = GetTickCount64();
        if (lastTick > 0) {
            double sec = (now - lastTick) / 1000.0;
            if (sec > 0.1) {
                if (totalIn >= lastIn) g_netDownMbps = (float)(((totalIn - lastIn) * 8.0 / 1e6) / sec);
                else g_netDownMbps = 0.0f;
                if (totalOut >= lastOut) g_netUpMbps = (float)(((totalOut - lastOut) * 8.0 / 1e6) / sec);
                else g_netUpMbps = 0.0f;
            }
        }
        lastTick = now; lastIn = totalIn; lastOut = totalOut;
        FreeMibTable(table);
    }
}

static void GameBooster(bool enable);
static void ApplySmartGameMode(bool enable) {
    static std::map<DWORD, DWORD> originalPriorities;
    std::lock_guard<std::mutex> lk(g_boostMutex);
    if (!enable) {
        EnablePrivilege(SE_DEBUG_NAME);
        for (auto& pair : originalPriorities) {
            UniqueHandle h(OpenProcess(PROCESS_SET_INFORMATION, FALSE, pair.first));
            if (h) SetPriorityClass(h.get(), pair.second);
        }
        originalPriorities.clear();
        return;
    }
    EnablePrivilege(SE_DEBUG_NAME);
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (pe.th32ProcessID <= 4 || pe.th32ProcessID == foregroundPid || IsCriticalProcess(pe.szExeFile)) continue;
            UniqueHandle h(OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
            if (h) {
                DWORD priority = GetPriorityClass(h.get());
                if (priority != BELOW_NORMAL_PRIORITY_CLASS) {
                    originalPriorities[pe.th32ProcessID] = priority;
                    SetPriorityClass(h.get(), BELOW_NORMAL_PRIORITY_CLASS);
                }
            }
        } while (Process32NextW(snap.get(), &pe));
    }
}

static void CleanCacheEnhanced();

static void MonitorThread() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool ownCom = SUCCEEDED(hr);

    WmiConnection wmiCim, wmiStorage, wmiWmi;
    wmiCim.Open(L"ROOT\\CIMV2");
    wmiStorage.Open(L"ROOT\\Microsoft\\Windows\\Storage");
    wmiWmi.Open(L"ROOT\\WMI");

    {
        std::wstring info = GetPcInfo(wmiCim.get());
        std::lock_guard<std::mutex> lk(g_pcInfoMutex); g_pcInfoJson = info;
    }
    {
        std::wstring disks = GetDiskHealthJson(wmiStorage.get());
        std::lock_guard<std::mutex> lk(g_diskHealthMutex); g_diskHealthJson = disks;
    }

    auto sleepStop = [](int ms) { for (int i = 0; i < ms && !g_stopMonitor; i += 100) std::this_thread::sleep_for(std::chrono::milliseconds(100)); };
    try {
        CpuSampler sampler;
        sleepStop(800); sampler.Sample();
        sleepStop(500);
        if (g_stopMonitor) { if (ownCom) CoUninitialize(); return; }
        g_cpuUsage = sampler.Sample();
        g_gpuUsage = GetGpuUsage();
        g_procCount = CountProcesses();
        UpdateSelfMetrics();
        UpdateGpuMetrics();
        int tick = 0;
        bool wasInGame = false;
        ULONGLONG lastCacheClean = GetTickCount64();
        ULONGLONG lastAutoRamClean = GetTickCount64();

        while (!g_stopMonitor) {
            sleepStop(1000);
            if (g_stopMonitor) break;
            g_cpuUsage = sampler.Sample();
            float gpuFromPdh = GetGpuUsage();
            if (gpuFromPdh >= 0) g_gpuUsage = gpuFromPdh;
            UpdateNetworkSpeed();
            UpdateGpuMetrics();
            UpdatePingMonitor();
            UpdateDpcIsrMetrics();
            if ((++tick % 2) == 0) g_procCount = CountProcesses();
            if ((tick % 5) == 0) UpdateThrottleDetection();
            UpdateSelfMetrics();

            if (g_cacheTimerEnabled.load()) {
                ULONGLONG now = GetTickCount64();
                if (now - lastCacheClean > 300000) {
                    lastCacheClean = now;
                    CleanCacheEnhanced();
                }
            }

            UserSettings s;
            { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }

            if (s.ramAutoCleanEnabled) {
                bool shouldClean = false;
                MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
                GlobalMemoryStatusEx(&ms);
                int usedPct = (int)((ms.ullTotalPhys - ms.ullAvailPhys) * 100 / ms.ullTotalPhys);
                if (usedPct >= s.ramAutoCleanThresholdPct) shouldClean = true;

                if (s.ramAutoCleanOnIdle) {
                    LASTINPUTINFO lii = { sizeof(lii) };
                    if (GetLastInputInfo(&lii)) {
                        DWORD idleMs = GetTickCount() - lii.dwTime;
                        if (idleMs > 60000) shouldClean = true;
                    }
                }

                ULONGLONG now = GetTickCount64();
                if (now - lastAutoRamClean > (ULONGLONG)s.ramAutoCleanIntervalMin * 60000ULL) {
                    shouldClean = true;
                }

                if (shouldClean && now - lastAutoRamClean > 60000) {
                    lastAutoRamClean = now;
                    PurgeStandbyEnhanced();
                }
            }

            if ((tick % 60) == 0) {
                std::wstring info = GetPcInfo(wmiCim.get());
                { std::lock_guard<std::mutex> lk(g_pcInfoMutex); g_pcInfoJson = info; }

                std::wstring dh = GetDiskHealthJson(wmiStorage.get());
                { std::lock_guard<std::mutex> lk(g_diskHealthMutex); g_diskHealthJson = dh; }

                float ct = -1.0f, gt = -1.0f;
                if (GetTemperatures(wmiWmi.get(), ct, gt)) { g_cpuTemp = ct; g_gpuTemp = gt; }
            }

            QUERY_USER_NOTIFICATION_STATE st;
            bool inGame = false;
            if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
                if (st == QUNS_RUNNING_D3D_FULL_SCREEN || st == QUNS_PRESENTATION_MODE) inGame = true;
            }
            if (s.autoBoost && inGame != wasInGame) {
                std::thread([](bool en) {
                    try { GameBooster(en); }
                    catch (...) {}
                    }, inGame).detach();
                wasInGame = inGame;
            }
        }
    }
    catch (...) { WriteLogFile(L"[MonitorThread] unhandled exception"); }
    if (ownCom) CoUninitialize();
}

static void CleanTemp() {
    RunCmd(L"del /f /s /q \"%TEMP%\\*\" >nul 2>&1", L"Temp", false);
    RunCmd(L"del /f /s /q \"%SystemRoot%\\Temp\\*\" >nul 2>&1", L"Windows Temp", false);
}
static void CleanPrefetch() { RunCmd(L"del /f /s /q \"%SystemRoot%\\Prefetch\\*\" >nul 2>&1", L"Prefetch", false); }
static void CleanRecycle() { RunCmd(L"PowerShell -NoProfile -Command \"Clear-RecycleBin -Force -ErrorAction SilentlyContinue\"", L"Recycle Bin", false, 120); }
static void CleanRecycleForDrive(wchar_t letter) {
    std::wstring cmd = L"PowerShell -NoProfile -Command \"Clear-RecycleBin -DriveLetter "
        + std::wstring(1, letter) + L" -Force -ErrorAction SilentlyContinue\"";
    RunCmd(cmd, L"Recycle Bin", false, 120);
}
static void CleanThumbnails() { RunCmd(L"del /f /s /q \"%LOCALAPPDATA%\\Microsoft\\Windows\\Explorer\\thumbcache_*.db\" >nul 2>&1", L"Thumbnails", false); }
static void CleanShaderCache() {
    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    RunCmd(L"del /f /s /q \"" + local + L"\\D3DSCache\\*\" >nul 2>&1", L"D3D Shader cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\NVIDIA\\DXCache\\*\" >nul 2>&1", L"NVIDIA cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\AMD\\DxCache\\*\" >nul 2>&1", L"AMD DX cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\AMD\\GLCache\\*\" >nul 2>&1", L"AMD GL cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Intel\\ShaderCache\\*\" >nul 2>&1", L"Intel cache", false);
}
static void FlushDns() { RunCmd(L"ipconfig /flushdns", L"Flush DNS", false, 30); }

static void CleanDeliveryOptimization() {
    RunCmd(L"net stop dosvc >nul 2>&1", L"Stop DO Service", false, 30);
    std::wstring path = g_paths.SystemRoot + L"\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache\\*";
    RunCmd(L"del /f /s /q \"" + path + L"\" >nul 2>&1", L"DO Cache", false, 60);
    RunCmd(L"net start dosvc >nul 2>&1", L"Start DO Service", false, 30);
}

static void CleanOldPrefetch(int days) {
    if (days <= 0) days = 30;
    std::wstring cmd = L"forfiles /p \"%SystemRoot%\\Prefetch\" /m *.pf /d -" + std::to_wstring(days) + L" /c \"cmd /c del @path\" >nul 2>&1";
    RunCmd(cmd, L"Old Prefetch", false, 60);
}

static void Clean0ByteFiles(wchar_t driveLetter) {
    std::wstring root = std::wstring(1, driveLetter) + L":\\";
    std::error_code ec{};
    try {
        if (!fs::exists(root, ec) || ec) return;
        int removed = 0;
        std::vector<std::wstring> dirs;
        if (driveLetter == g_systemDriveLetter) {
            dirs = { GetEnvW(L"TEMP"), GetEnvW(L"USERPROFILE") + L"\\Downloads" };
        }
        else {
            dirs = { root };
        }
        for (const auto& dir : dirs) {
            if (!fs::exists(dir, ec) || ec) continue;
            for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
                it != fs::recursive_directory_iterator() && removed < 500; it.increment(ec)) {
                if (g_stopMonitor) break;
                if (ec) { ec.clear(); continue; }
                try {
                    if (it->is_regular_file(ec) && !ec && it->file_size(ec) == 0) {
                        std::wstring p = it->path().wstring();
                        if (p.find(L"Windows\\System32") != std::wstring::npos || p.find(L"Windows\\SysWOW64") != std::wstring::npos) continue;
                        fs::remove(it->path(), ec);
                        if (!ec) ++removed;
                    }
                }
                catch (...) {}
            }
        }
    }
    catch (...) {}
}

static void CleanGameLaunchers() {
    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    RunCmd(L"del /f /s /q \"" + local + L"\\Steam\\htmlcache\\*\" >nul 2>&1", L"Steam Cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\EpicGamesLauncher\\Saved\\webcache\\*\" >nul 2>&1", L"Epic Cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Battle.net\\Cache\\*\" >nul 2>&1", L"Battle.net Cache", false);
}

static void CleanBrowserCaches() {
    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    RunCmd(L"del /f /s /q \"" + local + L"\\Google\\Chrome\\User Data\\Default\\Cache\\*\" >nul 2>&1", L"Chrome Cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Google\\Chrome\\User Data\\Default\\Code Cache\\*\" >nul 2>&1", L"Chrome Code Cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Microsoft\\Edge\\User Data\\Default\\Cache\\*\" >nul 2>&1", L"Edge Cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Microsoft\\Edge\\User Data\\Default\\Code Cache\\*\" >nul 2>&1", L"Edge Code Cache", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Mozilla\\Firefox\\Profiles\\*\\cache2\\*\" >nul 2>&1", L"Firefox Cache", false);
}

static void CleanChatCaches() {
    std::wstring appdata = GetEnvW(L"APPDATA");
    RunCmd(L"del /f /s /q \"" + appdata + L"\\discord\\Cache\\*\" >nul 2>&1", L"Discord Cache", false);
    RunCmd(L"del /f /s /q \"" + appdata + L"\\discord\\Code Cache\\*\" >nul 2>&1", L"Discord Code Cache", false);
    RunCmd(L"del /f /s /q \"" + appdata + L"\\Telegram Desktop\\tdata\\user_data\\cache\\*\" >nul 2>&1", L"Telegram Cache", false);
}

static void CleanMemoryDumps() {
    RunCmd(L"del /f /s /q \"%LOCALAPPDATA%\\CrashDumps\\*\" >nul 2>&1", L"User Crash Dumps", false);
    RunCmd(L"del /f /s /q \"%SystemRoot%\\Minidump\\*\" >nul 2>&1", L"System Minidumps", false);
    RunCmd(L"del /f /q \"%SystemRoot%\\MEMORY.DMP\" >nul 2>&1", L"Full Memory Dump", false);
}

static void CleanMinecraftCache() {
    std::wstring appdata = GetEnvW(L"APPDATA");
    RunCmd(L"del /f /s /q \"" + appdata + L"\\.minecraft\\logs\\*\" >nul 2>&1", L"Minecraft logs", false);
    RunCmd(L"del /f /s /q \"" + appdata + L"\\.minecraft\\crash-reports\\*\" >nul 2>&1", L"Minecraft crash reports", false);
    RunCmd(L"del /f /s /q \"" + appdata + L"\\.minecraft\\versions\\*\\*.log\" >nul 2>&1", L"Minecraft version logs", false);
    RunCmd(L"del /f /s /q \"" + appdata + L"\\.minecraft\\assets\\indexes\\*\" >nul 2>&1", L"Minecraft asset cache", false);
}

static void CleanPrintQueue() {
    RunCmd(L"net stop spooler >nul 2>&1", L"Stop spooler", false, 30);
    RunCmd(L"del /f /s /q \"%SystemRoot%\\System32\\spool\\PRINTERS\\*\" >nul 2>&1", L"Print queue", false, 30);
    RunCmd(L"net start spooler >nul 2>&1", L"Start spooler", false, 30);
}

static void CleanIconCache() {
    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    RunCmd(L"del /f /s /q \"" + local + L"\\IconCache.db\" >nul 2>&1", L"Icon cache (user)", false);
    RunCmd(L"del /f /s /q \"" + local + L"\\Microsoft\\Windows\\Explorer\\iconcache_*.db\" >nul 2>&1", L"Icon cache (explorer)", false);
    RunCmd(L"del /f /s /q \"%SystemRoot%\\System32\\iconcache_*.db\" >nul 2>&1", L"Icon cache (system)", false);
}

static void CleanCacheEnhanced() {
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
    std::lock_guard<std::mutex> lk(g_cleanMutex);
    SendProgress(5, L"Очистка Temp...");
    if (s.cleanTemp) CleanTemp();
    SendProgress(15, L"Очистка Prefetch...");
    if (s.cleanPrefetch) CleanPrefetch();
    if (s.cleanOldPrefetch) CleanOldPrefetch(s.oldPrefetchDays);
    SendProgress(25, L"Очистка корзины...");
    if (s.cleanRecycle) CleanRecycleForDrive(g_activeDrive.load());
    SendProgress(35, L"Очистка миниатюр и шейдеров...");
    if (s.cleanThumbs) CleanThumbnails();
    if (s.cleanShader) CleanShaderCache();
    if (s.cleanDns) FlushDns();
    SendProgress(50, L"Delivery Optimization...");
    if (s.cleanDO) CleanDeliveryOptimization();
    if (s.cleanFontCache) {
        RunCmd(L"net stop FontCache >nul 2>&1", L"", false, 30);
        RunCmd(L"del /f /s /q \"%SystemRoot%\\ServiceProfiles\\LocalService\\AppData\\Local\\FontCache\\*\" >nul 2>&1", L"Font cache", false, 60);
        RunCmd(L"net start FontCache >nul 2>&1", L"", false, 30);
    }
    if (s.cleanStoreCache) {
        RunCmd(L"wsreset.exe -i >nul 2>&1", L"Windows Store cache", false, 60);
    }
    if (s.cleanUpdateCache) {
        RunCmd(L"net stop wuauserv >nul 2>&1", L"", false, 30);
        RunCmd(L"del /f /s /q \"%SystemRoot%\\SoftwareDistribution\\Download\\*\" >nul 2>&1", L"Update cache", false, 120);
        RunCmd(L"net start wuauserv >nul 2>&1", L"", false, 30);
    }
    SendProgress(65, L"Очистка кэшей игр и браузеров...");
    if (s.cleanLaunchers) CleanGameLaunchers();
    if (s.cleanBrowsers) CleanBrowserCaches();
    if (s.cleanChats) CleanChatCaches();
    if (s.cleanDumps) CleanMemoryDumps();
    if (s.clean0ByteFiles) Clean0ByteFiles(g_activeDrive.load());
    SendProgress(90, L"Завершение...");
    SendProgress(100, L"Готово");
}

static void DisableTelemetry() {
    RunCmd(L"sc config DiagTrack start= disabled >nul 2>&1", L"DiagTrack disabled", false);
    RunCmd(L"sc stop DiagTrack >nul 2>&1", L"DiagTrack stopped", false);
    RunCmd(L"sc config dmwappushservice start= disabled >nul 2>&1", L"dmwappushservice", false);
    RunCmd(L"sc stop dmwappushservice >nul 2>&1", L"dmwappushservice stopped", false);
    HKEY rawKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &rawKey, nullptr) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey); DWORD v = 0;
        RegSetValueExW(key.get(), L"AllowTelemetry", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
}

static void DisableDeepServices() {
    const wchar_t* svcs[] = { L"MapsBroker", L"RetailDemo", L"DiagTrack", L"dmwappushservice", L"WSearch", L"SysMain", L"XblAuthManager", L"XblGameSave", L"XboxNetApiSvc", L"XboxGipSvc" };
    for (auto s : svcs) {
        RunCmd(L"sc config " + std::wstring(s) + L" start= disabled", L"", false);
        RunCmd(L"sc stop " + std::wstring(s), L"", false);
    }
}

static void DisableDefender() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows Defender", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = 1;
        RegSetValueExW(hKey, L"DisableAntiSpyware", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void OptimizeFSO(bool disable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = disable ? 2 : 0;
        RegSetValueExW(hKey, L"GameDVR_FSEBehaviorMode", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegSetValueExW(hKey, L"GameDVR_HonorUserFSEBehaviorMode", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void SetFastDNS() {
    RunCmd(L"powershell -NoProfile -Command \"Get-NetAdapter | Where-Object {$_.Status -eq 'Up'} | Set-DnsClientServerAddress -ServerAddresses ('1.1.1.1','1.0.0.1')\"", L"Установка Cloudflare DNS", false, 60);
}

static void OptimizeSSDTrim() {
    std::wstring sysDrive = std::wstring(1, g_systemDriveLetter);
    RunCmd(L"powershell -NoProfile -Command \"Optimize-Volume -DriveLetter " + sysDrive + L" -ReTrim -Verbose\"", L"TRIM SSD", false, 300);
}

static void SetGameMode(bool enabled) {
    HKEY rawKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &rawKey, nullptr) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey); DWORD v = enabled ? 1 : 0;
        RegSetValueExW(key.get(), L"AutoGameModeEnabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
}

static void SetClassicContextMenu(bool enableClassic) {
    if (MessageBoxW(nullptr, L"Explorer will be restarted. Continue?", L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    HKEY hKey = nullptr;
    std::wstring path = L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32";
    if (enableClassic) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)L"", sizeof(wchar_t));
            RegCloseKey(hKey);
        }
    }
    else {
        RunCmd(L"reg delete \"HKCU\\Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\" /f >nul 2>&1", L"Restore Context Menu", false);
    }
    RunCmd(L"taskkill /f /im explorer.exe >nul 2>&1", L"", false, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    RunCmd(L"start explorer.exe", L"", false, 10);
}

static void SetWin32PrioritySeparation(DWORD value) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\PriorityControl", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"Win32PrioritySeparation", 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

static void DisableDynamictick() { RunCmd(L"bcdedit /set disabledynamictick yes", L"", false, 30); }
static void DisableNetworkThrottling() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = 0xFFFFFFFF;
        RegSetValueExW(hKey, L"NetworkThrottlingIndex", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        DWORD s = 1;
        RegSetValueExW(hKey, L"SystemResponsiveness", 0, REG_DWORD, (const BYTE*)&s, sizeof(s));
        RegCloseKey(hKey);
    }
}
static void DisableNagle() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces", 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKey) != ERROR_SUCCESS) return;
    DWORD idx = 0; wchar_t sub[256] = {}; DWORD subLen = 256;
    for (;;) {
        subLen = 256;
        DWORD res = RegEnumKeyExW(hKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr);
        if (res == ERROR_NO_MORE_ITEMS) break;
        if (res != ERROR_SUCCESS) continue;

        HKEY hSub = nullptr;
        if (RegOpenKeyExW(hKey, sub, 0, KEY_SET_VALUE, &hSub) == ERROR_SUCCESS) {
            DWORD one = 1;
            RegSetValueExW(hSub, L"TcpAckFrequency", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
            RegSetValueExW(hSub, L"TCPNoDelay", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
            RegCloseKey(hSub);
        }
    }
    RegCloseKey(hKey);
}
static void CleanWinSxS() { RunCmd(L"DISM /Online /Cleanup-Image /StartComponentCleanup /ResetBase", L"WinSxS", false, 3600); }

static void UpgradeAllWinget() {
    RunCmd(L"winget upgrade --all --silent --accept-package-agreements --accept-source-agreements", L"Upgrade All", false, 3600);
}

static bool CreateRestorePoint(const std::wstring& desc) {
    RESTOREPOINTINFO pt{};
    pt.dwEventType = BEGIN_SYSTEM_CHANGE;
    pt.dwRestorePtType = MODIFY_SETTINGS;
    pt.llSequenceNumber = 0;
    wcsncpy_s(pt.szDescription, desc.c_str(), _TRUNCATE);
    STATEMGRSTATUS status{};
    if (SRSetRestorePointW(&pt, &status)) {
        if (status.nStatus == ERROR_SUCCESS) {
            pt.dwEventType = END_SYSTEM_CHANGE;
            pt.llSequenceNumber = status.llSequenceNumber;
            SRSetRestorePointW(&pt, &status);
            return true;
        }
    }
    return false;
}

static void BackupRegistry() {
    std::wstring dir = g_paths.BackupDir + L"\\RegBackup";
    std::error_code ec; fs::create_directories(dir, ec);
    RunCmd(L"reg export \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\PriorityControl\" \"" + dir + L"\\PriorityControl.reg\" /y", L"", false);
    RunCmd(L"reg export \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\" \"" + dir + L"\\SystemProfile.reg\" /y", L"", false);
    RunCmd(L"reg export \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\" \"" + dir + L"\\Advanced.reg\" /y", L"", false);
}

static void PerformLiteDebloat() {
    Print(L"=== STARTING LITE WINDOWS DEBLOAT ===");
    Print(L"[+] Creating restore point...");
    CreateRestorePoint(L"Optimizer Pro - Lite Debloat");
    Print(L"[+] Backing up registry...");
    BackupRegistry();

    Print(L"[+] Removing UWP Bloatware...");
    std::wstring psCmd = L"powershell -NoProfile -ExecutionPolicy Bypass -Command \""
        L"$bloat = @('Microsoft.BingWeather','Microsoft.GetHelp','Microsoft.Getstarted',"
        L"'Microsoft.MicrosoftOfficeHub','Microsoft.MicrosoftSolitaireCollection','Microsoft.People',"
        L"'Microsoft.SkypeApp','Microsoft.WindowsAlarms','Microsoft.WindowsCamera','Microsoft.WindowsMaps',"
        L"'Microsoft.XboxApp','Microsoft.XboxGameOverlay','Microsoft.XboxGamingOverlay',"
        L"'Microsoft.XboxIdentityProvider','Microsoft.XboxSpeechToTextOverlay','Microsoft.ZuneMusic',"
        L"'Microsoft.ZuneVideo','Microsoft.YourPhone','Microsoft.MixedReality.Portal'); "
        L"foreach ($app in $bloat) { Get-AppxPackage -AllUsers -Name $app | Remove-AppxPackage -AllUsers -ErrorAction SilentlyContinue }; "
        L"Get-AppxProvisionedPackage -Online | Where-Object { $bloat -contains $_.DisplayName } | Remove-AppxProvisionedPackage -Online -ErrorAction SilentlyContinue\"";
    RunCmd(psCmd, L"Remove UWP Apps", false, 300);
    Print(L"[+] Disabling Telemetry & Tracking...");
    DisableTelemetry();
    RunCmd(L"sc config WSearch start= disabled", L"Disable Windows Search", false);
    RunCmd(L"sc stop WSearch", L"Stop Windows Search", false);
    Print(L"[+] Applying Registry Tweaks...");
    RunCmd(L"reg add \"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search\" /v AllowCortana /t REG_DWORD /d 0 /f", L"Cortana", false);
    RunCmd(L"reg add \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo\" /v Enabled /t REG_DWORD /d 0 /f", L"Ad ID", false);
    RunCmd(L"reg add \"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System\" /v EnableActivityFeed /t REG_DWORD /d 0 /f", L"Activity Feed", false);
    RunCmd(L"reg add \"HKCU\\System\\GameConfigStore\" /v GameDVR_Enabled /t REG_DWORD /d 0 /f", L"Game DVR", false);
    Print(L"[+] Cleaning Temp Files...");
    CleanTemp(); CleanPrefetch(); CleanThumbnails();
    Print(L"=== LITE WINDOWS DEBLOAT COMPLETE ===");
}

static void GameBooster(bool enable) {
    std::lock_guard<std::mutex> lk(g_boostMutex);
    static std::map<std::wstring, DWORD> originalStartTypes;
    const wchar_t* svcs[] = { L"SysMain", L"WSearch", L"DiagTrack" };

    if (enable) {
        ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (scm) {
            for (auto s : svcs) {
                ScHandle svc(OpenServiceW(scm.get(), s, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_STOP));
                if (svc) {
                    DWORD cfgSize = 0; (void)QueryServiceConfigW(svc.get(), nullptr, 0, &cfgSize);
                    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && cfgSize > 0) {
                        std::vector<BYTE> cfg(cfgSize);
                        auto* qc = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(cfg.data());
                        if (QueryServiceConfigW(svc.get(), qc, cfgSize, &cfgSize)) {
                            originalStartTypes[s] = qc->dwStartType;
                        }
                    }
                    SERVICE_STATUS ss{};
                    ControlService(svc.get(), SERVICE_CONTROL_STOP, &ss);
                }
            }
        }
    }
    else {
        ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (scm) {
            for (auto s : svcs) {
                ScHandle svc(OpenServiceW(scm.get(), s, SERVICE_CHANGE_CONFIG | SERVICE_START));
                if (svc) {
                    DWORD startType = SERVICE_AUTO_START;
                    if (originalStartTypes.count(s)) startType = originalStartTypes[s];
                    ChangeServiceConfigW(svc.get(), SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                    StartServiceW(svc.get(), 0, nullptr);
                }
            }
        }
        originalStartTypes.clear();
    }
    ApplySmartGameMode(enable);
}

struct GameInfo { std::wstring name, path, launcher; };
static std::vector<GameInfo> ScanGames() {
    std::vector<GameInfo> games; std::error_code ec{};
    std::wstring steam = L"C:\\Program Files (x86)\\Steam\\steamapps\\common";
    if (fs::exists(steam, ec) && !ec) {
        for (auto& p : fs::directory_iterator(steam, ec)) {
            if (!ec && p.is_directory()) games.push_back({ p.path().filename().wstring(), p.path().wstring(), L"Steam" });
        }
    }
    std::wstring epic = L"C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    if (fs::exists(epic, ec) && !ec) {
        for (auto& p : fs::directory_iterator(epic, ec)) {
            if (!ec && p.path().extension() == L".item") {
                std::ifstream f(p.path().wstring().c_str());
                if (f) {
                    std::stringstream ss; ss << f.rdbuf();
                    std::string s = ss.str();
                    auto pos = s.find("\"InstallLocation\":");
                    if (pos != std::string::npos) {
                        auto start = s.find("\"", pos + 18);
                        if (start != std::string::npos) {
                            auto end = s.find("\"", start + 1);
                            if (end != std::string::npos) {
                                std::wstring wpath = Utf8ToWide(s.substr(start + 1, end - start - 1));
                                games.push_back({ p.path().stem().wstring(), wpath, L"Epic" });
                            }
                        }
                    }
                }
            }
        }
    }
    return games;
}

static bool CompressGameFolder(const std::wstring& path) {
    if (path.empty() || path.find(L"..") != std::wstring::npos) return false;
    std::wstring cmd = L"compact.exe /c /s /a /i /f /exe:LZX \"" + path + L"\"";
    Print(L"[+] Сжатие папки: " + path + L" (может занять время)...");
    return RunCmd(cmd, L"Сжатие LZX", false, 3600);
}

static bool InstallAppWinget(const std::wstring& packageId) {
    if (packageId.empty() || packageId.find(L" ") != std::wstring::npos) return false;
    std::wstring cmd = L"winget install --id " + packageId + L" -e --accept-package-agreements --accept-source-agreements --silent";
    return RunCmd(cmd, L"Installing " + packageId, false, 1200, { 0 });
}

static void LaunchAfterburnerHidden() {
    std::wstring path = L"C:\\Program Files (x86)\\MSI Afterburner\\MSIAfterburner.exe";
    std::error_code ec{};
    if (fs::exists(path, ec) && !ec) {
        ShellExecuteW(nullptr, L"open", path.c_str(), L"/Profile1 /StartMinimized", nullptr, SW_HIDE);
    }
    else {
        RunCmd(L"winget install --id MSI.Afterburner -e --silent --accept-package-agreements --accept-source-agreements", L"MSI Afterburner", false, 600, { 0 });
        ShellExecuteW(nullptr, L"open", path.c_str(), L"/Profile1 /StartMinimized", nullptr, SW_HIDE);
    }
}

static bool ShredFileW(const std::wstring& path) {
    UniqueHandle hFile(CreateFileW(path.c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile) return false;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile.get(), &sz)) return false;
    if (sz.QuadPart == 0) {
        hFile.reset();
        return DeleteFileW(path.c_str()) != FALSE;
    }
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::vector<char> buf(65536);
    for (int pass = 0; pass < 3; ++pass) {
        LARGE_INTEGER pos{};
        if (!SetFilePointerEx(hFile.get(), pos, nullptr, FILE_BEGIN)) return false;
        uint64_t remaining = static_cast<uint64_t>(sz.QuadPart);
        while (remaining > 0) {
            DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(remaining, buf.size()));
            if (pass == 1) {
                std::fill(buf.begin(), buf.begin() + toWrite, 0);
            }
            else {
                for (DWORD i = 0; i < toWrite; ++i) buf[i] = static_cast<char>(gen() & 0xFF);
            }
            DWORD written = 0;
            if (!WriteFile(hFile.get(), buf.data(), toWrite, &written, nullptr) || written != toWrite) return false;
            remaining -= toWrite;
        }
        if (!FlushFileBuffers(hFile.get())) return false;
    }
    hFile.reset();
    return DeleteFileW(path.c_str()) != FALSE;
}

static std::wstring HashFileHeader(const std::wstring& path, uint64_t size) {
    UniqueHandle hFile(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile) return L"";
    size_t cap = static_cast<size_t>(std::min<uint64_t>(size, 8 * 1024 * 1024));
    if (cap == 0) return L"";
    std::vector<char> buf(cap);
    DWORD read = 0;
    if (!ReadFile(hFile.get(), buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr) || read == 0) return L"";
    uint64_t hash = 1469598103934665603ULL;
    for (DWORD i = 0; i < read; ++i) {
        hash ^= static_cast<uint8_t>(buf[i]);
        hash *= 1099511628211ULL;
    }
    hash ^= size;
    wchar_t out[32] = {};
    swprintf_s(out, L"%016llx", static_cast<unsigned long long>(hash));
    return out;
}

struct SmartItem {
    std::wstring id, category, title, desc, safety, impact;
    uint64_t sizeBytes = 0;
    bool defaultOn = true;
};

static uint64_t GetFolderSizeBytes(const std::wstring& path) {
    uint64_t total = 0;
    try {
        std::error_code ec{};
        if (path.empty()) return 0;
        if (!fs::exists(path, ec) || ec) return 0;
        for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
            it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            try {
                if (it->is_regular_file(ec) && !ec) {
                    uintmax_t sz = it->file_size(ec);
                    if (!ec) total += static_cast<uint64_t>(sz);
                }
            }
            catch (...) {}
        }
    }
    catch (...) {}
    return total;
}

static uint64_t GetRecycleBinSizeForDrive(wchar_t letter) {
    std::wstring path = std::wstring(1, letter) + L":\\$Recycle.Bin";
    std::error_code ec{};
    if (!fs::exists(path, ec) || ec) return 0;
    return GetFolderSizeBytes(path);
}

static std::vector<SmartItem> SmartScan() {
    std::vector<SmartItem> items;
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    uint64_t ramFree = ms.ullAvailPhys;
    wchar_t activeLetter = g_activeDrive.load();

    std::wstring tempPath = GetEnvW(L"TEMP");
    uint64_t tempSize = tempPath.empty() ? 0 : GetFolderSizeBytes(tempPath);
    if (tempSize > 5ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_temp"; it.category = L"Очистка";
        it.title = L"Временные файлы пользователя";
        it.desc = L"Файлы из %TEMP%. Полностью безопасно удалять — приложения создадут их заново при необходимости.";
        it.safety = L"safe";
        it.impact = tempSize > 500ULL * 1024 * 1024 ? L"high" : L"low";
        it.sizeBytes = tempSize; it.defaultOn = true;
        items.push_back(it);
    }
    uint64_t winTempSize = GetFolderSizeBytes(g_paths.SystemRoot + L"\\Temp");
    if (winTempSize > 5ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_win_temp"; it.category = L"Очистка";
        it.title = L"Системные временные файлы";
        it.desc = L"Файлы из Windows\\Temp. Безопасно удалять (некоторые могут быть заняты системой).";
        it.safety = L"safe";
        it.impact = winTempSize > 500ULL * 1024 * 1024 ? L"high" : L"low";
        it.sizeBytes = winTempSize; it.defaultOn = true;
        items.push_back(it);
    }
    uint64_t rbSize = GetRecycleBinSizeForDrive(activeLetter);
    if (rbSize > 1024ULL * 1024) {
        SmartItem it;
        it.id = L"clean_recycle"; it.category = L"Очистка";
        it.title = L"Корзина (" + std::wstring(1, activeLetter) + L":)";
        it.desc = L"Файлы из корзины выбранного диска. Убедитесь, что там нет нужных файлов.";
        it.safety = L"safe";
        it.impact = rbSize > 1024ULL * 1024 * 1024 ? L"high" : L"medium";
        it.sizeBytes = rbSize; it.defaultOn = true;
        items.push_back(it);
    }
    std::wstring thumbPath = GetEnvW(L"LOCALAPPDATA") + L"\\Microsoft\\Windows\\Explorer";
    uint64_t thumbSize = 0;
    try {
        std::error_code ec{};
        if (fs::exists(thumbPath, ec) && !ec) {
            for (auto& p : fs::directory_iterator(thumbPath, ec)) {
                if (ec) break;
                if (p.is_regular_file(ec)) {
                    std::wstring fn = p.path().filename().wstring();
                    if (fn.find(L"thumbcache_") == 0) thumbSize += p.file_size(ec);
                }
            }
        }
    }
    catch (...) {}
    if (thumbSize > 5ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_thumbs"; it.category = L"Очистка";
        it.title = L"Кэш миниатюр";
        it.desc = L"Кэш эскизов Проводника. Windows пересоздаст его автоматически.";
        it.safety = L"safe"; it.impact = L"low";
        it.sizeBytes = thumbSize; it.defaultOn = true;
        items.push_back(it);
    }
    std::wstring localApp = GetEnvW(L"LOCALAPPDATA");
    uint64_t shaderSize = GetFolderSizeBytes(localApp + L"\\D3DSCache") + GetFolderSizeBytes(localApp + L"\\NVIDIA\\DXCache") +
        GetFolderSizeBytes(localApp + L"\\AMD\\DxCache") + GetFolderSizeBytes(localApp + L"\\Intel\\ShaderCache");
    if (shaderSize > 50ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_shader"; it.category = L"Очистка";
        it.title = L"Кэш шейдеров (D3D, NVIDIA, AMD, Intel)";
        it.desc = L"Кэш графических шейдеров. Игры пересоздадут его при следующем запуске (первый запуск может быть медленнее).";
        it.safety = L"safe";
        it.impact = shaderSize > 500ULL * 1024 * 1024 ? L"medium" : L"low";
        it.sizeBytes = shaderSize; it.defaultOn = true;
        items.push_back(it);
    }
    uint64_t doSize = GetFolderSizeBytes(L"C:\\Windows\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache");
    if (doSize > 50ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_do"; it.category = L"Очистка";
        it.title = L"Кэш обновлений Windows";
        it.desc = L"Временный кэш Delivery Optimization. Безопасно очищать.";
        it.safety = L"safe";
        it.impact = doSize > 1024ULL * 1024 * 1024 ? L"high" : L"medium";
        it.sizeBytes = doSize; it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"clean_dns"; it.category = L"Сеть";
        it.title = L"Сброс DNS-кэша";
        it.desc = L"Очищает кэш DNS. Ускоряет открытие сайтов после смены DNS.";
        it.safety = L"safe"; it.impact = L"low"; it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"clean_standby"; it.category = L"Память";
        it.title = L"Очистка списка ожидания (Standby)";
        it.desc = L"Освобождает память, занятую кэшем. Приложения запустятся с диска, но памяти станет больше.";
        it.safety = L"safe";
        it.impact = ramFree < 4ULL * 1024 * 1024 * 1024 ? L"medium" : L"low";
        it.defaultOn = true;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"power_high"; it.category = L"Производительность";
        it.title = L"Схема питания: Высокая производительность";
        it.desc = L"Отключает энергосбережение CPU. Рекомендуется для игр и работы.";
        it.safety = L"safe"; it.impact = L"medium"; it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"game_mode"; it.category = L"Игры";
        it.title = L"Включить Game Mode";
        it.desc = L"Приоритизация игр в Windows. Безопасно и обратимо.";
        it.safety = L"safe"; it.impact = L"low"; it.defaultOn = true;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"disable_net_throttle"; it.category = L"Сеть";
        it.title = L"Отключить NetworkThrottling";
        it.desc = L"Отключает ограничение сети Windows для мультимедиа. Полезно для стриминга и игр.";
        it.safety = L"caution"; it.impact = L"low"; it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"priority_sep"; it.category = L"Производительность";
        it.title = L"Win32PrioritySeparation = 0x26";
        it.desc = L"Приоритет CPU для активного приложения. Обратимо.";
        it.safety = L"caution"; it.impact = L"low"; it.defaultOn = false;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"disable_nagle"; it.category = L"Сеть";
        it.title = L"Отключить алгоритм Nagle";
        it.desc = L"Снижает задержку в онлайн-играх. Обратимо.";
        it.safety = L"caution"; it.impact = L"low"; it.defaultOn = false;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"disable_telemetry"; it.category = L"Приватность";
        it.title = L"Отключить телеметрию Windows";
        it.desc = L"Отключает службы сбора телеметрии. Обратимо.";
        it.safety = L"caution"; it.impact = L"low"; it.defaultOn = false;
        items.push_back(it);
    }
    return items;
}

static bool SmartApplyItem(const std::wstring& id, uint64_t& freed) {
    freed = 0;
    if (id == L"clean_temp") {
        std::wstring t = GetEnvW(L"TEMP");
        uint64_t sz = t.empty() ? 0 : GetFolderSizeBytes(t);
        CleanTemp();
        freed = sz;
        return true;
    }
    if (id == L"clean_win_temp") {
        uint64_t sz = GetFolderSizeBytes(g_paths.SystemRoot + L"\\Temp");
        RunCmd(L"del /f /s /q \"%SystemRoot%\\Temp\\*\" >nul 2>&1", L"", false);
        freed = sz;
        return true;
    }
    if (id == L"clean_recycle") {
        uint64_t sz = GetRecycleBinSizeForDrive(g_activeDrive.load());
        CleanRecycleForDrive(g_activeDrive.load());
        freed = sz;
        return true;
    }
    if (id == L"clean_thumbs") {
        std::wstring thumbPath = GetEnvW(L"LOCALAPPDATA") + L"\\Microsoft\\Windows\\Explorer";
        freed = GetFolderSizeBytes(thumbPath);
        CleanThumbnails();
        return true;
    }
    if (id == L"clean_shader") {
        std::wstring la = GetEnvW(L"LOCALAPPDATA");
        uint64_t sz = GetFolderSizeBytes(la + L"\\D3DSCache") + GetFolderSizeBytes(la + L"\\NVIDIA\\DXCache") +
            GetFolderSizeBytes(la + L"\\AMD\\DxCache") + GetFolderSizeBytes(la + L"\\Intel\\ShaderCache");
        CleanShaderCache();
        freed = sz;
        return true;
    }
    if (id == L"clean_do") {
        uint64_t sz = GetFolderSizeBytes(L"C:\\Windows\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache");
        CleanDeliveryOptimization();
        freed = sz;
        return true;
    }
    if (id == L"clean_dns") {
        FlushDns();
        return true;
    }
    if (id == L"clean_standby") {
        freed = PurgeStandbyEnhanced();
        return true;
    }
    if (id == L"power_high") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        bool ok = RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"Power High", false);
        return ok;
    }
    if (id == L"game_mode") { SetGameMode(true); return true; }
    if (id == L"disable_net_throttle") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        DisableNetworkThrottling(); return true;
    }
    if (id == L"priority_sep") { SetWin32PrioritySeparation(0x26); return true; }
    if (id == L"disable_nagle") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        DisableNagle(); return true;
    }
    if (id == L"disable_telemetry") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        DisableTelemetry(); return true;
    }
    return false;
}

struct AutoEntry { std::wstring name, command, location, keyPath, filePath; bool isRegistry = true; bool isHKLM = false; bool isEnabled = true; };
static std::vector<AutoEntry> ListAutostart() {
    std::vector<AutoEntry> out;
    struct RegLoc { HKEY hive; const wchar_t* path; const wchar_t* loc; bool hklm; };
    const RegLoc locs[] = {
        { HKEY_CURRENT_USER,  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKCU", false },
        { HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKLM", true },
        { HKEY_CURRENT_USER,  L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", L"HKCU", false },
        { HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", L"HKLM", true },
        { HKEY_LOCAL_MACHINE, L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKLM", true },
        { HKEY_LOCAL_MACHINE, L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\RunOnce", L"HKLM", true },
        { HKEY_CURRENT_USER,  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run-OptimizerProBackup", L"HKCU", false },
        { HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run-OptimizerProBackup", L"HKLM", true },
    };
    for (const auto& loc : locs) {
        HKEY rawKey = nullptr;
        if (RegOpenKeyExW(loc.hive, loc.path, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) continue;
        UniqueRegKey key(rawKey);
        DWORD idx = 0;
        for (;;) {
            wchar_t name[512] = {}; DWORD nameSize = 512;
            if (RegEnumValueW(key.get(), idx++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            DWORD type = 0, size = 0;
            if (RegQueryValueExW(key.get(), name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) continue;
            if ((type != REG_SZ && type != REG_EXPAND_SZ) || size == 0 || size > 1024 * 1024) continue;
            std::wstring cmd(size / sizeof(wchar_t) + 2, L'\0');
            if (RegQueryValueExW(key.get(), name, nullptr, &type, (LPBYTE)cmd.data(), &size) != ERROR_SUCCESS) continue;
            cmd.resize(wcslen(cmd.c_str()));
            AutoEntry e; e.name = name; e.command = cmd; e.location = loc.loc; e.isRegistry = true; e.keyPath = loc.path; e.isHKLM = loc.hklm;
            e.isEnabled = (wcsstr(loc.path, L"Backup") == nullptr);
            out.push_back(std::move(e));
        }
    }

    int csidl[] = { CSIDL_STARTUP, CSIDL_COMMON_STARTUP };
    for (int id : csidl) {
        wchar_t startup[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, id, nullptr, 0, startup))) {
            std::error_code ec;
            if (fs::exists(startup, ec)) {
                for (auto& p : fs::directory_iterator(startup, ec)) {
                    if (p.is_regular_file()) {
                        std::wstring ext = p.path().extension().wstring();
                        if (ext == L".lnk" || ext == L".url" || ext == L".exe") {
                            AutoEntry e; e.name = p.path().filename().wstring(); e.command = p.path().wstring(); e.location = (id == CSIDL_STARTUP) ? L"User Startup" : L"Common Startup"; e.isRegistry = false; e.isEnabled = true;
                            out.push_back(e);
                        }
                    }
                }
            }
        }
    }
    return out;
}

static bool ToggleAutostart(const std::wstring& name, const std::wstring& loc, const std::wstring& act) {
    if (loc == L"User Startup" || loc == L"Common Startup") return false;
    HKEY hive = (loc == L"HKCU") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring runPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring bakPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run-OptimizerProBackup";

    if (act == L"disable") {
        HKEY hRun = nullptr;
        if (RegOpenKeyExW(hive, runPath.c_str(), 0, KEY_READ | KEY_WRITE, &hRun) != ERROR_SUCCESS) return false;
        wchar_t data[2048] = {}; DWORD size = sizeof(data); DWORD type = 0;
        if (RegQueryValueExW(hRun, name.c_str(), nullptr, &type, (LPBYTE)data, &size) == ERROR_SUCCESS) {
            HKEY hBak = nullptr;
            if (RegCreateKeyExW(hive, bakPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hBak, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hBak, name.c_str(), 0, type, (const BYTE*)data, size);
                RegCloseKey(hBak);
            }
            RegDeleteValueW(hRun, name.c_str());
            RegCloseKey(hRun);
            return true;
        }
        RegCloseKey(hRun);
        return false;
    }
    else if (act == L"enable") {
        HKEY hBak = nullptr;
        if (RegOpenKeyExW(hive, bakPath.c_str(), 0, KEY_READ | KEY_WRITE, &hBak) != ERROR_SUCCESS) return false;
        wchar_t data[2048] = {}; DWORD size = sizeof(data); DWORD type = 0;
        if (RegQueryValueExW(hBak, name.c_str(), nullptr, &type, (LPBYTE)data, &size) == ERROR_SUCCESS) {
            HKEY hRun = nullptr;
            if (RegCreateKeyExW(hive, runPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hRun, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hRun, name.c_str(), 0, type, (const BYTE*)data, size);
                RegCloseKey(hRun);
            }
            RegDeleteValueW(hBak, name.c_str());
            RegCloseKey(hBak);
            return true;
        }
        RegCloseKey(hBak);
        return false;
    }
    return false;
}

struct ServiceInfo { std::wstring name, display, state, startType; };
static std::vector<ServiceInfo> ListServices() {
    std::vector<ServiceInfo> out;
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (!scm) return out;
    AlignedBuffer buf(64 * 1024);
    DWORD needed = 0, count = 0, resume = 0;
    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 4; ++attempt) {
        needed = 0; count = 0; resume = 0;
        ok = EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, (LPBYTE)buf.data(), (DWORD)buf.size_bytes(), &needed, &count, &resume, nullptr);
        if (ok) break;
        if (GetLastError() != ERROR_MORE_DATA) return out;
        if (needed <= buf.size_bytes()) return out;
        buf.resize_bytes(needed + 8192);
    }
    if (!ok) return out;
    auto* arr = buf.as<ENUM_SERVICE_STATUS_PROCESSW>();
    for (DWORD i = 0; i < count; ++i) {
        ServiceInfo si;
        si.name = arr[i].lpServiceName ? arr[i].lpServiceName : L"";
        si.display = arr[i].lpDisplayName ? arr[i].lpDisplayName : L"";
        si.startType = L"other";
        DWORD st = arr[i].ServiceStatusProcess.dwCurrentState;
        si.state = (st == SERVICE_RUNNING) ? L"running" : (st == SERVICE_STOPPED) ? L"stopped" : L"other";
        ScHandle svc(OpenServiceW(scm.get(), si.name.c_str(), SERVICE_QUERY_CONFIG));
        if (svc) {
            DWORD cfgSize = 0; (void)QueryServiceConfigW(svc.get(), nullptr, 0, &cfgSize);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && cfgSize > 0) {
                std::vector<BYTE> cfg(cfgSize);
                auto* qc = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(cfg.data());
                if (QueryServiceConfigW(svc.get(), qc, cfgSize, &cfgSize)) {
                    si.startType = (qc->dwStartType == SERVICE_AUTO_START) ? L"auto"
                        : (qc->dwStartType == SERVICE_DEMAND_START) ? L"manual"
                        : (qc->dwStartType == SERVICE_DISABLED) ? L"disabled" : L"other";
                }
            }
        }
        out.push_back(std::move(si));
    }
    return out;
}

static bool ControlService2(const std::wstring& name, const std::wstring& action, std::wstring& err) {
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) { err = L"OpenSCManager failed"; return false; }
    DWORD access = SERVICE_QUERY_STATUS;
    if (action == L"start") access |= SERVICE_START;
    else if (action == L"stop") access |= SERVICE_STOP;
    else access |= SERVICE_CHANGE_CONFIG;
    ScHandle svc(OpenServiceW(scm.get(), name.c_str(), access));
    if (!svc) { err = L"OpenService failed"; return false; }
    bool ok = false;
    if (action == L"start") { ok = StartServiceW(svc.get(), 0, nullptr) != FALSE; if (!ok && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) ok = true; }
    else if (action == L"stop") { SERVICE_STATUS ss{}; ok = ControlService(svc.get(), SERVICE_CONTROL_STOP, &ss) != FALSE; }
    else {
        DWORD startType = SERVICE_NO_CHANGE;
        if (action == L"auto") startType = SERVICE_AUTO_START;
        else if (action == L"manual") startType = SERVICE_DEMAND_START;
        else if (action == L"disable") startType = SERVICE_DISABLED;
        ok = ChangeServiceConfigW(svc.get(), SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != FALSE;
    }
    if (!ok) err = L"Error";
    return ok;
}

struct DriveInfo {
    wchar_t letter = L'C';
    std::wstring label;
    bool isSystem = false;
};

static std::vector<DriveInfo> GetDrivesList() {
    std::vector<DriveInfo> result;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        wchar_t letter = static_cast<wchar_t>(L'A' + i);
        std::wstring root = std::wstring(1, letter) + L":\\";
        if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) continue;
        wchar_t volName[MAX_PATH] = {};
        wchar_t fsName[MAX_PATH] = {};
        if (!GetVolumeInformationW(root.c_str(), volName, MAX_PATH, nullptr, nullptr, nullptr, fsName, MAX_PATH)) continue;
        DriveInfo d;
        d.letter = letter;
        d.label = volName[0] ? volName : L"Local Disk";
        d.isSystem = (letter == g_systemDriveLetter);
        result.push_back(d);
    }
    return result;
}

static std::wstring ListContextMenuJson() {
    std::wstring j = L"[";
    struct Loc { HKEY hive; const wchar_t* path; };
    Loc locs[] = {
        { HKEY_CLASSES_ROOT, L"*\\shellex\\ContextMenuHandlers" },
        { HKEY_CLASSES_ROOT, L"Directory\\shellex\\ContextMenuHandlers" },
        { HKEY_CLASSES_ROOT, L"Directory\\Background\\shellex\\ContextMenuHandlers" }
    };
    bool first = true;
    for (auto& loc : locs) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(loc.hive, loc.path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD idx = 0;
            wchar_t sub[256] = {}; DWORD subLen = 256;
            while (true) {
                subLen = 256;
                if (RegEnumKeyExW(hKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                HKEY hSub = nullptr;
                if (RegOpenKeyExW(hKey, sub, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                    wchar_t clsid[256]; DWORD clsidSize = sizeof(clsid);
                    DWORD type = 0;
                    if (RegQueryValueExW(hSub, nullptr, nullptr, &type, (LPBYTE)clsid, &clsidSize) == ERROR_SUCCESS && type == REG_SZ) {
                        if (!first) j += L",";
                        first = false;
                        j += L"{\"name\":" + JsonStr(sub) + L",\"clsid\":" + JsonStr(clsid) + L",\"loc\":" + JsonStr(loc.path) + L"}";
                    }
                    RegCloseKey(hSub);
                }
            }
            RegCloseKey(hKey);
        }
    }
    j += L"]";
    return j;
}

static bool ToggleContextMenu(const std::wstring& clsid, bool disable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Blocked", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        if (disable) {
            RegSetValueExW(hKey, clsid.c_str(), 0, REG_SZ, (const BYTE*)L"", sizeof(wchar_t));
        }
        else {
            RegDeleteValueW(hKey, clsid.c_str());
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

static bool IsWebView2Installed() {
    HKEY hKey = nullptr;
    const wchar_t* guid = L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
    const wchar_t* paths[] = {
        L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\",
        L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\",
        L"Software\\Microsoft\\EdgeUpdate\\Clients\\"
    };
    HKEY hives[] = { HKEY_LOCAL_MACHINE, HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER };
    for (int i = 0; i < 3; ++i) {
        std::wstring fullPath = std::wstring(paths[i]) + guid;
        if (RegOpenKeyExW(hives[i], fullPath.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey); return true;
        }
    }
    std::vector<std::wstring> dirs = { L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application", L"C:\\Program Files\\Microsoft\\EdgeWebView\\Application" };
    for (const auto& d : dirs) {
        std::error_code ec{};
        if (fs::exists(d, ec) && !ec) {
            for (const auto& entry : fs::directory_iterator(d, ec)) {
                if (entry.is_directory(ec) && !ec) {
                    if (fs::exists(entry.path().wstring() + L"\\msedgewebview2.exe", ec) && !ec) return true;
                }
            }
        }
    }
    return false;
}

static bool g_bootstrapperDone = false;
static bool g_webviewReady = false;
static bool g_isInstalling = false;

static bool VerifySignature(const std::wstring& filePath) {
    WINTRUST_FILE_INFO fileInfo = { sizeof(WINTRUST_FILE_INFO) };
    fileInfo.pcwszFilePath = filePath.c_str();
    GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA winTrustData = { sizeof(WINTRUST_DATA) };
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.pFile = &fileInfo;
    winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrustData.dwProvFlags = WTD_SAFER_FLAG;
    winTrustData.dwUIContext = 0;
    LONG status = WinVerifyTrust(nullptr, &guidAction, &winTrustData);

    if (status == ERROR_SUCCESS) {
        DWORD dwEncoding, dwContentType, dwFormatType;
        HCERTSTORE hStore = NULL;
        HCRYPTMSG hMsg = NULL;
        if (CryptQueryObject(CERT_QUERY_OBJECT_FILE, filePath.c_str(), CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED, CERT_QUERY_FORMAT_FLAG_BINARY, 0, &dwEncoding, &dwContentType, &dwFormatType, &hStore, &hMsg, NULL)) {
            DWORD dwSignerInfo = 0;
            if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &dwSignerInfo)) {
                std::vector<BYTE> siBuf(dwSignerInfo);
                if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, siBuf.data(), &dwSignerInfo)) {
                    CMSG_SIGNER_INFO* pSignerInfo = (CMSG_SIGNER_INFO*)siBuf.data();
                    CERT_INFO certInfo;
                    certInfo.Issuer = pSignerInfo->Issuer;
                    certInfo.SerialNumber = pSignerInfo->SerialNumber;
                    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &certInfo, NULL);
                    if (pCertContext) {
                        wchar_t name[256];
                        CertGetNameStringW(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, 256);
                        if (wcsstr(name, L"Microsoft") == nullptr) status = TRUST_E_PROVIDER_UNKNOWN;
                        CertFreeCertificateContext(pCertContext);
                    }
                }
            }
            if (hMsg) CryptMsgClose(hMsg);
            if (hStore) CertCloseStore(hStore, 0);
        }
    }

    winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &guidAction, &winTrustData);
    return status == ERROR_SUCCESS;
}

static bool DownloadAndInstallWebView2() {
    TempFile tmp;
    if (!tmp.Create(L"wv2")) return false;
    WinHttpHandle hSession(WinHttpOpen(L"OptPro/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) return false;
    WinHttpHandle hConnect(WinHttpConnect(hSession.get(), L"go.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!hConnect) return false;
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.get(), L"GET", L"/fwlink/p/?LinkId=2124703", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!hRequest) return false;
    if (!WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
    if (!WinHttpReceiveResponse(hRequest.get(), nullptr)) return false;

    DWORD statusCode = 0, size = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
    if (statusCode < 200 || statusCode >= 300) return false;

    UniqueHandle hFile(CreateFileW(tmp.path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile) return false;
    std::vector<char> buf(65536); DWORD read = 0;
    while (WinHttpReadData(hRequest.get(), buf.data(), (DWORD)buf.size(), &read) && read > 0) {
        DWORD written = 0; WriteFile(hFile.get(), buf.data(), read, &written, nullptr);
    }
    hFile.reset();

    if (!VerifySignature(tmp.path)) {
        MessageBoxW(nullptr, L"Signature verification failed for WebView2 installer.", L"Security Error", MB_ICONERROR);
        return false;
    }

    std::wstring cmd = L"\"" + std::wstring(tmp.path) + L"\" /silent /install";
    STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cl(cmd.begin(), cmd.end()); cl.push_back(L'\0');
    if (!CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, 300000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
}

static LRESULT CALLBACK BootProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp; SetTextColor(hdc, RGB(230, 230, 240)); SetBkColor(hdc, RGB(18, 21, 30));
        static HBRUSH hbr = CreateSolidBrush(RGB(18, 21, 30)); return (LRESULT)hbr;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp; RECT rc; GetClientRect(hwnd, &rc);
        static HBRUSH br = CreateSolidBrush(RGB(18, 21, 30)); FillRect(hdc, &rc, br); return 1;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 100) {
            if (!g_webviewReady) {
                if (!g_isInstalling) {
                    g_isInstalling = true;
                    HWND hBtn = GetDlgItem(hwnd, 100);
                    SetWindowTextW(hBtn, L"Идет установка, подождите...");
                    EnableWindow(hBtn, FALSE);
                    std::thread([hwnd]() {
                        try {
                            bool ok = DownloadAndInstallWebView2();
                            PostMessageW(hwnd, WM_BOOT_DONE, ok ? 1 : 0, 0);
                        }
                        catch (...) {
                            PostMessageW(hwnd, WM_BOOT_DONE, 0, 0);
                        }
                        }).detach();
                }
                else {
                    MessageBoxW(hwnd, L"Установка уже идет. Пожалуйста, подождите.", L"Optimizer Pro", MB_ICONINFORMATION);
                }
                return 0;
            }
            g_bootstrapperDone = true; DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool ShowBootstrapper(HINSTANCE hInst) {
    if (IsWebView2Installed()) g_webviewReady = true;
    if (g_webviewReady) { g_bootstrapperDone = true; return true; }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = BootProc; wc.hInstance = hInst; wc.lpszClassName = L"OptBootWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = CreateLightningIcon(32); wc.hIconSm = CreateLightningIcon(16);
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 560, 340 };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    HWND hwnd = CreateWindowExW(0, L"OptBootWnd", L"Optimizer Pro - Подготовка",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) { g_bootstrapperDone = true; return false; }

    HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT fontBold = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HWND hTitle = CreateWindowW(L"STATIC", L"⚡ Optimizer Pro — Подготовка", WS_CHILD | WS_VISIBLE, 20, 15, 500, 30, hwnd, nullptr, hInst, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)fontBold, TRUE);
    HWND hSub = CreateWindowW(L"STATIC", L"Проверка необходимых компонентов...", WS_CHILD | WS_VISIBLE, 20, 55, 500, 24, hwnd, nullptr, hInst, nullptr);
    SendMessageW(hSub, WM_SETFONT, (WPARAM)font, TRUE);

    wchar_t wvText[512] = {};
    swprintf_s(wvText, L"%s WebView2 Runtime: %s", g_webviewReady ? L"✅" : L"⚠️", g_webviewReady ? L"установлен" : L"НЕ НАЙДЕН");
    HWND hWv = CreateWindowW(L"STATIC", wvText, WS_CHILD | WS_VISIBLE, 20, 95, 500, 24, hwnd, nullptr, hInst, nullptr);
    SendMessageW(hWv, WM_SETFONT, (WPARAM)font, TRUE);

    HWND hHint = CreateWindowW(L"STATIC", L"WebView2 обязателен для работы интерфейса.\nНажмите кнопку ниже, чтобы установить его автоматически.", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 165, 500, 50, hwnd, nullptr, hInst, nullptr);
    SendMessageW(hHint, WM_SETFONT, (WPARAM)font, TRUE);

    HWND hBtn = CreateWindowW(L"BUTTON", g_webviewReady ? L"Продолжить →" : L"Установить WebView2 и продолжить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 240, 500, 50, hwnd, (HMENU)100, hInst, nullptr);
    SendMessageW(hBtn, WM_SETFONT, (WPARAM)font, TRUE);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_BOOT_DONE && m.hwnd == hwnd) {
            g_webviewReady = (m.wParam != 0);
            if (g_webviewReady) { DestroyWindow(hwnd); g_bootstrapperDone = true; break; }
            else {
                MessageBoxW(hwnd, L"Не удалось установить WebView2.\nСкачайте вручную: https://developer.microsoft.com/microsoft-edge/webview2/", L"Ошибка", MB_ICONERROR);
                HWND hBtnFail = GetDlgItem(hwnd, 100);
                SetWindowTextW(hBtnFail, L"Попробовать снова");
                EnableWindow(hBtnFail, TRUE); g_isInstalling = false;
            }
        }
        TranslateMessage(&m); DispatchMessageW(&m);
    }
    DeleteObject(font); DeleteObject(fontBold); return g_webviewReady;
}

static NOTIFYICONDATAW g_nid = {};
static ULONGLONG g_lastHighLoadNotify = 0;

static void InitTray(HWND hwnd) {
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = CreateLightningIcon(16); // Иконка для трея
    wcsncpy_s(g_nid.szTip, L"Optimizer Pro", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void CleanupTray() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

class GameOverlay {
public:
    HWND hwnd = nullptr;
    std::atomic<bool> userEnabled{ true }, isVisible{ false };
    std::atomic<bool> showCpu{ true }, showGpu{ true }, showRam{ true }, showTemp{ true };
    std::atomic<bool> showVram{ true }, showFan{ false }, showPing{ true }, showFps{ false };
    std::atomic<bool> darkTheme{ true };
    std::atomic<int> posX{ 10 }, posY{ 10 }, alpha{ 255 };
    HFONT hFont = nullptr;

    void Create(HINSTANCE hInst) {
        WNDCLASSEXW wc = { sizeof(wc) }; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hInst; wc.lpszClassName = L"GameOverlayWnd";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"GameOverlayWnd", L"Overlay", WS_POPUP, posX.load(), posY.load(), 320, 240, nullptr, nullptr, hInst, nullptr);
        if (!hwnd) return;
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), (BYTE)alpha.load(), LWA_COLORKEY | LWA_ALPHA);
        hFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        ShowWindow(hwnd, SW_HIDE);
    }

    void Cleanup() { if (hFont) { DeleteObject(hFont); hFont = nullptr; } if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; } }

    void SetPosition(int x, int y) {
        posX = x; posY = y;
        if (hwnd) SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    void SetAlpha(int a) {
        alpha = a;
        if (hwnd) SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), (BYTE)a, LWA_COLORKEY | LWA_ALPHA);
    }

    void Draw() const {
        if (!hwnd || !isVisible) return;
        HDC hdc = GetDC(hwnd); if (!hdc) return;
        RECT rc; GetClientRect(hwnd, &rc);
        static HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, darkTheme.load() ? RGB(0, 255, 100) : RGB(0, 120, 50));
        HGDIOBJ old = hFont ? SelectObject(hdc, hFont) : nullptr;
        int y = 10; wchar_t buf[128] = {};
        if (showCpu) { swprintf_s(buf, L"CPU: %.1f%%%s", g_cpuUsage.load(), g_throttling.load() ? L" THR" : L""); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showGpu) { swprintf_s(buf, L"GPU: %.1f%%", g_gpuUsage.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showRam) { MEMORYSTATUSEX ms = { sizeof(ms) }; GlobalMemoryStatusEx(&ms); swprintf_s(buf, L"RAM: %llu MB", static_cast<unsigned long long>(ms.ullAvailPhys / (1024 * 1024))); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showVram && g_vramTotalMB.load() > 0) { swprintf_s(buf, L"VRAM: %llu/%llu MB", (unsigned long long)g_vramUsedMB.load(), (unsigned long long)g_vramTotalMB.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showTemp && g_cpuTemp.load() > 0) { swprintf_s(buf, L"CPU T: %.1fC", g_cpuTemp.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showTemp && g_gpuTemp.load() > 0) { swprintf_s(buf, L"GPU T: %.1fC", g_gpuTemp.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showFan && g_gpuFan.load() >= 0) { swprintf_s(buf, L"GPU Fan: %.0f%%", g_gpuFan.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showPing && g_pingMs.load() >= 0) { swprintf_s(buf, L"Ping: %d ms", g_pingMs.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (old) SelectObject(hdc, old);
        ReleaseDC(hwnd, hdc);
    }
    void Toggle() { userEnabled = !userEnabled; if (!userEnabled && hwnd) { ShowWindow(hwnd, SW_HIDE); isVisible = false; } }
    void UpdateVisibility(bool inGame) {
        bool should = userEnabled && inGame;
        if (isVisible != should) { isVisible = should; if (hwnd) ShowWindow(hwnd, isVisible ? SW_SHOWNA : SW_HIDE); }
    }
};

static GameOverlay g_overlay;

static void LoadOverlaySettings() {
    if (g_portableMode.load()) {
        std::wstring path = g_paths.BackupDir + L"\\overlay.json";
        std::ifstream f(path.c_str());
        if (f) {
            std::stringstream ss; ss << f.rdbuf();
            std::string utf8 = ss.str();
            auto getInt = [&](const char* key, std::atomic<int>& out) {
                std::string needle = std::string("\"") + key + "\":";
                auto pos = utf8.find(needle);
                if (pos != std::string::npos) {
                    try { out = std::stoi(utf8.substr(pos + needle.size())); }
                    catch (...) {}
                }
                };
            auto getBool = [&](const char* key, std::atomic<bool>& out) {
                std::string needle = std::string("\"") + key + "\":";
                auto pos = utf8.find(needle);
                if (pos != std::string::npos) {
                    out = (utf8.compare(pos + needle.size(), 4, "true") == 0);
                }
                };
            getInt("posX", g_overlay.posX); getInt("posY", g_overlay.posY); getInt("alpha", g_overlay.alpha);
            getBool("darkTheme", g_overlay.darkTheme); getBool("showCpu", g_overlay.showCpu); getBool("showGpu", g_overlay.showGpu);
            getBool("showRam", g_overlay.showRam); getBool("showTemp", g_overlay.showTemp); getBool("showVram", g_overlay.showVram);
            getBool("showFan", g_overlay.showFan); getBool("showPing", g_overlay.showPing);
        }
    }
    else {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\OptimizerPro\\Overlay", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0; DWORD sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"PosX", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.posX = val;
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"PosY", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.posY = val;
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"Alpha", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.alpha = val;
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"DarkTheme", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.darkTheme = (val != 0);

            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowCpu", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showCpu = (val != 0);
            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowGpu", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showGpu = (val != 0);
            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowRam", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showRam = (val != 0);
            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowTemp", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showTemp = (val != 0);
            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowVram", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showVram = (val != 0);
            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowFan", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showFan = (val != 0);
            sz = sizeof(DWORD); if (RegQueryValueExW(hKey, L"ShowPing", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showPing = (val != 0);

            RegCloseKey(hKey);
        }
    }
}

static void SaveOverlaySettings() {
    if (g_portableMode.load()) {
        std::wstring path = g_paths.BackupDir + L"\\overlay.json";
        std::wstring j = L"{";
        j += L"\"posX\":" + std::to_wstring(g_overlay.posX.load()) + L",";
        j += L"\"posY\":" + std::to_wstring(g_overlay.posY.load()) + L",";
        j += L"\"alpha\":" + std::to_wstring(g_overlay.alpha.load()) + L",";
        j += L"\"darkTheme\":" + std::wstring(g_overlay.darkTheme.load() ? L"true" : L"false") + L",";
        j += L"\"showCpu\":" + std::wstring(g_overlay.showCpu.load() ? L"true" : L"false") + L",";
        j += L"\"showGpu\":" + std::wstring(g_overlay.showGpu.load() ? L"true" : L"false") + L",";
        j += L"\"showRam\":" + std::wstring(g_overlay.showRam.load() ? L"true" : L"false") + L",";
        j += L"\"showTemp\":" + std::wstring(g_overlay.showTemp.load() ? L"true" : L"false") + L",";
        j += L"\"showVram\":" + std::wstring(g_overlay.showVram.load() ? L"true" : L"false") + L",";
        j += L"\"showFan\":" + std::wstring(g_overlay.showFan.load() ? L"true" : L"false") + L",";
        j += L"\"showPing\":" + std::wstring(g_overlay.showPing.load() ? L"true" : L"false") + L"";
        j += L"}";
        std::ofstream f(path.c_str(), std::ios::binary);
        if (f) { std::string u = WideToUtf8(j); f.write(u.data(), (std::streamsize)u.size()); }
    }
    else {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OptimizerPro\\Overlay", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            DWORD px = g_overlay.posX.load(); RegSetValueExW(hKey, L"PosX", 0, REG_DWORD, (const BYTE*)&px, sizeof(px));
            DWORD py = g_overlay.posY.load(); RegSetValueExW(hKey, L"PosY", 0, REG_DWORD, (const BYTE*)&py, sizeof(py));
            DWORD al = g_overlay.alpha.load(); RegSetValueExW(hKey, L"Alpha", 0, REG_DWORD, (const BYTE*)&al, sizeof(al));
            DWORD dt = g_overlay.darkTheme.load() ? 1 : 0; RegSetValueExW(hKey, L"DarkTheme", 0, REG_DWORD, (const BYTE*)&dt, sizeof(dt));

            DWORD c = g_overlay.showCpu.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowCpu", 0, REG_DWORD, (const BYTE*)&c, sizeof(c));
            DWORD g = g_overlay.showGpu.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowGpu", 0, REG_DWORD, (const BYTE*)&g, sizeof(g));
            DWORD r = g_overlay.showRam.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowRam", 0, REG_DWORD, (const BYTE*)&r, sizeof(r));
            DWORD t = g_overlay.showTemp.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowTemp", 0, REG_DWORD, (const BYTE*)&t, sizeof(t));
            DWORD v = g_overlay.showVram.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowVram", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
            DWORD f = g_overlay.showFan.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowFan", 0, REG_DWORD, (const BYTE*)&f, sizeof(f));
            DWORD p = g_overlay.showPing.load() ? 1 : 0; RegSetValueExW(hKey, L"ShowPing", 0, REG_DWORD, (const BYTE*)&p, sizeof(p));

            RegCloseKey(hKey);
        }
    }
}

static void EnumerateTasksRecursive(ITaskFolder* folder, std::vector<std::wstring>& result, int& count) {
    if (!folder || count > 500) return;
    ComPtr<IRegisteredTaskCollection> coll;
    if (SUCCEEDED(folder->GetTasks(TASK_ENUM_HIDDEN, &coll))) {
        LONG c = 0; coll->get_Count(&c);
        for (LONG i = 1; i <= c && count < 500; ++i) {
            ComPtr<IRegisteredTask> task;
            if (FAILED(coll->get_Item(_variant_t(i), &task))) continue;
            _bstr_t name; task->get_Name(name.GetAddress());
            _bstr_t path; task->get_Path(path.GetAddress());
            VARIANT_BOOL enabled = VARIANT_FALSE; task->get_Enabled(&enabled);
            const wchar_t* pName = (const wchar_t*)name;
            const wchar_t* pPath = (const wchar_t*)path;
            std::wstring entry = (pName ? pName : L"") + std::wstring(L"\x1F") + (pPath ? pPath : L"") + L"\x1F" + (enabled ? L"1" : L"0");
            result.push_back(entry);
            count++;
        }
    }

    ComPtr<ITaskFolderCollection> subColl;
    if (SUCCEEDED(folder->GetFolders(0, &subColl))) {
        LONG sc = 0; subColl->get_Count(&sc);
        for (LONG i = 1; i <= sc && count < 500; ++i) {
            ComPtr<ITaskFolder> sub;
            if (SUCCEEDED(subColl->get_Item(_variant_t(i), &sub))) {
                EnumerateTasksRecursive(sub.Get(), result, count);
            }
        }
    }
}

static std::vector<std::wstring> GetTaskSchedulerTasks() {
    std::vector<std::wstring> result;
    int count = 0;
    ComPtr<ITaskService> svc;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&svc));
    if (FAILED(hr)) return result;
    hr = svc->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) return result;
    ComPtr<ITaskFolder> root;
    hr = svc->GetFolder(_bstr_t(L"\\"), &root);
    if (FAILED(hr)) return result;
    EnumerateTasksRecursive(root.Get(), result, count);
    return result;
}

struct ProgramInfo {
    std::wstring name, version, publisher, installLoc, uninstallStr, quietUninstallStr;
    uint64_t sizeKB = 0;
    bool is64 = false;
};

static std::vector<ProgramInfo> ScanProgramsDetailed() {
    std::vector<ProgramInfo> result;
    struct RegLoc { HKEY hive; const wchar_t* path; REGSAM sam; bool is64; };
    RegLoc locs[] = {
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ | KEY_WOW64_64KEY, true },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ | KEY_WOW64_32KEY, false },
        { HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_READ, true }
    };
    for (auto& loc : locs) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(loc.hive, loc.path, 0, loc.sam, &hKey) != ERROR_SUCCESS) continue;
        DWORD idx = 0;
        for (;;) {
            wchar_t sub[256] = {}; DWORD subLen = 256;
            if (RegEnumKeyExW(hKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY hSub = nullptr;
            if (RegOpenKeyExW(hKey, sub, 0, loc.sam, &hSub) != ERROR_SUCCESS) continue;

            ProgramInfo p;
            p.is64 = loc.is64;

            auto readStr = [&](const wchar_t* name, std::wstring& out) {
                wchar_t buf[1024] = {}; DWORD size = sizeof(buf); DWORD type = 0;
                if (RegQueryValueExW(hSub, name, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
                    out = buf;
                }
                };
            auto readDword = [&](const wchar_t* name, uint64_t& out) {
                DWORD val = 0; DWORD size = sizeof(val); DWORD type = 0;
                if (RegQueryValueExW(hSub, name, nullptr, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                    out = val;
                }
                };

            readStr(L"DisplayName", p.name);
            if (p.name.empty()) { RegCloseKey(hSub); continue; }
            readStr(L"DisplayVersion", p.version);
            readStr(L"Publisher", p.publisher);
            readStr(L"InstallLocation", p.installLoc);
            readStr(L"UninstallString", p.uninstallStr);
            readStr(L"QuietUninstallString", p.quietUninstallStr);
            readDword(L"EstimatedSize", p.sizeKB);

            result.push_back(p);
            RegCloseKey(hSub);
        }
        RegCloseKey(hKey);
    }
    return result;
}

static std::wstring BuildSilentUninstallCmd(const std::wstring& uninstallStr, const std::wstring& quietUninstallStr) {
    if (!quietUninstallStr.empty()) return quietUninstallStr;
    if (uninstallStr.empty()) return L"";
    std::wstring cmd = uninstallStr;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::towlower);

    if (cmd.find(L"msiexec") != std::wstring::npos) {
        std::wstring guid;
        auto pos = cmd.find(L'{');
        if (pos != std::wstring::npos) {
            auto end = cmd.find(L'}', pos);
            if (end != std::wstring::npos) guid = uninstallStr.substr(pos, end - pos + 1);
        }
        if (!guid.empty()) return L"msiexec.exe /x " + guid + L" /qn /norestart";
    }

    if (cmd.find(L"unins") != std::wstring::npos || cmd.find(L"inno") != std::wstring::npos) {
        return uninstallStr + L" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    }

    if (cmd.find(L"uninst") != std::wstring::npos) {
        return uninstallStr + L" /S";
    }

    return uninstallStr + L" /S";
}

static std::wstring GenerateHtmlReport() {
    std::wstring html = L"<html><head><meta charset='utf-8'><title>Optimizer Pro Report</title><style>body{font-family:Segoe UI;background:#0f111a;color:#e2e8f0;padding:20px}h1{color:#ff0055}h2{color:#00f0ff;border-bottom:1px solid #22283a;padding-bottom:8px}table{border-collapse:collapse;width:100%;margin:10px 0}td,th{border:1px solid #22283a;padding:8px;text-align:left}th{background:#1a1f2e}</style></head><body>";
    html += L"<h1>Optimizer Pro — System Report</h1>";
    time_t now = time(nullptr); struct tm tmNow; localtime_s(&tmNow, &now);
    html += Fmt(L"<p>Date: %04d-%02d-%02d %02d:%02d:%02d</p>", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
    html += L"<h2>System Info</h2><table>";
    std::wstring pcInfo; { std::lock_guard<std::mutex> lk(g_pcInfoMutex); pcInfo = g_pcInfoJson; }
    html += L"<tr><td>PC Info</td><td>" + pcInfo + L"</td></tr>";
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
    html += L"<tr><td>RAM Total</td><td>" + std::to_wstring(ms.ullTotalPhys / (1024 * 1024)) + L" MB</td></tr>";
    html += L"<tr><td>RAM Free</td><td>" + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) + L" MB</td></tr>";
    html += L"<tr><td>Processes</td><td>" + std::to_wstring(g_procCount.load()) + L"</td></tr>";
    html += L"<tr><td>CPU Usage</td><td>" + FloatToJsonStr(g_cpuUsage.load()) + L"%</td></tr>";
    html += L"<tr><td>GPU Usage</td><td>" + FloatToJsonStr(g_gpuUsage.load()) + L"%</td></tr>";
    html += L"<tr><td>CPU Temp</td><td>" + FloatToJsonStr(g_cpuTemp.load()) + L" C</td></tr>";
    html += L"<tr><td>GPU Temp</td><td>" + FloatToJsonStr(g_gpuTemp.load()) + L" C</td></tr>";
    html += L"</table>";
    {
        std::wstring disks; { std::lock_guard<std::mutex> lk(g_diskHealthMutex); disks = g_diskHealthJson; }
        html += L"<h2>Disks</h2><pre>" + disks + L"</pre>";
    }
    html += L"</body></html>";
    std::wstring outPath = g_paths.BackupDir + L"\\report.html";
    std::ofstream f(outPath.c_str(), std::ios::binary);
    if (f) { std::string u = WideToUtf8(html); f.write(u.data(), (std::streamsize)u.size()); }
    return outPath;
}

static bool BackupSettings() {
    std::wstring path = g_paths.BackupDir + L"\\settings.json";
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
    std::wstring j = L"{";
    j += L"\"cleanTemp\":" + std::wstring(s.cleanTemp ? L"true" : L"false");
    j += L",\"cleanPrefetch\":" + std::wstring(s.cleanPrefetch ? L"true" : L"false");
    j += L",\"cleanOldPrefetch\":" + std::wstring(s.cleanOldPrefetch ? L"true" : L"false");
    j += L",\"oldPrefetchDays\":" + std::to_wstring(s.oldPrefetchDays);
    j += L",\"cleanRecycle\":" + std::wstring(s.cleanRecycle ? L"true" : L"false");
    j += L",\"cleanThumbs\":" + std::wstring(s.cleanThumbs ? L"true" : L"false");
    j += L",\"cleanShader\":" + std::wstring(s.cleanShader ? L"true" : L"false");
    j += L",\"cleanDns\":" + std::wstring(s.cleanDns ? L"true" : L"false");
    j += L",\"cleanDO\":" + std::wstring(s.cleanDO ? L"true" : L"false");
    j += L",\"cleanFontCache\":" + std::wstring(s.cleanFontCache ? L"true" : L"false");
    j += L",\"cleanStoreCache\":" + std::wstring(s.cleanStoreCache ? L"true" : L"false");
    j += L",\"cleanUpdateCache\":" + std::wstring(s.cleanUpdateCache ? L"true" : L"false");
    j += L",\"clean0ByteFiles\":" + std::wstring(s.clean0ByteFiles ? L"true" : L"false");
    j += L",\"cleanLaunchers\":" + std::wstring(s.cleanLaunchers ? L"true" : L"false");
    j += L",\"cleanBrowsers\":" + std::wstring(s.cleanBrowsers ? L"true" : L"false");
    j += L",\"cleanChats\":" + std::wstring(s.cleanChats ? L"true" : L"false");
    j += L",\"cleanDumps\":" + std::wstring(s.cleanDumps ? L"true" : L"false");
    j += L",\"ramAggressiveness\":" + std::to_wstring(s.ramAggressiveness);
    j += L",\"ramClearStandby\":" + std::wstring(s.ramClearStandby ? L"true" : L"false");
    j += L",\"ramFlushModified\":" + std::wstring(s.ramFlushModified ? L"true" : L"false");
    j += L",\"ramClearWorkingSets\":" + std::wstring(s.ramClearWorkingSets ? L"true" : L"false");
    j += L",\"ramClearFileCache\":" + std::wstring(s.ramClearFileCache ? L"true" : L"false");
    j += L",\"autoBoost\":" + std::wstring(s.autoBoost ? L"true" : L"false");
    j += L",\"disableFSO\":" + std::wstring(s.disableFSO ? L"true" : L"false");
    j += L",\"overlayDarkTheme\":" + std::wstring(s.overlayDarkTheme ? L"true" : L"false");
    j += L",\"hotkeysEnabled\":" + std::wstring(s.hotkeysEnabled ? L"true" : L"false");
    j += L",\"ramAutoCleanEnabled\":" + std::wstring(s.ramAutoCleanEnabled ? L"true" : L"false");
    j += L",\"ramAutoCleanThresholdPct\":" + std::to_wstring(s.ramAutoCleanThresholdPct);
    j += L",\"ramAutoCleanIntervalMin\":" + std::to_wstring(s.ramAutoCleanIntervalMin);
    j += L",\"ramAutoCleanOnIdle\":" + std::wstring(s.ramAutoCleanOnIdle ? L"true" : L"false");

    j += L",\"s50_telemetry\":" + std::wstring(s.s50_telemetry ? L"true" : L"false");
    j += L",\"s50_cortana\":" + std::wstring(s.s50_cortana ? L"true" : L"false");
    j += L",\"s50_adid\":" + std::wstring(s.s50_adid ? L"true" : L"false");
    j += L",\"s50_actfeed\":" + std::wstring(s.s50_actfeed ? L"true" : L"false");
    j += L",\"s50_smartscreen\":" + std::wstring(s.s50_smartscreen ? L"true" : L"false");
    j += L",\"s50_uac\":" + std::wstring(s.s50_uac ? L"true" : L"false");
    j += L",\"s50_defender\":" + std::wstring(s.s50_defender ? L"true" : L"false");
    j += L",\"s50_vbs\":" + std::wstring(s.s50_vbs ? L"true" : L"false");
    j += L",\"s50_etw\":" + std::wstring(s.s50_etw ? L"true" : L"false");
    j += L",\"s50_winupd\":" + std::wstring(s.s50_winupd ? L"true" : L"false");
    j += L",\"s50_gamemode\":" + std::wstring(s.s50_gamemode ? L"true" : L"false");
    j += L",\"s50_gamedvr\":" + std::wstring(s.s50_gamedvr ? L"true" : L"false");
    j += L",\"s50_prio\":" + std::wstring(s.s50_prio ? L"true" : L"false");
    j += L",\"s50_netthrottle\":" + std::wstring(s.s50_netthrottle ? L"true" : L"false");
    j += L",\"s50_nagle\":" + std::wstring(s.s50_nagle ? L"true" : L"false");
    j += L",\"s50_dynamictick\":" + std::wstring(s.s50_dynamictick ? L"true" : L"false");
    j += L",\"s50_hags\":" + std::wstring(s.s50_hags ? L"true" : L"false");
    j += L",\"s50_mouseaccel\":" + std::wstring(s.s50_mouseaccel ? L"true" : L"false");
    j += L",\"s50_power\":" + std::wstring(s.s50_power ? L"true" : L"false");
    j += L",\"s50_zero\":" + std::wstring(s.s50_zero ? L"true" : L"false");
    j += L",\"s50_minecraft\":" + std::wstring(s.s50_minecraft ? L"true" : L"false");
    j += L",\"s50_print\":" + std::wstring(s.s50_print ? L"true" : L"false");
    j += L",\"s50_icons\":" + std::wstring(s.s50_icons ? L"true" : L"false");
    j += L",\"s50_pools\":" + std::wstring(s.s50_pools ? L"true" : L"false");
    j += L",\"s50_paging\":" + std::wstring(s.s50_paging ? L"true" : L"false");
    j += L",\"s50_memcomp\":" + std::wstring(s.s50_memcomp ? L"true" : L"false");
    j += L",\"s50_fastdns\":" + std::wstring(s.s50_fastdns ? L"true" : L"false");
    j += L",\"s50_rss\":" + std::wstring(s.s50_rss ? L"true" : L"false");
    j += L",\"s50_resetnet\":" + std::wstring(s.s50_resetnet ? L"true" : L"false");
    j += L",\"s50_ping\":" + std::wstring(s.s50_ping ? L"true" : L"false");
    j += L",\"s50_services\":" + std::wstring(s.s50_services ? L"true" : L"false");
    j += L",\"s50_ctxmenu\":" + std::wstring(s.s50_ctxmenu ? L"true" : L"false");
    j += L",\"s50_usb\":" + std::wstring(s.s50_usb ? L"true" : L"false");
    j += L",\"s50_pcie\":" + std::wstring(s.s50_pcie ? L"true" : L"false");
    j += L",\"s50_nvapi\":" + std::wstring(s.s50_nvapi ? L"true" : L"false");

    j += L"}";
    std::ofstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::string u = WideToUtf8(j);
    f.write(u.data(), (std::streamsize)u.size());
    return true;
}

static bool RestoreSettingsFromBackup() {
    std::wstring path = g_paths.BackupDir + L"\\settings.json";
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    std::string utf8 = ss.str();
    auto getBool = [&](const char* key, bool& out) {
        std::string needle = std::string("\"") + key + "\":";
        auto pos = utf8.find(needle);
        if (pos == std::string::npos) return;
        auto start = pos + needle.size();
        if (utf8.compare(start, 4, "true") == 0) out = true;
        else out = false;
        };
    auto getInt = [&](const char* key, int& out) {
        std::string needle = std::string("\"") + key + "\":";
        auto pos = utf8.find(needle);
        if (pos == std::string::npos) return;
        auto start = pos + needle.size();
        try { out = std::stoi(utf8.substr(start)); }
        catch (...) {}
        };
    std::lock_guard<std::mutex> lk(g_settingsMutex);
    getBool("cleanTemp", g_userSettings.cleanTemp);
    getBool("cleanPrefetch", g_userSettings.cleanPrefetch);
    getBool("cleanOldPrefetch", g_userSettings.cleanOldPrefetch);
    getInt("oldPrefetchDays", g_userSettings.oldPrefetchDays);
    getBool("cleanRecycle", g_userSettings.cleanRecycle);
    getBool("cleanThumbs", g_userSettings.cleanThumbs);
    getBool("cleanShader", g_userSettings.cleanShader);
    getBool("cleanDns", g_userSettings.cleanDns);
    getBool("cleanDO", g_userSettings.cleanDO);
    getBool("cleanFontCache", g_userSettings.cleanFontCache);
    getBool("cleanStoreCache", g_userSettings.cleanStoreCache);
    getBool("cleanUpdateCache", g_userSettings.cleanUpdateCache);
    getBool("clean0ByteFiles", g_userSettings.clean0ByteFiles);
    getBool("cleanLaunchers", g_userSettings.cleanLaunchers);
    getBool("cleanBrowsers", g_userSettings.cleanBrowsers);
    getBool("cleanChats", g_userSettings.cleanChats);
    getBool("cleanDumps", g_userSettings.cleanDumps);
    getInt("ramAggressiveness", g_userSettings.ramAggressiveness);
    getBool("ramClearStandby", g_userSettings.ramClearStandby);
    getBool("ramFlushModified", g_userSettings.ramFlushModified);
    getBool("ramClearWorkingSets", g_userSettings.ramClearWorkingSets);
    getBool("ramClearFileCache", g_userSettings.ramClearFileCache);
    getBool("autoBoost", g_userSettings.autoBoost);
    getBool("disableFSO", g_userSettings.disableFSO);
    getBool("overlayDarkTheme", g_userSettings.overlayDarkTheme);
    getBool("hotkeysEnabled", g_userSettings.hotkeysEnabled);
    getBool("ramAutoCleanEnabled", g_userSettings.ramAutoCleanEnabled);
    getInt("ramAutoCleanThresholdPct", g_userSettings.ramAutoCleanThresholdPct);
    getInt("ramAutoCleanIntervalMin", g_userSettings.ramAutoCleanIntervalMin);
    getBool("ramAutoCleanOnIdle", g_userSettings.ramAutoCleanOnIdle);

    getBool("s50_telemetry", g_userSettings.s50_telemetry);
    getBool("s50_cortana", g_userSettings.s50_cortana);
    getBool("s50_adid", g_userSettings.s50_adid);
    getBool("s50_actfeed", g_userSettings.s50_actfeed);
    getBool("s50_smartscreen", g_userSettings.s50_smartscreen);
    getBool("s50_uac", g_userSettings.s50_uac);
    getBool("s50_defender", g_userSettings.s50_defender);
    getBool("s50_vbs", g_userSettings.s50_vbs);
    getBool("s50_etw", g_userSettings.s50_etw);
    getBool("s50_winupd", g_userSettings.s50_winupd);
    getBool("s50_gamemode", g_userSettings.s50_gamemode);
    getBool("s50_gamedvr", g_userSettings.s50_gamedvr);
    getBool("s50_prio", g_userSettings.s50_prio);
    getBool("s50_netthrottle", g_userSettings.s50_netthrottle);
    getBool("s50_nagle", g_userSettings.s50_nagle);
    getBool("s50_dynamictick", g_userSettings.s50_dynamictick);
    getBool("s50_hags", g_userSettings.s50_hags);
    getBool("s50_mouseaccel", g_userSettings.s50_mouseaccel);
    getBool("s50_power", g_userSettings.s50_power);
    getBool("s50_zero", g_userSettings.s50_zero);
    getBool("s50_minecraft", g_userSettings.s50_minecraft);
    getBool("s50_print", g_userSettings.s50_print);
    getBool("s50_icons", g_userSettings.s50_icons);
    getBool("s50_pools", g_userSettings.s50_pools);
    getBool("s50_paging", g_userSettings.s50_paging);
    getBool("s50_memcomp", g_userSettings.s50_memcomp);
    getBool("s50_fastdns", g_userSettings.s50_fastdns);
    getBool("s50_rss", g_userSettings.s50_rss);
    getBool("s50_resetnet", g_userSettings.s50_resetnet);
    getBool("s50_ping", g_userSettings.s50_ping);
    getBool("s50_services", g_userSettings.s50_services);
    getBool("s50_ctxmenu", g_userSettings.s50_ctxmenu);
    getBool("s50_usb", g_userSettings.s50_usb);
    getBool("s50_pcie", g_userSettings.s50_pcie);
    getBool("s50_nvapi", g_userSettings.s50_nvapi);

    g_overlay.darkTheme = g_userSettings.overlayDarkTheme;
    return true;
}

static bool RestoreToPoint(uint32_t seq) {
    std::wstring cmd = L"powershell -NoProfile -Command \"Restore-Computer -RestorePoint " + std::to_wstring(seq) + L" -Confirm:$false\"";
    return RunCmd(cmd, L"Restore point", false, 600);
}

static std::wstring RunSFC() {
    CmdResult r = RunCmdImpl(L"sfc /scannow", 3600);
    std::wstring out = Utf8ToWide(r.out);
    if (out.empty()) out = Utf8ToWide(r.err);
    if (out.size() > 4000) out = out.substr(out.size() - 4000);
    return out;
}

static std::wstring RunDISM() {
    CmdResult r = RunCmdImpl(L"DISM /Online /Cleanup-Image /RestoreHealth", 3600);
    std::wstring out = Utf8ToWide(r.out);
    if (out.empty()) out = Utf8ToWide(r.err);
    if (out.size() > 4000) out = out.substr(out.size() - 4000);
    return out;
}

static std::wstring PauseWindowsUpdates(int days) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
        u.QuadPart += (ULONGLONG)days * 24ULL * 60ULL * 60ULL * 10000000ULL;
        FILETIME ftOut; ftOut.dwLowDateTime = u.LowPart; ftOut.dwHighDateTime = u.HighPart;
        SYSTEMTIME st; FileTimeToSystemTime(&ftOut, &st);
        std::wstring iso = Fmt(L"%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        RegSetValueExW(hKey, L"PauseUpdatesExpiryTime", 0, REG_SZ, (const BYTE*)iso.c_str(), (DWORD)((iso.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"PauseFeatureUpdatesStartTime", 0, REG_SZ, (const BYTE*)iso.c_str(), (DWORD)((iso.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"PauseQualityUpdatesStartTime", 0, REG_SZ, (const BYTE*)iso.c_str(), (DWORD)((iso.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return L"{\"ok\":true,\"until\":" + JsonStr(iso) + L"}";
    }
    return L"{\"ok\":false}";
}

static std::wstring ResumeWindowsUpdates() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings", 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"PauseUpdatesExpiryTime");
        RegDeleteValueW(hKey, L"PauseFeatureUpdatesStartTime");
        RegDeleteValueW(hKey, L"PauseQualityUpdatesStartTime");
        RegCloseKey(hKey);
    }
    RunCmd(L"sc config wuauserv start= auto", L"", false);
    RunCmd(L"net start wuauserv", L"", false);
    return L"{\"ok\":true}";
}

static void ResetNetwork() {
    RunCmd(L"ipconfig /release", L"Release IP", false, 60);
    RunCmd(L"ipconfig /renew", L"Renew IP", false, 120);
    RunCmd(L"ipconfig /flushdns", L"Flush DNS", false, 30);
    RunCmd(L"netsh winsock reset", L"Winsock reset", false, 60);
    RunCmd(L"netsh int ip reset", L"TCP/IP reset", false, 60);
    RunCmd(L"netsh advfirewall reset", L"Firewall reset", false, 60);
}

static bool ToggleHAGS(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = enable ? 2 : 1;
        RegSetValueExW(hKey, L"HwSchMode", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

static void ToggleMouseAcceleration(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Mouse", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        const wchar_t* v1 = enable ? L"1" : L"0";
        const wchar_t* v2 = enable ? L"6" : L"0";
        const wchar_t* v3 = enable ? L"10" : L"0";
        RegSetValueExW(hKey, L"MouseSpeed", 0, REG_SZ, (const BYTE*)v1, (DWORD)((wcslen(v1) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"MouseThreshold1", 0, REG_SZ, (const BYTE*)v2, (DWORD)((wcslen(v2) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"MouseThreshold2", 0, REG_SZ, (const BYTE*)v3, (DWORD)((wcslen(v3) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        int mouseParams[3] = { enable ? 1 : 0, enable ? 6 : 0, enable ? 10 : 0 };
        SystemParametersInfoW(SPI_SETMOUSE, 0, mouseParams, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }
}

static void ToggleUSBSelectiveSuspend(bool enable) {
    HKEY hKey = nullptr;
    std::wstring path = L"SYSTEM\\CurrentControlSet\\Services\\USB\\Parameters";
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = enable ? 0 : 1;
        RegSetValueExW(hKey, L"DisableSelectiveSuspend", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    std::wstring val = enable ? L"1" : L"0";
    RunCmd(L"powercfg /setacvalueindex SCHEME_CURRENT 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 " + val, L"", false);
    RunCmd(L"powercfg /setdcvalueindex SCHEME_CURRENT 2a737441-1930-4402-8d77-b2bebba308a3 48e6b7a6-50f5-4782-a5d4-53bb8f07e226 " + val, L"", false);
    RunCmd(L"powercfg /setactive SCHEME_CURRENT", L"", false);
}

static void SetPCIELinkState(bool maxPerf) {
    std::wstring val = maxPerf ? L"0" : L"2";
    RunCmd(L"powercfg /setacvalueindex SCHEME_CURRENT 501a4d13-42af-4429-9fd1-a8218c268e20 ee12f906-d277-404b-b6da-e5fa1a576df5 " + val, L"", false);
    RunCmd(L"powercfg /setactive SCHEME_CURRENT", L"", false);
}

static std::wstring GetPageFileInfo() {
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
    uint64_t totalRamMB = ms.ullTotalPhys / (1024 * 1024);
    uint64_t recommendedMin = totalRamMB / 2;
    uint64_t recommendedMax = totalRamMB;
    if (totalRamMB >= 32ULL * 1024) { recommendedMin = 2048; recommendedMax = 8192; }
    else if (totalRamMB >= 16ULL * 1024) { recommendedMin = 4096; recommendedMax = 16384; }
    return L"{\"totalRamMB\":" + std::to_wstring(totalRamMB) + L",\"recommendedMin\":" + std::to_wstring(recommendedMin) + L",\"recommendedMax\":" + std::to_wstring(recommendedMax) + L"}";
}

static bool SetPageFileSize(uint32_t initialMB, uint32_t maxMB) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD pm = 0;
        RegSetValueExW(hKey, L"AutomaticManagedPagefile", 0, REG_DWORD, (const BYTE*)&pm, sizeof(pm));
        wchar_t buf[256] = {};
        swprintf_s(buf, L"%c:\\pagefile.sys %u %u", g_systemDriveLetter, initialMB, maxMB);
        RegSetValueExW(hKey, L"PagingFiles", 0, REG_MULTI_SZ, (const BYTE*)buf, (DWORD)((wcslen(buf) + 2) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

static std::wstring AnalyzeEventLog() {
    CmdResult r = RunCmdImpl(L"powershell -NoProfile -Command \"Get-WinEvent -FilterHashtable @{LogName='System'; Level=2,3; StartTime=(Get-Date).AddDays(-7)} -MaxEvents 50 | Select-Object -Property TimeCreated,Id,LevelDisplayName,ProviderName,Message | ConvertTo-Json -Compress\"", 60);
    if (r.exitCode == 0 && !r.out.empty()) {
        std::wstring w = Utf8ToWide(r.out);
        if (w.size() > 8000) w = w.substr(0, 8000);
        return w;
    }
    return L"[]";
}

static std::wstring GetBatteryHealth() {
    CmdResult r = RunCmdImpl(L"powershell -NoProfile -Command \"Get-CimInstance Win32_Battery | Select-Object -Property EstimatedChargeRemaining,BatteryStatus,DesignVoltage | ConvertTo-Json -Compress\"", 15);
    if (r.exitCode == 0 && !r.out.empty()) {
        return Utf8ToWide(r.out);
    }
    return L"{\"Battery\":0}";
}

static void ApplyNetworkTweaks(bool enable) {
    HKEY hNetKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\NetworkCards", 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hNetKey) == ERROR_SUCCESS) {
        DWORD idx = 0;
        wchar_t sub[256]; DWORD subLen = 256;
        while (true) {
            subLen = 256;
            if (RegEnumKeyExW(hNetKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY hCard = nullptr;
            if (RegOpenKeyExW(hNetKey, sub, 0, KEY_READ, &hCard) == ERROR_SUCCESS) {
                wchar_t guid[256] = {}; DWORD sz = sizeof(guid); DWORD type;
                if (RegQueryValueExW(hCard, L"NetCfgInstanceId", nullptr, &type, (LPBYTE)guid, &sz) == ERROR_SUCCESS) {
                    HKEY hParam = nullptr;
                    std::wstring paramPath = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces\\" + std::wstring(guid);
                    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, paramPath.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hParam, nullptr) == ERROR_SUCCESS) {
                        std::wstring val = enable ? L"1" : L"0";
                        RegSetValueExW(hParam, L"*RSS", 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
                        RegSetValueExW(hParam, L"*MSIX", 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));

                        DWORD rssProfile = enable ? 4 : 1;
                        RegSetValueExW(hParam, L"*RSSProfile", 0, REG_DWORD, (const BYTE*)&rssProfile, sizeof(rssProfile));
                        RegCloseKey(hParam);
                    }
                }
                RegCloseKey(hCard);
            }
        }
        RegCloseKey(hNetKey);
    }
}

static void DisableETW() {
    RunCmd(L"logman stop EventLog-Application -ets", L"", false);
    RunCmd(L"logman stop EventLog-System -ets", L"", false);
    RunCmd(L"logman stop EventLog-Security -ets", L"", false);
    RunCmd(L"logman stop WdiContextLog -ets", L"", false);
    RunCmd(L"logman stop AutoLogger-Diagtrack-Listener -ets", L"", false);
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\WMI\\Autologger\\AutoLogger-Diagtrack-Listener", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = 0;
        RegSetValueExW(hKey, L"Start", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

static void DisableVBS() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = 0;
        RegSetValueExW(hKey, L"Enabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    RunCmd(L"bcdedit /set hypervisorlaunchtype off", L"Disable Hypervisor", false);
}

static void ApplyNvidiaPState(int state) {
    wchar_t path[MAX_PATH] = {};
    if (SearchPathW(nullptr, L"nvidia-smi.exe", nullptr, MAX_PATH, path, nullptr) > 0) {
        std::wstring cmd;
        if (state == 0) cmd = L"\"" + std::wstring(path) + L"\" -pm 1";
        else if (state == 1) cmd = L"\"" + std::wstring(path) + L"\" -pm 0";
        else if (state == 2) cmd = L"\"" + std::wstring(path) + L"\" -lgc 0,2100";
        RunCmd(cmd, L"NVAPI P-State", false);
    }
}

static std::wstring RunDDU() {
    std::wstring dduPath = g_paths.AppDir + L"\\DDU\\Display Driver Uninstaller.exe";
    if (fs::exists(dduPath)) {
        ShellExecuteW(nullptr, L"open", dduPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return L"{\"ok\":true,\"msg\":\"DDU launched\"}";
    }
    return L"{\"ok\":false,\"msg\":\"DDU not found. Place DDU folder next to exe.\"}";
}

static void ApplyS50Settings() {
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
    if (s.s50_telemetry) DisableTelemetry();
    if (s.s50_cortana) { HKEY k; if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { DWORD v = 0; RegSetValueExW(k, L"AllowCortana", 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); RegCloseKey(k); } }
    if (s.s50_adid) { HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { DWORD v = 0; RegSetValueExW(k, L"Enabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); RegCloseKey(k); } }
    if (s.s50_actfeed) { HKEY k; if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { DWORD v = 0; RegSetValueExW(k, L"EnableActivityFeed", 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); RegCloseKey(k); } }
    if (s.s50_smartscreen) { HKEY k; if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { DWORD v = 0; RegSetValueExW(k, L"EnableSmartScreen", 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); RegCloseKey(k); } }
    if (s.s50_uac) { HKEY k; if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { DWORD v = 0; RegSetValueExW(k, L"EnableLUA", 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); RegCloseKey(k); } }
    if (s.s50_defender) DisableDefender();
    if (s.s50_vbs) DisableVBS();
    if (s.s50_etw) DisableETW();
    if (s.s50_winupd) RunCmd(L"sc config wuauserv start= disabled", L"Disable WU", false);
    if (s.s50_gamemode) SetGameMode(true);
    if (s.s50_gamedvr) { HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { DWORD v = 0; RegSetValueExW(k, L"GameDVR_Enabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); RegCloseKey(k); } }
    if (s.s50_prio) SetWin32PrioritySeparation(0x26);
    if (s.s50_netthrottle) DisableNetworkThrottling();
    if (s.s50_nagle) DisableNagle();
    if (s.s50_dynamictick) DisableDynamictick();
    if (s.s50_hags) ToggleHAGS(true);
    if (s.s50_mouseaccel) ToggleMouseAcceleration(false);
    if (s.s50_power) RunCmd(std::wstring(L"powercfg /setactive ") + ULTIMATE_GUID, L"Power Ultimate", false);
    if (s.s50_zero) Clean0ByteFiles(g_activeDrive.load());
    if (s.s50_minecraft) CleanMinecraftCache();
    if (s.s50_print) CleanPrintQueue();
    if (s.s50_icons) CleanIconCache();
    if (s.s50_pools) ClearKernelPools();
    if (s.s50_paging) {
        MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms); GlobalMemoryStatusEx(&ms);
        if (ms.ullTotalPhys >= 32ULL * 1024 * 1024 * 1024) {
            RunCmd(L"wmic computersystem set AutomaticManagedPagefile=False", L"Disable Paging");
            RunCmd(L"wmic pagefileset where name=\"C:\\\\pagefile.sys\" delete", L"Delete pagefile");
        }
    }
    if (s.s50_memcomp) RunCmd(L"powershell -NoProfile -Command \"Disable-MMAgent -MemoryCompression\"", L"Disable Mem Comp");
    if (s.s50_fastdns) SetFastDNS();
    if (s.s50_rss) ApplyNetworkTweaks(true);
    if (s.s50_resetnet) ResetNetwork();
    if (s.s50_ping) {}
    if (s.s50_services) DisableDeepServices();
    if (s.s50_ctxmenu) SetClassicContextMenu(true);
    if (s.s50_usb) ToggleUSBSelectiveSuspend(false);
    if (s.s50_pcie) SetPCIELinkState(true);
    if (s.s50_nvapi) ApplyNvidiaPState(0);
}

static std::wstring GetSecurityStatus() {
    bool defender = false, firewall = false, uac = false, wu = false;
    HKEY hk;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD val = 0, sz = sizeof(val);
        if (RegQueryValueExW(hk, L"DisableRealtimeMonitoring", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) defender = (val == 0);
        RegCloseKey(hk);
    }
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD val = 0, sz = sizeof(val);
        if (RegQueryValueExW(hk, L"EnableFirewall", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) firewall = (val != 0);
        RegCloseKey(hk);
    }
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD val = 0, sz = sizeof(val);
        if (RegQueryValueExW(hk, L"EnableLUA", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) uac = (val != 0);
        RegCloseKey(hk);
    }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, L"wuauserv", SERVICE_QUERY_STATUS);
        if (svc) {
            SERVICE_STATUS status;
            if (QueryServiceStatus(svc, &status)) wu = (status.dwCurrentState == SERVICE_RUNNING);
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }
    return L"{\"defender\":" + std::wstring(defender ? L"true" : L"false") +
        L",\"firewall\":" + std::wstring(firewall ? L"true" : L"false") +
        L",\"uac\":" + std::wstring(uac ? L"true" : L"false") +
        L",\"wu\":" + std::wstring(wu ? L"true" : L"false") + L"}";
}

static std::wstring ScanDriveFast(wchar_t drive) {
    std::wstring root = std::wstring(1, drive) + L":\\";
    std::error_code ec{};
    std::vector<std::pair<std::wstring, uint64_t>> folders;
    if (fs::exists(root, ec) && !ec) {
        for (auto& p : fs::directory_iterator(root, ec)) {
            if (p.is_directory(ec) && !ec) {
                std::wstring path = p.path().wstring();
                uint64_t size = 0;
                HANDLE hFind = FindFirstFileExW((path + L"\\*").c_str(), FindExInfoStandard, nullptr, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
                if (hFind != INVALID_HANDLE_VALUE) {
                    WIN32_FIND_DATAW fd;
                    if (FindNextFileW(hFind, &fd)) {
                        if (fd.nFileSizeHigh > 0 || fd.nFileSizeLow > 0) size += ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                    }
                    FindClose(hFind);
                }
                folders.push_back({ p.path().filename().wstring(), size });
            }
        }
    }
    std::sort(folders.begin(), folders.end(), [](auto& a, auto& b) { return a.second > b.second; });
    std::wstring j = L"[";
    for (size_t i = 0; i < folders.size() && i < 50; ++i) {
        if (i) j += L",";
        j += L"{\"name\":" + JsonStr(folders[i].first) + L",\"size\":" + std::to_wstring(folders[i].second) + L"}";
    }
    return L"{\"items\":" + j + L"]}";
}

static const char* HTML_UI = R"HTML(<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8"><title>Optimizer Pro</title><style>
@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&family=Rajdhani:wght@400;600;700&display=swap');
:root{--bg:#050508;--surface:#0f111a;--border:#22283a;--text:#e2e8f0;--muted:#64748b;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(15,17,26,.85);--bg-img:none}
body[data-theme="black"]{--bg:#000000;--surface:#111111;--border:#333333;--text:#ffffff;--muted:#888888;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(0,0,0,.9);--bg-img:none}
body[data-theme="white"]{--bg:#ffffff;--surface:#f5f5f5;--border:#e0e0e0;--text:#000000;--muted:#666666;--accent:#ff0055;--accent2:#0088ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(255,255,255,.9);--bg-img:none}
body[data-theme="midnight"]{--bg:#050508;--surface:#0f111a;--border:#22283a;--text:#e2e8f0;--muted:#64748b;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(15,17,26,.85);--bg-img:none}
body[data-theme="glass"]{--bg:#a1c4fd;--surface:rgba(255,255,255,.2);--border:rgba(255,255,255,.4);--text:#000;--muted:#333;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(255,255,255,.1);--bg-img:linear-gradient(135deg, #a1c4fd 0%, #c2e9fb 100%)}
body[data-theme="glass"] *{border-radius:12px!important;box-shadow:0 4px 15px rgba(0,0,0,.1)!important}
body[data-theme="glass"] button, body[data-theme="glass"] .nav-item, body[data-theme="glass"] .card, body[data-theme="glass"] .section, body[data-theme="glass"] .sidebar, body[data-theme="glass"] .titlebar{backdrop-filter:blur(20px) saturate(180%);-webkit-backdrop-filter:blur(20px) saturate(180%);background:rgba(255,255,255,.25);border:1px solid rgba(255,255,255,.5);color:#000}
body[data-theme="glass"] button:hover{background:rgba(255,255,255,.5);color:#000}
body[data-theme="glass"] input{background:rgba(255,255,255,.3);color:#000;border:1px solid rgba(255,255,255,.5)}
*{box-sizing:border-box;margin:0;padding:0}html,body{height:100%}
body{font-family:'Rajdhani',sans-serif;background-color:var(--bg);color:var(--text);overflow:hidden;font-size:15px;user-select:none;transition:background-color .3s,color .3s; background-image:var(--bg-img); position:relative}
h1,h2,.logo,.titlebar-text{font-family:'Orbitron',sans-serif;text-transform:uppercase;letter-spacing:1px;}
.titlebar{height:36px;background:var(--surface);display:flex;justify-content:space-between;align-items:center;border-bottom:2px solid var(--border);position:relative;z-index:100}
.titlebar-text{padding-left:15px;font-weight:900;font-size:14px;display:flex;align-items:center;gap:8px;color:var(--accent);text-shadow:0 0 8px rgba(255,0,85,.6);}
.titlebar-controls{display:flex;height:100%}
.tb-btn{width:46px;height:100%;background:transparent;border:none;color:var(--muted);cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:16px}
.tb-btn:hover{background:var(--border);color:var(--text)}.tb-btn.close:hover{background:var(--accent);color:#fff;}
.app{display:grid;grid-template-columns:240px 1fr;height:calc(100vh - 36px)}
.sidebar{backdrop-filter:blur(12px);background:var(--glass);border-right:1px solid var(--border);padding:20px 0;display:flex;flex-direction:column;overflow-y:auto}
.logo{padding:0 20px 20px;font-size:20px;font-weight:900;display:flex;align-items:center;gap:10px;color:var(--text);border-bottom:1px solid var(--border);margin-bottom:10px}
.nav-item{padding:12px 20px;cursor:pointer;color:var(--muted);border-left:4px solid transparent;display:flex;gap:12px;align-items:center;font-size:15px;font-weight:600}
.nav-item:hover,.nav-item.active{background:rgba(255,0,85,.1);color:var(--text);border-left-color:var(--accent);}
.nav-spacer{flex:1}
.main{display:flex;flex-direction:column;overflow:hidden}
.content{flex:1;overflow-y:auto;padding:30px 40px}
::-webkit-scrollbar{width:10px}::-webkit-scrollbar-thumb{background:var(--accent);border-radius:5px}
.tab{display:none}.tab.active{display:block}
h1{font-size:28px;margin-bottom:8px;color:var(--accent2);}h2{font-size:16px;margin:24px 0 14px;color:var(--muted);border-bottom:1px solid var(--border);padding-bottom:8px;}
.subtitle{color:var(--muted);margin-bottom:24px;font-size:15px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:16px;margin-bottom:24px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:18px;position:relative}
.card::before{content:'';position:absolute;top:0;left:0;width:100%;height:3px;background:linear-gradient(90deg,var(--accent),var(--accent2));}
.card-label{color:var(--muted);font-size:13px;text-transform:uppercase;font-weight:700;margin-bottom:8px}
.card-value{font-size:26px;font-weight:700;font-family:'Orbitron';}
.card-value.good{color:var(--success)}
.section{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:24px;margin-bottom:16px;}
button{background:var(--surface);color:var(--accent2);border:1px solid var(--accent2);padding:12px 24px;border-radius:4px;font-weight:700;cursor:pointer;display:inline-flex;align-items:center;gap:8px;font-size:14px;text-transform:uppercase;}
button:hover{background:var(--accent2);color:#000;}
button.secondary{border-color:var(--border);color:var(--muted);}
button.danger{border-color:var(--accent);color:var(--accent);}
button.danger:hover{background:var(--accent);color:#fff;}
button.success{border-color:var(--success);color:var(--success);}
button.success:hover{background:var(--success);color:#fff;}
button.loading{opacity:.7;pointer-events:none}
.check-item{display:flex;align-items:center;padding:12px 0;border-bottom:1px solid var(--border);cursor:pointer}
.check-item label{margin-left:12px;cursor:pointer;flex:1;font-weight:600;font-size:16px;}
input[type=checkbox]{width:20px;height:20px;border:2px solid var(--border);border-radius:2px;cursor:pointer;background:var(--bg);}
input[type=checkbox]:checked{background:var(--accent);border-color:var(--accent);}
input[type=text],input[type=number],select,textarea{padding:10px 14px;background:var(--bg);border:1px solid var(--border);border-radius:4px;color:var(--text);font-size:15px;outline:none;font-family:'Rajdhani';}
input[type=range]{height:6px;background:var(--border);border-radius:3px;outline:none;flex:1;min-width:100px}
textarea{width:100%;min-height:300px;font-family:'Consolas',monospace;font-size:14px;resize:vertical}
.log{background:rgba(0,0,0,.6);border:1px solid var(--border);border-radius:4px;padding:14px;font-family:'Consolas',monospace;font-size:13px;height:calc(100vh - 240px);color:var(--accent2);overflow-y:auto;user-select:text}
.status-bar{padding:12px 40px;background:var(--surface);border-top:1px solid var(--border);font-size:14px;color:var(--muted);display:flex;gap:12px;align-items:center;font-weight:600;text-transform:uppercase;position:relative}
#global-progress{position:absolute;bottom:0;left:0;height:4px;background:linear-gradient(90deg,var(--accent),var(--accent2));width:0%;transition:width .3s;}
.status-dot{width:12px;height:12px;border-radius:50%;background:var(--success);flex-shrink:0;}
.status-dot.busy{background:var(--warn);}
.hint{background:rgba(0,240,255,.05);border-left:4px solid var(--accent2);padding:16px;border-radius:0 4px 4px 0;font-size:15px;margin:16px 0;font-weight:500}
.list-row{display:flex;align-items:center;padding:14px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:10px;gap:12px;}
.list-row .info{flex:1;min-width:0}
.list-row .title{font-weight:700;font-size:16px}.list-row .sub{font-size:13px;color:var(--muted);font-family:Consolas,monospace;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#toasts{position:fixed;bottom:20px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:10px;pointer-events:none}
.toast{background:var(--surface);color:var(--text);padding:14px 20px;border-radius:4px;border-left:4px solid var(--accent2);box-shadow:0 8px 24px rgba(0,0,0,.8);max-width:340px;pointer-events:auto;font-weight:600}
.toast.error{border-left-color:var(--accent)}.toast.success{border-left-color:var(--success)}
.progress-bar{height:8px;background:var(--border);border-radius:4px;overflow:hidden;margin:16px 0}
.progress-fill{height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));width:0%;transition:width .3s;border-radius:4px;}
.chart-wrap{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:16px;margin-bottom:16px;}
#loadChart{width:100%;height:180px;display:block}
#treemapCanvas{width:100%;height:300px;display:block;border:1px solid var(--border);background:#0a0a0f}
.toolbar{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:16px}
.feat-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:12px;margin-top:12px}
.feat-item{background:var(--bg);border:1px solid var(--border);padding:12px;border-radius:4px;display:flex;justify-content:space-between;align-items:center;font-family:'Orbitron';font-size:12px;}
.feat-item.supported{border-color:var(--success);color:var(--success);}
.feat-item.unsupported{border-color:var(--border);color:var(--muted)}
.self-usage{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:20px}
.self-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:16px;text-align:center;}
.self-card .lbl{font-size:12px;color:var(--muted);text-transform:uppercase;font-weight:700;margin-bottom:6px}
.self-card .val{font-size:20px;font-weight:700;color:var(--accent2);font-family:'Orbitron';}
.app-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:12px;margin-top:12px}
.app-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;display:flex;justify-content:space-between;align-items:center;}
.app-card .title{font-weight:700;font-size:15px;color:var(--text)}
.app-card button{padding:8px 14px;font-size:12px}
.modal-overlay{display:none;position:fixed;inset:0;z-index:10000;background:rgba(0,0,0,.85);align-items:center;justify-content:center}
.modal-overlay.active{display:flex}
.modal-box{background:var(--surface);border:2px solid var(--accent);border-radius:8px;padding:24px;position:relative;max-width:90vw;max-height:90vh;overflow:auto;z-index:10001;width:540px;text-align:center;}
.modal-close{position:fixed;top:30px;right:30px;background:var(--accent);width:50px;height:50px;border-radius:50%;padding:0;justify-content:center;font-size:24px;z-index:99999;border:none;cursor:pointer;color:#fff;}
.warn-box{background:rgba(255,0,85,.1);border-left:4px solid var(--accent);padding:16px;border-radius:0 4px 4px 0;margin-bottom:16px;font-size:15px;line-height:1.6;font-weight:500}
.smart-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;margin-bottom:10px;}
.adv-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:16px}
.adv-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:16px;}
.adv-card h3{font-size:14px;margin-bottom:8px;color:var(--accent2);font-family:'Orbitron';}
.adv-card p{font-size:13px;color:var(--muted);margin-bottom:12px;line-height:1.5}
.smart-item{display:flex;align-items:flex-start;gap:14px;padding:16px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:10px;}
.smart-info{flex:1;min-width:0}
.smart-title{font-weight:700;font-size:16px;margin-bottom:6px;color:var(--text)}
.smart-badge{display:inline-block;padding:2px 8px;font-size:11px;border:1px solid;border-radius:10px;margin-left:6px;font-weight:700;text-transform:uppercase;}
.smart-size{display:inline-block;padding:2px 8px;font-size:11px;background:var(--surface);border:1px solid var(--border);border-radius:10px;margin-left:6px;font-weight:700;color:var(--accent2);}
.smart-desc{font-size:13px;color:var(--muted);line-height:1.5}
.smart-category{font-size:11px;font-weight:700;color:var(--accent);text-transform:uppercase;padding:4px 10px;border:1px solid var(--accent);border-radius:4px;flex-shrink:0;align-self:flex-start;white-space:nowrap}
.smart-total{padding:16px;background:rgba(0,240,255,.05);border:1px solid var(--accent2);border-radius:4px;font-size:16px;font-weight:700;text-align:center;margin:16px 0;color:var(--text);}
.smart-total b{color:var(--accent2);font-family:'Orbitron'}
.setting-group{padding:16px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:12px;}
.setting-group h3{font-size:13px;color:var(--accent2);margin-bottom:12px;font-family:'Orbitron';text-transform:uppercase}
.setting-row{display:flex;align-items:center;gap:12px;padding:8px 0;border-bottom:1px solid var(--border)}
.setting-row:last-child{border-bottom:none}
.setting-row label{flex:1;font-weight:600;font-size:14px}
.setting-row input[type=number]{width:80px}
.drive-current{padding:12px 16px;background:rgba(0,240,255,.08);border:1px solid var(--accent2);border-radius:4px;margin-bottom:12px;font-weight:700;color:var(--accent2);}
body[data-is-system="false"] .requires-system{display:none!important}
.badge-safe{background:rgba(16,185,129,.2);color:#10b981;padding:2px 6px;border-radius:4px;font-size:11px;margin-left:8px}
.badge-danger{background:rgba(239,68,68,.2);color:#ef4444;padding:2px 6px;border-radius:4px;font-size:11px;margin-left:8px}
</style></head><body data-theme="midnight" data-is-system="true">

<div class="titlebar" id="titlebar">
  <div class="titlebar-text">⚡ OPTIMIZER PRO</div>
  <div class="titlebar-controls">
    <button class="tb-btn" data-wc="min">—</button>
    <button class="tb-btn" data-wc="max">□</button>
    <button class="tb-btn close" data-wc="close">✕</button>
  </div>
</div>

<div class="app">
  <div class="sidebar">
    <div class="logo">⚡<span class="lbl">OP PRO</span></div>
    <div class="nav-item active" data-tab="home">🏠 <span data-i18n="nav_dash">Дашборд</span></div>
    <div class="nav-item" data-tab="smart">✨ <span data-i18n="nav_smart">Умная оптимизация</span></div>
    <div class="nav-item" data-tab="autostart">🔌 <span data-i18n="nav_autostart">Автозагрузка</span></div>
    <div class="nav-item" data-tab="games">🎮 <span data-i18n="nav_games">Игры</span></div>
    <div class="nav-item" data-tab="overclock">🔥 <span data-i18n="nav_overclock">Разгон</span></div>
    <div class="nav-item" data-tab="drive_scanner">💾 <span data-i18n="nav_drive">Сканер диска (MFT)</span></div>
    <div class="nav-item" data-tab="bulk_uninstall">📦 <span data-i18n="nav_bulk">Массовое удаление</span></div>
    <div class="nav-item" data-tab="security">🛡️ <span data-i18n="nav_sec">Безопасность</span></div>
    <div class="nav-item requires-system" data-tab="lite">⚡ <span data-i18n="nav_lite">Lite Windows</span></div>
    <div class="nav-item" data-tab="cleanup">🧹 <span data-i18n="nav_cleanup">Очистка</span></div>
    <div class="nav-item" data-tab="apps">📦 <span data-i18n="nav_apps">Софт</span></div>
    <div class="nav-item" data-tab="overlay">🎯 <span data-i18n="nav_overlay">Оверлей</span></div>
    <div class="nav-item requires-system" data-tab="services">⚙️ <span data-i18n="nav_services">Службы</span></div>
    <div class="nav-item" data-tab="files">📁 <span data-i18n="nav_files">Файлы</span></div>
    <div class="nav-item" data-tab="tweaks">🔧 <span data-i18n="nav_tweaks">Твики</span></div>
    <div class="nav-item requires-system" data-tab="advanced">⚠️ <span data-i18n="nav_advanced">Расширенное</span></div>
    <div class="nav-item requires-system" data-tab="power">⚡ <span data-i18n="nav_power">Питание</span></div>
    <div class="nav-item requires-system" data-tab="maint">🛠️ <span data-i18n="nav_maint">Обслуживание</span></div>
    <div class="nav-item requires-system" data-tab="network">🌐 <span data-i18n="nav_network">Сеть</span></div>
    <div class="nav-item requires-system" data-tab="diag">🩺 <span data-i18n="nav_diag">Диагностика</span></div>
    <div class="nav-item" data-tab="report">📊 <span data-i18n="nav_report">Отчёт</span></div>
    <div class="nav-item" data-tab="settings">⚙️ <span data-i18n="nav_settings">Настройки</span></div>
    <div class="nav-item" data-tab="log">📜 <span data-i18n="nav_log">Лог</span></div>
    <div class="nav-spacer"></div>
    <div class="nav-item" id="btn-donate">❤️ <span data-i18n="nav_donate">Поддержать</span></div>
  </div>

  <div class="main">
    <div class="content">
      <div class="tab active" id="tab-home">
        <h1 data-i18n="home_title">Панель управления</h1>
        <p class="subtitle" data-i18n="home_sub">Мониторинг в реальном времени</p>
        <div class="cards">
          <div class="card"><div class="card-label">CPU</div><div class="card-value" id="stat-cpu">—</div></div>
          <div class="card"><div class="card-label">GPU</div><div class="card-value" id="stat-gpu">—</div></div>
          <div class="card"><div class="card-label" data-i18n="stat_ram">ОЗУ свободно</div><div class="card-value good" id="stat-ram">—</div></div>
          <div class="card"><div class="card-label" data-i18n="stat_procs">Процессов</div><div class="card-value" id="stat-procs">—</div></div>
          <div class="card"><div class="card-label">↓ Download</div><div class="card-value" id="stat-down">0</div></div>
          <div class="card"><div class="card-label">↑ Upload</div><div class="card-value" id="stat-up">0</div></div>
          <div class="card"><div class="card-label">Ping</div><div class="card-value" id="stat-ping">—</div></div>
          <div class="card"><div class="card-label" data-i18n="stat_battery">Батарея</div><div class="card-value" id="stat-battery">—</div></div>
          <div class="card"><div class="card-label">DPC/ISR</div><div class="card-value" id="stat-dpc">—</div></div>
        </div>
        <div class="chart-wrap"><h2 data-i18n="chart_title">Загрузка (последние 60 сек)</h2><canvas id="loadChart"></canvas></div>
        <div class="section">
          <h2 data-i18n="self_usage_title">Использование ресурсов Optimizer Pro</h2>
          <div class="self-usage">
            <div class="self-card"><div class="lbl">CPU</div><div class="val" id="app-cpu">0.0%</div></div>
            <div class="self-card"><div class="lbl">RAM</div><div class="val" id="app-ram">0 MB</div></div>
            <div class="self-card"><div class="lbl">GPU</div><div class="val" id="app-gpu">0.0%</div></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="disk_health_title">Здоровье дисков (S.M.A.R.T.)</h2>
          <div id="smart-list"><div class="hint">Загрузка данных...</div></div>
        </div>
        <div class="section">
          <h2>GPU / VRAM / Fans</h2>
          <div class="self-usage">
            <div class="self-card"><div class="lbl">GPU Temp</div><div class="val" id="gpu-temp">—</div></div>
            <div class="self-card"><div class="lbl">GPU Fan</div><div class="val" id="gpu-fan">—</div></div>
            <div class="self-card"><div class="lbl">VRAM</div><div class="val" id="vram-info">—</div></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="info_title">Информация о системе</h2>
          <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px">
            <div><div class="card-label" data-i18n="info_cpu">Процессор</div><div id="info-cpu">—</div></div>
            <div><div class="card-label" data-i18n="info_gpu">Видеокарта</div><div id="info-gpu">—</div></div>
            <div><div class="card-label" data-i18n="info_ram">ОЗУ (всего)</div><div id="info-ram">—</div></div>
            <div><div class="card-label" data-i18n="info_temp">Температура CPU</div><div id="info-temp">—</div></div>
            <div><div class="card-label" data-i18n="info_throttle">Троттлинг</div><div id="info-throttle">—</div></div>
            <div><div class="card-label" data-i18n="info_boot">Boot Time</div><div id="info-boot">—</div></div>
          </div>
        </div>
        <div class="section">
          <h2>CPU-инструкции</h2>
          <div class="feat-grid" id="cpu-features-grid">
            <div class="feat-item" id="feat-sse41">SSE4.1 <span>—</span></div>
            <div class="feat-item" id="feat-sse42">SSE4.2 <span>—</span></div>
            <div class="feat-item" id="feat-popcnt">POPCNT <span>—</span></div>
            <div class="feat-item" id="feat-avx">AVX <span>—</span></div>
            <div class="feat-item" id="feat-fma3">FMA3 <span>—</span></div>
            <div class="feat-item" id="feat-avx2">AVX2 <span>—</span></div>
            <div class="feat-item" id="feat-bmi1">BMI1 <span>—</span></div>
            <div class="feat-item" id="feat-bmi2">BMI2 <span>—</span></div>
            <div class="feat-item" id="feat-avx512">AVX-512 <span>—</span></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="quick_title">Быстрые действия</h2>
          <div class="toolbar">
            <button id="btn-qa-temp">🧹 Temp</button>
            <button id="btn-qa-dns">🌐 Flush DNS</button>
            <button id="btn-qa-ram">💾 Defrag RAM</button>
            <button id="btn-qa-standby" class="success">🧠 Standby Purger</button>
            <button id="btn-qa-maintenance" class="success">✨ One-Click Maintenance</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-smart">
        <h1 data-i18n="smart_title">✨ Умная оптимизация</h1>
        <p class="subtitle" data-i18n="smart_sub">Автоматический анализ ПК и подбор безопасных улучшений</p>
        <div class="section">
          <div class="hint" data-i18n="smart_hint">Нажмите кнопку ниже, чтобы проанализировать систему. Программа подберёт безопасные оптимизации, покажет что будет сделано, и вы сможете отредактировать список перед применением.</div>
          <div class="toolbar">
            <button id="btn-smart-scan" class="success">🔍 <span data-i18n="smart_scan_btn">Анализировать ПК</span></button>
          </div>
          <div id="smart-results"></div>
          <div class="toolbar" style="margin-top:16px">
            <button id="btn-smart-apply" class="danger" style="display:none">⚡ <span data-i18n="smart_apply_btn">Применить выбранное</span></button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-autostart">
        <h1 data-i18n="nav_autostart">Автозагрузка</h1>
        <p class="subtitle" data-i18n="autostart_sub">Управление программами при старте Windows</p>
        <div class="section">
          <div class="toolbar">
            <button id="btn-autostart-refresh" class="secondary">🔄 <span data-i18n="btn_refresh">Обновить</span></button>
            <button id="btn-autostart-impact" class="secondary">📊 <span data-i18n="btn_boot_impact">Анализ влияния на старт</span></button>
          </div>
          <div id="autostart-list"></div>
          <div id="boot-impact-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-games">
        <h1 data-i18n="nav_games">Игры</h1>
        <p class="subtitle" data-i18n="games_sub">Game Booster, сжатие, сканер, пресеты</p>
        <div class="section requires-system">
          <h2>Game Booster</h2>
          <div class="toolbar">
            <button id="btn-game-boost-on" class="success">▶ <span data-i18n="btn_boost_on">Включить буст</span></button>
            <button id="btn-game-boost-off" class="secondary">⏹ <span data-i18n="btn_boost_off">Выключить буст</span></button>
          </div>
          <div class="check-item" style="margin-top:12px;border:none">
            <input type="checkbox" id="auto-boost-chk"><label for="auto-boost-chk" data-i18n="auto_boost_chk">Автоматический игровой режим</label>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="games_fso_title">Оптимизация FSO</h2>
          <button id="btn-disable-fso" class="danger" data-i18n="btn_disable_fso">Отключить Fullscreen Optimizations</button>
        </div>
        <div class="section">
          <h2 data-i18n="games_compress_title">Прозрачное сжатие игр (LZX)</h2>
          <div class="toolbar">
            <input type="text" id="compress-path" placeholder="C:\Games\Game" style="flex:1;min-width:200px">
            <button id="btn-compress-game" class="danger" data-i18n="btn_compress">🗜 Сжать папку</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="games_scan_title">Сканер установленных игр</h2>
          <button id="btn-scan-games" class="secondary">🔍 <span data-i18n="btn_scan_games">Сканировать</span></button>
          <div id="games-list" style="margin-top:15px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="games_presets_title">Игровые пресеты</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3>Competitive FPS</h3><p data-i18n="preset_comp_desc">Максимальный FPS, минимум задержек</p><button id="btn-preset-comp" class="success" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3>Balanced</h3><p data-i18n="preset_bal_desc">Компромисс между FPS и качеством</p><button id="btn-preset-bal" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3>Quality</h3><p data-i18n="preset_qual_desc">Максимальное качество картинки</p><button id="btn-preset-qual" class="secondary" data-i18n="btn_apply">Применить</button></div>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-overclock">
        <h1 data-i18n="nav_overclock">Разгон и Буст</h1>
        <p class="subtitle">GPU, CPU, RAM & System Tweaks</p>
        <div class="warn-box" data-i18n="oc_warn">⚠️ ВНИМАНИЕ! Разгон может привести к нестабильности системы.</div>
        <div class="section">
          <h2 data-i18n="oc_afterburner_title">GPU Overclock (скрытый запуск MSI)</h2>
          <div class="toolbar">
            <button id="btn-msi-afterburner" class="danger" data-i18n="btn_apply_msi">🎮 Применить профиль MSI</button>
          </div>
        </div>
        <div class="section">
          <h2>CPU & RAM Boost</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3>High Priority CPU</h3><p>Win32PrioritySeparation = 0x26</p><button id="btn-priority-sep" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3>Disable Paging</h3><p data-i18n="oc_paging_desc">Только для 32GB+ RAM</p><button id="btn-disable-paging" class="danger" data-i18n="btn_disable">Отключить</button></div>
            <div class="adv-card"><h3>Memory Compression Off</h3><p data-i18n="oc_memcomp_desc">Отключает сжатие памяти</p><button id="btn-disable-mem-comp" class="danger" data-i18n="btn_disable">Отключить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="oc_stress_title">Стресс-тест CPU (30 сек)</h2>
          <div class="hint" data-i18n="oc_stress_hint">Нагружает все ядра CPU для проверки стабильности и температур. Мониторьте температуру!</div>
          <div class="toolbar">
            <button id="btn-stress-test" class="danger" data-i18n="btn_stress">🔥 Запустить стресс-тест</button>
            <button id="btn-benchmark" class="success" data-i18n="btn_bench">📊 Бенчмарк</button>
          </div>
          <div id="stress-results"></div>
        </div>
      </div>

      <div class="tab" id="tab-drive_scanner">
        <h1 data-i18n="nav_drive">Сканер диска (MFT)</h1>
        <div class="section">
          <h2>Treemap дискового пространства</h2>
          <div class="toolbar">
            <button id="btn-scan-drive" class="success">🚀 Быстрый скан (MFT)</button>
          </div>
          <canvas id="treemapCanvas" width="800" height="300"></canvas>
        </div>
      </div>

      <div class="tab" id="tab-bulk_uninstall">
        <h1 data-i18n="nav_bulk">Массовое удаление</h1>
        <div class="section">
          <div class="toolbar">
            <button id="btn-load-programs-detailed" class="secondary">📋 Загрузить программы</button>
            <button id="btn-bulk-uninstall" class="danger" style="display:none">🗑 Удалить выбранные</button>
          </div>
          <div id="programs-detailed-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-security">
        <h1 data-i18n="nav_sec">Безопасность</h1>
        <div class="section">
          <div class="toolbar">
            <button id="btn-load-security" class="secondary">🔄 Обновить статус</button>
          </div>
          <div id="security-status-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-lite">
        <h1 data-i18n="lite_title">Lite Windows</h1>
        <p class="subtitle" data-i18n="lite_sub">Агрессивный деблоат</p>
        <div class="warn-box"><b>⚠️ <span data-i18n="lite_warn_title">ВНИМАНИЕ:</span></b> <span data-i18n="lite_warn_text">Удаляет UWP-приложения, отключает телеметрию и службы. Необратимо!</span></div>
        <div class="section">
          <button id="btn-lite-debloat" class="danger" style="width:100%;justify-content:center;padding:15px;font-size:16px" data-i18n="lite_btn_run">
            ⚡ СДЕЛАТЬ WINDOWS LITE
          </button>
        </div>
      </div>

      <div class="tab" id="tab-cleanup">
        <h1 data-i18n="nav_cleanup">Очистка</h1>
        <p class="subtitle" data-i18n="cleanup_sub">Выберите компоненты</p>
        <div class="section">
          <div class="check-item"><input type="checkbox" id="cl-temp" checked><label for="cl-temp">Temp</label></div>
          <div class="check-item"><input type="checkbox" id="cl-prefetch" checked><label for="cl-prefetch">Prefetch</label></div>
          <div class="check-item"><input type="checkbox" id="cl-recycle" checked><label for="cl-recycle" data-i18n="cl_recycle">Корзина</label></div>
          <div class="check-item"><input type="checkbox" id="cl-thumbs" checked><label for="cl-thumbs" data-i18n="cl_thumbs">Кэш миниатюр</label></div>
          <div class="check-item"><input type="checkbox" id="cl-shader" checked><label for="cl-shader">Shader Cache</label></div>
          <div class="check-item"><input type="checkbox" id="cl-dns" checked><label for="cl-dns">DNS Cache</label></div>
          <div class="check-item"><input type="checkbox" id="cl-do" checked><label for="cl-do" data-i18n="cl_do">Delivery Optimization</label></div>
          <div class="check-item"><input type="checkbox" id="cl-launchers" checked><label for="cl-launchers" data-i18n="cl_launchers">Кэш лаунчеров</label></div>
          <div class="check-item"><input type="checkbox" id="cl-browsers" checked><label for="cl-browsers" data-i18n="cl_browsers">Кэш браузеров</label></div>
          <div class="check-item"><input type="checkbox" id="cl-chats" checked><label for="cl-chats" data-i18n="cl_chats">Discord/Telegram</label></div>
          <div class="check-item"><input type="checkbox" id="cl-dumps" checked><label for="cl-dumps" data-i18n="cl_dumps">Дампы памяти</label></div>
          <div class="check-item"><input type="checkbox" id="cl-minecraft"><label for="cl-minecraft" data-i18n="cl_minecraft">Minecraft Cache</label></div>
          <div class="check-item"><input type="checkbox" id="cl-print"><label for="cl-print" data-i18n="cl_print">Очередь печати</label></div>
          <div class="check-item"><input type="checkbox" id="cl-icons"><label for="cl-icons" data-i18n="cl_icons">Кэш иконок</label></div>
        </div>
        <div class="progress-bar"><div class="progress-fill" id="cleanup-progress"></div></div>
        <div class="toolbar">
          <button id="btn-run-cleanup" data-i18n="btn_run_cleanup">▶ Очистить</button>
          <button id="btn-enhanced-cache" class="success" data-i18n="btn_enh_cache">🚀 Enhanced Cache</button>
          <button id="btn-enhanced-ram" class="success" data-i18n="btn_enh_ram">🧠 Enhanced RAM</button>
        </div>
      </div>

      <div class="tab" id="tab-apps">
        <h1 data-i18n="nav_apps">Менеджер программ</h1>
        <div class="section">
          <h2 data-i18n="apps_update_title">Обновление софта</h2>
          <button id="btn-winget-upgrade" class="success" data-i18n="btn_update_all">Обновить всё (Winget)</button>
        </div>
        <div class="section">
          <h2 data-i18n="apps_uninstaller_title">Деинсталлятор</h2>
          <div class="hint" data-i18n="apps_uninstaller_desc">Удаление программ с очисткой остатков в реестре и AppData.</div>
          <div class="toolbar">
            <button id="btn-load-installed-programs" class="secondary" data-i18n="btn_load_programs">📋 Список программ</button>
          </div>
          <div id="programs-list"></div>
        </div>
        <div class="section">
          <h2 data-i18n="apps_runtimes">Библиотеки Windows</h2>
          <div class="app-grid">
            <div class="app-card"><span class="title">VC++ 2015-2022</span><button class="install-app-btn" data-pkg="Microsoft.VCRedist.2015+.x64" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">.NET Runtime 8</span><button class="install-app-btn" data-pkg="Microsoft.DotNet.Runtime.8" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">Java (Temurin 21)</span><button class="install-app-btn" data-pkg="EclipseAdoptium.Temurin.21.JRE" data-i18n="btn_install">Установить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="apps_browsers">Браузеры и утилиты</h2>
          <div class="app-grid">
            <div class="app-card"><span class="title">Chrome</span><button class="install-app-btn" data-pkg="Google.Chrome" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">Firefox</span><button class="install-app-btn" data-pkg="Mozilla.Firefox" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">7-Zip</span><button class="install-app-btn" data-pkg="7zip.7zip" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">Steam</span><button class="install-app-btn" data-pkg="Valve.Steam" data-i18n="btn_install">Установить</button></div>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-overlay">
        <h1 data-i18n="nav_overlay">Оверлей</h1>
        <p class="subtitle" data-i18n="ov_sub">Появляется только в полноэкранных играх</p>
        <div class="section">
          <div class="check-item"><input type="checkbox" id="ov-cpu" checked><label for="ov-cpu" data-i18n="ov_cpu">CPU</label></div>
          <div class="check-item"><input type="checkbox" id="ov-gpu" checked><label for="ov-gpu" data-i18n="ov_gpu">GPU</label></div>
          <div class="check-item"><input type="checkbox" id="ov-ram" checked><label for="ov-ram" data-i18n="ov_ram">RAM</label></div>
          <div class="check-item"><input type="checkbox" id="ov-temp" checked><label for="ov-temp" data-i18n="ov_temp">Температура</label></div>
          <div class="check-item"><input type="checkbox" id="ov-vram" checked><label for="ov-vram" data-i18n="ov_vram">VRAM</label></div>
          <div class="check-item"><input type="checkbox" id="ov-fan"><label for="ov-fan" data-i18n="ov_fan">Вентилятор</label></div>
          <div class="check-item"><input type="checkbox" id="ov-ping" checked><label for="ov-ping" data-i18n="ov_ping">Ping</label></div>
          <div class="check-item"><input type="checkbox" id="ov-fps" checked><label for="ov-fps" data-i18n="ov_fps">FPS & 1% Low</label></div>
          <div class="check-item"><input type="checkbox" id="ov-dark" checked><label for="ov-dark" data-i18n="ov_dark">Тёмная тема оверлея</label></div>
        </div>
        <div class="section">
          <button id="btn-overlay-toggle" data-i18n="btn_ov_toggle">🎯 Вкл/Выкл оверлей</button>
        </div>
      </div>

      <div class="tab" id="tab-services">
        <h1 data-i18n="nav_services">Службы Windows</h1>
        <div class="toolbar"><button id="btn-refresh-services" class="secondary" data-i18n="btn_refresh">🔄 Обновить</button></div>
        <div id="services-list"></div>
      </div>

      <div class="tab" id="tab-files">
        <h1 data-i18n="nav_files">Файлы</h1>
        <div class="section">
          <h2 data-i18n="files_large">Поиск больших файлов</h2>
          <div class="toolbar">
            <input type="text" id="large-path" placeholder="Путь (пусто = профиль)" style="flex:1">
            <input type="number" id="large-mb" value="100" min="1" style="width:100px">
            <button id="btn-find-large" data-i18n="btn_find">Найти</button>
          </div>
          <div id="large-results"></div>
        </div>
        <div class="section">
          <h2 data-i18n="files_dups">Поиск дубликатов</h2>
          <div class="toolbar">
            <input type="text" id="dup-path" placeholder="Путь (пусто = Downloads)" style="flex:1">
            <button id="btn-find-dups" data-i18n="btn_scan">Сканировать</button>
          </div>
          <div id="dup-results"></div>
        </div>
        <div class="section">
          <h2 data-i18n="files_shred">Шредер файлов</h2>
          <div class="toolbar">
            <input type="text" id="shred-path" placeholder="Полный путь к файлу" style="flex:1">
            <button id="btn-shred-file" class="danger" data-i18n="btn_shred">Уничтожить</button>
          </div>
        </div>
        <div class="section requires-system">
          <h2 data-i18n="files_hosts">Редактор файла Hosts</h2>
          <textarea id="hosts-editor" placeholder="Загрузка..."></textarea>
          <div class="toolbar" style="margin-top:10px">
            <button id="btn-load-hosts" class="secondary" data-i18n="btn_load">🔄 Загрузить</button>
            <button id="btn-save-hosts" data-i18n="btn_save">💾 Сохранить</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-tweaks">
        <h1 data-i18n="nav_tweaks">Твики</h1>
        <div class="section">
          <h2 data-i18n="tw_gm">Game Mode</h2>
          <div class="toolbar">
            <button id="btn-game-mode-on" data-i18n="tw_gm_on">Включить</button>
            <button id="btn-game-mode-off" class="secondary" data-i18n="tw_gm_off">Выключить</button>
          </div>
        </div>
        <div class="section requires-system">
          <h2 data-i18n="tw_telemetry">Телеметрия и обновления</h2>
          <div class="toolbar">
            <button id="btn-disable-telemetry" class="danger" data-i18n="tw_disable_tel">Отключить телеметрию</button>
            <button id="btn-disable-updates" class="danger" data-i18n="tw_disable_upd">Отключить Windows Update</button>
            <button id="btn-enable-updates" class="secondary" data-i18n="tw_enable_upd">Включить обновления</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-advanced">
        <h1 data-i18n="nav_advanced">Расширенные настройки</h1>
        <div class="warn-box" data-i18n="adv_warn">⚠️ Осторожно! Некоторые функции могут повлиять на стабильность системы.</div>
        
        <div class="section">
          <h2>Пакет 1: 100% Безопасные оптимизации <span class="badge-safe">Safe</span></h2>
          <div class="adv-grid">
            <div class="adv-card">
              <h3>Кэш-таймер (5 мин)</h3>
              <p>Автоматическая очистка кэша ФС каждые 5 минут.</p>
              <button id="btn-cache-timer" class="success">Включить/Выключить</button>
            </div>
            <div class="adv-card">
              <h3>Анализатор DPC/ISR</h3>
              <p>Мониторинг задержек драйверов. Не вносит изменений.</p>
              <button id="btn-check-dpc" class="secondary">Проверить</button>
            </div>
            <div class="adv-card">
              <h3>Точки восстановления</h3>
              <p>Создание бэкапа системы перед изменениями.</p>
              <button id="btn-create-restore" class="success">💾 Создать</button>
            </div>
            <div class="adv-card">
              <h3>Сброс сети (Winsock/TCP)</h3>
              <p>Устраняет проблемы с интернетом. Требует перезагрузки.</p>
              <button id="btn-reset-network" class="secondary">🔄 Сбросить</button>
            </div>
            <div class="adv-card">
              <h3>Твикер RSS / MSI-X</h3>
              <p>Включает прерывания сетевых адаптеров для снижения пинга.</p>
              <button id="btn-rss-on" class="success">Включить</button>
            </div>
          </div>
        </div>

        <div class="section">
          <h2>Пакет 2: Использовать с осторожностью <span class="badge-danger">Caution</span></h2>
          <div class="adv-grid">
            <div class="adv-card">
              <h3>Отключить VBS / HVCI</h3>
              <p>Отключает виртуализацию безопасности. Дает прирост FPS, но снижает защиту.</p>
              <button id="btn-disable-vbs" class="danger">Отключить</button>
            </div>
            <div class="adv-card">
              <h3>Отключить ETW / WPP</h3>
              <p>Останавливает системное логирование событий.</p>
              <button id="btn-disable-etw" class="danger">Отключить</button>
            </div>
            <div class="adv-card">
              <h3>Агрессивный Debloat меню</h3>
              <p>Удаляет сторонние CLSID из контекстного меню.</p>
              <button id="btn-load-ctx-menu" class="danger">Сканировать</button>
            </div>
            <div class="adv-card">
              <h3>Удаление шпионского ПО</h3>
              <p>Полное удаление UWP-мусора и отключение Cortana.</p>
              <button id="btn-remove-spyware" class="danger">Очистить</button>
            </div>
            <div class="adv-card">
              <h3>NVIDIA P-States (NVAPI)</h3>
              <p>Принудительный перевод GPU в режим максимальной производительности.</p>
              <button id="btn-nvidia-max-perf" class="danger">P0 Mode</button>
            </div>
            <div class="adv-card">
              <h3>Сжатие пулов ядра</h3>
              <p>Очистка Non-Paged и Paged Pool.</p>
              <button id="btn-clear-kernel-pools" class="danger">Очистить</button>
            </div>
            <div class="adv-card">
              <h3>Интеграция DDU</h3>
              <p>Запуск Display Driver Uninstaller.</p>
              <button id="btn-run-ddu" class="danger">Запустить DDU</button>
            </div>
          </div>
        </div>

        <div class="section">
          <h2>Опасные зоны</h2>
          <div class="toolbar">
            <button id="btn-disable-defender" class="danger" data-i18n="btn_disable_defender">Отключить Defender</button>
            <button id="btn-deep-services" class="danger" data-i18n="btn_deep_services">Глубокое отключение служб</button>
            <button id="btn-clean-winsxs" class="danger" data-i18n="btn_clean">Очистить WinSxS</button>
          </div>
        </div>
        <div class="section">
          <h2>Восстановление системы</h2>
          <button id="btn-list-restore" class="secondary" data-i18n="btn_list_rp">📋 Список точек</button>
          <div id="restore-points-list" style="margin-top:15px"></div>
        </div>
      </div>

      <div class="tab" id="tab-power">
        <h1 data-i18n="nav_power">Питание</h1>
        <div class="section">
          <h2 data-i18n="power_scheme">Схема питания</h2>
          <div class="toolbar">
            <button id="btn-power-ultimate" data-i18n="power_ult">⚡ Максимальная</button>
            <button id="btn-power-high" data-i18n="power_high">Высокая</button>
            <button id="btn-power-balanced" class="secondary" data-i18n="power_bal">Сбалансированная</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="power_usb_pcie">USB / PCIe Power Management</h2>
          <div class="toolbar">
            <button id="btn-usb-no-suspend" class="success" data-i18n="btn_usb_no_suspend">USB без suspend</button>
            <button id="btn-usb-suspend-ok" class="secondary" data-i18n="btn_usb_suspend_ok">USB suspend OK</button>
            <button id="btn-pcie-max-perf" class="success" data-i18n="btn_pcie_max_perf">PCIe max perf</button>
            <button id="btn-pcie-power-save" class="secondary" data-i18n="btn_pcie_power_save">PCIe power save</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-maint">
        <h1 data-i18n="nav_maint">Обслуживание системы</h1>
        <div class="section">
          <h2>SFC / DISM</h2>
          <div class="hint" data-i18n="maint_sfc_hint">Проверка и восстановление системных файлов. Может занять 10-30 минут.</div>
          <div class="toolbar">
            <button id="btn-run-sfc" class="success" data-i18n="btn_sfc">🛠 SFC /scannow</button>
            <button id="btn-run-dism" class="success" data-i18n="btn_dism">🔧 DISM /RestoreHealth</button>
          </div>
          <div id="sfc-output"></div>
        </div>
        <div class="section">
          <h2>Windows Update</h2>
          <div class="toolbar">
            <button id="btn-pause-updates-7" class="secondary" data-i18n="btn_pause_7">⏸ Пауза 7 дней</button>
            <button id="btn-pause-updates-35" class="secondary" data-i18n="btn_pause_35">⏸ Пауза 35 дней</button>
            <button id="btn-resume-updates" class="success" data-i18n="btn_resume_upd">▶ Возобновить</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="maint_tasks_title">Планировщик заданий</h2>
          <div class="toolbar">
            <button id="btn-load-tasks" class="secondary" data-i18n="btn_load_tasks">📋 Загрузить задачи</button>
          </div>
          <div id="tasks-list"></div>
        </div>
        <div class="section">
          <h2 data-i18n="maint_pagefile_title">Файл подкачки</h2>
          <div id="pagefile-info"></div>
          <div class="toolbar">
            <button id="btn-pagefile-info" class="secondary" data-i18n="btn_pagefile_info">📊 Информация</button>
          </div>
          <div class="setting-row">
            <label data-i18n="maint_pagefile_initial">Initial (MB)</label><input type="number" id="pf-initial" value="4096">
            <label data-i18n="maint_pagefile_max">Max (MB)</label><input type="number" id="pf-max" value="8192">
            <button id="btn-set-pagefile" class="danger" data-i18n="btn_apply">Применить</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-network">
        <h1 data-i18n="nav_network">Сеть</h1>
        <div class="section">
          <h2 data-i18n="net_ping_title">Ping-монитор</h2>
          <div class="toolbar">
            <input type="text" id="ping-host" value="8.8.8.8" style="flex:1">
            <button id="btn-ping-host" class="success" data-i18n="btn_ping">📡 Ping</button>
          </div>
          <div id="ping-result"></div>
        </div>
        <div class="section">
          <h2 data-i18n="net_reset_title">Сброс сети</h2>
          <div class="hint" data-i18n="net_reset_hint">Сбрасывает Winsock, TCP/IP, DNS, firewall. Требуется перезагрузка.</div>
          <button id="btn-reset-network-2" class="danger" data-i18n="btn_reset_net">🔄 Сбросить сетевые настройки</button>
        </div>
        <div class="section">
          <h2>Твики сетевых адаптеров (RSS / MSI-X)</h2>
          <div class="hint">Включает Receive Side Scaling и MSI-X прерывания для всех сетевых карт. Снижает задержки сети.</div>
          <div class="toolbar">
            <button id="btn-rss-on-2" class="success">Включить RSS/MSI-X</button>
            <button id="btn-rss-off-2" class="secondary">Отключить RSS/MSI-X</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-diag">
        <h1 data-i18n="nav_diag">Диагностика</h1>
        <div class="section">
          <h2 data-i18n="diag_events_title">Анализ журнала событий</h2>
          <button id="btn-analyze-events" class="secondary" data-i18n="btn_analyze_events">📋 Анализ (7 дней)</button>
          <div id="event-log-output" style="margin-top:15px;max-height:400px;overflow:auto;font-family:Consolas;font-size:12px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="diag_battery_title">Батарея</h2>
          <button id="btn-battery-health" class="secondary" data-i18n="btn_batt_health">🔋 Проверить</button>
          <div id="battery-output" style="margin-top:15px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="diag_hags_title">HAGS (Hardware GPU Scheduling)</h2>
          <div class="toolbar">
            <button id="btn-hags-on" class="success" data-i18n="btn_enable">Включить HAGS</button>
            <button id="btn-hags-off" class="secondary" data-i18n="btn_disable">Выключить HAGS</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="diag_mouse_title">Акселерация мыши</h2>
          <div class="toolbar">
            <button id="btn-mouse-accel-off" class="success" data-i18n="btn_disable_for_games">Отключить (для игр)</button>
            <button id="btn-mouse-accel-on" class="secondary" data-i18n="btn_enable">Включить</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-report">
        <h1 data-i18n="nav_report">Отчёты</h1>
        <div class="section">
          <h2 data-i18n="report_html_title">HTML-отчёт о системе</h2>
          <div class="hint" data-i18n="report_html_desc">Генерирует полный HTML-отчёт с информацией о системе, дисках, температурах.</div>
          <button id="btn-generate-report" class="success" data-i18n="btn_generate_report">📊 Сгенерировать отчёт</button>
          <div id="report-result" style="margin-top:15px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="report_backup_title">Резервное копирование настроек</h2>
          <div class="toolbar">
            <button id="btn-backup-settings" class="success" data-i18n="btn_backup_settings">💾 Сохранить</button>
            <button id="btn-restore-settings" class="secondary" data-i18n="btn_restore_settings">🔄 Восстановить</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-settings">
        <h1 data-i18n="nav_settings">Настройки</h1>
        <div class="section">
          <h2 data-i18n="settings_lang_title">🌐 Язык / Language</h2>
          <div class="toolbar">
            <select id="lang-select" style="min-width:200px">
              <option value="ru">🇷🇺 Русский</option>
              <option value="en">🇬🇧 English</option>
            </select>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="settings_drive_title">💾 Активный диск</h2>
          <div class="drive-current" id="drive-current-label"></div>
          <div class="toolbar">
            <select id="drive-select" style="min-width:280px"></select>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="settings_theme_title">🎨 Тема</h2>
          <div class="toolbar">
            <select id="theme-select" style="min-width:260px">
              <option value="black">⬛ Black</option>
              <option value="white">⬜ White</option>
              <option value="midnight">🌑 Midnight</option>
              <option value="glass">🪟 Liquid Glass (Жидкое стекло)</option>
            </select>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="settings_fine_title">⚙️ Тонкая настройка очистки</h2>
          <div class="setting-group">
            <h3 data-i18n="settings_cache_clean">Очистка кэша</h3>
            <div class="setting-row"><label>Temp / Windows Temp</label><input type="checkbox" id="s-cleanTemp" checked></div>
            <div class="setting-row"><label>Prefetch</label><input type="checkbox" id="s-cleanPrefetch" checked></div>
            <div class="setting-row"><label data-i18n="cl_recycle">Корзина</label><input type="checkbox" id="s-cleanRecycle" checked></div>
            <div class="setting-row"><label data-i18n="cl_thumbs">Кэш миниатюр</label><input type="checkbox" id="s-cleanThumbs" checked></div>
            <div class="setting-row"><label data-i18n="cl_shader">Кэш шейдеров</label><input type="checkbox" id="s-cleanShader" checked></div>
            <div class="setting-row"><label>DNS-кэш</label><input type="checkbox" id="s-cleanDns" checked></div>
            <div class="setting-row"><label>Delivery Optimization</label><input type="checkbox" id="s-cleanDO" checked></div>
            <div class="setting-row"><label data-i18n="cl_launchers">Кэш лаунчеров</label><input type="checkbox" id="s-cleanLaunchers" checked></div>
            <div class="setting-row"><label data-i18n="cl_browsers">Кэш браузеров</label><input type="checkbox" id="s-cleanBrowsers" checked></div>
            <div class="setting-row"><label data-i18n="cl_chats">Кэш Discord/Telegram</label><input type="checkbox" id="s-cleanChats" checked></div>
            <div class="setting-row"><label data-i18n="cl_dumps">Дампы памяти</label><input type="checkbox" id="s-cleanDumps"></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_ram_clean">Очистка ОЗУ</h3>
            <div class="setting-row"><label data-i18n="settings_ram_standby">Очищать standby list</label><input type="checkbox" id="s-ramClearStandby" checked></div>
            <div class="setting-row"><label data-i18n="settings_ram_modified">Сбрасывать modified page list</label><input type="checkbox" id="s-ramFlushModified" checked></div>
            <div class="setting-row"><label data-i18n="settings_ram_ws">Trim working sets</label><input type="checkbox" id="s-ramClearWorkingSets" checked></div>
            <div class="setting-row"><label data-i18n="settings_ram_cache">Очищать system file cache</label><input type="checkbox" id="s-ramClearFileCache" checked></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_auto_ram">Автоочистка ОЗУ</h3>
            <div class="setting-row"><label data-i18n="settings_auto_ram_enable">Включить автоочистку</label><input type="checkbox" id="s-ramAutoCleanEnabled"></div>
            <div class="setting-row"><label data-i18n="settings_auto_ram_threshold">Порог использования (%)</label><input type="number" id="s-ramAutoCleanThresholdPct" value="90" min="50" max="99"></div>
            <div class="setting-row"><label data-i18n="settings_auto_ram_interval">Интервал (мин)</label><input type="number" id="s-ramAutoCleanIntervalMin" value="30" min="5" max="120"></div>
            <div class="setting-row"><label data-i18n="settings_auto_ram_idle">Очищать при простое</label><input type="checkbox" id="s-ramAutoCleanOnIdle" checked></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_ui">Интерфейс</h3>
            <div class="setting-row"><label data-i18n="ov_dark">Тёмная тема оверлея</label><input type="checkbox" id="s-overlayDarkTheme" checked></div>
            <div class="setting-row"><label data-i18n="settings_hotkeys">Горячие клавиши (Ctrl+Alt+O)</label><input type="checkbox" id="s-hotkeysEnabled" checked></div>
          </div>
          <div class="toolbar">
            <button id="btn-save-fine-settings" class="success" data-i18n="btn_save_settings">💾 Сохранить настройки</button>
          </div>
        </div>
        
        <div class="section">
          <h2 data-i18n="settings_deep">Глубокая настройка (50+ параметров)</h2>
          <div class="setting-group">
            <h3 data-i18n="settings_sec_privacy">Безопасность и Приватность</h3>
            <div class="setting-row"><label data-i18n="s50_telemetry_lbl">Отключить телеметрию</label><input type="checkbox" id="s50_telemetry"></div>
            <div class="setting-row"><label data-i18n="s50_cortana_lbl">Отключить Cortana</label><input type="checkbox" id="s50_cortana"></div>
            <div class="setting-row"><label data-i18n="s50_adid_lbl">Отключить рекламный ID</label><input type="checkbox" id="s50_adid"></div>
            <div class="setting-row"><label data-i18n="s50_actfeed_lbl">Отключить историю активности</label><input type="checkbox" id="s50_actfeed"></div>
            <div class="setting-row"><label data-i18n="s50_smartscreen_lbl">Отключить SmartScreen</label><input type="checkbox" id="s50_smartscreen"></div>
            <div class="setting-row"><label data-i18n="s50_uac_lbl">Отключить UAC</label><input type="checkbox" id="s50_uac"></div>
            <div class="setting-row"><label data-i18n="s50_defender_lbl">Отключить Windows Defender</label><input type="checkbox" id="s50_defender"></div>
            <div class="setting-row"><label data-i18n="s50_vbs_lbl">Отключить VBS/HVCI</label><input type="checkbox" id="s50_vbs"></div>
            <div class="setting-row"><label data-i18n="s50_etw_lbl">Отключить ETW/WPP</label><input type="checkbox" id="s50_etw"></div>
            <div class="setting-row"><label data-i18n="s50_winupd_lbl">Отключить обновления Windows</label><input type="checkbox" id="s50_winupd"></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_perf_games">Производительность и Игры</h3>
            <div class="setting-row"><label data-i18n="s50_gamemode_lbl">Game Mode</label><input type="checkbox" id="s50_gamemode"></div>
            <div class="setting-row"><label data-i18n="s50_gamedvr_lbl">Game DVR</label><input type="checkbox" id="s50_gamedvr"></div>
            <div class="setting-row"><label data-i18n="s50_prio_lbl">Win32 Priority Separation</label><input type="checkbox" id="s50_prio"></div>
            <div class="setting-row"><label data-i18n="s50_netthrottle_lbl">Network Throttling</label><input type="checkbox" id="s50_netthrottle"></div>
            <div class="setting-row"><label data-i18n="s50_nagle_lbl">Nagle Algorithm</label><input type="checkbox" id="s50_nagle"></div>
            <div class="setting-row"><label data-i18n="s50_dynamictick_lbl">Dynamictick</label><input type="checkbox" id="s50_dynamictick"></div>
            <div class="setting-row"><label data-i18n="s50_hags_lbl">HAGS (GPU Scheduling)</label><input type="checkbox" id="s50_hags"></div>
            <div class="setting-row"><label data-i18n="s50_mouseaccel_lbl">Mouse Acceleration</label><input type="checkbox" id="s50_mouseaccel"></div>
            <div class="setting-row"><label data-i18n="s50_power_lbl">Power Plan Ultimate</label><input type="checkbox" id="s50_power"></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_clean_disk">Очистка и Диск</h3>
            <div class="setting-row"><label data-i18n="s50_zero_lbl">0-byte Files</label><input type="checkbox" id="s50_zero"></div>
            <div class="setting-row"><label data-i18n="s50_minecraft_lbl">Minecraft Cache</label><input type="checkbox" id="s50_minecraft"></div>
            <div class="setting-row"><label data-i18n="s50_print_lbl">Print Queue</label><input type="checkbox" id="s50_print"></div>
            <div class="setting-row"><label data-i18n="s50_icons_lbl">Icon Cache</label><input type="checkbox" id="s50_icons"></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_ram_mem">ОЗУ и Память</h3>
            <div class="setting-row"><label data-i18n="s50_pools_lbl">Clear Kernel Pools</label><input type="checkbox" id="s50_pools"></div>
            <div class="setting-row"><label data-i18n="s50_paging_lbl">Disable Paging</label><input type="checkbox" id="s50_paging"></div>
            <div class="setting-row"><label data-i18n="s50_memcomp_lbl">Disable Memory Compression</label><input type="checkbox" id="s50_memcomp"></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_network">Сеть</h3>
            <div class="setting-row"><label data-i18n="s50_fastdns_lbl">Fast DNS</label><input type="checkbox" id="s50_fastdns"></div>
            <div class="setting-row"><label data-i18n="s50_rss_lbl">RSS / MSI-X</label><input type="checkbox" id="s50_rss"></div>
            <div class="setting-row"><label data-i18n="s50_resetnet_lbl">Reset Network</label><input type="checkbox" id="s50_resetnet"></div>
            <div class="setting-row"><label data-i18n="s50_ping_lbl">Ping Monitor</label><input type="checkbox" id="s50_ping"></div>
          </div>
          <div class="setting-group">
            <h3 data-i18n="settings_system">Система</h3>
            <div class="setting-row"><label data-i18n="s50_services_lbl">Disable Deep Services</label><input type="checkbox" id="s50_services"></div>
            <div class="setting-row"><label data-i18n="s50_ctxmenu_lbl">Classic Context Menu</label><input type="checkbox" id="s50_ctxmenu"></div>
            <div class="setting-row"><label data-i18n="s50_usb_lbl">USB Selective Suspend</label><input type="checkbox" id="s50_usb"></div>
            <div class="setting-row"><label data-i18n="s50_pcie_lbl">PCIe Link State</label><input type="checkbox" id="s50_pcie"></div>
            <div class="setting-row"><label data-i18n="s50_nvapi_lbl">NVAPI P-States</label><input type="checkbox" id="s50_nvapi"></div>
          </div>
          <div class="toolbar">
            <button id="btn-apply-s50" class="danger" data-i18n="btn_apply_s50">Применить 50+ настроек</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-log">
        <h1 data-i18n="nav_log">Лог</h1>
        <div class="log" id="log-container"></div>
      </div>

    </div>
    <div class="status-bar">
      <div class="status-dot" id="status-dot"></div>
      <span id="status-text" data-i18n="status_ready">Готово</span>
      <div id="global-progress"></div>
    </div>
  </div>
</div>

<div id="donate-modal" class="modal-overlay">
  <div class="modal-box">
    <button class="modal-close" id="btn-close-donate">✕</button>
    <h2 data-i18n="donate_title">Поддержать проект</h2>
    <p style="margin-top:12px;color:var(--muted)" data-i18n="donate_desc">Спасибо за поддержку!</p>
    <button class="secondary" style="margin-top:15px;width:100%;justify-content:center" id="btn-close-donate-2" data-i18n="btn_close">Закрыть</button>
  </div>
</div>

<div id="toasts"></div>

<script>
'use strict';
var msgId = 0, pending = {}, API = null, chart = null, bootTries = 0, currentLang = 'ru', smartItems = [];
var smartScanId = 0;
var store = { get: function(k, d) { try { return localStorage.getItem(k) || d; } catch(e) { return d; } }, set: function(k, v) { try { localStorage.setItem(k, v); } catch(e) {} } };

var i18n = {
  ru: {
    nav_dash: "Дашборд", nav_smart: "Умная оптимизация", nav_autostart: "Автозагрузка", nav_games: "Игры", nav_overclock: "Разгон",
    nav_lite: "Lite Windows", nav_cleanup: "Очистка", nav_apps: "Софт", nav_overlay: "Оверлей", nav_services: "Службы",
    nav_files: "Файлы", nav_tweaks: "Твики", nav_advanced: "Расширенное", nav_power: "Питание", nav_maint: "Обслуживание",
    nav_network: "Сеть", nav_diag: "Диагностика", nav_report: "Отчёт", nav_settings: "Настройки", nav_log: "Лог", nav_donate: "Поддержать",
    nav_drive: "Сканер диска", nav_bulk: "Массовое удаление", nav_sec: "Безопасность",
    smart_title: "✨ Умная оптимизация", smart_sub: "Автоматический анализ ПК и подбор безопасных улучшений",
    smart_hint: "Нажмите кнопку ниже, чтобы проанализировать систему. Программа подберёт безопасные оптимизации, покажет что будет сделано, и вы сможете отредактировать список перед применением.",
    smart_scan_btn: "Анализировать ПК", smart_apply_btn: "Применить выбранное",
    home_title: "Панель управления", home_sub: "Мониторинг в реальном времени",
    stat_ram: "ОЗУ свободно", stat_procs: "Процессов", stat_battery: "Батарея", chart_title: "Загрузка (последние 60 сек)",
    self_usage_title: "Использование ресурсов Optimizer Pro", disk_health_title: "Здоровье дисков (S.M.A.R.T.)",
    info_title: "Информация о системе", info_cpu: "Процессор", info_gpu: "Видеокарта", info_ram: "ОЗУ (всего)", info_temp: "Температура CPU",
    info_throttle: "Троттлинг", info_boot: "Boot Time", quick_title: "Быстрые действия",
    autostart_sub: "Управление программами при старте Windows", btn_refresh: "Обновить", btn_boot_impact: "Анализ влияния на старт",
    games_sub: "Game Booster, сжатие, сканер, пресеты", btn_boost_on: "Включить буст", btn_boost_off: "Выключить буст",
    auto_boost_chk: "Автоматический игровой режим", games_fso_title: "Оптимизация FSO", btn_disable_fso: "Отключить Fullscreen Optimizations",
    games_compress_title: "Прозрачное сжатие игр (LZX)", btn_compress: "🗜 Сжать папку", games_scan_title: "Сканер установленных игр",
    btn_scan_games: "Сканировать", games_presets_title: "Игровые пресеты", preset_comp_desc: "Максимальный FPS, минимум задержек",
    preset_bal_desc: "Компромисс между FPS и качеством", preset_qual_desc: "Максимальное качество картинки", btn_apply: "Применить",
    oc_warn: "⚠️ ВНИМАНИЕ! Разгон может привести к нестабильности системы.", oc_afterburner_title: "GPU Overclock (скрытый запуск MSI)",
    btn_apply_msi: "🎮 Применить профиль MSI", oc_paging_desc: "Только для 32GB+ RAM", oc_memcomp_desc: "Отключает сжатие памяти",
    oc_stress_title: "Стресс-тест CPU (30 сек)", oc_stress_hint: "Нагружает все ядра CPU для проверки стабильности и температур. Мониторьте температуру!",
    btn_stress: "🔥 Запустить стресс-тест", btn_bench: "📊 Бенчмарк", btn_disable: "Отключить",
    lite_title: "Lite Windows", lite_sub: "Агрессивный деблоат", lite_warn_title: "ВНИМАНИЕ:",
    lite_warn_text: "Удаляет UWP-приложения, отключает телеметрию и службы. Необратимо!", lite_btn_run: "⚡ СДЕЛАТЬ WINDOWS LITE",
    cleanup_sub: "Выберите компоненты", cl_recycle: "Корзина", cl_thumbs: "Кэш миниатюр", cl_shader: "Кэш шейдеров",
    cl_do: "Delivery Optimization", cl_launchers: "Кэш лаунчеров", cl_browsers: "Кэш браузеров", cl_chats: "Discord/Telegram",
    cl_dumps: "Дампы памяти", cl_minecraft: "Minecraft Cache", cl_print: "Очередь печати", cl_icons: "Кэш иконок",
    btn_run_cleanup: "▶ Очистить", btn_enh_cache: "🚀 Enhanced Cache", btn_enh_ram: "🧠 Enhanced RAM",
    apps_update_title: "Обновление софта", btn_update_all: "Обновить всё (Winget)", apps_uninstaller_title: "Деинсталлятор",
    apps_uninstaller_desc: "Удаление программ с очисткой остатков в реестре и AppData.", btn_load_programs: "📋 Список программ",
    apps_runtimes: "Библиотеки Windows", btn_install: "Установить", apps_browsers: "Браузеры и утилиты",
    ov_sub: "Появляется только в полноэкранных играх", ov_cpu: "CPU", ov_gpu: "GPU", ov_ram: "RAM", ov_temp: "Температура",
    ov_vram: "VRAM", ov_fan: "Вентилятор", ov_ping: "Ping", ov_fps: "FPS & 1% Low", ov_dark: "Тёмная тема оверлея", btn_ov_toggle: "🎯 Вкл/Выкл оверлей",
    files_large: "Поиск больших файлов", btn_find: "Найти", files_dups: "Поиск дубликатов", btn_scan: "Сканировать",
    files_shred: "Шредер файлов", btn_shred: "Уничтожить", files_hosts: "Редактор файла Hosts", btn_load: "Загрузить", btn_save: "Сохранить",
    tw_gm: "Game Mode", tw_gm_on: "Включить", tw_gm_off: "Выключить", tw_telemetry: "Телеметрия и обновления",
    tw_disable_tel: "Отключить телеметрию", tw_disable_upd: "Отключить Windows Update", tw_enable_upd: "Включить обновления",
    adv_warn: "⚠️ Осторожно!", adv_security: "Безопасность и Службы", btn_disable_defender: "Отключить Defender",
    btn_deep_services: "Глубокое отключение служб", adv_ctx: "Контекстное меню Windows 11", adv_ctx_classic: "Классическое меню",
    adv_ctx_modern: "Новое меню Win11", adv_reg: "Твики реестра", adv_deep: "Глубокая очистка", adv_do: "Сброс Dosvc",
    btn_clean: "Очистить", btn_optimize: "Оптимизировать", adv_act: "Активация Windows / Office", adv_act_btn: "🔑 Активировать",
    adv_restore_points: "Точки восстановления", btn_create_rp: "💾 Создать точку", btn_list_rp: "📋 Список",
    power_scheme: "Схема питания", power_ult: "⚡ Максимальная", power_high: "Высокая", power_bal: "Сбалансированная",
    power_usb_pcie: "USB / PCIe Power Management", btn_usb_no_suspend: "USB без suspend", btn_usb_suspend_ok: "USB suspend OK",
    btn_pcie_max_perf: "PCIe max perf", btn_pcie_power_save: "PCIe power save",
    maint_sfc_hint: "Проверка и восстановление системных файлов. Может занять 10-30 минут.", btn_sfc: "🛠 SFC /scannow",
    btn_dism: "🔧 DISM /RestoreHealth", btn_pause_7: "⏸ Пауза 7 дней", btn_pause_35: "⏸ Пауза 35 дней", btn_resume_upd: "▶ Возобновить",
    maint_tasks_title: "Планировщик заданий", btn_load_tasks: "📋 Загрузить задачи", maint_pagefile_title: "Файл подкачки",
    btn_pagefile_info: "📊 Информация", maint_pagefile_initial: "Initial (MB)", maint_pagefile_max: "Max (MB)",
    net_ping_title: "Ping-монитор", btn_ping: "📡 Ping", net_reset_title: "Сброс сети",
    net_reset_hint: "Сбрасывает Winsock, TCP/IP, DNS, firewall. Требуется перезагрузка.", btn_reset_net: "🔄 Сбросить сетевые настройки",
    diag_events_title: "Анализ журнала событий", btn_analyze_events: "📋 Анализ (7 дней)", diag_battery_title: "Батарея",
    btn_batt_health: "🔋 Проверить", diag_hags_title: "HAGS (Hardware GPU Scheduling)", btn_enable: "Включить HAGS",
    diag_mouse_title: "Акселерация мыши", btn_disable_for_games: "Отключить (для игр)",
    report_html_title: "HTML-отчёт о системе", report_html_desc: "Генерирует полный HTML-отчёт с информацией о системе, дисках, температурах.",
    btn_generate_report: "📊 Сгенерировать отчёт", report_backup_title: "Резервное копирование настроек", btn_backup_settings: "💾 Сохранить",
    btn_restore_settings: "🔄 Восстановить", settings_lang_title: "🌐 Язык / Language", settings_drive_title: "💾 Активный диск",
    settings_theme_title: "🎨 Тема", settings_fine_title: "⚙️ Тонкая настройка очистки", settings_cache_clean: "Очистка кэша",
    settings_ram_clean: "Очистка ОЗУ", settings_ram_standby: "Очищать standby list", settings_ram_modified: "Сбрасывать modified page list",
    settings_ram_ws: "Trim working sets", settings_ram_cache: "Очищать system file cache", settings_auto_ram: "Автоочистка ОЗУ",
    settings_auto_ram_enable: "Включить автоочистку", settings_auto_ram_threshold: "Порог использования (%)", settings_auto_ram_interval: "Интервал (мин)",
    settings_auto_ram_idle: "Очищать при простое", settings_ui: "Интерфейс",
    settings_hotkeys: "Горячие клавиши (Ctrl+Alt+O)", btn_save_settings: "💾 Сохранить настройки", status_ready: "Готово",
    donate_title: "Поддержать проект", donate_desc: "Спасибо за поддержку!", btn_close: "Закрыть",
    drive_system: "системный диск", drive_non_system: "не-системный",
    settings_deep: "Глубокая настройка (50+ параметров)", settings_sec_privacy: "Безопасность и Приватность",
    settings_perf_games: "Производительность и Игры", settings_clean_disk: "Очистка и Диск", settings_ram_mem: "ОЗУ и Память",
    settings_network: "Сеть", settings_system: "Система", btn_apply_s50: "Применить 50+ настроек",
    s50_telemetry_lbl: "Отключить телеметрию", s50_cortana_lbl: "Отключить Cortana", s50_adid_lbl: "Отключить рекламный ID",
    s50_actfeed_lbl: "Отключить историю активности", s50_smartscreen_lbl: "Отключить SmartScreen", s50_uac_lbl: "Отключить UAC",
    s50_defender_lbl: "Отключить Windows Defender", s50_vbs_lbl: "Отключить VBS/HVCI", s50_etw_lbl: "Отключить ETW/WPP",
    s50_winupd_lbl: "Отключить обновления Windows", s50_gamemode_lbl: "Game Mode", s50_gamedvr_lbl: "Game DVR",
    s50_prio_lbl: "Win32 Priority Separation", s50_netthrottle_lbl: "Network Throttling", s50_nagle_lbl: "Nagle Algorithm",
    s50_dynamictick_lbl: "Dynamictick", s50_hags_lbl: "HAGS (GPU Scheduling)", s50_mouseaccel_lbl: "Mouse Acceleration",
    s50_power_lbl: "Power Plan Ultimate", s50_zero_lbl: "0-byte Files", s50_minecraft_lbl: "Minecraft Cache",
    s50_print_lbl: "Print Queue", s50_icons_lbl: "Icon Cache", s50_pools_lbl: "Clear Kernel Pools", s50_paging_lbl: "Disable Paging",
    s50_memcomp_lbl: "Disable Memory Compression", s50_fastdns_lbl: "Fast DNS", s50_rss_lbl: "RSS / MSI-X",
    s50_resetnet_lbl: "Reset Network", s50_ping_lbl: "Ping Monitor", s50_services_lbl: "Disable Deep Services",
    s50_ctxmenu_lbl: "Classic Context Menu", s50_usb_lbl: "USB Selective Suspend", s50_pcie_lbl: "PCIe Link State",
    s50_nvapi_lbl: "NVAPI P-States"
  },
  en: {
    nav_dash: "Dashboard", nav_smart: "Smart Optimize", nav_autostart: "Autostart", nav_games: "Games", nav_overclock: "Overclock",
    nav_lite: "Lite Windows", nav_cleanup: "Cleanup", nav_apps: "Apps", nav_overlay: "Overlay", nav_services: "Services",
    nav_files: "Files", nav_tweaks: "Tweaks", nav_advanced: "Advanced", nav_power: "Power", nav_maint: "Maintenance",
    nav_network: "Network", nav_diag: "Diagnostics", nav_report: "Report", nav_settings: "Settings", nav_log: "Log", nav_donate: "Support",
    nav_drive: "Drive Scanner", nav_bulk: "Bulk Uninstall", nav_sec: "Security",
    smart_title: "✨ Smart Optimize", smart_sub: "Automatic PC analysis and safe improvements selection",
    smart_hint: "Click the button below to analyze the system. The program will pick only safe optimizations, show what will be done, and you can edit the list before applying.",
    smart_scan_btn: "Analyze PC", smart_apply_btn: "Apply Selected",
    home_title: "Control Panel", home_sub: "Real-time monitoring",
    stat_ram: "RAM Free", stat_procs: "Processes", stat_battery: "Battery", chart_title: "Load (last 60s)",
    self_usage_title: "Optimizer Pro Resource Usage", disk_health_title: "Disk Health (S.M.A.R.T.)",
    info_title: "System Information", info_cpu: "Processor", info_gpu: "Video Card", info_ram: "RAM (Total)", info_temp: "CPU Temp",
    info_throttle: "Throttling", info_boot: "Boot Time", quick_title: "Quick Actions",
    autostart_sub: "Manage programs at Windows startup", btn_refresh: "Refresh", btn_boot_impact: "Analyze boot impact",
    games_sub: "Game Booster, compression, scanner, presets", btn_boost_on: "Enable Boost", btn_boost_off: "Disable Boost",
    auto_boost_chk: "Automatic game mode", games_fso_title: "FSO Optimization", btn_disable_fso: "Disable Fullscreen Optimizations",
    games_compress_title: "Transparent Game Compression (LZX)", btn_compress: "🗜 Compress Folder", games_scan_title: "Installed Games Scanner",
    btn_scan_games: "Scan", games_presets_title: "Game Presets", preset_comp_desc: "Maximum FPS, minimum latency",
    preset_bal_desc: "Balance between FPS and quality", preset_qual_desc: "Maximum image quality", btn_apply: "Apply",
    oc_warn: "⚠️ WARNING! Overclocking can cause system instability.", oc_afterburner_title: "GPU Overclock (hidden MSI launch)",
    btn_apply_msi: "🎮 Apply MSI Profile", oc_paging_desc: "Only for 32GB+ RAM", oc_memcomp_desc: "Disables memory compression",
    oc_stress_title: "CPU Stress Test (30s)", oc_stress_hint: "Loads all CPU cores to check stability and temperatures. Monitor the temperature!",
    btn_stress: "🔥 Start Stress Test", btn_bench: "📊 Benchmark", btn_disable: "Disable",
    lite_title: "Lite Windows", lite_sub: "Aggressive debloat", lite_warn_title: "WARNING:",
    lite_warn_text: "Removes UWP apps, disables telemetry and services. Irreversible!", lite_btn_run: "⚡ MAKE WINDOWS LITE",
    cleanup_sub: "Select components", cl_recycle: "Recycle Bin", cl_thumbs: "Thumbnails", cl_shader: "Shader Cache",
    cl_do: "Delivery Optimization", cl_launchers: "Launchers Cache", cl_browsers: "Browsers Cache", cl_chats: "Discord/Telegram",
    cl_dumps: "Memory Dumps", cl_minecraft: "Minecraft Cache", cl_print: "Print Queue", cl_icons: "Icon Cache",
    btn_run_cleanup: "▶ Clean", btn_enh_cache: "🚀 Enhanced Cache", btn_enh_ram: "🧠 Enhanced RAM",
    apps_update_title: "Software Update", btn_update_all: "Update All (Winget)", apps_uninstaller_title: "Uninstaller",
    apps_uninstaller_desc: "Uninstall programs with leftover cleanup in registry and AppData.", btn_load_programs: "📋 Program List",
    apps_runtimes: "Windows Libraries", btn_install: "Install", apps_browsers: "Browsers & Utilities",
    ov_sub: "Appears only in fullscreen games", ov_cpu: "CPU", ov_gpu: "GPU", ov_ram: "RAM", ov_temp: "Temperature",
    ov_vram: "VRAM", ov_fan: "Fan", ov_ping: "Ping", ov_fps: "FPS & 1% Low", ov_dark: "Overlay Dark Theme", btn_ov_toggle: "🎯 Toggle Overlay",
    files_large: "Find Large Files", btn_find: "Find", files_dups: "Find Duplicates", btn_scan: "Scan",
    files_shred: "File Shredder", btn_shred: "Shred", files_hosts: "Hosts File Editor", btn_load: "Load", btn_save: "Save",
    tw_gm: "Game Mode", tw_gm_on: "Enable", tw_gm_off: "Disable", tw_telemetry: "Telemetry & Updates",
    tw_disable_tel: "Disable Telemetry", tw_disable_upd: "Disable Windows Update", tw_enable_upd: "Enable Updates",
    adv_warn: "⚠️ Caution!", adv_security: "Security & Services", btn_disable_defender: "Disable Defender",
    btn_deep_services: "Deep Service Disable", adv_ctx: "Windows 11 Context Menu", adv_ctx_classic: "Classic Menu",
    adv_ctx_modern: "Modern Win11 Menu", adv_reg: "Registry Tweaks", adv_deep: "Deep Clean", adv_do: "Reset Dosvc",
    btn_clean: "Clean", btn_optimize: "Optimize", adv_act: "Windows / Office Activation", adv_act_btn: "🔑 Activate",
    adv_restore_points: "Restore Points", btn_create_rp: "💾 Create Point", btn_list_rp: "📋 List",
    power_scheme: "Power Plan", power_ult: "⚡ Ultimate", power_high: "High", power_bal: "Balanced",
    power_usb_pcie: "USB / PCIe Power Management", btn_usb_no_suspend: "USB no suspend", btn_usb_suspend_ok: "USB suspend OK",
    btn_pcie_max_perf: "PCIe max perf", btn_pcie_power_save: "PCIe power save",
    maint_sfc_hint: "System file check and repair. May take 10-30 minutes.", btn_sfc: "🛠 SFC /scannow",
    btn_dism: "🔧 DISM /RestoreHealth", btn_pause_7: "⏸ Pause 7 days", btn_pause_35: "⏸ Pause 35 days", btn_resume_upd: "▶ Resume",
    maint_tasks_title: "Task Scheduler", btn_load_tasks: "📋 Load Tasks", maint_pagefile_title: "Pagefile",
    btn_pagefile_info: "📊 Info", maint_pagefile_initial: "Initial (MB)", maint_pagefile_max: "Max (MB)",
    net_ping_title: "Ping Monitor", btn_ping: "📡 Ping", net_reset_title: "Network Reset",
    net_reset_hint: "Resets Winsock, TCP/IP, DNS, firewall. Requires reboot.", btn_reset_net: "🔄 Reset Network Settings",
    diag_events_title: "Event Log Analysis", btn_analyze_events: "📋 Analyze (7 days)", diag_battery_title: "Battery",
    btn_batt_health: "🔋 Check", diag_hags_title: "HAGS (Hardware GPU Scheduling)", btn_enable: "Enable HAGS",
    diag_mouse_title: "Mouse Acceleration", btn_disable_for_games: "Disable (for games)",
    report_html_title: "HTML System Report", report_html_desc: "Generates a full HTML report with system, disk, and temperature info.",
    btn_generate_report: "📊 Generate Report", report_backup_title: "Settings Backup", btn_backup_settings: "💾 Backup",
    btn_restore_settings: "🔄 Restore", settings_lang_title: "🌐 Language", settings_drive_title: "💾 Active Drive",
    settings_theme_title: "🎨 Theme", settings_fine_title: "⚙️ Fine Tuning", settings_cache_clean: "Cache Cleaning",
    settings_ram_clean: "RAM Cleaning", settings_ram_standby: "Clear standby list", settings_ram_modified: "Flush modified page list",
    settings_ram_ws: "Trim working sets", settings_ram_cache: "Clear system file cache", settings_auto_ram: "Auto RAM Clean",
    settings_auto_ram_enable: "Enable auto clean", settings_auto_ram_threshold: "Threshold (%)", settings_auto_ram_interval: "Interval (min)",
    settings_auto_ram_idle: "Clean on idle", settings_ui: "Interface",
    settings_hotkeys: "Hotkeys (Ctrl+Alt+O)", btn_save_settings: "💾 Save Settings", status_ready: "Ready",
    donate_title: "Support Project", donate_desc: "Thank you for your support!", btn_close: "Close",
    drive_system: "system drive", drive_non_system: "non-system drive",
    settings_deep: "Deep Customization (50+ parameters)", settings_sec_privacy: "Security & Privacy",
    settings_perf_games: "Performance & Games", settings_clean_disk: "Cleanup & Disk", settings_ram_mem: "RAM & Memory",
    settings_network: "Network", settings_system: "System", btn_apply_s50: "Apply 50+ Settings",
    s50_telemetry_lbl: "Disable Telemetry", s50_cortana_lbl: "Disable Cortana", s50_adid_lbl: "Disable Advertising ID",
    s50_actfeed_lbl: "Disable Activity History", s50_smartscreen_lbl: "Disable SmartScreen", s50_uac_lbl: "Disable UAC",
    s50_defender_lbl: "Disable Windows Defender", s50_vbs_lbl: "Disable VBS/HVCI", s50_etw_lbl: "Disable ETW/WPP",
    s50_winupd_lbl: "Disable Windows Updates", s50_gamemode_lbl: "Game Mode", s50_gamedvr_lbl: "Game DVR",
    s50_prio_lbl: "Win32 Priority Separation", s50_netthrottle_lbl: "Network Throttling", s50_nagle_lbl: "Nagle Algorithm",
    s50_dynamictick_lbl: "Dynamictick", s50_hags_lbl: "HAGS (GPU Scheduling)", s50_mouseaccel_lbl: "Mouse Acceleration",
    s50_power_lbl: "Power Plan Ultimate", s50_zero_lbl: "0-byte Files", s50_minecraft_lbl: "Minecraft Cache",
    s50_print_lbl: "Print Queue", s50_icons_lbl: "Icon Cache", s50_pools_lbl: "Clear Kernel Pools", s50_paging_lbl: "Disable Paging",
    s50_memcomp_lbl: "Disable Memory Compression", s50_fastdns_lbl: "Fast DNS", s50_rss_lbl: "RSS / MSI-X",
    s50_resetnet_lbl: "Reset Network", s50_ping_lbl: "Ping Monitor", s50_services_lbl: "Disable Deep Services",
    s50_ctxmenu_lbl: "Classic Context Menu", s50_usb_lbl: "USB Selective Suspend", s50_pcie_lbl: "PCIe Link State",
    s50_nvapi_lbl: "NVAPI P-States"
  }
};

function initBridge() {
  try {
    if (!window.chrome || !window.chrome.webview) return false;
    if (API) return true;
    API = window.chrome.webview;
    API.addEventListener('message', function(e) {
      try {
        if (typeof e.data !== 'string') return;
        var p = e.data.split('\x1F');
        if (p[0] === 'L') { __log(p.slice(1).join('\x1F')); }
        else if (p[0] === 'P') { __progress(parseInt(p[1], 10), p.slice(2).join('\x1F')); }
        else if (p[0] === 'R') {
          var id = parseInt(p[1], 10);
          var q = pending[id];
          if (q) { delete pending[id]; try { q.resolve(JSON.parse(p.slice(2).join('\x1F'))); } catch (ex) { q.reject(ex); } }
        }
      } catch (err) { console.error(err); }
    });
    return true;
  } catch (ex) { return false; }
}

function postRaw(s) { try { if (API) API.postMessage(s); } catch (e) {} }

function call(method) {
  var args = Array.prototype.slice.call(arguments, 1);
  return new Promise(function(resolve, reject) {
    if (!API) { reject(new Error('API not ready')); return; }
    var id = ++msgId; pending[id] = { resolve: resolve, reject: reject };
    var flat = [];
    args.forEach(function(a) {
      if (Array.isArray(a)) a.forEach(function(x) { flat.push(String(x)); });
      else flat.push(String(a));
    });
    postRaw('C\x1F' + id + '\x1F' + method + '\x1F' + flat.length + '\x1F' + flat.join('\x1F'));
    setTimeout(function() { if (pending[id]) { delete pending[id]; reject(new Error('timeout ' + method)); } }, 3600000);
  });
}

function wc(a) { postRaw('W\x1F' + a); }
document.querySelectorAll('.tb-btn').forEach(function(b) {
  b.addEventListener('click', function(e) { e.stopPropagation(); wc(b.dataset.wc); });
});
var dragging = false;
document.getElementById('titlebar').addEventListener('mousedown', function(e) {
  if (e.button !== 0 || e.target.closest('.tb-btn')) return;
  dragging = true; postRaw('W\x1Fdrag');
});
var BORDER = 6;
function getCursor(dir) {
    if (dir === 'topleft' || dir === 'bottomright') return 'nwse-resize';
    if (dir === 'topright' || dir === 'bottomleft') return 'nesw-resize';
    if (dir === 'top' || dir === 'bottom') return 'ns-resize';
    if (dir === 'left' || dir === 'right') return 'ew-resize';
    return '';
}
function resizeDir(x, y, w, h) {
  var L = x < BORDER, R = x > w - BORDER, T = y < BORDER, B = y > h - BORDER;
  if (T && L) return 'topleft'; if (T && R) return 'topright';
  if (B && L) return 'bottomleft'; if (B && R) return 'bottomright';
  if (T) return 'top'; if (B) return 'bottom'; if (L) return 'left'; if (R) return 'right';
  return null;
}
document.addEventListener('mousemove', function(e) {
  if (dragging) return;
  var d = resizeDir(e.clientX, e.clientY, window.innerWidth, window.innerHeight);
  document.body.style.cursor = d ? getCursor(d) : '';
});
document.addEventListener('mousedown', function(e) {
  if (e.button !== 0) return;
  var d = resizeDir(e.clientX, e.clientY, window.innerWidth, window.innerHeight);
  if (!d) return; e.preventDefault(); e.stopPropagation();
  postRaw('W\x1Fresize\x1F' + d);
});
document.addEventListener('mouseup', function() { dragging = false; });

function initChart() {
  var cv = document.getElementById('loadChart');
  if (!cv) return;
  var dpr = window.devicePixelRatio || 1;
  cv.width = cv.clientWidth * dpr; cv.height = cv.clientHeight * dpr;
  var ctx = cv.getContext('2d');
  ctx.scale(dpr, dpr);
  var cpu = [], gpu = [], MAX = 60;
  chart = { push: function(c, g) {
    cpu.push(c); gpu.push(g);
    if (cpu.length > MAX) { cpu.shift(); gpu.shift(); }
    draw();
  }};
  function draw() {
    var w = cv.clientWidth, h = cv.clientHeight;
    ctx.clearRect(0, 0, w, h);
    ctx.strokeStyle = '#333'; ctx.lineWidth = 1;
    for (var i = 0; i <= 4; i++) {
      var y = (h / 4) * i;
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
    }
    function line(arr, color, fillColor) {
      if (arr.length < 2) return;
      ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
      for (var i = 0; i < arr.length; i++) {
        var x = (i / (MAX - 1)) * w;
        var y = h - (arr[i] / 100) * h;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath();
      ctx.fillStyle = fillColor; ctx.fill();
    }
    line(cpu, '#ff0055', 'rgba(255,0,85,0.12)');
    line(gpu, '#00f0ff', 'rgba(0,240,255,0.10)');
  }
}
window.addEventListener('resize', function() { setTimeout(initChart, 100); });

function drawTreemap(items) {
    var canvas = document.getElementById('treemapCanvas');
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    var w = canvas.width, h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    if (!items || !items.length) return;
    
    var total = items.reduce(function(sum, i) { return sum + i.size; }, 0);
    var x = 0;
    var colors = ['#ff0055', '#00f0ff', '#10b981', '#f59e0b', '#8b5cf6'];
    for (var i = 0; i < items.length; i++) {
        var ratio = items[i].size / total;
        var width = ratio * w;
        if (width < 2) width = 2;
        
        ctx.fillStyle = colors[i % colors.length];
        ctx.fillRect(x, 0, width - 2, h);
        
        if (width > 50) {
            ctx.fillStyle = '#000';
            ctx.font = '12px Consolas';
            var text = items[i].name;
            if (text.length > 10) text = text.substring(0, 8) + '...';
            ctx.fillText(text, x + 5, 20);
            
            var sizeText = (items[i].size / (1024*1024)).toFixed(1) + ' MB';
            ctx.fillText(sizeText, x + 5, 35);
        }
        x += width;
    }
}

function toast(m, t) {
  var c = document.getElementById('toasts');
  var e = document.createElement('div');
  e.className = 'toast ' + (t || '');
  e.textContent = m;
  c.appendChild(e);
  setTimeout(function() { e.remove(); }, 4000);
}
function __log(text) {
  var c = document.getElementById('log-container'); if (!c) return;
  var l = document.createElement('div'); l.textContent = text;
  c.appendChild(l); c.scrollTop = c.scrollHeight;
}
function __progress(percent, msg) {
  var p = document.getElementById('global-progress');
  var s = document.getElementById('status-text');
  if (!p || !s) return;
  if (percent > 0 && percent < 100) { p.style.width = percent + '%'; s.textContent = msg; }
  else { p.style.width = '0%'; s.textContent = i18n[currentLang].status_ready; }
}
function setBusy(btn, busy, text) {
  document.getElementById('status-dot').className = 'status-dot ' + (busy ? 'busy' : '');
  document.getElementById('status-text').textContent = text || (i18n[currentLang].status_ready);
  if (btn) { btn.classList.toggle('loading', busy); btn.disabled = !!busy; }
}

document.querySelectorAll('.nav-item').forEach(function(el) {
  el.addEventListener('click', function() {
    document.querySelectorAll('.nav-item').forEach(function(e) { e.classList.remove('active'); });
    el.classList.add('active');
    document.querySelectorAll('.tab').forEach(function(t) { t.classList.remove('active'); });
    var tab = document.getElementById('tab-' + el.dataset.tab);
    if (tab) tab.classList.add('active');
    if (el.dataset.tab === 'home') setTimeout(initChart, 50);
    if (el.dataset.tab === 'files') loadHosts(null);
    if (el.dataset.tab === 'services') refreshServices(null);
    if (el.dataset.tab === 'autostart') loadAutostart(null);
    if (el.dataset.tab === 'security') loadSecurityStatus(null);
  });
});

function pollStats() {
  call('get_status').then(function(s) {
    if (!s) return;
    var cpu = Math.round((s.cpu || 0) * 10) / 10;
    var gpu = Math.round((s.gpu || 0) * 10) / 10;
    var e;
    if ((e = document.getElementById('stat-cpu'))) e.textContent = cpu + '%';
    if ((e = document.getElementById('stat-gpu'))) e.textContent = (s.gpu < 0) ? 'N/A' : gpu + '%';
    if ((e = document.getElementById('stat-ram'))) e.textContent = (s.ram_free || 0) + ' MB';
    if ((e = document.getElementById('stat-procs'))) e.textContent = s.procs || 0;
    if ((e = document.getElementById('stat-down'))) e.textContent = (s.net_down || 0).toFixed(2) + ' Mbps';
    if ((e = document.getElementById('stat-up'))) e.textContent = (s.net_up || 0).toFixed(2) + ' Mbps';
    if ((e = document.getElementById('stat-ping'))) e.textContent = (s.ping >= 0) ? s.ping + ' ms' : 'N/A';
    if ((e = document.getElementById('stat-battery'))) e.textContent = (s.battery >= 0) ? s.battery + '%' : '—';
    if ((e = document.getElementById('stat-dpc'))) e.textContent = (s.dpc || 0).toFixed(1) + '%';
    if (chart) chart.push(cpu, Math.max(0, gpu));
    if (s.sys) {
      if ((e = document.getElementById('info-cpu'))) e.textContent = s.sys.cpu || '—';
      if ((e = document.getElementById('info-gpu'))) e.textContent = s.sys.gpu || '—';
      if ((e = document.getElementById('info-ram'))) e.textContent = s.sys.ram || '—';
    }
    if (s.temp && s.temp > 0 && (e = document.getElementById('info-temp'))) e.textContent = s.temp.toFixed(1) + ' °C';
    if ((e = document.getElementById('gpu-temp'))) e.textContent = (s.gpuTemp >= 0) ? s.gpuTemp.toFixed(1) + ' °C' : '—';
    if ((e = document.getElementById('gpu-fan'))) e.textContent = (s.gpuFan >= 0) ? s.gpuFan.toFixed(0) + '%' : '—';
    if ((e = document.getElementById('vram-info'))) e.textContent = (s.vramTotal > 0) ? s.vramUsed + '/' + s.vramTotal + ' MB' : '—';
    if ((e = document.getElementById('info-throttle'))) e.textContent = s.throttle ? '⚠️ ТРОТТЛИНГ' : '✅ OK';
    if ((e = document.getElementById('app-cpu'))) e.textContent = (s.appCpu || 0).toFixed(1) + '%';
    if ((e = document.getElementById('app-ram'))) e.textContent = (s.appRam || 0) + ' MB';
    if ((e = document.getElementById('app-gpu'))) e.textContent = (s.appGpu || 0).toFixed(1) + '%';
    if (s.disks && (e = document.getElementById('smart-list'))) {
      e.innerHTML = '';
      if (!s.disks.length) {
          var hint = document.createElement('div');
          hint.className = 'hint';
          hint.textContent = 'Нет данных SMART';
          e.appendChild(hint);
      } else {
        s.disks.forEach(function(d) {
          var healthStr = d.status === 0 ? '✅ Healthy' : (d.status === 1 ? '⚠️ Warning' : '❌ Unhealthy');
          var card = document.createElement('div');
          card.className = 'smart-card';
          var title = document.createElement('div');
          title.className = 'title';
          title.textContent = d.model + ' (' + d.media + ')';
          var status = document.createElement('div');
          status.style.color = d.status === 0 ? 'var(--success)' : 'var(--accent)';
          status.textContent = healthStr;
          card.appendChild(title);
          card.appendChild(status);
          e.appendChild(card);
        });
      }
    }
  }).catch(function(){});
}

function loadCpuFeatures() {
  call('get_cpu_features').then(function(f) {
    if (!f) return;
    var keys = ['sse41','sse42','popcnt','avx','fma3','avx2','bmi1','bmi2','avx512'];
    keys.forEach(function(k) {
      var el = document.getElementById('feat-' + k); if (!el) return;
      var supported = f[k];
      el.className = 'feat-item ' + (supported ? 'supported' : 'unsupported');
      el.textContent = k.toUpperCase();
      var span = document.createElement('span');
      span.textContent = supported ? '✔' : '✘';
      el.appendChild(span);
    });
  });
}

function quickAction(btn, action) {
  if (['disable_defender','deep_services','adv_clean_winsxs','disable_vbs','disable_etw','remove_spyware'].includes(action)) {
      if (!confirm('Вы уверены, что хотите выполнить это действие? Оно может повлиять на систему.')) return;
  }
  setBusy(btn, true, 'Выполнение...');
  call('quick_action', action).then(function(r) {
    if (!r || r.ok === false) toast('Ошибка: ' + (r && r.error ? r.error : 'unknown'), 'error');
    else toast('Успешно', 'success');
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}

function formatSize(bytes) {
  if (!bytes || bytes < 0) return '0 B';
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
  if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + ' MB';
  return (bytes / 1073741824).toFixed(2) + ' GB';
}

function loadSmartScan(btn) {
  if (btn) setBusy(btn, true, 'Анализ ПК...');
  var ap = document.getElementById('btn-smart-apply');
  if (ap) ap.style.display = 'none';
  var currentScan = ++smartScanId;
  call('smart_scan').then(function(r) {
    if (currentScan !== smartScanId) return;
    var c = document.getElementById('smart-results');
    c.innerHTML = '';
    if (!r || !r.items) { c.textContent = 'Ошибка анализа'; return; }
    smartItems = r.items;
    if (!smartItems.length) { c.innerHTML = '<div class="hint">✅ ПК уже оптимизирован.</div>'; return; }
    var totalBytes = 0;
    smartItems.forEach(function(it, i) {
      totalBytes += (it.size || 0);
      var safetyColor = it.safety === 'safe' ? 'var(--success)' : 'var(--warn)';
      var safetyText = it.safety === 'safe' ? '✓ Безопасно' : '⚠ Осторожно';
      var sizeStr = (it.size && it.size > 0) ? formatSize(it.size) : '';
      
      var itemDiv = document.createElement('div');
      itemDiv.className = 'smart-item';
      
      var cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.dataset.idx = i;
      cb.checked = it.defaultOn;
      
      var infoDiv = document.createElement('div');
      infoDiv.className = 'smart-info';
      
      var titleDiv = document.createElement('div');
      titleDiv.className = 'smart-title';
      titleDiv.textContent = it.title;
      
      var badge = document.createElement('span');
      badge.className = 'smart-badge';
      badge.style.color = safetyColor;
      badge.style.borderColor = safetyColor;
      badge.textContent = safetyText;
      titleDiv.appendChild(badge);
      
      if (sizeStr) {
          var sizeSpan = document.createElement('span');
          sizeSpan.className = 'smart-size';
          sizeSpan.textContent = sizeStr;
          titleDiv.appendChild(sizeSpan);
      }
      
      var descDiv = document.createElement('div');
      descDiv.className = 'smart-desc';
      descDiv.textContent = it.desc;
      
      infoDiv.appendChild(titleDiv);
      infoDiv.appendChild(descDiv);
      
      var catDiv = document.createElement('div');
      catDiv.className = 'smart-category';
      catDiv.textContent = it.category;
      
      itemDiv.appendChild(cb);
      itemDiv.appendChild(infoDiv);
      itemDiv.appendChild(catDiv);
      c.appendChild(itemDiv);
    });
    
    var totalDiv = document.createElement('div');
    totalDiv.className = 'smart-total';
    totalDiv.textContent = 'Итого можно освободить: ';
    var b = document.createElement('b');
    b.textContent = formatSize(totalBytes);
    totalDiv.appendChild(b);
    c.appendChild(totalDiv);
    
    if (ap) ap.style.display = 'inline-flex';
  }).catch(function() { toast('Ошибка анализа', 'error'); })
    .finally(function() { if (btn) setBusy(btn, false); });
}

function applySmart(btn) {
  var checks = document.querySelectorAll('.smart-item input[type=checkbox]');
  var ids = [];
  checks.forEach(function(c) {
    if (c.checked) {
      var idx = parseInt(c.getAttribute('data-idx'), 10);
      if (!isNaN(idx) && smartItems[idx]) ids.push(smartItems[idx].id);
    }
  });
  if (!ids.length) { toast('Ничего не выбрано', 'error'); return; }
  if (!confirm('Применить ' + ids.length + ' оптимизаций?')) return;
  setBusy(btn, true, 'Применение...');
  call('smart_apply', ids).then(function(r) {
    if (r && r.ok) {
      var msg = 'Применено: ' + (r.applied || 0) + ' оптимизаций';
      if (r.freed > 0) msg += '. Освобождено: ' + formatSize(r.freed);
      toast(msg, 'success');
      loadSmartScan(null);
    } else toast('Ошибка', 'error');
  }).catch(function() { toast('Ошибка', 'error'); })
    .finally(function() { setBusy(btn, false); });
}

function loadAutostart(btn) {
  if (btn) setBusy(btn, true, 'Загрузка...');
  call('get_autostart').then(function(r) {
    var c = document.getElementById('autostart-list');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет элементов</div>'; return; }
    r.items.forEach(function(a, i) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = a.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = a.command;
      info.appendChild(title);
      info.appendChild(sub);
      
      var btnToggle = document.createElement('button');
      btnToggle.className = 'secondary';
      btnToggle.textContent = a.isEnabled ? 'Отключить' : 'Включить';
      btnToggle.addEventListener('click', function() {
        toggleAutostart(btnToggle, a.name, a.location, a.isEnabled ? 'disable' : 'enable');
      });
      
      row.appendChild(info);
      row.appendChild(btnToggle);
      c.appendChild(row);
    });
  }).finally(function() { if (btn) setBusy(btn, false); });
}
function toggleAutostart(btn, name, loc, act) {
  setBusy(btn, true, '...');
  call('toggle_autostart', name, loc, act).then(function(r) {
    if (r.ok) { toast('Готово', 'success'); loadAutostart(null); }
    else toast('Ошибка', 'error');
  }).finally(function() { setBusy(btn, false); });
}

function analyzeBootImpact(btn) {
  setBusy(btn, true, 'Анализ...');
  call('boot_impact').then(function(r) {
    var c = document.getElementById('boot-impact-list');
    c.innerHTML = '';
    if (!r || !r.items) { c.innerHTML = '<div class="hint">Нет данных</div>'; return; }
    var sec = document.createElement('div');
    sec.className = 'section';
    sec.innerHTML = '<h2>Влияние автозагрузки на старт</h2>';
    r.items.forEach(function(x) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = x.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = x.command;
      info.appendChild(title);
      info.appendChild(sub);
      
      var impact = document.createElement('span');
      impact.style.color = 'var(--accent)';
      impact.textContent = x.impact + ' ms';
      
      row.appendChild(info);
      row.appendChild(impact);
      sec.appendChild(row);
    });
    c.appendChild(sec);
  }).finally(function() { setBusy(btn, false); });
}

function runLiteDebloat(btn) {
  if (!confirm('Это удалит встроенные приложения и отключит службы. Необратимо. Продолжить?')) return;
  setBusy(btn, true, 'Debloat...');
  call('lite_debloat').then(function(r) {
    toast(r.ok ? 'Готово!' : 'Ошибка', r.ok ? 'success' : 'error');
  }).catch(function(){ toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}

function runCleanup(btn) {
  setBusy(btn, true, 'Выполнение...');
  var p = document.getElementById('cleanup-progress');
  var prog = 0;
  var iv = setInterval(function() { prog += 8; if (prog > 90) prog = 90; if (p) p.style.width = prog + '%'; }, 100);
  var opts = [];
  ['temp', 'prefetch', 'recycle', 'thumbs', 'shader', 'dns', 'do', 'launchers', 'browsers', 'chats', 'dumps', 'minecraft', 'print', 'icons'].forEach(function(k) {
    var e = document.getElementById('cl-' + k); if (e && e.checked) opts.push(k);
  });
  call('run_cleanup', opts).then(function(r) {
    if (p) p.style.width = '100%';
    clearInterval(iv);
    if (r && r.ok) toast('Готово', 'success');
    else toast('Ошибка', 'error');
  }).catch(function(){ clearInterval(iv); }).finally(function() { setBusy(btn, false); });
}

function runEnhancedCache(btn) {
  setBusy(btn, true, 'Enhanced cache...');
  call('enhanced_cache').then(function(r) {
    var msg = 'Enhanced cache завершён';
    if (r && r.freed > 0) msg += '. Освобождено ~' + formatSize(r.freed);
    toast(msg, 'success');
  }).catch(function() { toast('Timeout', 'error'); })
    .finally(function() { setBusy(btn, false); });
}

function runEnhancedRam(btn) {
  setBusy(btn, true, 'Enhanced RAM...');
  call('enhanced_ram').then(function(r) {
    var msg = 'RAM очищена';
    if (r && r.freed > 0) msg += '. Освобождено ' + formatSize(r.freed);
    toast(msg, 'success');
  }).catch(function() { toast('Timeout', 'error'); })
    .finally(function() { setBusy(btn, false); });
}

function installApp(btn, packageId) {
  setBusy(btn, true, 'Установка...');
  call('install_app', packageId).then(function(r) {
    toast(r.ok ? 'Установлено!' : 'Ошибка установки', r.ok ? 'success' : 'error');
  }).catch(function(){ toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}

function compressGame(btn) {
  var path = document.getElementById('compress-path').value.trim();
  if (!path) { toast('Введите путь', 'error'); return; }
  if (!confirm('Сжать папку?\n' + path)) return;
  setBusy(btn, true, 'Сжатие...');
  call('compress_game', path).then(function(r) {
    toast(r.ok ? 'Сжатие завершено' : 'Ошибка', r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function scanGames(btn) {
  setBusy(btn, true, 'Сканирование...');
  call('scan_games').then(function(r) {
    var c = document.getElementById('games-list');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Игры не найдены</div>'; return; }
    r.items.forEach(function(g) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = g.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = g.launcher + ' • ' + g.path;
      info.appendChild(title);
      info.appendChild(sub);
      
      var btnSel = document.createElement('button');
      btnSel.className = 'secondary';
      btnSel.textContent = 'Выбрать';
      btnSel.addEventListener('click', function() {
        document.getElementById('compress-path').value = g.path;
      });
      
      row.appendChild(info);
      row.appendChild(btnSel);
      c.appendChild(row);
    });
  }).finally(function() { setBusy(btn, false); });
}

function applyGamePreset(btn, name) {
  setBusy(btn, true, 'Применение...');
  call('game_preset', name).then(function(r) {
    toast(r && r.ok ? 'Пресет применён' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function loadHosts(btn) {
  if (btn) setBusy(btn, true, 'Загрузка...');
  call('read_hosts').then(function(r) {
    if (r.ok) document.getElementById('hosts-editor').value = r.content || '';
  }).finally(function() { if (btn) setBusy(btn, false); });
}
function saveHosts(btn) {
  var content = document.getElementById('hosts-editor').value;
  if (!confirm('Сохранить hosts?')) return;
  setBusy(btn, true, 'Сохранение...');
  call('write_hosts', content).then(function(r) {
    toast(r.ok ? 'Сохранено' : 'Ошибка', r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function refreshServices(btn) {
  if (btn) setBusy(btn, true, 'Загрузка...');
  call('list_services').then(function(r) {
    var c = document.getElementById('services-list');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет служб</div>'; return; }
    r.items.slice(0, 200).forEach(function(s, i) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = s.display;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = s.name + ' • ' + s.startType;
      info.appendChild(title);
      info.appendChild(sub);
      
      var btnCtrl = document.createElement('button');
      btnCtrl.className = 'secondary';
      btnCtrl.textContent = s.state === 'running' ? 'Стоп' : 'Старт';
      btnCtrl.addEventListener('click', function() {
        call('control_service', s.name, s.state === 'running' ? 'stop' : 'start').then(function(res) {
            toast(res.ok ? 'OK' : 'Ошибка', res.ok ? 'success' : 'error');
            refreshServices(null);
        });
      });
      
      var btnDisable = document.createElement('button');
      btnDisable.className = 'danger';
      btnDisable.textContent = '✕';
      btnDisable.addEventListener('click', function() {
        call('control_service', s.name, 'disable').then(function(res) {
            toast(res.ok ? 'OK' : 'Ошибка', res.ok ? 'success' : 'error');
            refreshServices(null);
        });
      });
      
      row.appendChild(info);
      row.appendChild(btnCtrl);
      row.appendChild(btnDisable);
      c.appendChild(row);
    });
  }).catch(function(){}).finally(function() { if (btn) setBusy(btn, false); });
}

function loadInstalledPrograms(btn) {
  if (btn) setBusy(btn, true, 'Загрузка...');
  call('load_programs').then(function(r) {
    var c = document.getElementById('programs-list');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет программ</div>'; return; }
    r.items.forEach(function(p, i) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = p.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = p.uninstall;
      info.appendChild(title);
      info.appendChild(sub);
      
      var btnDel = document.createElement('button');
      btnDel.className = 'danger';
      btnDel.textContent = 'Удалить';
      btnDel.addEventListener('click', function() { uninstallProgram(btnDel, p.uninstall); });
      
      row.appendChild(info);
      row.appendChild(btnDel);
      c.appendChild(row);
    });
  }).finally(function() { if (btn) setBusy(btn, false); });
}

function uninstallProgram(btn, uninstCmd) {
  if (!confirm('Удалить программу?\n' + uninstCmd)) return;
  setBusy(btn, true, 'Удаление...');
  call('uninstall_program', uninstCmd).then(function(r) {
    toast(r.ok ? 'Удалено' : 'Ошибка', r.ok ? 'success' : 'error');
    loadInstalledPrograms(null);
  }).finally(function() { setBusy(btn, false); });
}

function loadProgramsDetailed(btn) {
    if (btn) setBusy(btn, true, 'Загрузка...');
    call('scan_programs_detailed').then(function(r) {
        var c = document.getElementById('programs-detailed-list');
        c.innerHTML = '';
        var bulkBtn = document.getElementById('btn-bulk-uninstall');
        if (!r.items || !r.items.length) {
            c.innerHTML = '<div class="hint">Нет программ</div>';
            if (bulkBtn) bulkBtn.style.display = 'none';
            return;
        }
        if (bulkBtn) bulkBtn.style.display = 'inline-flex';
        
        r.items.forEach(function(p, i) {
            var row = document.createElement('div');
            row.className = 'list-row';
            
            var cb = document.createElement('input');
            cb.type = 'checkbox';
            cb.dataset.idx = i;
            cb.dataset.name = p.name;
            cb.dataset.silent = p.quietUninstallStr || p.uninstallStr;
            
            var info = document.createElement('div');
            info.className = 'info';
            var title = document.createElement('div');
            title.className = 'title';
            title.textContent = p.name + ' (' + p.version + ')';
            var sub = document.createElement('div');
            sub.className = 'sub';
            sub.textContent = p.publisher + ' | ' + p.installLoc + ' | ' + (p.sizeKB / 1024).toFixed(1) + ' MB';
            info.appendChild(title);
            info.appendChild(sub);
            
            row.appendChild(cb);
            row.appendChild(info);
            c.appendChild(row);
        });
    }).finally(function() { if (btn) setBusy(btn, false); });
}

function uninstallSelected(btn) {
    var cbs = document.querySelectorAll('#programs-detailed-list input[type=checkbox]:checked');
    if (!cbs.length) { toast('Ничего не выбрано', 'error'); return; }
    if (!confirm('Удалить ' + cbs.length + ' программ?')) return;
    
    setBusy(btn, true, 'Удаление...');
    var progs = [];
    cbs.forEach(function(cb) {
        progs.push({
            name: cb.dataset.name,
            silentCmd: cb.dataset.silent
        });
    });
    
    call('bulk_uninstall', JSON.stringify(progs)).then(function(r) {
        if (r.ok) {
            toast('Удаление завершено', 'success');
            loadProgramsDetailed(null);
        } else {
            toast('Ошибка', 'error');
        }
    }).finally(function() { setBusy(btn, false); });
}

function scanDriveFast(btn) {
    setBusy(btn, true, 'Сканирование MFT...');
    call('scan_drive_fast', 'C').then(function(r) {
        if (r && r.items) {
            drawTreemap(r.items);
            toast('Сканирование завершено', 'success');
        } else {
            toast('Ошибка сканирования', 'error');
        }
    }).finally(function() { setBusy(btn, false); });
}

function loadSecurityStatus(btn) {
    if (btn) setBusy(btn, true, 'Проверка...');
    call('get_security_status').then(function(r) {
        var c = document.getElementById('security-status-list');
        c.innerHTML = '';
        if (!r) return;
        
        var items = [
            { name: 'Windows Defender', status: r.defender, okText: 'Включен', badText: 'Отключен' },
            { name: 'Брандмауэр', status: r.firewall, okText: 'Включен', badText: 'Отключен' },
            { name: 'UAC', status: r.uac, okText: 'Включен', badText: 'Отключен' },
            { name: 'Windows Update', status: r.wu, okText: 'Работает', badText: 'Отключен' }
        ];
        
        items.forEach(function(item) {
            var row = document.createElement('div');
            row.className = 'list-row';
            var info = document.createElement('div');
            info.className = 'info';
            var title = document.createElement('div');
            title.className = 'title';
            title.textContent = item.name;
            info.appendChild(title);
            
            var status = document.createElement('span');
            status.style.color = item.status ? 'var(--success)' : 'var(--accent)';
            status.textContent = item.status ? item.okText : item.badText;
            
            row.appendChild(info);
            row.appendChild(status);
            c.appendChild(row);
        });
    }).finally(function() { if (btn) setBusy(btn, false); });
}

function loadContextMenu(btn) {
  if (btn) setBusy(btn, true, 'Сканирование...');
  call('get_context_menu').then(function(r) {
    var c = document.getElementById('context-menu-list');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Не найдено</div>'; return; }
    r.items.forEach(function(a) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = a.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = a.clsid;
      info.appendChild(title);
      info.appendChild(sub);
      
      var btnDisable = document.createElement('button');
      btnDisable.className = 'secondary';
      btnDisable.textContent = 'Отключить';
      btnDisable.addEventListener('click', function() {
        toggleContextMenu(btnDisable, a.clsid, true);
      });
      
      row.appendChild(info);
      row.appendChild(btnDisable);
      c.appendChild(row);
    });
  }).finally(function() { if (btn) setBusy(btn, false); });
}
function toggleContextMenu(btn, clsid, disable) {
  setBusy(btn, true, '...');
  call('toggle_context_menu', clsid, disable ? '1' : '0').then(function(r) {
    if (r.ok) { toast('Применено', 'success'); btn.disabled = true; }
    else toast('Ошибка', 'error');
  }).finally(function() { setBusy(btn, false); });
}

function findLargeFiles(btn) {
  setBusy(btn, true, 'Поиск...');
  var path = document.getElementById('large-path').value.trim();
  var mb = parseInt(document.getElementById('large-mb').value) || 100;
  call('find_large_files', path, mb).then(function(r) {
    var c = document.getElementById('large-results');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Не найдено</div>'; return; }
    r.items.forEach(function(f) {
      var sizeMB = (f.size / 1048576).toFixed(1);
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = sizeMB + ' MB';
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = f.path;
      info.appendChild(title);
      info.appendChild(sub);
      
      row.appendChild(info);
      c.appendChild(row);
    });
  }).finally(function() { setBusy(btn, false); });
}

function findDuplicates(btn) {
  setBusy(btn, true, 'Сканирование...');
  var path = document.getElementById('dup-path').value.trim();
  call('find_duplicates', path).then(function(r) {
    var c = document.getElementById('dup-results');
    c.innerHTML = '';
    if (!r.groups || !r.groups.length) { c.innerHTML = '<div class="hint">Нет дубликатов</div>'; return; }
    r.groups.forEach(function(g) {
      var div = document.createElement('div');
      div.className = 'section';
      
      var h2 = document.createElement('h2');
      h2.textContent = 'Размер: ' + formatSize(g.size) + ' • ' + g.files.length + ' копий';
      div.appendChild(h2);
      
      g.files.forEach(function(f) {
        var sub = document.createElement('div');
        sub.className = 'sub';
        sub.style.padding = '6px 0';
        sub.style.color = 'var(--muted)';
        sub.textContent = f;
        div.appendChild(sub);
      });
      c.appendChild(div);
    });
  }).finally(function() { setBusy(btn, false); });
}

function shredFile(btn) {
  var p = document.getElementById('shred-path').value.trim();
  if (!p) { toast('Введите путь', 'error'); return; }
  if (!confirm('Уничтожить файл?\n' + p)) return;
  setBusy(btn, true, 'Уничтожение...');
  call('shred_file', p).then(function(r) { toast(r.ok ? 'Уничтожен' : 'Ошибка', r.ok ? 'success' : 'error'); }).finally(function() { setBusy(btn, false); });
}

function setTheme(t) {
  document.body.dataset.theme = t;
  store.set('optpro-theme', t);
  setTimeout(initChart, 100);
}

function setDrive(letter) {
  call('set_drive', letter).then(function(r) {
    if (!r || !r.ok) { toast('Ошибка смены диска', 'error'); return; }
    applyDriveUI(r);
    toast('Активный диск: ' + letter + ':', 'success');
  });
}

function applyDriveUI(d) {
  var isSystem = !!d.isSystem;
  document.body.dataset.isSystem = isSystem ? 'true' : 'false';
  var lbl = document.getElementById('drive-current-label');
  if (lbl) {
    var dict = i18n[currentLang] || i18n.ru;
    var sysText = isSystem ? dict.drive_system : dict.drive_non_system;
    lbl.textContent = d.letter + ': — ' + sysText;
  }
}

function loadDrives() {
  call('get_drives').then(function(r) {
    var sel = document.getElementById('drive-select');
    if (!sel || !r.drives) return;
    sel.innerHTML = '';
    r.drives.forEach(function(d) {
      var opt = document.createElement('option');
      opt.value = String.fromCharCode(d.letter);
      opt.textContent = String.fromCharCode(d.letter) + ': — ' + d.label + (d.isSystem ? ' (системный)' : '');
      if (String.fromCharCode(d.letter) === r.active) opt.selected = true;
      sel.appendChild(opt);
    });
    applyDriveUI({ letter: r.active, isSystem: r.active === r.system });
  });
}

function loadSettingsFromServer() {
  call('get_settings').then(function(s) {
    if (!s || !s.settings) return;
    var st = s.settings;
    var map = ['cleanTemp','cleanPrefetch','cleanRecycle','cleanThumbs','cleanShader','cleanDns','cleanDO','cleanLaunchers','cleanBrowsers','cleanChats','cleanDumps','ramClearStandby','ramFlushModified','ramClearWorkingSets','ramClearFileCache','autoBoost','ramAutoCleanEnabled','overlayDarkTheme','hotkeysEnabled'];
    map.forEach(function(k) {
      var el = document.getElementById('s-' + k);
      if (el) el.checked = !!st[k];
    });
    var ab = document.getElementById('auto-boost-chk'); if (ab) ab.checked = !!st.autoBoost;
    var tv = document.getElementById('s-ramAutoCleanThresholdPct'); if (tv) tv.value = st.ramAutoCleanThresholdPct || 90;
    var tv2 = document.getElementById('s-ramAutoCleanIntervalMin'); if (tv2) tv2.value = st.ramAutoCleanIntervalMin || 30;
    var tv3 = document.getElementById('s-ramAutoCleanOnIdle'); if (tv3) tv3.checked = !!st.ramAutoCleanOnIdle;
    
    var map50 = ['s50_telemetry','s50_cortana','s50_adid','s50_actfeed','s50_smartscreen','s50_uac','s50_defender','s50_vbs','s50_etw','s50_winupd','s50_gamemode','s50_gamedvr','s50_prio','s50_netthrottle','s50_nagle','s50_dynamictick','s50_hags','s50_mouseaccel','s50_power','s50_zero','s50_minecraft','s50_print','s50_icons','s50_pools','s50_paging','s50_memcomp','s50_fastdns','s50_rss','s50_resetnet','s50_ping','s50_services','s50_ctxmenu','s50_usb','s50_pcie','s50_nvapi'];
    map50.forEach(function(k) {
      var el = document.getElementById(k);
      if (el) el.checked = !!st[k];
    });
  });
}

function saveFineSettings(btn) {
  var s = {
    cleanTemp: getChk('s-cleanTemp'), cleanPrefetch: getChk('s-cleanPrefetch'),
    cleanRecycle: getChk('s-cleanRecycle'), cleanThumbs: getChk('s-cleanThumbs'),
    cleanShader: getChk('s-cleanShader'), cleanDns: getChk('s-cleanDns'),
    cleanDO: getChk('s-cleanDO'), cleanLaunchers: getChk('s-cleanLaunchers'),
    cleanBrowsers: getChk('s-cleanBrowsers'), cleanChats: getChk('s-cleanChats'),
    cleanDumps: getChk('s-cleanDumps'),
    ramClearStandby: getChk('s-ramClearStandby'), ramFlushModified: getChk('s-ramFlushModified'),
    ramClearWorkingSets: getChk('s-ramClearWorkingSets'), ramClearFileCache: getChk('s-ramClearFileCache'),
    ramAutoCleanEnabled: getChk('s-ramAutoCleanEnabled'),
    ramAutoCleanThresholdPct: parseInt((document.getElementById('s-ramAutoCleanThresholdPct') || {}).value) || 90,
    ramAutoCleanIntervalMin: parseInt((document.getElementById('s-ramAutoCleanIntervalMin') || {}).value) || 30,
    ramAutoCleanOnIdle: getChk('s-ramAutoCleanOnIdle'),
    overlayDarkTheme: getChk('s-overlayDarkTheme'),
    hotkeysEnabled: getChk('s-hotkeysEnabled'),
    autoBoost: getChk('auto-boost-chk'),
    
    s50_telemetry: getChk('s50_telemetry'), s50_cortana: getChk('s50_cortana'), s50_adid: getChk('s50_adid'), s50_actfeed: getChk('s50_actfeed'),
    s50_smartscreen: getChk('s50_smartscreen'), s50_uac: getChk('s50_uac'), s50_defender: getChk('s50_defender'), s50_vbs: getChk('s50_vbs'),
    s50_etw: getChk('s50_etw'), s50_winupd: getChk('s50_winupd'), s50_gamemode: getChk('s50_gamemode'), s50_gamedvr: getChk('s50_gamedvr'),
    s50_prio: getChk('s50_prio'), s50_netthrottle: getChk('s50_netthrottle'), s50_nagle: getChk('s50_nagle'), s50_dynamictick: getChk('s50_dynamictick'),
    s50_hags: getChk('s50_hags'), s50_mouseaccel: getChk('s50_mouseaccel'), s50_power: getChk('s50_power'), s50_zero: getChk('s50_zero'),
    s50_minecraft: getChk('s50_minecraft'), s50_print: getChk('s50_print'), s50_icons: getChk('s50_icons'), s50_pools: getChk('s50_pools'),
    s50_paging: getChk('s50_paging'), s50_memcomp: getChk('s50_memcomp'), s50_fastdns: getChk('s50_fastdns'), s50_rss: getChk('s50_rss'),
    s50_resetnet: getChk('s50_resetnet'), s50_ping: getChk('s50_ping'), s50_services: getChk('s50_services'),
    s50_ctxmenu: getChk('s50_ctxmenu'), s50_usb: getChk('s50_usb'), s50_pcie: getChk('s50_pcie'), s50_nvapi: getChk('s50_nvapi')
  };
  setBusy(btn, true, 'Сохранение...');
  call('set_settings', JSON.stringify(s)).then(function(r) {
    toast(r && r.ok ? 'Настройки сохранены' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function applyS50(btn) {
  if (!confirm('Применить 50+ настроек? Это может повлиять на систему.')) return;
  setBusy(btn, true, 'Применение 50+ настроек...');
  call('apply_s50').then(function(r) {
    toast(r && r.ok ? 'Применено!' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function getChk(id) { var e = document.getElementById(id); return e ? !!e.checked : false; }

document.getElementById('ov-cpu').addEventListener('change', function(e) { call('set_overlay_setting', 'cpu', e.target.checked ? '1' : '0'); });
document.getElementById('ov-gpu').addEventListener('change', function(e) { call('set_overlay_setting', 'gpu', e.target.checked ? '1' : '0'); });
document.getElementById('ov-ram').addEventListener('change', function(e) { call('set_overlay_setting', 'ram', e.target.checked ? '1' : '0'); });
document.getElementById('ov-temp').addEventListener('change', function(e) { call('set_overlay_setting', 'temp', e.target.checked ? '1' : '0'); });
document.getElementById('ov-vram').addEventListener('change', function(e) { call('set_overlay_setting', 'vram', e.target.checked ? '1' : '0'); });
document.getElementById('ov-fan').addEventListener('change', function(e) { call('set_overlay_setting', 'fan', e.target.checked ? '1' : '0'); });
document.getElementById('ov-ping').addEventListener('change', function(e) { call('set_overlay_setting', 'ping', e.target.checked ? '1' : '0'); });
document.getElementById('ov-fps').addEventListener('change', function(e) { call('set_overlay_setting', 'fps', e.target.checked ? '1' : '0'); });
document.getElementById('ov-dark').addEventListener('change', function(e) { call('set_overlay_setting', 'dark', e.target.checked ? '1' : '0'); });

function runStressTest(btn) {
  setBusy(btn, true, 'Стресс-тест 30с...');
  var c = document.getElementById('stress-results');
  c.innerHTML = '<div class="hint">Стресс-тест выполняется... Не запускайте другие тяжёлые задачи.</div>';
  call('stress_test').then(function(r) {
    if (r && r.ok) {
      c.innerHTML = '<div class="success">✅ Стресс-тест завершён. Средняя загрузка CPU: ' + r.avgCpu + '%, Max Temp: ' + r.maxTemp + '°C</div>';
    } else c.innerHTML = '<div class="error">Ошибка</div>';
  }).finally(function() { setBusy(btn, false); });
}

function runBenchmark(btn) {
  setBusy(btn, true, 'Бенчмарк...');
  var c = document.getElementById('stress-results');
  call('benchmark').then(function(r) {
    if (r && r.ok) {
      c.innerHTML = '<div class="success">📊 Бенчмарк: ' + r.score + ' ops/sec (single) за ' + r.duration + 'ms</div>';
    }
  }).finally(function() { setBusy(btn, false); });
}

function createRestorePoint(btn) {
  var name = prompt('Название точки восстановления:', 'Optimizer Pro');
  if (!name) return;
  setBusy(btn, true, 'Создание...');
  call('create_restore_point', name).then(function(r) {
    toast(r && r.ok ? 'Точка создана' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function listRestorePoints(btn) {
  setBusy(btn, true, 'Загрузка...');
  call('list_restore_points').then(function(r) {
    var c = document.getElementById('restore-points-list');
    if (!c) return;
    c.innerHTML = '';
    if (!r || !r.items) { c.innerHTML = '<div class="hint">Нет данных</div>'; return; }
    r.items.forEach(function(x) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = x.desc || 'Point';
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = 'Seq: ' + x.seq;
      info.appendChild(title);
      info.appendChild(sub);
      
      var btnRestore = document.createElement('button');
      btnRestore.className = 'secondary';
      btnRestore.textContent = 'Восстановить';
      btnRestore.addEventListener('click', function() { restoreToPoint(x.seq); });
      
      row.appendChild(info);
      row.appendChild(btnRestore);
      c.appendChild(row);
    });
  }).finally(function() { setBusy(btn, false); });
}

function restoreToPoint(seq) {
  if (!confirm('Восстановить систему? Перезагрузка обязательна.')) return;
  call('restore_to_point', seq).then(function(r) {
    toast(r && r.ok ? 'Восстановление начато' : 'Ошибка', r && r.ok ? 'success' : 'error');
  });
}

function runSFC(btn) {
  setBusy(btn, true, 'SFC 10-30 мин...');
  call('run_sfc').then(function(r) {
    var c = document.getElementById('sfc-output');
    c.innerHTML = '<pre style="font-size:12px;color:var(--muted);max-height:300px;overflow:auto">' + escapeHtml(r.output || '') + '</pre>';
  }).finally(function() { setBusy(btn, false); });
}

function runDISM(btn) {
  setBusy(btn, true, 'DISM...');
  call('run_dism').then(function(r) {
    var c = document.getElementById('sfc-output');
    c.innerHTML = '<pre style="font-size:12px;color:var(--muted);max-height:300px;overflow:auto">' + escapeHtml(r.output || '') + '</pre>';
  }).finally(function() { setBusy(btn, false); });
}

function pauseUpdates(btn, days) {
  setBusy(btn, true, '...');
  call('pause_updates', days).then(function(r) {
    toast(r && r.ok ? 'Обновления на паузе' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function resumeUpdates(btn) {
  setBusy(btn, true, '...');
  call('resume_updates').then(function(r) {
    toast(r && r.ok ? 'Обновления возобновлены' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function loadTasks(btn) {
  setBusy(btn, true, 'Загрузка задач...');
  call('list_tasks').then(function(r) {
    var c = document.getElementById('tasks-list');
    c.innerHTML = '';
    if (!r || !r.items) { c.innerHTML = '<div class="hint">Нет данных</div>'; return; }
    r.items.forEach(function(t) {
      var row = document.createElement('div');
      row.className = 'list-row';
      
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = t.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = t.path;
      info.appendChild(title);
      info.appendChild(sub);
      
      var status = document.createElement('span');
      status.style.color = 'var(--accent)';
      status.textContent = t.enabled ? 'Enabled' : 'Disabled';
      
      row.appendChild(info);
      row.appendChild(status);
      c.appendChild(row);
    });
  }).finally(function() { setBusy(btn, false); });
}

function loadPageFileInfo(btn) {
  setBusy(btn, true, '...');
  call('pagefile_info').then(function(r) {
    var c = document.getElementById('pagefile-info');
    if (!r) return;
    c.innerHTML = '<div class="hint">RAM: ' + r.totalRamMB + ' MB. Рекомендации: ' + r.recommendedMin + '-' + r.recommendedMax + ' MB</div>';
    document.getElementById('pf-initial').value = r.recommendedMin;
    document.getElementById('pf-max').value = r.recommendedMax;
  }).finally(function() { setBusy(btn, false); });
}

function setPageFile(btn) {
  var initial = parseInt(document.getElementById('pf-initial').value);
  var max = parseInt(document.getElementById('pf-max').value);
  if (!initial || !max) { toast('Заполните поля', 'error'); return; }
  if (!confirm('Установить размер pagefile ' + initial + '-' + max + ' MB? Перезагрузка обязательна.')) return;
  setBusy(btn, true, '...');
  call('set_pagefile', initial, max).then(function(r) {
    toast(r && r.ok ? 'Применено. Перезагрузите.' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function pingHost(btn) {
  var host = document.getElementById('ping-host').value.trim() || '8.8.8.8';
  setBusy(btn, true, 'Ping...');
  call('ping_host', host).then(function(r) {
    var c = document.getElementById('ping-result');
    c.innerHTML = '<div class="success">Ping: ' + (r && r.ms >= 0 ? r.ms + ' ms' : 'timeout') + '</div>';
  }).finally(function() { setBusy(btn, false); });
}

function resetNetwork(btn) {
  if (!confirm('Сбросить сеть? Требуется перезагрузка.')) return;
  setBusy(btn, true, 'Сброс...');
  call('reset_network').then(function(r) {
    toast(r && r.ok ? 'Готово. Перезагрузите.' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function analyzeEvents(btn) {
  setBusy(btn, true, '...');
  call('analyze_events').then(function(r) {
    var c = document.getElementById('event-log-output');
    c.textContent = (r && r.events) || '[]';
  }).finally(function() { setBusy(btn, false); });
}

function getBatteryHealth(btn) {
  setBusy(btn, true, '...');
  call('battery_health').then(function(r) {
    var c = document.getElementById('battery-output');
    c.textContent = (r && r.data) || '[]';
  }).finally(function() { setBusy(btn, false); });
}

function setHAGS(btn, enabled) {
  setBusy(btn, true, '...');
  call('set_hags', enabled ? '1' : '0').then(function(r) {
    toast(r && r.ok ? 'Применено. Перезагрузка.' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function setMouseAccel(btn, enabled) {
  setBusy(btn, true, '...');
  call('set_mouse_accel', enabled ? '1' : '0').then(function(r) {
    toast(r && r.ok ? 'Применено' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function generateReport(btn) {
  setBusy(btn, true, 'Генерация...');
  call('generate_report').then(function(r) {
    var c = document.getElementById('report-result');
    if (r && r.ok) c.innerHTML = '<div class="success">✅ Отчёт сохранён: ' + escapeHtml(r.path) + '</div>';
    else c.innerHTML = '<div class="error">Ошибка</div>';
  }).finally(function() { setBusy(btn, false); });
}

function backupSettings(btn) {
  setBusy(btn, true, '...');
  call('backup_settings').then(function(r) {
    toast(r && r.ok ? 'Настройки сохранены' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function restoreSettings(btn) {
  if (!confirm('Восстановить настройки из бэкапа?')) return;
  setBusy(btn, true, '...');
  call('restore_settings').then(function(r) {
    toast(r && r.ok ? 'Восстановлено' : 'Ошибка', r && r.ok ? 'success' : 'error');
    if (r && r.ok) loadSettingsFromServer();
  }).finally(function() { setBusy(btn, false); });
}

function openDonate() { document.getElementById('donate-modal').classList.add('active'); }
function closeDonate() { document.getElementById('donate-modal').classList.remove('active'); }
document.addEventListener('keydown', function(e) { if (e.key === 'Escape') closeDonate(); });

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, function(c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}

function setLang(l) {
  currentLang = l || 'ru';
  var dict = i18n[currentLang] || i18n.ru;
  document.querySelectorAll('[data-i18n]').forEach(function(el) {
    var k = el.getAttribute('data-i18n');
    if (dict[k]) el.innerHTML = dict[k];
  });
  store.set('optpro-lang', currentLang);
  var sel = document.getElementById('lang-select');
  if (sel) sel.value = currentLang;
  
  var activeDriveStr = document.getElementById('drive-select').value || 'C';
  call('get_drives').then(function(r) {
     if (r && r.drives) {
         var isSys = (activeDriveStr === r.system);
         applyDriveUI({ letter: activeDriveStr, isSystem: isSys });
     }
  });
}

document.getElementById('lang-select').addEventListener('change', function(e) { setLang(e.target.value); });
document.getElementById('drive-select').addEventListener('change', function(e) { setDrive(e.target.value); });
document.getElementById('theme-select').addEventListener('change', function(e) { setTheme(e.target.value); });

document.getElementById('btn-donate').addEventListener('click', openDonate);
document.getElementById('btn-close-donate').addEventListener('click', closeDonate);
document.getElementById('btn-close-donate-2').addEventListener('click', closeDonate);

document.getElementById('btn-qa-temp').addEventListener('click', function() { quickAction(this, 'cleanup_temp'); });
document.getElementById('btn-qa-dns').addEventListener('click', function() { quickAction(this, 'flush_dns'); });
document.getElementById('btn-qa-ram').addEventListener('click', function() { quickAction(this, 'defrag_memory'); });
document.getElementById('btn-qa-standby').addEventListener('click', function() { quickAction(this, 'clear_standby'); });
document.getElementById('btn-qa-maintenance').addEventListener('click', function() { quickAction(this, 'one_click_maintenance'); });

document.getElementById('btn-smart-scan').addEventListener('click', function() { loadSmartScan(this); });
document.getElementById('btn-smart-apply').addEventListener('click', function() { applySmart(this); });

document.getElementById('btn-autostart-refresh').addEventListener('click', function() { loadAutostart(this); });
document.getElementById('btn-autostart-impact').addEventListener('click', function() { analyzeBootImpact(this); });

document.getElementById('btn-game-boost-on').addEventListener('click', function() { quickAction(this, 'game_booster_on'); });
document.getElementById('btn-game-boost-off').addEventListener('click', function() { quickAction(this, 'game_booster_off'); });
document.getElementById('btn-disable-fso').addEventListener('click', function() { quickAction(this, 'disable_fso'); });
document.getElementById('btn-compress-game').addEventListener('click', function() { compressGame(this); });
document.getElementById('btn-scan-games').addEventListener('click', function() { scanGames(this); });
document.getElementById('btn-preset-comp').addEventListener('click', function() { applyGamePreset(this, 'competitive'); });
document.getElementById('btn-preset-bal').addEventListener('click', function() { applyGamePreset(this, 'balanced'); });
document.getElementById('btn-preset-qual').addEventListener('click', function() { applyGamePreset(this, 'quality'); });

document.getElementById('btn-msi-afterburner').addEventListener('click', function() { quickAction(this, 'launch_afterburner'); });
document.getElementById('btn-priority-sep').addEventListener('click', function() { quickAction(this, 'adv_priority_sep'); });
document.getElementById('btn-disable-paging').addEventListener('click', function() { quickAction(this, 'disable_paging'); });
document.getElementById('btn-disable-mem-comp').addEventListener('click', function() { quickAction(this, 'disable_mem_comp'); });
document.getElementById('btn-stress-test').addEventListener('click', function() { runStressTest(this); });
document.getElementById('btn-benchmark').addEventListener('click', function() { runBenchmark(this); });

document.getElementById('btn-scan-drive').addEventListener('click', function() { scanDriveFast(this); });

document.getElementById('btn-load-programs-detailed').addEventListener('click', function() { loadProgramsDetailed(this); });
document.getElementById('btn-bulk-uninstall').addEventListener('click', function() { uninstallSelected(this); });

document.getElementById('btn-load-security').addEventListener('click', function() { loadSecurityStatus(this); });

document.getElementById('btn-lite-debloat').addEventListener('click', function() { runLiteDebloat(this); });

document.getElementById('btn-run-cleanup').addEventListener('click', function() { runCleanup(this); });
document.getElementById('btn-enhanced-cache').addEventListener('click', function() { runEnhancedCache(this); });
document.getElementById('btn-enhanced-ram').addEventListener('click', function() { runEnhancedRam(this); });

document.getElementById('btn-winget-upgrade').addEventListener('click', function() { quickAction(this, 'winget_upgrade_all'); });
document.getElementById('btn-load-installed-programs').addEventListener('click', function() { loadInstalledPrograms(this); });
document.querySelectorAll('.install-app-btn').forEach(function(btn) {
    btn.addEventListener('click', function() { installApp(this, this.dataset.pkg); });
});

document.getElementById('btn-overlay-toggle').addEventListener('click', function() { quickAction(this, 'overlay_toggle'); });

document.getElementById('btn-refresh-services').addEventListener('click', function() { refreshServices(this); });

document.getElementById('btn-find-large').addEventListener('click', function() { findLargeFiles(this); });
document.getElementById('btn-find-dups').addEventListener('click', function() { findDuplicates(this); });
document.getElementById('btn-shred-file').addEventListener('click', function() { shredFile(this); });
document.getElementById('btn-load-hosts').addEventListener('click', function() { loadHosts(this); });
document.getElementById('btn-save-hosts').addEventListener('click', function() { saveHosts(this); });

document.getElementById('btn-game-mode-on').addEventListener('click', function() { quickAction(this, 'game_mode_on'); });
document.getElementById('btn-game-mode-off').addEventListener('click', function() { quickAction(this, 'game_mode_off'); });
document.getElementById('btn-disable-telemetry').addEventListener('click', function() { quickAction(this, 'disable_telemetry'); });
document.getElementById('btn-disable-updates').addEventListener('click', function() { quickAction(this, 'disable_updates'); });
document.getElementById('btn-enable-updates').addEventListener('click', function() { quickAction(this, 'enable_updates'); });

document.getElementById('btn-cache-timer').addEventListener('click', function() { quickAction(this, 'toggle_cache_timer'); });
document.getElementById('btn-check-dpc').addEventListener('click', function() { quickAction(this, 'check_dpc'); });
document.getElementById('btn-create-restore').addEventListener('click', function() { createRestorePoint(this); });
document.getElementById('btn-reset-network').addEventListener('click', function() { resetNetwork(this); });
document.getElementById('btn-rss-on').addEventListener('click', function() { quickAction(this, 'net_rss_on'); });
document.getElementById('btn-disable-vbs').addEventListener('click', function() { quickAction(this, 'disable_vbs'); });
document.getElementById('btn-disable-etw').addEventListener('click', function() { quickAction(this, 'disable_etw'); });
document.getElementById('btn-load-ctx-menu').addEventListener('click', function() { loadContextMenu(this); });
document.getElementById('btn-remove-spyware').addEventListener('click', function() { quickAction(this, 'remove_spyware'); });
document.getElementById('btn-nvidia-max-perf').addEventListener('click', function() { quickAction(this, 'nvidia_max_perf'); });
document.getElementById('btn-clear-kernel-pools').addEventListener('click', function() { quickAction(this, 'clear_kernel_pools'); });
document.getElementById('btn-run-ddu').addEventListener('click', function() { quickAction(this, 'run_ddu'); });
document.getElementById('btn-disable-defender').addEventListener('click', function() { quickAction(this, 'disable_defender'); });
document.getElementById('btn-deep-services').addEventListener('click', function() { quickAction(this, 'deep_services'); });
document.getElementById('btn-clean-winsxs').addEventListener('click', function() { quickAction(this, 'adv_clean_winsxs'); });
document.getElementById('btn-list-restore').addEventListener('click', function() { listRestorePoints(this); });

document.getElementById('btn-power-ultimate').addEventListener('click', function() { quickAction(this, 'power_ultimate'); });
document.getElementById('btn-power-high').addEventListener('click', function() { quickAction(this, 'power_high'); });
document.getElementById('btn-power-balanced').addEventListener('click', function() { quickAction(this, 'power_balanced'); });
document.getElementById('btn-usb-no-suspend').addEventListener('click', function() { quickAction(this, 'usb_no_suspend'); });
document.getElementById('btn-usb-suspend-ok').addEventListener('click', function() { quickAction(this, 'usb_suspend_ok'); });
document.getElementById('btn-pcie-max-perf').addEventListener('click', function() { quickAction(this, 'pcie_max_perf'); });
document.getElementById('btn-pcie-power-save').addEventListener('click', function() { quickAction(this, 'pcie_power_save'); });

document.getElementById('btn-run-sfc').addEventListener('click', function() { runSFC(this); });
document.getElementById('btn-run-dism').addEventListener('click', function() { runDISM(this); });
document.getElementById('btn-pause-updates-7').addEventListener('click', function() { pauseUpdates(this, 7); });
document.getElementById('btn-pause-updates-35').addEventListener('click', function() { pauseUpdates(this, 35); });
document.getElementById('btn-resume-updates').addEventListener('click', function() { resumeUpdates(this); });
document.getElementById('btn-load-tasks').addEventListener('click', function() { loadTasks(this); });
document.getElementById('btn-pagefile-info').addEventListener('click', function() { loadPageFileInfo(this); });
document.getElementById('btn-set-pagefile').addEventListener('click', function() { setPageFile(this); });

document.getElementById('btn-ping-host').addEventListener('click', function() { pingHost(this); });
document.getElementById('btn-reset-network-2').addEventListener('click', function() { resetNetwork(this); });
document.getElementById('btn-rss-on-2').addEventListener('click', function() { quickAction(this, 'net_rss_on'); });
document.getElementById('btn-rss-off-2').addEventListener('click', function() { quickAction(this, 'net_rss_off'); });

document.getElementById('btn-analyze-events').addEventListener('click', function() { analyzeEvents(this); });
document.getElementById('btn-battery-health').addEventListener('click', function() { getBatteryHealth(this); });
document.getElementById('btn-hags-on').addEventListener('click', function() { setHAGS(this, true); });
document.getElementById('btn-hags-off').addEventListener('click', function() { setHAGS(this, false); });
document.getElementById('btn-mouse-accel-off').addEventListener('click', function() { setMouseAccel(this, false); });
document.getElementById('btn-mouse-accel-on').addEventListener('click', function() { setMouseAccel(this, true); });

document.getElementById('btn-generate-report').addEventListener('click', function() { generateReport(this); });
document.getElementById('btn-backup-settings').addEventListener('click', function() { backupSettings(this); });
document.getElementById('btn-restore-settings').addEventListener('click', function() { restoreSettings(this); });

document.getElementById('btn-save-fine-settings').addEventListener('click', function() { saveFineSettings(this); });
document.getElementById('btn-apply-s50').addEventListener('click', function() { applyS50(this); });

function boot() {
  if (!initBridge()) {
    if (++bootTries < 100) { setTimeout(boot, 100); return; }
    return;
  }
  var lang = store.get('optpro-lang', 'ru');
  setLang(lang);
  var savedTheme = store.get('optpro-theme', 'midnight');
  var sel = document.getElementById('theme-select');
  if (sel) sel.value = savedTheme;
  setTheme(savedTheme);
  loadDrives();
  loadSettingsFromServer();
  setInterval(pollStats, 2000);
  pollStats();
  loadCpuFeatures();
  setTimeout(initChart, 200);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
else boot();
</script></body></html>)HTML";

class App : public std::enable_shared_from_this<App> {
public:
    HWND hwnd = nullptr;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::atomic<bool> shuttingDown{ false };

    void PostUI(std::function<void()> fn) {
        if (shuttingDown || !hwnd) return;
        auto* p = new std::function<void()>(std::move(fn));
        if (!PostMessageW(hwnd, WM_APP_RUNUI, 0, (LPARAM)p)) delete p;
    }

    bool Initialize(HWND hWnd) {
        hwnd = hWnd;
        {
            std::lock_guard<std::mutex> lk(g_sinkMutex);
            g_uiLogSink = [this](const std::wstring& s) { PostUI([this, s]() { SendLog(s); }); };
            g_uiProgressSink = [this](int p, const std::wstring& m) { PostUI([this, p, m]() { SendProgress(p, m); }); };
        }

        std::wstring indexPath = g_paths.BackupDir + L"\\index.html";
        std::ofstream f(indexPath.c_str(), std::ios::binary);
        if (f) {
            f.write(HTML_UI, strlen(HTML_UI));
            f.close();
        }

        auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, g_paths.WebView2Dir.c_str(), options.Get(),
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result) || !env) {
                        MessageBoxW(hwnd, L"Failed to create WebView2 environment.", L"Error", MB_ICONERROR);
                        return result;
                    }
                    HRESULT ch = env->CreateCoreWebView2Controller(hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT r, ICoreWebView2Controller* ctrl) -> HRESULT {
                                if (FAILED(r) || !ctrl) {
                                    MessageBoxW(hwnd, L"Failed to create WebView2 controller.", L"Error", MB_ICONERROR);
                                    return r;
                                }
                                controller = ctrl;
                                HRESULT gh = controller->get_CoreWebView2(&webview);
                                if (FAILED(gh) || !webview) return gh;
                                ComPtr<ICoreWebView2_3> webview3;
                                if (SUCCEEDED(webview.As(&webview3))) {
                                    webview3->SetVirtualHostNameToFolderMapping(L"optpro.local", g_paths.BackupDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                }
                                RECT rc; GetClientRect(hwnd, &rc);
                                controller->put_Bounds(rc);
                                webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR msg = nullptr;
                                            HRESULT mh = args->TryGetWebMessageAsString(&msg);
                                            if (SUCCEEDED(mh) && msg) {
                                                std::wstring s(msg); CoTaskMemFree(msg);
                                                OnMessage(s);
                                                return S_OK;
                                            }
                                            return S_OK;
                                        }).Get(), nullptr);
                                webview->Navigate(L"https://optpro.local/index.html");
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());
        return SUCCEEDED(hr);
    }

    void Suspend() {
        if (webview) {
            ComPtr<ICoreWebView2_3> webview3;
            if (SUCCEEDED(webview.As(&webview3))) {
                webview3->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
                    [](HRESULT error, BOOL isSuccessful) -> HRESULT {
                        return S_OK;
                    }).Get());
            }
        }
    }

    void Resume() {
        if (webview) {
            ComPtr<ICoreWebView2_3> webview3;
            if (SUCCEEDED(webview.As(&webview3))) {
                webview3->Resume();
            }
        }
    }

    void Resize(int w, int h) { if (w > 0 && h > 0 && controller) { RECT rc = { 0, 0, w, h }; controller->put_Bounds(rc); } }
    void Send(const std::wstring& msg) { if (webview && !shuttingDown) webview->PostWebMessageAsString(msg.c_str()); }
    void SendLog(const std::wstring& s) { Send(L"L\x1F" + s); }
    void SendProgress(int p, const std::wstring& msg) { Send(L"P\x1F" + std::to_wstring(p) + L"\x1F" + msg); }
    void SendResp(int id, const std::wstring& json) { Send(L"R\x1F" + std::to_wstring(id) + L"\x1F" + json); }

    void Shutdown() {
        shuttingDown = true;
        { std::lock_guard<std::mutex> lk(g_sinkMutex); g_uiLogSink = nullptr; g_uiProgressSink = nullptr; }
        if (controller) { controller->Close(); controller = nullptr; }
        webview = nullptr;
    }

    void HandleWinCtrl(const std::wstring& act) const {
        if (!hwnd) return;
        if (act == L"min") ShowWindow(hwnd, SW_MINIMIZE);
        else if (act == L"max") { if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE); else ShowWindow(hwnd, SW_MAXIMIZE); }
        else if (act == L"close") PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    void HandleDrag() const { if (hwnd) { ReleaseCapture(); SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); } }
    void HandleResize(const std::wstring& dir) const {
        if (!hwnd) return;
        UINT hit = HTCLIENT;
        if (dir == L"topleft") hit = HTTOPLEFT; else if (dir == L"topright") hit = HTTOPRIGHT;
        else if (dir == L"bottomleft") hit = HTBOTTOMLEFT; else if (dir == L"bottomright") hit = HTBOTTOMRIGHT;
        else if (dir == L"left") hit = HTLEFT; else if (dir == L"right") hit = HTRIGHT;
        else if (dir == L"top") hit = HTTOP; else if (dir == L"bottom") hit = HTBOTTOM;
        if (hit == HTCLIENT) return;
        ReleaseCapture(); SendMessageW(hwnd, WM_NCLBUTTONDOWN, hit, 0);
    }

    void AsyncData(int id, std::function<std::wstring()> fn) {
        auto self = shared_from_this();
        g_activeAsyncTasks++;
        std::thread([self, id, fn]() {
            try {
                std::wstring result = fn();
                self->PostUI([self, id, result]() { if (!self->shuttingDown) self->SendResp(id, result); });
            }
            catch (...) {
                self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false,\"error\":\"exception\"}"); });
            }
            g_activeAsyncTasks--;
            g_asyncCv.notify_all();
            }).detach();
    }

    void OnMessage(const std::wstring& raw) {
        int id = -1;
        try {
            if (raw.size() >= 3 && raw[0] == L'W' && raw[1] == L'\x1F') {
                std::wstring payload = raw.substr(2);
                auto p = payload.find(L'\x1F');
                if (p == std::wstring::npos) { if (payload == L"drag") HandleDrag(); else HandleWinCtrl(payload); }
                else {
                    std::wstring cmd = payload.substr(0, p);
                    std::wstring arg = payload.substr(p + 1);
                    if (cmd == L"resize") HandleResize(arg);
                }
                return;
            }
            if (raw.size() < 4 || raw[0] != L'C') return;
            std::vector<std::wstring> parts; std::wstring cur;
            for (wchar_t c : raw) { if (c == L'\x1F') { parts.push_back(cur); cur.clear(); } else cur += c; }
            parts.push_back(cur);
            if (parts.size() < 4) return;
            id = SafeStoi(parts[1], -1);
            if (id < 0) return;
            const std::wstring& m = parts[2];
            auto arg = [&](size_t i) -> std::wstring { return i < parts.size() ? parts[i] : L""; };

            if (m == L"get_status") {
                MEMORYSTATUSEX ms = { sizeof(ms) }; GlobalMemoryStatusEx(&ms);
                std::wstring sys; { std::lock_guard<std::mutex> lk(g_pcInfoMutex); sys = g_pcInfoJson; }
                std::wstring disks; { std::lock_guard<std::mutex> lk(g_diskHealthMutex); disks = g_diskHealthJson; }
                std::wstring j = L"{\"cpu\":" + FloatToJsonStr(g_cpuUsage.load()) + L",\"gpu\":" + FloatToJsonStr(g_gpuUsage.load()) +
                    L",\"ram_free\":" + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) + L",\"procs\":" + std::to_wstring(g_procCount.load()) +
                    L",\"temp\":" + FloatToJsonStr(g_cpuTemp.load()) + L",\"gpuTemp\":" + FloatToJsonStr(g_gpuTemp.load()) +
                    L",\"gpuFan\":" + FloatToJsonStr(g_gpuFan.load()) +
                    L",\"vramUsed\":" + std::to_wstring(g_vramUsedMB.load()) + L",\"vramTotal\":" + std::to_wstring(g_vramTotalMB.load()) +
                    L",\"throttle\":" + std::wstring(g_throttling.load() ? L"true" : L"false") +
                    L",\"ping\":" + std::to_wstring(g_pingMs.load()) +
                    L",\"battery\":" + FloatToJsonStr(g_batteryPercent.load()) +
                    L",\"dpc\":" + FloatToJsonStr(g_dpcIsrTime.load()) +
                    L",\"appCpu\":" + FloatToJsonStr(g_appCpuUsage.load()) +
                    L",\"appGpu\":" + FloatToJsonStr(g_appGpuUsage.load()) + L",\"appRam\":" + std::to_wstring(g_appRamMB.load()) +
                    L",\"net_down\":" + FloatToJsonStr(g_netDownMbps.load()) + L",\"net_up\":" + FloatToJsonStr(g_netUpMbps.load()) +
                    L",\"sys\":" + sys + L",\"disks\":" + disks + L"}";
                SendResp(id, j); return;
            }
            if (m == L"get_cpu_features") {
                auto f = GetCpuFeatures();
                std::wstring j = L"{\"sse41\":" + std::wstring(f.sse41 ? L"true" : L"false") + L",\"sse42\":" + std::wstring(f.sse42 ? L"true" : L"false") +
                    L",\"popcnt\":" + std::wstring(f.popcnt ? L"true" : L"false") + L",\"avx\":" + std::wstring(f.avx ? L"true" : L"false") +
                    L",\"fma3\":" + std::wstring(f.fma3 ? L"true" : L"false") + L",\"avx2\":" + std::wstring(f.avx2 ? L"true" : L"false") +
                    L",\"bmi1\":" + std::wstring(f.bmi1 ? L"true" : L"false") + L",\"bmi2\":" + std::wstring(f.bmi2 ? L"true" : L"false") +
                    L",\"avx512\":" + std::wstring(f.avx512 ? L"true" : L"false") + L"}";
                SendResp(id, j); return;
            }
            if (m == L"get_drives") {
                auto drives = GetDrivesList();
                std::wstring j = L"{\"active\":\"" + std::wstring(1, g_activeDrive.load()) + L"\",\"system\":\"" + std::wstring(1, g_systemDriveLetter) + L"\",\"drives\":[";
                for (size_t i = 0; i < drives.size(); ++i) {
                    if (i) j += L",";
                    j += L"{\"letter\":" + std::to_wstring((int)drives[i].letter)
                        + L",\"label\":" + JsonStr(drives[i].label)
                        + L",\"isSystem\":" + std::wstring(drives[i].isSystem ? L"true" : L"false") + L"}";
                }
                j += L"]}";
                SendResp(id, j); return;
            }
            if (m == L"set_drive") {
                std::wstring letterStr = arg(4);
                if (letterStr.empty()) { SendResp(id, L"{\"ok\":false}"); return; }
                wchar_t L = letterStr[0];
                if (L >= L'a' && L <= L'z') L = static_cast<wchar_t>(L - L'a' + L'A');
                if (L < L'A' || L > L'Z') { SendResp(id, L"{\"ok\":false}"); return; }
                std::wstring root = std::wstring(1, L) + L":\\";
                if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) { SendResp(id, L"{\"ok\":false}"); return; }
                g_activeDrive = L;
                bool isSys = (L == g_systemDriveLetter);
                SendResp(id, L"{\"ok\":true,\"letter\":\"" + std::wstring(1, L) + L"\",\"isSystem\":" + std::wstring(isSys ? L"true" : L"false") + L"}");
                return;
            }
            if (m == L"get_settings") {
                UserSettings s; { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
                std::wstring j = L"{\"ok\":true,\"settings\":{"
                    L"\"cleanTemp\":" + std::wstring(s.cleanTemp ? L"true" : L"false")
                    + L",\"cleanPrefetch\":" + std::wstring(s.cleanPrefetch ? L"true" : L"false")
                    + L",\"cleanRecycle\":" + std::wstring(s.cleanRecycle ? L"true" : L"false")
                    + L",\"cleanThumbs\":" + std::wstring(s.cleanThumbs ? L"true" : L"false")
                    + L",\"cleanShader\":" + std::wstring(s.cleanShader ? L"true" : L"false")
                    + L",\"cleanDns\":" + std::wstring(s.cleanDns ? L"true" : L"false")
                    + L",\"cleanDO\":" + std::wstring(s.cleanDO ? L"true" : L"false")
                    + L",\"cleanLaunchers\":" + std::wstring(s.cleanLaunchers ? L"true" : L"false")
                    + L",\"cleanBrowsers\":" + std::wstring(s.cleanBrowsers ? L"true" : L"false")
                    + L",\"cleanChats\":" + std::wstring(s.cleanChats ? L"true" : L"false")
                    + L",\"cleanDumps\":" + std::wstring(s.cleanDumps ? L"true" : L"false")
                    + L",\"ramClearStandby\":" + std::wstring(s.ramClearStandby ? L"true" : L"false")
                    + L",\"ramFlushModified\":" + std::wstring(s.ramFlushModified ? L"true" : L"false")
                    + L",\"ramClearWorkingSets\":" + std::wstring(s.ramClearWorkingSets ? L"true" : L"false")
                    + L",\"ramClearFileCache\":" + std::wstring(s.ramClearFileCache ? L"true" : L"false")
                    + L",\"autoBoost\":" + std::wstring(s.autoBoost ? L"true" : L"false")
                    + L",\"ramAutoCleanEnabled\":" + std::wstring(s.ramAutoCleanEnabled ? L"true" : L"false")
                    + L",\"ramAutoCleanThresholdPct\":" + std::to_wstring(s.ramAutoCleanThresholdPct)
                    + L",\"ramAutoCleanIntervalMin\":" + std::to_wstring(s.ramAutoCleanIntervalMin)
                    + L",\"ramAutoCleanOnIdle\":" + std::wstring(s.ramAutoCleanOnIdle ? L"true" : L"false")
                    + L",\"overlayDarkTheme\":" + std::wstring(s.overlayDarkTheme ? L"true" : L"false")
                    + L",\"hotkeysEnabled\":" + std::wstring(s.hotkeysEnabled ? L"true" : L"false")

                    + L",\"s50_telemetry\":" + std::wstring(s.s50_telemetry ? L"true" : L"false")
                    + L",\"s50_cortana\":" + std::wstring(s.s50_cortana ? L"true" : L"false")
                    + L",\"s50_adid\":" + std::wstring(s.s50_adid ? L"true" : L"false")
                    + L",\"s50_actfeed\":" + std::wstring(s.s50_actfeed ? L"true" : L"false")
                    + L",\"s50_smartscreen\":" + std::wstring(s.s50_smartscreen ? L"true" : L"false")
                    + L",\"s50_uac\":" + std::wstring(s.s50_uac ? L"true" : L"false")
                    + L",\"s50_defender\":" + std::wstring(s.s50_defender ? L"true" : L"false")
                    + L",\"s50_vbs\":" + std::wstring(s.s50_vbs ? L"true" : L"false")
                    + L",\"s50_etw\":" + std::wstring(s.s50_etw ? L"true" : L"false")
                    + L",\"s50_winupd\":" + std::wstring(s.s50_winupd ? L"true" : L"false")
                    + L",\"s50_gamemode\":" + std::wstring(s.s50_gamemode ? L"true" : L"false")
                    + L",\"s50_gamedvr\":" + std::wstring(s.s50_gamedvr ? L"true" : L"false")
                    + L",\"s50_prio\":" + std::wstring(s.s50_prio ? L"true" : L"false")
                    + L",\"s50_netthrottle\":" + std::wstring(s.s50_netthrottle ? L"true" : L"false")
                    + L",\"s50_nagle\":" + std::wstring(s.s50_nagle ? L"true" : L"false")
                    + L",\"s50_dynamictick\":" + std::wstring(s.s50_dynamictick ? L"true" : L"false")
                    + L",\"s50_hags\":" + std::wstring(s.s50_hags ? L"true" : L"false")
                    + L",\"s50_mouseaccel\":" + std::wstring(s.s50_mouseaccel ? L"true" : L"false")
                    + L",\"s50_power\":" + std::wstring(s.s50_power ? L"true" : L"false")
                    + L",\"s50_zero\":" + std::wstring(s.s50_zero ? L"true" : L"false")
                    + L",\"s50_minecraft\":" + std::wstring(s.s50_minecraft ? L"true" : L"false")
                    + L",\"s50_print\":" + std::wstring(s.s50_print ? L"true" : L"false")
                    + L",\"s50_icons\":" + std::wstring(s.s50_icons ? L"true" : L"false")
                    + L",\"s50_pools\":" + std::wstring(s.s50_pools ? L"true" : L"false")
                    + L",\"s50_paging\":" + std::wstring(s.s50_paging ? L"true" : L"false")
                    + L",\"s50_memcomp\":" + std::wstring(s.s50_memcomp ? L"true" : L"false")
                    + L",\"s50_fastdns\":" + std::wstring(s.s50_fastdns ? L"true" : L"false")
                    + L",\"s50_rss\":" + std::wstring(s.s50_rss ? L"true" : L"false")
                    + L",\"s50_resetnet\":" + std::wstring(s.s50_resetnet ? L"true" : L"false")
                    + L",\"s50_ping\":" + std::wstring(s.s50_ping ? L"true" : L"false")
                    + L",\"s50_services\":" + std::wstring(s.s50_services ? L"true" : L"false")
                    + L",\"s50_ctxmenu\":" + std::wstring(s.s50_ctxmenu ? L"true" : L"false")
                    + L",\"s50_usb\":" + std::wstring(s.s50_usb ? L"true" : L"false")
                    + L",\"s50_pcie\":" + std::wstring(s.s50_pcie ? L"true" : L"false")
                    + L",\"s50_nvapi\":" + std::wstring(s.s50_nvapi ? L"true" : L"false")

                    + L"}}";
                SendResp(id, j); return;
            }
            if (m == L"set_settings") {
                std::wstring payload = arg(4);
                std::string utf8 = WideToUtf8(payload);
                auto getBool = [&](const char* key, bool& out) {
                    std::string needle = std::string("\"") + key + "\":";
                    auto pos = utf8.find(needle);
                    if (pos == std::string::npos) return;
                    auto start = pos + needle.size();
                    if (utf8.compare(start, 4, "true") == 0) out = true;
                    else out = false;
                    };
                auto getInt = [&](const char* key, int& out) {
                    std::string needle = std::string("\"") + key + "\":";
                    auto pos = utf8.find(needle);
                    if (pos == std::string::npos) return;
                    auto start = pos + needle.size();
                    try { out = std::stoi(utf8.substr(start)); }
                    catch (...) {}
                    };
                UserSettings s;
                { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
                getBool("cleanTemp", s.cleanTemp);
                getBool("cleanPrefetch", s.cleanPrefetch);
                getBool("cleanRecycle", s.cleanRecycle);
                getBool("cleanThumbs", s.cleanThumbs);
                getBool("cleanShader", s.cleanShader);
                getBool("cleanDns", s.cleanDns);
                getBool("cleanDO", s.cleanDO);
                getBool("cleanLaunchers", s.cleanLaunchers);
                getBool("cleanBrowsers", s.cleanBrowsers);
                getBool("cleanChats", s.cleanChats);
                getBool("cleanDumps", s.cleanDumps);
                getBool("ramClearStandby", s.ramClearStandby);
                getBool("ramFlushModified", s.ramFlushModified);
                getBool("ramClearWorkingSets", s.ramClearWorkingSets);
                getBool("ramClearFileCache", s.ramClearFileCache);
                getBool("autoBoost", s.autoBoost);
                getBool("ramAutoCleanEnabled", s.ramAutoCleanEnabled);
                getInt("ramAutoCleanThresholdPct", s.ramAutoCleanThresholdPct);
                getInt("ramAutoCleanIntervalMin", s.ramAutoCleanIntervalMin);
                getBool("ramAutoCleanOnIdle", s.ramAutoCleanOnIdle);
                getBool("overlayDarkTheme", s.overlayDarkTheme);
                getBool("hotkeysEnabled", s.hotkeysEnabled);

                getBool("s50_telemetry", s.s50_telemetry);
                getBool("s50_cortana", s.s50_cortana);
                getBool("s50_adid", s.s50_adid);
                getBool("s50_actfeed", s.s50_actfeed);
                getBool("s50_smartscreen", s.s50_smartscreen);
                getBool("s50_uac", s.s50_uac);
                getBool("s50_defender", s.s50_defender);
                getBool("s50_vbs", s.s50_vbs);
                getBool("s50_etw", s.s50_etw);
                getBool("s50_winupd", s.s50_winupd);
                getBool("s50_gamemode", s.s50_gamemode);
                getBool("s50_gamedvr", s.s50_gamedvr);
                getBool("s50_prio", s.s50_prio);
                getBool("s50_netthrottle", s.s50_netthrottle);
                getBool("s50_nagle", s.s50_nagle);
                getBool("s50_dynamictick", s.s50_dynamictick);
                getBool("s50_hags", s.s50_hags);
                getBool("s50_mouseaccel", s.s50_mouseaccel);
                getBool("s50_power", s.s50_power);
                getBool("s50_zero", s.s50_zero);
                getBool("s50_minecraft", s.s50_minecraft);
                getBool("s50_print", s.s50_print);
                getBool("s50_icons", s.s50_icons);
                getBool("s50_pools", s.s50_pools);
                getBool("s50_paging", s.s50_paging);
                getBool("s50_memcomp", s.s50_memcomp);
                getBool("s50_fastdns", s.s50_fastdns);
                getBool("s50_rss", s.s50_rss);
                getBool("s50_resetnet", s.s50_resetnet);
                getBool("s50_ping", s.s50_ping);
                getBool("s50_services", s.s50_services);
                getBool("s50_ctxmenu", s.s50_ctxmenu);
                getBool("s50_usb", s.s50_usb);
                getBool("s50_pcie", s.s50_pcie);
                getBool("s50_nvapi", s.s50_nvapi);

                if (s.ramAutoCleanThresholdPct < 50) s.ramAutoCleanThresholdPct = 50;
                if (s.ramAutoCleanThresholdPct > 99) s.ramAutoCleanThresholdPct = 99;
                if (s.ramAutoCleanIntervalMin < 5) s.ramAutoCleanIntervalMin = 5;
                if (s.ramAutoCleanIntervalMin > 120) s.ramAutoCleanIntervalMin = 120;

                { std::lock_guard<std::mutex> lk(g_settingsMutex); g_userSettings = s; }
                g_overlay.darkTheme = s.overlayDarkTheme;

                std::thread([]() { BackupSettings(); }).detach();

                SendResp(id, L"{\"ok\":true}"); return;
            }
            if (m == L"apply_s50") {
                ApplyS50Settings();
                SendResp(id, L"{\"ok\":true}"); return;
            }
            if (m == L"smart_scan") {
                return AsyncData(id, []() -> std::wstring {
                    auto items = SmartScan();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < items.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"id\":" + JsonStr(items[i].id)
                            + L",\"category\":" + JsonStr(items[i].category)
                            + L",\"title\":" + JsonStr(items[i].title)
                            + L",\"desc\":" + JsonStr(items[i].desc)
                            + L",\"safety\":" + JsonStr(items[i].safety)
                            + L",\"impact\":" + JsonStr(items[i].impact)
                            + L",\"size\":" + std::to_wstring(items[i].sizeBytes)
                            + L",\"defaultOn\":" + std::wstring(items[i].defaultOn ? L"true" : L"false")
                            + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"smart_apply") {
                std::vector<std::wstring> ids;
                for (size_t i = 4; i < parts.size(); ++i) ids.push_back(parts[i]);
                return AsyncData(id, [ids]() -> std::wstring {
                    uint64_t totalFreed = 0;
                    int okCount = 0;
                    for (auto& sid : ids) {
                        uint64_t freed = 0;
                        if (SmartApplyItem(sid, freed)) { okCount++; totalFreed += freed; }
                    }
                    return L"{\"ok\":true,\"applied\":" + std::to_wstring(okCount) + L",\"freed\":" + std::to_wstring(totalFreed) + L"}";
                    });
            }
            if (m == L"get_autostart") {
                auto v = ListAutostart();
                std::wstring j = L"[";
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i) j += L",";
                    j += L"{\"name\":" + JsonStr(v[i].name) + L",\"command\":" + JsonStr(v[i].command) +
                        L",\"location\":" + JsonStr(v[i].location) + L",\"isEnabled\":" + std::wstring(v[i].isEnabled ? L"true" : L"false") + L"}";
                }
                SendResp(id, L"{\"items\":" + j + L"]}"); return;
            }
            if (m == L"toggle_autostart") {
                std::wstring name = arg(4), loc = arg(5), act = arg(6);
                bool ok = ToggleAutostart(name, loc, act);
                SendResp(id, ok ? L"{\"ok\":true}" : L"{\"ok\":false}"); return;
            }
            if (m == L"boot_impact") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ListAutostart();
                    std::wstring j = L"[";
                    bool first = true;
                    for (auto& a : v) {
                        if (!first) j += L",";
                        first = false;
                        int impact = (int)(a.name.size() * 15 + a.command.size() / 10);
                        if (impact > 5000) impact = 5000;
                        j += L"{\"name\":" + JsonStr(a.name) + L",\"command\":" + JsonStr(a.command) + L",\"impact\":" + std::to_wstring(impact) + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"enhanced_cache") {
                return AsyncData(id, []() -> std::wstring {
                    MEMORYSTATUSEX before{ sizeof(before) };
                    GlobalMemoryStatusEx(&before);
                    CleanCacheEnhanced();
                    MEMORYSTATUSEX after{ sizeof(after) };
                    GlobalMemoryStatusEx(&after);
                    uint64_t freed = after.ullAvailPhys > before.ullAvailPhys ? after.ullAvailPhys - before.ullAvailPhys : 0;
                    return L"{\"ok\":true,\"freed\":" + std::to_wstring(freed) + L"}";
                    });
            }
            if (m == L"enhanced_ram") {
                return AsyncData(id, []() -> std::wstring {
                    uint64_t freed = PurgeStandbyEnhanced();
                    return L"{\"ok\":true,\"freed\":" + std::to_wstring(freed) + L"}";
                    });
            }
            if (m == L"lite_debloat") {
                if (g_activeDrive.load() != g_systemDriveLetter) {
                    SendResp(id, L"{\"ok\":false,\"error\":\"Только для системного диска\"}");
                    return;
                }
                auto self = shared_from_this();
                std::thread([self, id]() {
                    try {
                        int msgbox = MessageBoxW(self->hwnd,
                            L"ВНИМАНИЕ! Это удалит встроенные приложения и отключит службы.\n\nПродолжить?",
                            L"Подтверждение", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
                        if (msgbox == IDYES) {
                            PerformLiteDebloat();
                            self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":true}"); });
                        }
                        else {
                            self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false}"); });
                        }
                    }
                    catch (...) {
                        self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false}"); });
                    }
                    }).detach();
                return;
            }
            if (m == L"compress_game") {
                std::wstring path = arg(4);
                return AsyncData(id, [path]() -> std::wstring {
                    bool ok = CompressGameFolder(path);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"install_app") {
                std::wstring pkgId = arg(4);
                return AsyncData(id, [pkgId]() -> std::wstring {
                    bool ok = InstallAppWinget(pkgId);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"uninstall_program") {
                std::wstring uninst = arg(4);
                return AsyncData(id, [uninst]() -> std::wstring {
                    if (uninst.empty()) return L"{\"ok\":false}";
                    bool ok = false;
                    if (uninst.find(L"msiexec") != std::wstring::npos) {
                        ok = RunCmd(uninst, L"Uninstall", false, 600);
                    }
                    else {
                        STARTUPINFOW si{ sizeof(si) };
                        PROCESS_INFORMATION pi{};
                        std::wstring cmd = uninst;
                        std::vector<wchar_t> buf(cmd.begin(), cmd.end());
                        buf.push_back(0);
                        if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                            ok = true;
                        }
                    }
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"scan_programs_detailed") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ScanProgramsDetailed();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(v[i].name) + L",\"version\":" + JsonStr(v[i].version)
                            + L",\"publisher\":" + JsonStr(v[i].publisher) + L",\"installLoc\":" + JsonStr(v[i].installLoc)
                            + L",\"uninstallStr\":" + JsonStr(v[i].uninstallStr) + L",\"quietUninstallStr\":" + JsonStr(v[i].quietUninstallStr)
                            + L",\"sizeKB\":" + std::to_wstring(v[i].sizeKB) + L",\"is64\":" + std::wstring(v[i].is64 ? L"true" : L"false") + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"bulk_uninstall") {
                std::wstring payload = arg(4);
                return AsyncData(id, [payload]() -> std::wstring {
                    std::string utf8 = WideToUtf8(payload);
                    std::wstring silentCmds;
                    size_t pos = 0;
                    while ((pos = utf8.find("\"silentCmd\":\"", pos)) != std::string::npos) {
                        pos += 13;
                        size_t end = utf8.find("\"", pos);
                        if (end != std::string::npos) {
                            silentCmds += Utf8ToWide(utf8.substr(pos, end - pos)) + L"\n";
                            pos = end;
                        }
                    }
                    std::wstringstream ss(silentCmds);
                    std::wstring line;
                    int count = 0;
                    while (std::getline(ss, line)) {
                        if (!line.empty()) {
                            RunCmd(line, L"Uninstalling", false, 600);
                            count++;
                        }
                    }
                    return L"{\"ok\":true,\"count\":" + std::to_wstring(count) + L"}";
                    });
            }
            if (m == L"open_url") {
                std::wstring u = arg(4);
                ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                SendResp(id, L"{\"ok\":true}"); return;
            }
            if (m == L"get_context_menu") {
                std::wstring j = ListContextMenuJson();
                SendResp(id, L"{\"items\":" + j + L"}"); return;
            }
            if (m == L"toggle_context_menu") {
                std::wstring clsid = arg(4), dis = arg(5);
                bool ok = ToggleContextMenu(clsid, dis == L"1");
                SendResp(id, ok ? L"{\"ok\":true}" : L"{\"ok\":false}"); return;
            }
            if (m == L"set_overlay_setting") {
                std::wstring key = arg(4); std::wstring val = arg(5);
                bool valBool = val == L"1";
                if (key == L"cpu") g_overlay.showCpu = valBool;
                else if (key == L"gpu") g_overlay.showGpu = valBool;
                else if (key == L"ram") g_overlay.showRam = valBool;
                else if (key == L"temp") g_overlay.showTemp = valBool;
                else if (key == L"vram") g_overlay.showVram = valBool;
                else if (key == L"fan") g_overlay.showFan = valBool;
                else if (key == L"ping") g_overlay.showPing = valBool;
                else if (key == L"fps") g_overlay.showFps = valBool;
                else if (key == L"dark") { g_overlay.darkTheme = valBool; SaveOverlaySettings(); }
                SendResp(id, L"{\"ok\":true}"); return;
            }
            if (m == L"scan_games") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ScanGames();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(v[i].name) + L",\"path\":" + JsonStr(v[i].path) + L",\"launcher\":" + JsonStr(v[i].launcher) + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"game_preset") {
                std::wstring preset = arg(4);
                auto self = shared_from_this();
                std::thread([self, id, preset]() {
                    try {
                        if (preset == L"competitive") {
                            SetWin32PrioritySeparation(0x26);
                            SetGameMode(true);
                            RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"High perf", false);
                            DisableNetworkThrottling();
                            g_overlay.showCpu = true; g_overlay.showGpu = true; g_overlay.showFps = true;
                        }
                        else if (preset == L"balanced") {
                            SetWin32PrioritySeparation(0x2);
                            SetGameMode(true);
                            RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Balanced", false);
                        }
                        else if (preset == L"quality") {
                            RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"High perf", false);
                        }
                        self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":true}"); });
                    }
                    catch (...) {
                        self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false}"); });
                    }
                    }).detach();
                return;
            }
            if (m == L"read_hosts") {
                if (g_activeDrive.load() != g_systemDriveLetter) { SendResp(id, L"{\"ok\":false}"); return; }
                std::wstring path = g_paths.SystemRoot + L"\\System32\\drivers\\etc\\hosts";
                std::ifstream f(path.c_str(), std::ios::binary);
                std::stringstream ss; ss << f.rdbuf();
                SendResp(id, L"{\"ok\":true,\"content\":" + JsonStr(Utf8ToWide(ss.str())) + L"}"); return;
            }
            if (m == L"write_hosts") {
                if (g_activeDrive.load() != g_systemDriveLetter) { SendResp(id, L"{\"ok\":false}"); return; }
                std::wstring content = arg(4);
                std::wstring path = g_paths.SystemRoot + L"\\System32\\drivers\\etc\\hosts";

                time_t t = time(nullptr);
                struct tm tm;
                localtime_s(&tm, &t);
                wchar_t ts[32];
                swprintf_s(ts, L"_%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
                std::wstring backup = g_paths.BackupDir + L"\\hosts.bak" + ts;

                std::error_code ec{};
                fs::copy_file(path, backup, fs::copy_options::overwrite_existing, ec);

                std::wstring tempPath = path + L".tmp";
                std::ofstream f(tempPath.c_str(), std::ios::binary);
                bool ok = false;
                if (f) {
                    std::string utf8 = WideToUtf8(content);
                    f.write(utf8.data(), utf8.size());
                    f.close();
                    if (MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) ok = true;
                    else DeleteFileW(tempPath.c_str());
                }
                SendResp(id, ok ? L"{\"ok\":true}" : L"{\"ok\":false}"); return;
            }
            if (m == L"list_services") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ListServices();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(v[i].name) + L",\"display\":" + JsonStr(v[i].display) +
                            L",\"state\":" + JsonStr(v[i].state) + L",\"startType\":" + JsonStr(v[i].startType) + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"control_service") {
                std::wstring name = arg(4), action = arg(5);
                return AsyncData(id, [name, action]() -> std::wstring {
                    std::wstring err;
                    bool ok = ControlService2(name, action, err);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"find_large_files") {
                std::wstring path = arg(4); if (path.empty()) path = GetEnvW(L"USERPROFILE");
                int mb = SafeStoi(arg(5), 100);
                return AsyncData(id, [path, mb]() -> std::wstring {
                    std::vector<std::pair<std::wstring, uint64_t>> out;
                    std::mutex outMutex;
                    std::vector<std::future<void>> futures;
                    std::atomic<int> activeTasks{ 0 };
                    std::error_code ec{};
                    if (fs::exists(path, ec) && !ec) {
                        for (auto& p : fs::directory_iterator(path, ec)) {
                            if (p.is_directory(ec) && !ec) {
                                while (activeTasks >= 8) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                activeTasks++;
                                std::wstring d = p.path().wstring();
                                futures.push_back(std::async(std::launch::async, [d, mb, &out, &outMutex, &activeTasks]() {
                                    try {
                                        std::vector<std::pair<std::wstring, uint64_t>> local;
                                        std::error_code e{};
                                        for (auto it = fs::recursive_directory_iterator(d, fs::directory_options::skip_permission_denied, e); it != fs::recursive_directory_iterator(); it.increment(e)) {
                                            if (g_stopMonitor) break;
                                            if (e) { e.clear(); continue; }
                                            if (it->is_regular_file(e) && !e) {
                                                auto sz = it->file_size(e);
                                                if (!e && sz >= (uint64_t)mb * 1024 * 1024) local.push_back({ it->path().wstring(), sz });
                                            }
                                        }
                                        if (!local.empty()) {
                                            std::lock_guard<std::mutex> lk(outMutex);
                                            out.insert(out.end(), local.begin(), local.end());
                                        }
                                    }
                                    catch (...) {}
                                    activeTasks--;
                                    }));
                            }
                        }
                    }
                    for (auto& f : futures) f.get();
                    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.second > b.second; });
                    std::wstring j = L"[";
                    for (size_t i = 0; i < out.size() && i < 100; ++i) {
                        if (i) j += L",";
                        j += L"{\"path\":" + JsonStr(out[i].first) + L",\"size\":" + std::to_wstring(out[i].second) + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"find_duplicates") {
                std::wstring path = arg(4); if (path.empty()) path = GetEnvW(L"USERPROFILE") + L"\\Downloads";
                return AsyncData(id, [path]() -> std::wstring {
                    std::map<uint64_t, std::vector<std::wstring>> bySize;
                    std::error_code ec{};
                    try {
                        if (fs::exists(path, ec) && !ec) {
                            for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
                                it != fs::recursive_directory_iterator(); it.increment(ec)) {
                                if (g_stopMonitor) break;
                                if (ec) { ec.clear(); continue; }
                                try {
                                    if (it->is_regular_file(ec) && !ec) {
                                        auto sz = it->file_size(ec);
                                        if (!ec && sz > 0 && sz < 4ULL * 1024 * 1024 * 1024) bySize[sz].push_back(it->path().wstring());
                                    }
                                }
                                catch (...) {}
                            }
                        }
                    }
                    catch (...) {}
                    std::map<uint64_t, std::map<std::wstring, std::vector<std::wstring>>> byHash;
                    std::mutex mapMutex;
                    std::vector<std::future<void>> futures;
                    std::atomic<int> activeTasks{ 0 };
                    for (auto& kv : bySize) {
                        if (kv.second.size() < 2) continue;
                        for (auto& f : kv.second) {
                            while (activeTasks >= 8) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            activeTasks++;
                            uint64_t sz = kv.first;
                            futures.push_back(std::async(std::launch::async, [f, sz, &byHash, &mapMutex, &activeTasks]() {
                                try {
                                    std::wstring h = HashFileHeader(f, sz);
                                    if (!h.empty()) {
                                        std::lock_guard<std::mutex> lk(mapMutex);
                                        byHash[sz][h].push_back(f);
                                    }
                                }
                                catch (...) {}
                                activeTasks--;
                                }));
                        }
                    }
                    for (auto& f : futures) f.get();
                    std::wstring j = L"[";
                    bool first = true;
                    for (auto& kv : byHash) {
                        for (auto& hkv : kv.second) {
                            if (hkv.second.size() < 2) continue;
                            if (!first) j += L",";
                            first = false;
                            j += L"{\"size\":" + std::to_wstring(kv.first) + L",\"files\":[";
                            for (size_t i = 0; i < hkv.second.size(); ++i) {
                                if (i) j += L",";
                                j += JsonStr(hkv.second[i]);
                            }
                            j += L"]}";
                        }
                    }
                    return L"{\"groups\":" + j + L"]}";
                    });
            }
            if (m == L"shred_file") {
                std::wstring path = arg(4);
                return AsyncData(id, [path]() -> std::wstring {
                    bool ok = ShredFileW(path);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"stress_test") {
                return AsyncData(id, []() -> std::wstring {
                    MEMORYSTATUSEX msb{}; msb.dwLength = sizeof(msb); GlobalMemoryStatusEx(&msb);
                    float maxTemp = -1.0f;
                    float sumCpu = 0; int n = 0;
                    auto start = std::chrono::steady_clock::now();
                    std::atomic<bool> stop{ false };
                    std::vector<std::thread> workers;
                    unsigned nThreads = std::thread::hardware_concurrency();
                    if (nThreads == 0) nThreads = 4;
                    for (unsigned i = 0; i < nThreads; ++i) {
                        workers.emplace_back([&stop]() {
                            volatile double x = 1.0;
                            while (!stop) { x = std::sqrt(x) * 3.14159 + 1.0; if (x > 1e15) x = 1.0; }
                            });
                    }
                    CpuSampler s;
                    for (int i = 0; i < 30; ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        float c = s.Sample(); sumCpu += c; n++;
                        if (g_cpuTemp.load() > maxTemp) maxTemp = g_cpuTemp.load();
                    }
                    stop = true;
                    for (auto& t : workers) t.join();
                    float avg = n > 0 ? sumCpu / n : 0;
                    return L"{\"ok\":true,\"avgCpu\":" + FloatToJsonStr(avg) + L",\"maxTemp\":" + FloatToJsonStr(maxTemp) + L"}";
                    });
            }
            if (m == L"benchmark") {
                return AsyncData(id, []() -> std::wstring {
                    auto start = std::chrono::high_resolution_clock::now();
                    volatile double acc = 0.0;
                    for (int i = 0; i < 10000000; ++i) acc += std::sqrt((double)i);
                    auto end = std::chrono::high_resolution_clock::now();
                    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    double ops = dur > 0 ? (10000000.0 / dur) * 1000.0 : 0;
                    return L"{\"ok\":true,\"score\":" + FloatToJsonStr((float)ops) + L",\"duration\":" + std::to_wstring(dur) + L"}";
                    });
            }
            if (m == L"create_restore_point") {
                std::wstring name = arg(4);
                return AsyncData(id, [name]() -> std::wstring {
                    bool ok = CreateRestorePoint(name.empty() ? L"Optimizer Pro" : name);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"list_restore_points") {
                return AsyncData(id, []() -> std::wstring {
                    CmdResult r = RunCmdImpl(L"powershell -NoProfile -Command \"Get-ComputerRestorePoint | Select-Object -Property SequenceNumber,Description,RestorePointType,EventType | ConvertTo-Json -Compress\"", 30);
                    if (r.exitCode != 0 || r.out.empty()) return L"{\"items\":[]}";
                    std::string out = r.out;
                    std::wstring items = L"[";
                    bool first = true;
                    size_t pos = 0;
                    while (true) {
                        auto s1 = out.find("\"SequenceNumber\":", pos);
                        if (s1 == std::string::npos) break;
                        size_t numStart = s1 + 17;
                        auto numEnd = out.find_first_of(",}", numStart);
                        std::string numStr = out.substr(numStart, numEnd - numStart);
                        auto d1 = out.find("\"Description\":\"", pos);
                        std::string desc = "Point";
                        if (d1 != std::string::npos) {
                            size_t ds = d1 + 15;
                            size_t de = out.find("\"", ds);
                            if (de != std::string::npos) desc = out.substr(ds, de - ds);
                        }
                        if (!first) items += L",";
                        first = false;
                        items += L"{\"seq\":" + Utf8ToWide(numStr) + L",\"desc\":" + JsonStr(Utf8ToWide(desc)) + L"}";
                        pos = numEnd;
                    }
                    items += L"]";
                    return L"{\"items\":" + items + L"}";
                    });
            }
            if (m == L"restore_to_point") {
                uint32_t seq = (uint32_t)SafeStoi(arg(4), 0);
                return AsyncData(id, [seq]() -> std::wstring {
                    bool ok = RestoreToPoint(seq);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"run_sfc") {
                return AsyncData(id, []() -> std::wstring {
                    std::wstring out = RunSFC();
                    return L"{\"ok\":true,\"output\":" + JsonStr(out) + L"}";
                    });
            }
            if (m == L"run_dism") {
                return AsyncData(id, []() -> std::wstring {
                    std::wstring out = RunDISM();
                    return L"{\"ok\":true,\"output\":" + JsonStr(out) + L"}";
                    });
            }
            if (m == L"pause_updates") {
                int days = SafeStoi(arg(4), 7);
                return AsyncData(id, [days]() -> std::wstring {
                    return PauseWindowsUpdates(days);
                    });
            }
            if (m == L"resume_updates") {
                return AsyncData(id, []() -> std::wstring {
                    return ResumeWindowsUpdates();
                    });
            }
            if (m == L"list_tasks") {
                return AsyncData(id, []() -> std::wstring {
                    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                    auto v = GetTaskSchedulerTasks();
                    if (SUCCEEDED(hr)) CoUninitialize();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) j += L",";
                        std::wstring entry = v[i];
                        auto p1 = entry.find(L'\x1F');
                        auto p2 = entry.find(L'\x1F', p1 + 1);
                        std::wstring name = entry.substr(0, p1);
                        std::wstring path = entry.substr(p1 + 1, p2 - p1 - 1);
                        std::wstring enabled = entry.substr(p2 + 1);
                        j += L"{\"name\":" + JsonStr(name) + L",\"path\":" + JsonStr(path) + L",\"enabled\":" + std::wstring(enabled == L"1" ? L"true" : L"false") + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"pagefile_info") {
                SendResp(id, GetPageFileInfo()); return;
            }
            if (m == L"set_pagefile") {
                uint32_t initial = (uint32_t)SafeStoi(arg(4), 4096);
                uint32_t max = (uint32_t)SafeStoi(arg(5), 8192);
                return AsyncData(id, [initial, max]() -> std::wstring {
                    bool ok = SetPageFileSize(initial, max);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"ping_host") {
                std::wstring host = arg(4);
                if (host.empty()) host = L"8.8.8.8";
                return AsyncData(id, [host]() -> std::wstring {
                    std::wstring cmd = L"ping -n 1 -w 2000 " + host;
                    CmdResult r = RunCmdImpl(cmd, 5);
                    int ms = -1;
                    if (r.exitCode == 0) {
                        auto pos = r.out.find("time=");
                        if (pos != std::string::npos) { try { ms = std::stoi(r.out.substr(pos + 5)); } catch (...) {} }
                        else { auto p2 = r.out.find("time<"); if (p2 != std::string::npos) ms = 1; }
                    }
                    return L"{\"ok\":true,\"ms\":" + std::to_wstring(ms) + L"}";
                    });
            }
            if (m == L"reset_network") {
                return AsyncData(id, []() -> std::wstring {
                    ResetNetwork();
                    return L"{\"ok\":true}";
                    });
            }
            if (m == L"analyze_events") {
                return AsyncData(id, []() -> std::wstring {
                    std::wstring out = AnalyzeEventLog();
                    return L"{\"ok\":true,\"events\":" + JsonStr(out) + L"}";
                    });
            }
            if (m == L"battery_health") {
                return AsyncData(id, []() -> std::wstring {
                    std::wstring out = GetBatteryHealth();
                    return L"{\"ok\":true,\"data\":" + JsonStr(out) + L"}";
                    });
            }
            if (m == L"set_hags") {
                bool enable = arg(4) == L"1";
                return AsyncData(id, [enable]() -> std::wstring {
                    bool ok = ToggleHAGS(enable);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"set_mouse_accel") {
                bool enable = arg(4) == L"1";
                return AsyncData(id, [enable]() -> std::wstring {
                    ToggleMouseAcceleration(enable);
                    return L"{\"ok\":true}";
                    });
            }
            if (m == L"generate_report") {
                return AsyncData(id, []() -> std::wstring {
                    std::wstring path = GenerateHtmlReport();
                    return L"{\"ok\":true,\"path\":" + JsonStr(path) + L"}";
                    });
            }
            if (m == L"backup_settings") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = BackupSettings();
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"restore_settings") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = RestoreSettingsFromBackup();
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"get_security_status") {
                SendResp(id, GetSecurityStatus()); return;
            }
            if (m == L"scan_drive_fast") {
                std::wstring driveStr = arg(4);
                wchar_t drive = driveStr.empty() ? L'C' : driveStr[0];
                return AsyncData(id, [drive]() -> std::wstring {
                    return ScanDriveFast(drive);
                    });
            }
            if (m == L"quick_action") {
                std::wstring act = arg(4);
                auto self = shared_from_this();
                std::thread([self, id, act]() {
                    bool ok = false;
                    try {
                        bool isSys = (g_activeDrive.load() == g_systemDriveLetter);
                        if (act == L"cleanup_temp") { CleanTemp(); ok = true; }
                        else if (act == L"flush_dns") { FlushDns(); ok = true; }
                        else if (act == L"defrag_memory") { int freed = 0; DefragmentMemory(freed); ok = true; }
                        else if (act == L"clear_standby") { PurgeStandbyEnhanced(); ok = true; }
                        else if (act == L"one_click_maintenance") { CleanCacheEnhanced(); PurgeStandbyEnhanced(); ok = true; }
                        else if (act == L"game_booster_on") { if (!isSys) ok = false; else { GameBooster(true); ok = true; } }
                        else if (act == L"game_booster_off") { if (!isSys) ok = false; else { GameBooster(false); ok = true; } }
                        else if (act == L"game_mode_on") { SetGameMode(true); ok = true; }
                        else if (act == L"game_mode_off") { SetGameMode(false); ok = true; }
                        else if (act == L"disable_telemetry") { if (!isSys) ok = false; else { DisableTelemetry(); ok = true; } }
                        else if (act == L"disable_updates") { if (!isSys) ok = false; else { RunCmd(L"sc config wuauserv start= disabled", L"Disable WU"); ok = true; } }
                        else if (act == L"enable_updates") { if (!isSys) ok = false; else { RunCmd(L"sc config wuauserv start= auto", L"Enable WU"); ok = true; } }
                        else if (act == L"power_ultimate") { if (!isSys) ok = false; else { ok = RunCmd(std::wstring(L"powercfg /setactive ") + ULTIMATE_GUID, L"Power Ultimate"); } }
                        else if (act == L"power_high") { if (!isSys) ok = false; else { ok = RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"Power High"); } }
                        else if (act == L"power_balanced") { if (!isSys) ok = false; else { ok = RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Power Balanced"); } }
                        else if (act == L"ctx_menu_classic") { if (!isSys) ok = false; else { SetClassicContextMenu(true); ok = true; } }
                        else if (act == L"ctx_menu_modern") { if (!isSys) ok = false; else { SetClassicContextMenu(false); ok = true; } }
                        else if (act == L"disable_defender") { if (!isSys) ok = false; else { DisableDefender(); ok = true; } }
                        else if (act == L"deep_services") { if (!isSys) ok = false; else { DisableDeepServices(); ok = true; } }
                        else if (act == L"disable_fso") { if (!isSys) ok = false; else { OptimizeFSO(true); ok = true; } }
                        else if (act == L"winget_upgrade_all") { if (!isSys) ok = false; else { UpgradeAllWinget(); ok = true; } }
                        else if (act == L"set_fast_dns") { if (!isSys) ok = false; else { SetFastDNS(); ok = true; } }
                        else if (act == L"optimize_ssd_trim") { if (!isSys) ok = false; else { OptimizeSSDTrim(); ok = true; } }
                        else if (act == L"adv_priority_sep") { SetWin32PrioritySeparation(0x26); ok = true; }
                        else if (act == L"adv_disable_dynamictick") { if (!isSys) ok = false; else { DisableDynamictick(); ok = true; } }
                        else if (act == L"adv_disable_net_throttle") { if (!isSys) ok = false; else { DisableNetworkThrottling(); ok = true; } }
                        else if (act == L"adv_disable_nagle") { if (!isSys) ok = false; else { DisableNagle(); ok = true; } }
                        else if (act == L"adv_clean_winsxs") { if (!isSys) ok = false; else { CleanWinSxS(); ok = true; } }
                        else if (act == L"clean_do_cache") { if (!isSys) ok = false; else { CleanDeliveryOptimization(); ok = true; } }
                        else if (act == L"launch_afterburner") { LaunchAfterburnerHidden(); ok = true; }
                        else if (act == L"overlay_toggle") { g_overlay.Toggle(); ok = true; }
                        else if (act == L"usb_no_suspend") { if (!isSys) ok = false; else { ToggleUSBSelectiveSuspend(false); ok = true; } }
                        else if (act == L"usb_suspend_ok") { if (!isSys) ok = false; else { ToggleUSBSelectiveSuspend(true); ok = true; } }
                        else if (act == L"pcie_max_perf") { if (!isSys) ok = false; else { SetPCIELinkState(true); ok = true; } }
                        else if (act == L"pcie_power_save") { if (!isSys) ok = false; else { SetPCIELinkState(false); ok = true; } }
                        else if (act == L"disable_paging") {
                            if (!isSys) ok = false;
                            else {
                                MEMORYSTATUSEX ms{};
                                ms.dwLength = sizeof(ms);
                                GlobalMemoryStatusEx(&ms);
                                if (ms.ullTotalPhys < 32ULL * 1024 * 1024 * 1024) {
                                    Print(L"[-] RAM < 32GB. Отключение файла подкачки заблокировано.");
                                    self->PostUI([self]() { MessageBoxW(self->hwnd, L"У вас меньше 32 ГБ ОЗУ.\nОперация отменена.", L"Защита", MB_ICONWARNING); });
                                }
                                else {
                                    ok = RunCmd(L"wmic computersystem set AutomaticManagedPagefile=False", L"Disable Paging");
                                    if (ok) ok = RunCmd(L"wmic pagefileset where name=\"C:\\\\pagefile.sys\" delete", L"Delete pagefile");
                                }
                            }
                        }
                        else if (act == L"disable_mem_comp") { ok = RunCmd(L"powershell -NoProfile -Command \"Disable-MMAgent -MemoryCompression\"", L"Disable Mem Comp"); }
                        else if (act == L"toggle_cache_timer") { g_cacheTimerEnabled = !g_cacheTimerEnabled.load(); ok = true; }
                        else if (act == L"check_dpc") { float d = g_dpcIsrTime.load(); if (d > 5.0f) { Print(L"[-] Высокие задержки DPC/ISR: " + Fmt(L"%.2f", d) + L"%"); } else { Print(L"[+] DPC/ISR в норме."); } ok = true; }
                        else if (act == L"net_rss_on") { ApplyNetworkTweaks(true); ok = true; }
                        else if (act == L"net_rss_off") { ApplyNetworkTweaks(false); ok = true; }
                        else if (act == L"disable_vbs") { DisableVBS(); ok = true; }
                        else if (act == L"disable_etw") { DisableETW(); ok = true; }
                        else if (act == L"remove_spyware") { PerformLiteDebloat(); ok = true; }
                        else if (act == L"nvidia_max_perf") { ApplyNvidiaPState(0); ok = true; }
                        else if (act == L"clear_kernel_pools") { ClearKernelPools(); ok = true; }
                        else if (act == L"run_ddu") { std::wstring res = RunDDU(); if (res.find(L"true") != std::wstring::npos) ok = true; }
                    }
                    catch (...) { ok = false; }
                    self->PostUI([self, id, ok]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}"); });
                    }).detach();
                return;
            }
            if (m == L"run_cleanup") {
                std::vector<std::wstring> ops;
                for (size_t i = 4; i < parts.size(); ++i) ops.push_back(parts[i]);
                auto self = shared_from_this();
                std::thread([self, id, ops]() {
                    bool allOk = true;
                    wchar_t active = g_activeDrive.load();
                    try {
                        for (auto& o : ops) {
                            if (o == L"temp") CleanTemp();
                            else if (o == L"prefetch") CleanPrefetch();
                            else if (o == L"recycle") CleanRecycleForDrive(active);
                            else if (o == L"thumbs") CleanThumbnails();
                            else if (o == L"shader") CleanShaderCache();
                            else if (o == L"dns") FlushDns();
                            else if (o == L"do") { if (active == g_systemDriveLetter) CleanDeliveryOptimization(); }
                            else if (o == L"launchers") CleanGameLaunchers();
                            else if (o == L"browsers") CleanBrowserCaches();
                            else if (o == L"chats") CleanChatCaches();
                            else if (o == L"dumps") CleanMemoryDumps();
                            else if (o == L"minecraft") CleanMinecraftCache();
                            else if (o == L"print") CleanPrintQueue();
                            else if (o == L"icons") CleanIconCache();
                        }
                    }
                    catch (...) { allOk = false; }
                    self->PostUI([self, id, allOk]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":" + std::wstring(allOk ? L"true" : L"false") + L"}"); });
                    }).detach();
                return;
            }
            if (m == L"load_programs") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ScanProgramsDetailed();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size() && i < 200; ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(v[i].name) + L",\"uninstall\":" + JsonStr(v[i].uninstallStr) + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            SendResp(id, L"{\"ok\":false}");
        }
        catch (...) { if (id >= 0) SendResp(id, L"{\"ok\":false}"); }
    }
};

static std::shared_ptr<App> g_app;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp == TRUE) {
            if (IsZoomed(hWnd)) {
                auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
                MONITORINFO mi = { sizeof(mi) };
                if (GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi)) {
                    p->rgrc[0] = mi.rcWork;
                }
            }
            return 0;
        }
        break;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = 900; mmi->ptMinTrackSize.y = 600;
        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        return 0;
    }
    case WM_SIZE:
        if (g_app && wp != SIZE_MINIMIZED) {
            int w = LOWORD(lp), h = HIWORD(lp);
            if (w > 0 && h > 0) g_app->Resize(w, h);
        }
        if (wp == SIZE_MINIMIZED) {
            if (g_app) g_app->Suspend();
        }
        else if (wp == SIZE_RESTORED || wp == SIZE_MAXIMIZED) {
            if (g_app) g_app->Resume();
        }
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            g_overlay.Draw();
            QUERY_USER_NOTIFICATION_STATE st;
            bool inGame = false;
            if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
                if (st == QUNS_RUNNING_D3D_FULL_SCREEN || st == QUNS_PRESENTATION_MODE) inGame = true;
            }
            if (!inGame) {
                HWND fg = GetForegroundWindow();
                if (fg) {
                    DWORD pid;
                    GetWindowThreadProcessId(fg, &pid);
                    if (pid > 0) {
                        UniqueHandle hProc(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
                        if (hProc) {
                            wchar_t path[MAX_PATH];
                            DWORD sz = MAX_PATH;
                            if (QueryFullProcessImageNameW(hProc.get(), 0, path, &sz)) {
                                std::wstring exe = path;
                                std::transform(exe.begin(), exe.end(), exe.begin(), ::towlower);
                                if (exe.find(L"game") != std::wstring::npos) inGame = true;
                            }
                        }
                    }
                }
            }
            g_overlay.UpdateVisibility(inGame);
            return 0;
        }
        if (wp == 2) {
            return 0;
        }
        break;
    case WM_HOTKEY:
        if (wp == 1) { g_overlay.Toggle(); }
        else if (wp == 2) { PurgeStandbyEnhanced(); }
        else if (wp == 3) { CleanTemp(); FlushDns(); }
        return 0;
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) { ShowWindow(hWnd, IsIconic(hWnd) ? SW_RESTORE : SW_SHOW); SetForegroundWindow(hWnd); }
        return 0;
    case WM_APP_RUNUI: {
        auto* fn = reinterpret_cast<std::function<void()>*>(lp);
        if (fn) { try { (*fn)(); } catch (...) {} delete fn; }
        return 0;
    }
    case WM_CLOSE:
        std::thread([]() { BackupSettings(); }).detach();
        if (g_app) { g_app->Shutdown(); }
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

static bool IsAdmin() {
    BOOL a = FALSE; PSID id = nullptr; SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &id)) {
        CheckTokenMembership(nullptr, id, &a); FreeSid(id);
    }
    return a != FALSE;
}

static bool RelaunchAsAdmin(LPWSTR lpCmdLine) {
    wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; sei.lpFile = exe; sei.nShow = SW_SHOWNORMAL;

    std::wstring cmd = lpCmdLine;
    if (!cmd.empty()) {
        if (cmd.front() == L'"') {
            auto pos = cmd.find(L'"', 1);
            if (pos != std::wstring::npos) cmd = cmd.substr(pos + 1);
        }
        else {
            auto pos = cmd.find(L' ');
            if (pos != std::wstring::npos) cmd = cmd.substr(pos + 1);
            else cmd.clear();
        }
    }
    if (!cmd.empty()) sei.lpParameters = cmd.c_str();

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() == ERROR_CANCELLED) {
            MessageBoxW(nullptr, L"Запуск от имени администратора отменен.", L"Optimizer Pro", MB_ICONERROR);
        }
        return false;
    }
    return true;
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Global\\OptimizerPro_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Optimizer Pro already running.", L"Info", MB_ICONINFORMATION);
        return 0;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool skipAdmin = false;
    bool startMinimized = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--no-admin") == 0) skipAdmin = true;
        else if (wcscmp(argv[i], L"--silent") == 0) startMinimized = true;
        else if (wcscmp(argv[i], L"--portable") == 0) g_portableMode = true;
        else if (wcscmp(argv[i], L"--help") == 0) {
            MessageBoxW(nullptr, L"Optimizer Pro CLI:\n--no-admin : без прав админа\n--silent : свернуто в трей\n--portable : portable-режим\n--help : это сообщение", L"Optimizer Pro Help", MB_OK | MB_ICONINFORMATION);
            if (argv) LocalFree(argv);
            return 0;
        }
    }
    if (argv) LocalFree(argv);

    if (!skipAdmin && !IsAdmin()) {
        if (RelaunchAsAdmin(GetCommandLineW())) return 0;
        return 1;
    }
    else if (skipAdmin && !IsAdmin()) {
        MessageBoxW(nullptr, L"ВНИМАНИЕ! Вы запустили программу без прав администратора.\nМногие функции (очистка, службы, реестр) не будут работать.", L"Предупреждение", MB_ICONWARNING);
    }

    EnablePrivilege(SE_TAKE_OWNERSHIP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);
    EnablePrivilege(SE_DEBUG_NAME);
    EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    (void)CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    InitPaths();
    if (!ShowBootstrapper(hInst)) { CoUninitialize(); return 0; }

    g_overlay.Create(hInst);
    LoadOverlaySettings();
    g_overlay.SetPosition(g_overlay.posX.load(), g_overlay.posY.load());
    g_overlay.SetAlpha(g_overlay.alpha.load());

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"OptProWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = CreateLightningIcon(32); wc.hIconSm = CreateLightningIcon(16);
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"OptProWnd", L"Optimizer Pro", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) { CoUninitialize(); return 1; }
    ShowWindow(hWnd, startMinimized ? SW_HIDE : SW_SHOW);

    InitTray(hWnd);

    if (startMinimized) {
        g_nid.uFlags |= NIF_INFO;
        wcsncpy_s(g_nid.szInfoTitle, L"Optimizer Pro", _TRUNCATE);
        wcsncpy_s(g_nid.szInfo, L"Работает в фоновом режиме", _TRUNCATE);
        g_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    }

    if (!g_portableMode.load()) {
        if (!RegisterHotKey(hWnd, 1, MOD_CONTROL | MOD_ALT, 'O')) Print(L"Failed to register Ctrl+Alt+O");
        if (!RegisterHotKey(hWnd, 2, MOD_CONTROL | MOD_ALT, 'M')) Print(L"Failed to register Ctrl+Alt+M");
        if (!RegisterHotKey(hWnd, 3, MOD_CONTROL | MOD_ALT, 'C')) Print(L"Failed to register Ctrl+Alt+C");
    }

    g_stopMonitor = false;
    g_monThread = std::thread(MonitorThread);

    g_app = std::make_shared<App>();
    if (!g_app->Initialize(hWnd)) {
        MessageBoxW(hWnd, L"Failed to initialize WebView2 interface.", L"Error", MB_ICONERROR);
        DestroyWindow(hWnd);
        CoUninitialize();
        return 1;
    }

    SetTimer(hWnd, 1, 1000, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }

    KillTimer(hWnd, 1);
    UnregisterHotKey(hWnd, 1);
    UnregisterHotKey(hWnd, 2);
    UnregisterHotKey(hWnd, 3);
    g_stopMonitor = true;
    if (g_monThread.joinable()) g_monThread.join();

    {
        std::unique_lock<std::mutex> lock(g_asyncMutex);
        g_asyncCv.wait_for(lock, std::chrono::seconds(3), [] { return g_activeAsyncTasks.load() == 0; });
    }

    if (g_app) { g_app->Shutdown(); g_app.reset(); }
    g_overlay.Cleanup();
    CleanupTray();
    CoUninitialize();
    CloseHandle(hMutex);
    return 0;
}
