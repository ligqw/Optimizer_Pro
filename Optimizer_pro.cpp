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
#include <userenv.h>
#include <dwmapi.h>
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
#include <sddl.h>
#include <intrin.h>
#include <immintrin.h>
#include <cwctype>
#include <srrestoreptapi.h>

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
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Sfc.lib")
#pragma comment(lib, "SrClient.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

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

struct Paths { std::wstring SystemRoot, BackupDir, WebView2Dir, ThemesDir, AppDir, LogFile, SystemDrive; };
static Paths g_paths;
static std::mutex g_logMutex;

static std::atomic<double> g_cpuUsage{ 0.0 };
static std::atomic<double> g_gpuUsage{ 0.0 };
static std::atomic<double> g_cpuTemp{ -1.0 };
static std::atomic<double> g_gpuTemp{ -1.0 };
static std::atomic<DWORD>  g_procCount{ 0 };
static std::atomic<bool>   g_stopMonitor{ false };
static std::thread         g_monThread;

static std::atomic<double> g_appCpuUsage{ 0.0 };
static std::atomic<double> g_appGpuUsage{ 0.0 };
static std::atomic<uint64_t> g_appRamMB{ 0 };

static std::atomic<double> g_netDownMbps{ 0.0 };
static std::atomic<double> g_netUpMbps{ 0.0 };

static std::wstring g_pcInfoJson = L"{\"cpu\":\"Loading...\",\"gpu\":\"Loading...\",\"ram\":\"Loading...\"}";
static std::mutex g_pcInfoMutex;

static std::wstring g_diskHealthJson = L"[]";
static std::mutex g_diskHealthMutex;

static std::atomic<wchar_t> g_activeDrive{ L'C' };
static wchar_t g_systemDriveLetter = L'C';

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
    bool cleanFontCache = true;
    bool cleanStoreCache = true;
    bool cleanUpdateCache = true;
    bool clean0ByteFiles = false;
    bool cleanLaunchers = true;
    bool cleanBrowsers = true;
    bool cleanChats = true;
    bool cleanDumps = true;
    int ramAggressiveness = 3;
    bool ramClearStandby = true;
    bool ramFlushModified = true;
    bool ramClearWorkingSets = true;
    bool ramClearFileCache = true;
    bool autoBoost = true;
    bool disableFSO = false;
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

static std::wstring DoubleToJsonStr(double v) {
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

static std::wstring UnescapeJsonString(const std::wstring& j) {
    if (j.size() < 2 || j.front() != L'"' || j.back() != L'"') return j;
    std::wstring o; o.reserve(j.size() - 2);
    for (size_t i = 1; i + 1 < j.size(); ++i) {
        wchar_t c = j[i];
        if (c == L'\\' && i + 2 < j.size()) {
            wchar_t n = j[i + 1];
            if (n == L'u' && i + 5 < j.size()) {
                unsigned v = 0; bool ok = true;
                for (int k = 0; k < 4; ++k) {
                    wchar_t h = j[i + 2 + k]; v <<= 4;
                    if (h >= L'0' && h <= L'9') v |= (h - L'0');
                    else if (h >= L'a' && h <= L'f') v |= (h - L'a' + 10);
                    else if (h >= L'A' && h <= L'F') v |= (h - L'A' + 10);
                    else { ok = false; break; }
                }
                if (ok) { o += (wchar_t)v; i += 5; continue; }
            }
            switch (n) {
            case L'"': case L'\\': case L'/': o += n; i++; continue;
            case L'n': o += L'\n'; i++; continue;
            case L'r': o += L'\r'; i++; continue;
            case L't': o += L'\t'; i++; continue;
            case L'b': o += L'\b'; i++; continue;
            case L'f': o += L'\f'; i++; continue;
            }
        }
        o += c;
    }
    return o;
}

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

    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    if (local.empty()) {
        wchar_t b[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, b))) local = b;
        else local = g_paths.SystemRoot + L"\\Temp";
    }
    g_paths.BackupDir = local + L"\\OptimizerPro";
    g_paths.WebView2Dir = g_paths.BackupDir + L"\\WebView2";
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
        std::ofstream f(g_paths.LogFile, std::ios::app | std::ios::binary);
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
    BOOL ok = CreateProcessW(nullptr, clbuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
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

static bool ClearStandbyList() {
    EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME);
    using NtSetSystemInformation_t = NTSTATUS(WINAPI*)(INT, PVOID, ULONG);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto NtSetSystemInformation = (NtSetSystemInformation_t)GetProcAddress(ntdll, "NtSetSystemInformation");
    if (!NtSetSystemInformation) return false;
    int command = 4;
    NTSTATUS status = NtSetSystemInformation(0x50, &command, sizeof(command));
    return status == 0;
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
            if (g_userSettings.ramFlushModified) {
                int cmd = 3;
                fn(0x50, &cmd, sizeof(cmd));
            }
            if (g_userSettings.ramClearStandby) {
                int cmd = 4;
                fn(0x50, &cmd, sizeof(cmd));
            }
        }
    }

    if (g_userSettings.ramClearFileCache) {
        SetSystemFileCacheSize((SIZE_T)-1, (SIZE_T)-1, 0);
    }

    if (g_userSettings.ramClearWorkingSets) {
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
            double val = (double)procDiff / (double)sysDiff / si.dwNumberOfProcessors * 100.0;
            g_appCpuUsage = std::clamp(val, 0.0, 100.0);
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
    try {
        if (svc) {
            ComPtr<IEnumWbemClassObject> en;
            HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"),
                _bstr_t(L"SELECT FriendlyName, HealthStatus, MediaType, Size, Temperature, Wear FROM MSFT_PhysicalDisk"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
            if (SUCCEEDED(hr) && en) {
                bool first = true;
                for (;;) {
                    if (g_stopMonitor) break;
                    ComPtr<IWbemClassObject> obj; ULONG ret = 0;
                    hr = en->Next(4000, 1, &obj, &ret);
                    if (FAILED(hr) || ret == 0 || !obj) break;
                    VARIANT vName{}, vHealth{}, vMedia{}, vSize{}, vTemp{}, vWear{};
                    VariantInit(&vName); VariantInit(&vHealth); VariantInit(&vMedia); VariantInit(&vSize);
                    VariantInit(&vTemp); VariantInit(&vWear);
                    std::wstring name = L"Unknown Disk", media = L"Unknown";
                    int health = 0, temp = -1, wear = -1; uint64_t sz = 0;
                    if (SUCCEEDED(obj->Get(L"FriendlyName", 0, &vName, nullptr, nullptr)) && vName.vt == VT_BSTR) name = vName.bstrVal;
                    if (SUCCEEDED(obj->Get(L"HealthStatus", 0, &vHealth, nullptr, nullptr)) && vHealth.vt == VT_I4) health = vHealth.lVal;
                    if (SUCCEEDED(obj->Get(L"MediaType", 0, &vMedia, nullptr, nullptr)) && vMedia.vt == VT_I4) {
                        int mt = vMedia.lVal;
                        media = (mt == 3) ? L"HDD" : (mt == 4) ? L"SSD" : (mt == 5) ? L"SCM" : L"Unspecified";
                    }
                    if (SUCCEEDED(obj->Get(L"Size", 0, &vSize, nullptr, nullptr)) && vSize.vt == VT_UI8) sz = vSize.ullVal;
                    if (SUCCEEDED(obj->Get(L"Temperature", 0, &vTemp, nullptr, nullptr)) && (vTemp.vt == VT_I2 || vTemp.vt == VT_UI2)) temp = vTemp.iVal;
                    if (SUCCEEDED(obj->Get(L"Wear", 0, &vWear, nullptr, nullptr)) && (vWear.vt == VT_I2 || vWear.vt == VT_UI2)) wear = vWear.iVal;

                    if (!first) result += L",";
                    result += L"{\"model\":" + JsonStr(name) + L",\"media\":" + JsonStr(media) + L",\"status\":" + std::to_wstring(health) +
                        L",\"size\":" + std::to_wstring(sz) + L",\"temp\":" + std::to_wstring(temp) + L",\"wear\":" + std::to_wstring(wear) + L"}";
                    first = false;
                    VariantClear(&vName); VariantClear(&vHealth); VariantClear(&vMedia); VariantClear(&vSize);
                    VariantClear(&vTemp); VariantClear(&vWear);
                    obj.Reset();
                }
            }
        }
    }
    catch (...) {}
    result += L"]";
    return result;
}

static bool WmiQueryMaxThermalZone(IWbemServices* svc, double& celsius) {
    celsius = -1;
    if (!svc) return false;
    ComPtr<IEnumWbemClassObject> en;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
    if (FAILED(hr) || !en) return false;
    double maxT = 0;
    for (;;) {
        if (g_stopMonitor) break;
        ComPtr<IWbemClassObject> obj; ULONG ret = 0;
        hr = en->Next(4000, 1, &obj, &ret);
        if (FAILED(hr) || ret == 0 || !obj) break;
        VARIANT v{}; VariantInit(&v);
        if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &v, nullptr, nullptr))) {
            double c = -1;
            if (v.vt == VT_I4) c = (v.lVal / 10.0) - 273.15;
            else if (v.vt == VT_UI4) c = (v.ulVal / 10.0) - 273.15;
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
    double Sample() {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0;
        ULONGLONG i = FileTimeToULL(idle), k = FileTimeToULL(kernel), u = FileTimeToULL(user);
        if (first_) { prevIdle_ = i; prevKernel_ = k; prevUser_ = u; first_ = false; return 0.0; }
        ULONGLONG total = (k - prevKernel_) + (u - prevUser_);
        ULONGLONG idleDiff = i - prevIdle_;
        prevIdle_ = i; prevKernel_ = k; prevUser_ = u;
        if (total == 0) return 0.0;
        double val = (double)(total - idleDiff) * 100.0 / (double)total;
        return std::clamp(val, 0.0, 100.0);
    }
private:
    ULONGLONG prevIdle_ = 0, prevKernel_ = 0, prevUser_ = 0;
    bool first_ = true;
};

static double GetGpuUsage() {
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
    if (!st.ready) return -1.0;
    if (PdhCollectQueryData(st.guard.q) != ERROR_SUCCESS) return -1.0;
    DWORD bufSize = 0, itemCount = 0;
    PDH_STATUS s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
    if (s == ERROR_SUCCESS) return 0.0;
    if (s != PDH_MORE_DATA || bufSize == 0) return -1.0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        AlignedBuffer buf(bufSize); DWORD bytes = bufSize, count = itemCount;
        s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bytes, &count, buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>());
        if (s == PDH_MORE_DATA) { bufSize = bytes; continue; }
        if (s != ERROR_SUCCESS) return -1.0;
        auto* items = buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>();
        double sum = 0.0;
        for (DWORD i = 0; i < count; ++i) {
            PDH_STATUS cs = items[i].FmtValue.CStatus;
            if (cs == PDH_CSTATUS_VALID_DATA || cs == PDH_CSTATUS_NEW_DATA) sum += items[i].FmtValue.doubleValue;
        }
        return std::clamp(sum, 0.0, 100.0);
    }
    return -1.0;
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

static bool GetTemperatures(IWbemServices* svc, double& cpuTemp, double& gpuTemp) {
    cpuTemp = -1; gpuTemp = -1;
    try {
        if (svc) {
            double t = -1;
            if (WmiQueryMaxThermalZone(svc, t)) { cpuTemp = t; return true; }
        }
    }
    catch (...) {}
    return false;
}

static void UpdateNetworkSpeed() {
    static ULONGLONG lastTick = 0;
    static ULONGLONG lastIn = 0, lastOut = 0;
    PMIB_IFTABLE table = nullptr;
    DWORD size = 0;
    if (GetIfTable(nullptr, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER) return;
    table = (PMIB_IFTABLE)malloc(size);
    if (!table) return;
    if (GetIfTable(table, &size, FALSE) == NO_ERROR) {
        ULONGLONG totalIn = 0, totalOut = 0;
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            totalIn += table->table[i].dwInOctets;
            totalOut += table->table[i].dwOutOctets;
        }
        ULONGLONG now = GetTickCount64();
        if (lastTick > 0) {
            double sec = (now - lastTick) / 1000.0;
            if (sec > 0.1) {
                if (totalIn >= lastIn) g_netDownMbps = ((totalIn - lastIn) * 8.0 / 1e6) / sec;
                else g_netDownMbps = 0.0;
                if (totalOut >= lastOut) g_netUpMbps = ((totalOut - lastOut) * 8.0 / 1e6) / sec;
                else g_netUpMbps = 0.0;
            }
        }
        lastTick = now; lastIn = totalIn; lastOut = totalOut;
    }
    free(table);
}

static void GameBooster(bool enable);
static bool g_autoBoostActive = false;

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
        int tick = 0;
        bool wasInGame = false;
        while (!g_stopMonitor) {
            sleepStop(1000);
            if (g_stopMonitor) break;
            g_cpuUsage = sampler.Sample();
            g_gpuUsage = GetGpuUsage();
            UpdateNetworkSpeed();
            if ((++tick % 2) == 0) g_procCount = CountProcesses();
            UpdateSelfMetrics();

            if ((tick % 60) == 0) {
                std::wstring info = GetPcInfo(wmiCim.get());
                { std::lock_guard<std::mutex> lk(g_pcInfoMutex); g_pcInfoJson = info; }

                std::wstring dh = GetDiskHealthJson(wmiStorage.get());
                { std::lock_guard<std::mutex> lk(g_diskHealthMutex); g_diskHealthJson = dh; }

                double ct = -1, gt = -1;
                if (GetTemperatures(wmiWmi.get(), ct, gt)) { g_cpuTemp = ct; g_gpuTemp = gt; }
            }

            QUERY_USER_NOTIFICATION_STATE st;
            bool inGame = false;
            if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
                if (st == QUNS_RUNNING_D3D_FULL_SCREEN || st == QUNS_PRESENTATION_MODE) inGame = true;
            }
            if (g_userSettings.autoBoost && inGame != wasInGame) {
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
    std::wstring path = L"C:\\Windows\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache\\*";
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
        for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
            it != fs::recursive_directory_iterator() && removed < 500; it.increment(ec)) {
            if (g_stopMonitor) break;
            if (ec) { ec.clear(); continue; }
            try {
                if (it->is_regular_file(ec) && !ec && it->file_size(ec) == 0) {
                    fs::remove(it->path(), ec);
                    if (!ec) ++removed;
                }
            }
            catch (...) {}
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

static void CleanCacheEnhanced() {
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
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
    const wchar_t* svcs[] = { L"SysMain", L"WSearch", L"DiagTrack" };
    for (auto s : svcs) {
        if (enable) RunCmd(L"sc stop " + std::wstring(s) + L" >nul 2>&1", L"", false, 30);
        else RunCmd(L"sc start " + std::wstring(s) + L" >nul 2>&1", L"", false, 30);
    }
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
            if (!ec && p.path().extension() == L".item") games.push_back({ p.path().stem().wstring(), p.path().wstring(), L"Epic" });
        }
    }

    CmdResult r = RunCmdImpl(L"powershell -NoProfile -Command \"Get-AppxPackage | Where-Object { $_.Name -match 'Xbox|Game|Steam|Epic' } | Select-Object -ExpandProperty InstallLocation\"", 30);
    if (!r.out.empty()) {
        std::istringstream iss(r.out);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty() || line.find("InstallLocation") != std::string::npos) continue;
            std::wstring wpath = Utf8ToWide(line);
            if (!wpath.empty() && fs::exists(wpath, ec)) {
                games.push_back({ fs::path(wpath).filename().wstring(), wpath, L"UWP" });
            }
        }
    }

    return games;
}

static bool CompressGameFolder(const std::wstring& path) {
    if (path.empty()) return false;
    std::wstring cmd = L"compact.exe /c /s /a /i /f /exe:LZX \"" + path + L"\"";
    Print(L"[+] Сжатие папки: " + path + L" (может занять время)...");
    return RunCmd(cmd, L"Сжатие LZX", false, 3600);
}

static bool InstallAppWinget(const std::wstring& packageId) {
    std::wstring cmd = L"winget install --id " + packageId + L" -e --accept-package-agreements --accept-source-agreements --silent";
    return RunCmd(cmd, L"Installing " + packageId, false, 1200, { 0 });
}

static void ActivateWindowsOffice() {
    std::wstring cmd = L"powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"irm https://get.activated.win | iex\"";
    RunCmd(cmd, L"Activation", false, 600);
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
    size_t cap = static_cast<size_t>(std::min<uint64_t>(size, 65536));
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
    if (id == L"clean_thumbs") { CleanThumbnails(); return true; }
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
    if (id == L"clean_dns") { FlushDns(); return true; }
    if (id == L"clean_standby") { PurgeStandbyEnhanced(); return true; }
    if (id == L"power_high") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"Power High", false); return true;
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

struct AutoEntry { std::wstring name, command, location, keyPath, filePath; bool isRegistry = true; bool isHKLM = false; };
static std::vector<AutoEntry> ListAutostart() {
    std::vector<AutoEntry> out;
    struct RegLoc { HKEY hive; const wchar_t* path; const wchar_t* loc; bool hklm; };
    const RegLoc locs[] = {
        { HKEY_CURRENT_USER,  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKCU", false },
        { HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKLM", true },
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
            if (type != REG_SZ || size == 0 || size > 1024 * 1024) continue;
            std::wstring cmd(size / sizeof(wchar_t) + 2, L'\0');
            if (RegQueryValueExW(key.get(), name, nullptr, &type, (LPBYTE)cmd.data(), &size) != ERROR_SUCCESS) continue;
            cmd.resize(wcslen(cmd.c_str()));
            AutoEntry e; e.name = name; e.command = cmd; e.location = loc.loc; e.isRegistry = true; e.keyPath = loc.path; e.isHKLM = loc.hklm;
            out.push_back(std::move(e));
        }
    }
    return out;
}

static bool ToggleAutostart(const std::wstring& name, const std::wstring& loc, const std::wstring& act) {
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
        }
        RegCloseKey(hRun);
        return true;
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
        }
        RegCloseKey(hBak);
        return true;
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
    UniqueHandle hFile(CreateFileW(tmp.path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile) return false;
    std::vector<char> buf(65536); DWORD read = 0;
    while (WinHttpReadData(hRequest.get(), buf.data(), (DWORD)buf.size(), &read) && read > 0) {
        DWORD written = 0; WriteFile(hFile.get(), buf.data(), read, &written, nullptr);
    }
    hFile.reset();
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
        HBRUSH br = CreateSolidBrush(RGB(18, 21, 30)); FillRect(hdc, &rc, br); DeleteObject(br); return 1;
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
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(g_nid.szTip, L"Optimizer Pro", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void CleanupTray() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

class GameOverlay {
public:
    HWND hwnd = nullptr;
    std::atomic<bool> userEnabled{ true }, isVisible{ false };
    std::atomic<bool> showCpu{ true }, showGpu{ true }, showRam{ true }, showTemp{ true };
    std::atomic<int> posX{ 10 }, posY{ 10 }, alpha{ 255 };
    HFONT hFont = nullptr;

    void Create(HINSTANCE hInst) {
        WNDCLASSEXW wc = { sizeof(wc) }; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = hInst; wc.lpszClassName = L"GameOverlayWnd";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"GameOverlayWnd", L"Overlay", WS_POPUP, posX.load(), posY.load(), 280, 180, nullptr, nullptr, hInst, nullptr);
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

        auto cleanup = [this, hdc]() { ReleaseDC(hwnd, hdc); };

        RECT rc; GetClientRect(hwnd, &rc); FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(0, 255, 100));
        HGDIOBJ old = hFont ? SelectObject(hdc, hFont) : nullptr;
        int y = 10; wchar_t buf[128] = {};
        if (showCpu) { swprintf_s(buf, L"CPU: %.1f%%", g_cpuUsage.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showGpu) { swprintf_s(buf, L"GPU: %.1f%%", g_gpuUsage.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showRam) { MEMORYSTATUSEX ms = { sizeof(ms) }; GlobalMemoryStatusEx(&ms); swprintf_s(buf, L"RAM: %llu MB", static_cast<unsigned long long>(ms.ullAvailPhys / (1024 * 1024))); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showTemp && g_cpuTemp.load() > 0) { swprintf_s(buf, L"TEMP: %.1f C", g_cpuTemp.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (old) SelectObject(hdc, old);

        cleanup();
    }
    void Toggle() { userEnabled = !userEnabled; if (!userEnabled && hwnd) { ShowWindow(hwnd, SW_HIDE); isVisible = false; } }
    void UpdateVisibility(bool inGame) {
        bool should = userEnabled && inGame;
        if (isVisible != should) { isVisible = should; if (hwnd) ShowWindow(hwnd, isVisible ? SW_SHOWNA : SW_HIDE); }
    }
};

static GameOverlay g_overlay;

static void LoadOverlaySettings() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\OptimizerPro\\Overlay", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0; DWORD sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"PosX", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.posX = val;
        sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"PosY", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.posY = val;
        sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"Alpha", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.alpha = val;
        RegCloseKey(hKey);
    }
}

static void SaveOverlaySettings() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\OptimizerPro\\Overlay", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD px = g_overlay.posX.load();
        DWORD py = g_overlay.posY.load();
        DWORD al = g_overlay.alpha.load();
        RegSetValueExW(hKey, L"PosX", 0, REG_DWORD, (const BYTE*)&px, sizeof(px));
        RegSetValueExW(hKey, L"PosY", 0, REG_DWORD, (const BYTE*)&py, sizeof(py));
        RegSetValueExW(hKey, L"Alpha", 0, REG_DWORD, (const BYTE*)&al, sizeof(al));
        RegCloseKey(hKey);
    }
}

static const char* HTML_UI = R"HTML(<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8"><title>Optimizer Pro</title><style>
@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&family=Rajdhani:wght@400;600;700&display=swap');
:root{--bg:#050508;--surface:#0f111a;--border:#22283a;--text:#e2e8f0;--muted:#64748b;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(15,17,26,.85);--bg-img:none}
body[data-theme="black"]{--bg:#000000;--surface:#111111;--border:#333333;--text:#ffffff;--muted:#888888;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(0,0,0,.9);--bg-img:none}
body[data-theme="white"]{--bg:#ffffff;--surface:#f5f5f5;--border:#e0e0e0;--text:#000000;--muted:#666666;--accent:#ff0055;--accent2:#0088ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(255,255,255,.9);--bg-img:none}
body[data-theme="midnight"]{--bg:#050508;--surface:#0f111a;--border:#22283a;--text:#e2e8f0;--muted:#64748b;--accent:#ff0055;--accent2:#00f0ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(15,17,26,.85);--bg-img:none}
body[data-theme="charcoal"]{--bg:#0a0a0c;--surface:rgba(21,21,25,.75);--border:#28282f;--text:#e0e0e4;--muted:#808090;--accent:#ff0055;--accent2:#00e8ff;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(21,21,25,.75);--bg-img:url('http://optpro.local/themes/charcoal.jpg')}
body[data-theme="ocean"]{--bg:#001020;--surface:rgba(0,26,46,.75);--border:#1a3a5a;--text:#c8e0f0;--muted:#5a80a0;--accent:#0088ff;--accent2:#00e8d0;--success:#00d088;--error:#ff4466;--warn:#ffaa00;--glass:rgba(0,26,46,.75);--bg-img:url('http://optpro.local/themes/ocean.jpg')}
body[data-theme="neon"]{--bg:#08000f;--surface:rgba(21,8,37,.75);--border:#3a1050;--text:#ffe0ff;--muted:#a080c0;--accent:#ff00aa;--accent2:#00ffdd;--success:#00ff88;--error:#ff0055;--warn:#ffcc00;--glass:rgba(21,8,37,.75);--bg-img:url('http://optpro.local/themes/neon.jpg')}
body[data-theme="void"]{--bg:#000003;--surface:rgba(15,0,30,.6);--border:rgba(139,0,255,.35);--text:#e8dcff;--muted:#8878b8;--accent:#8b00ff;--accent2:#00e8ff;--success:#00ff88;--error:#ff0080;--warn:#ff9900;--glass:rgba(20,5,45,.6);--bg-img:url('http://optpro.local/themes/void.jpg')}
body[data-theme="forest"]{--bg:#001008;--surface:rgba(0,26,16,.75);--border:#0a3a20;--text:#c8f0d0;--muted:#609080;--accent:#00aa55;--accent2:#88ff00;--success:#00ff77;--error:#ff3366;--warn:#ffaa22;--glass:rgba(0,26,16,.75);--bg-img:url('http://optpro.local/themes/forest.jpg')}
body[data-theme="crimson"]{--bg:#100005;--surface:rgba(26,0,8,.75);--border:#3a0a15;--text:#f0d0d8;--muted:#a06070;--accent:#ff0033;--accent2:#ff6600;--success:#00bb55;--error:#ff0044;--warn:#ff8800;--glass:rgba(26,0,8,.75);--bg-img:url('http://optpro.local/themes/crimson.jpg')}
body[data-theme="ice"]{--bg:#001015;--surface:rgba(0,26,34,.75);--border:#0a3040;--text:#c8f0ff;--muted:#6090a0;--accent:#00aaff;--accent2:#aaffff;--success:#00ffbb;--error:#ff4488;--warn:#ffcc44;--glass:rgba(0,26,34,.75);--bg-img:url('http://optpro.local/themes/ice.jpg')}
body[data-theme="sunset"]{--bg:#100508;--surface:rgba(26,10,16,.75);--border:#3a1520;--text:#f0d8c0;--muted:#a08060;--accent:#ff6600;--accent2:#ff00aa;--success:#00cc88;--error:#ff2244;--warn:#ffaa00;--glass:rgba(26,10,16,.75);--bg-img:url('http://optpro.local/themes/sunset.jpg')}
body[data-theme="muted-light"]{--bg:#1a1a1f;--surface:#252530;--border:#3a3a48;--text:#d8d8e0;--muted:#8890a0;--accent:#ff0066;--accent2:#00c8e0;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(37,37,48,.85);--bg-img:none}
*{box-sizing:border-box;margin:0;padding:0}html,body{height:100%}
body{font-family:'Rajdhani',sans-serif;background-color:var(--bg);color:var(--text);overflow:hidden;font-size:15px;user-select:none;transition:background-color .3s,color .3s; background-image:var(--bg-img), linear-gradient(rgba(255, 0, 85, 0.05) 1px, transparent 1px), linear-gradient(90deg, rgba(255, 0, 85, 0.05) 1px, transparent 1px); background-size:cover, 30px 30px, 30px 30px; background-position:center, top left, top left; background-attachment:fixed, fixed, fixed; position:relative; isolation:isolate;}
body[data-theme="void"]::before{content:'';position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);width:600px;height:600px;border-radius:50%;background:radial-gradient(circle,#000 0%,#000 25%,#1a0030 32%,transparent 68%);box-shadow:0 0 120px 60px rgba(139,0,255,.35),0 0 240px 120px rgba(0,232,255,.15);pointer-events:none;z-index:0;animation:blackhole 24s ease-in-out infinite;opacity:0.6}
@keyframes blackhole{0%,100%{transform:translate(-50%,-50%) rotate(0deg) scale(1)}50%{transform:translate(-50%,-50%) rotate(180deg) scale(1.15)}}
body[data-theme="void"]::after{content:'';position:fixed;inset:0;background:radial-gradient(ellipse at center,transparent 0%,transparent 40%,rgba(139,0,255,.08) 70%,rgba(0,0,0,.7) 100%);pointer-events:none;z-index:0;animation:voidpulse 8s ease-in-out infinite}
@keyframes voidpulse{0%,100%{opacity:.6}50%{opacity:1}}
.bubble{position:fixed;border-radius:50%;background:radial-gradient(circle at 30% 30%,rgba(139,0,255,.35),rgba(0,232,255,.15),transparent);border:1px solid rgba(139,0,255,.25);animation:bubble-rise linear infinite;pointer-events:none;z-index:1;backdrop-filter:blur(2px);box-shadow:0 0 20px rgba(139,0,255,.3)}
@keyframes bubble-rise{0%{transform:translateY(100vh) scale(.5);opacity:0}10%{opacity:.8}90%{opacity:.8}100%{transform:translateY(-100vh) scale(1.3);opacity:0}}
h1,h2,.logo,.titlebar-text{font-family:'Orbitron',sans-serif;text-transform:uppercase;letter-spacing:1px;}
.titlebar{height:36px;background:var(--surface);display:flex;justify-content:space-between;align-items:center;border-bottom:2px solid var(--border);transition:background .3s;position:relative;backdrop-filter:blur(10px);z-index:100;}
.titlebar::after{content:'';position:absolute;bottom:-2px;left:0;width:100%;height:2px;background:linear-gradient(90deg,var(--accent),var(--accent2));box-shadow:0 0 10px var(--accent);animation:titleglow 4s ease-in-out infinite}
@keyframes titleglow{0%,100%{opacity:.7}50%{opacity:1;filter:brightness(1.4)}}
.titlebar-text{padding-left:15px;font-weight:900;font-size:14px;display:flex;align-items:center;gap:8px;color:var(--accent);text-shadow:0 0 8px rgba(255,0,85,.6);}
.titlebar-controls{display:flex;height:100%}
.tb-btn{width:46px;height:100%;background:transparent;border:none;color:var(--muted);cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:16px;transition:background .15s,color .15s}
.tb-btn:hover{background:var(--border);color:var(--text)}.tb-btn.close:hover{background:var(--accent);color:#fff;box-shadow:0 0 15px var(--accent);}
.app{display:grid;grid-template-columns:240px 1fr;height:calc(100vh - 36px);transition:grid-template-columns .3s;position:relative;z-index:5;}
.sidebar{backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);background:var(--glass);border-right:1px solid var(--border);padding:20px 0;display:flex;flex-direction:column;overflow-y:auto;overflow-x:hidden;transition:all .3s}
.logo{padding:0 20px 20px;font-size:20px;font-weight:900;display:flex;align-items:center;gap:10px;color:var(--text);white-space:nowrap;overflow:hidden;border-bottom:1px solid var(--border);margin-bottom:10px;text-shadow:0 0 10px rgba(255,255,255,.3);}
.nav-item{padding:12px 20px;cursor:pointer;color:var(--muted);border-left:4px solid transparent;display:flex;gap:12px;align-items:center;transition:all .2s;white-space:nowrap;overflow:hidden;font-size:16px;font-weight:600;clip-path:polygon(0 0,100% 0,100% 80%,95% 100%,0 100%);}
.nav-item svg{flex-shrink:0;width:20px;height:20px}
.nav-item:hover,.nav-item.active{background:rgba(255,0,85,.1);color:var(--text);border-left-color:var(--accent);text-shadow:0 0 8px var(--accent);}
.nav-spacer{flex:1}
.main{display:flex;flex-direction:column;overflow:hidden}
.content{flex:1;overflow-y:auto;padding:30px 40px;scrollbar-width:thin;position:relative;z-index:5;}
::-webkit-scrollbar{width:10px;height:10px}::-webkit-scrollbar-track{background:transparent}::-webkit-scrollbar-thumb{background:var(--accent);border-radius:5px;box-shadow:0 0 5px var(--accent);}
.tab{display:none;animation:fadeIn .35s ease}.tab.active{display:block}
@keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-3px)}}
h1{font-size:28px;margin-bottom:8px;color:var(--accent2);text-shadow:0 0 10px rgba(0,240,255,.5);}h2{font-size:16px;margin:24px 0 14px;color:var(--muted);border-bottom:1px solid var(--border);padding-bottom:8px;}
.subtitle{color:var(--muted);margin-bottom:24px;font-size:15px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:16px;margin-bottom:24px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:18px;position:relative;overflow:hidden;transition:transform .2s,box-shadow .2s;clip-path:polygon(0 0,100% 0,100% calc(100% - 15px),calc(100% - 15px) 100%,0 100%);backdrop-filter:blur(12px)}
.card:hover{transform:translateY(-2px);box-shadow:0 0 15px rgba(0,240,255,.2);border-color:var(--accent2);}
.card::before{content:'';position:absolute;top:0;left:0;width:100%;height:3px;background:linear-gradient(90deg,var(--accent),var(--accent2));}
.card-label{color:var(--muted);font-size:13px;text-transform:uppercase;font-weight:700;margin-bottom:8px}
.card-value{font-size:26px;font-weight:700;font-family:'Orbitron';}
.card-value.good{color:var(--success)}
.section{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:24px;margin-bottom:16px;position:relative;backdrop-filter:blur(12px)}
.section::after{content:'';position:absolute;top:0;right:0;width:10px;height:10px;border-top:2px solid var(--accent);border-right:2px solid var(--accent);}
button{background:var(--surface);color:var(--accent2);border:1px solid var(--accent2);padding:12px 24px;border-radius:4px;font-weight:700;cursor:pointer;display:inline-flex;align-items:center;gap:8px;transition:all .2s;font-size:14px;text-transform:uppercase;clip-path:polygon(10px 0,100% 0,100% calc(100% - 10px),calc(100% - 10px) 100%,0 100%,0 10px);backdrop-filter:blur(5px)}
button:hover{background:var(--accent2);color:#000;box-shadow:0 0 20px rgba(0,240,255,.6);}
button:active{transform:scale(.97)}
button.secondary{border-color:var(--border);color:var(--muted);}button.secondary:hover{border-color:var(--text);color:var(--text);background:transparent;box-shadow:none;}
button.danger{border-color:var(--accent);color:var(--accent);}button.danger:hover{background:var(--accent);color:#fff;box-shadow:0 0 20px rgba(255,0,85,.6);}
button.success{border-color:var(--success);color:var(--success);}button.success:hover{background:var(--success);color:#fff;box-shadow:0 0 20px rgba(16,185,129,.6);}
button.loading{opacity:.7;pointer-events:none}
.check-item{display:flex;align-items:center;padding:12px 0;border-bottom:1px solid var(--border);cursor:pointer}
.check-item label{margin-left:12px;cursor:pointer;flex:1;font-weight:600;font-size:16px;}
input[type=checkbox]{-webkit-appearance:none;appearance:none;width:20px;height:20px;border:2px solid var(--border);border-radius:2px;cursor:pointer;position:relative;transition:all .2s;background:var(--bg);flex-shrink:0}
input[type=checkbox]:checked{background:var(--accent);border-color:var(--accent);box-shadow:0 0 10px var(--accent);}
input[type=checkbox]:checked::after{content:'';position:absolute;left:5px;top:2px;width:6px;height:10px;border:solid #fff;border-width:0 2px 2px 0;transform:rotate(45deg)}
input[type=text],input[type=number],select,textarea{padding:10px 14px;background:var(--bg);border:1px solid var(--border);border-radius:4px;color:var(--text);font-size:15px;outline:none;font-family:'Rajdhani';transition:border .2s;}
input[type=text]:focus,input[type=number]:focus,select:focus,textarea:focus{border-color:var(--accent2);box-shadow:0 0 10px rgba(0,240,255,.2);}
input[type=range]{-webkit-appearance:none;appearance:none;height:6px;background:var(--border);border-radius:3px;outline:none;flex:1;min-width:100px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;background:var(--accent2);border-radius:50%;cursor:pointer;box-shadow:0 0 10px var(--accent2)}
textarea{width:100%;min-height:300px;font-family:'Consolas',monospace;font-size:14px;resize:vertical}
.log{background:rgba(0,0,0,.6);border:1px solid var(--border);border-radius:4px;padding:14px;font-family:'Consolas',monospace;font-size:13px;height:calc(100vh - 240px);color:var(--accent2);overflow-y:auto;user-select:text;box-shadow:inset 0 0 20px rgba(0,240,255,.05);backdrop-filter:blur(10px)}
.status-bar{padding:12px 40px;background:var(--surface);border-top:1px solid var(--border);font-size:14px;color:var(--muted);display:flex;gap:12px;align-items:center;transition:background .3s;font-weight:600;text-transform:uppercase;position:relative;z-index:100;backdrop-filter:blur(10px)}
#global-progress{position:absolute;bottom:0;left:0;height:4px;background:linear-gradient(90deg,var(--accent),var(--accent2));width:0%;transition:width .3s;box-shadow:0 0 10px var(--accent2);}
.status-dot{width:12px;height:12px;border-radius:50%;background:var(--success);flex-shrink:0;box-shadow:0 0 10px var(--success);}
.status-dot.busy{background:var(--warn);animation:pulse 1s infinite;box-shadow:0 0 10px var(--warn);}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
.hint{background:rgba(0,240,255,.05);border-left:4px solid var(--accent2);padding:16px;border-radius:0 4px 4px 0;font-size:15px;margin:16px 0;font-weight:500;backdrop-filter:blur(5px)}
.hint.danger{background:rgba(255,0,85,.05);border-left-color:var(--accent);}
.list-row{display:flex;align-items:center;padding:14px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:10px;gap:12px;transition:all .2s;backdrop-filter:blur(5px)}
.list-row:hover{border-color:var(--accent);box-shadow:0 0 15px rgba(255,0,85,.1);}.list-row .info{flex:1;min-width:0}
.list-row .title{font-weight:700;font-size:16px}.list-row .sub{font-size:13px;color:var(--muted);font-family:Consolas,monospace;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#toasts{position:fixed;bottom:20px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:10px;pointer-events:none}
.toast{background:var(--surface);color:var(--text);padding:14px 20px;border-radius:4px;border-left:4px solid var(--accent2);box-shadow:0 8px 24px rgba(0,0,0,.8);max-width:340px;animation:slideIn .3s;pointer-events:auto;font-weight:600;backdrop-filter:blur(10px)}
@keyframes slideIn{from{transform:translateX(100%);opacity:0}to{transform:translateX(0);opacity:1}}
.toast.error{border-left-color:var(--accent)}.toast.success{border-left-color:var(--success)}
.progress-bar{height:8px;background:var(--border);border-radius:4px;overflow:hidden;margin:16px 0}
.progress-fill{height:100%;background:linear-gradient(90deg,var(--accent),var(--accent2));width:0%;transition:width .3s;border-radius:4px;box-shadow:0 0 10px var(--accent2);}
.chart-wrap{background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:16px;margin-bottom:16px;backdrop-filter:blur(12px)}
#loadChart{width:100%;height:180px;display:block}
.danger-high{color:var(--accent);font-weight:700}.danger-low{color:var(--success)}
.toolbar{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:16px}
.feat-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:12px;margin-top:12px}
.feat-item{background:var(--bg);border:1px solid var(--border);padding:12px;border-radius:4px;display:flex;justify-content:space-between;align-items:center;font-family:'Orbitron';font-size:12px;backdrop-filter:blur(5px)}
.feat-item.supported{border-color:var(--success);color:var(--success);box-shadow:inset 0 0 10px rgba(16,185,129,.1);}
.feat-item.unsupported{border-color:var(--border);color:var(--muted)}
.self-usage{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:20px}
.self-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:16px;text-align:center;backdrop-filter:blur(5px)}
.self-card .lbl{font-size:12px;color:var(--muted);text-transform:uppercase;font-weight:700;margin-bottom:6px}
.self-card .val{font-size:20px;font-weight:700;color:var(--accent2);font-family:'Orbitron';}
.app-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:12px;margin-top:12px}
.app-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;display:flex;justify-content:space-between;align-items:center;backdrop-filter:blur(5px)}
.app-card .title{font-weight:700;font-size:15px;color:var(--text)}
.app-card button{padding:8px 14px;font-size:12px}
.modal-overlay{display:none;position:fixed;inset:0;z-index:10000;background:rgba(0,0,0,.85);backdrop-filter:blur(5px);align-items:center;justify-content:center}
.modal-overlay.active{display:flex}
.modal-box{background:var(--surface);border:2px solid var(--accent);border-radius:8px;padding:24px;position:relative;max-width:90vw;max-height:90vh;overflow:auto;z-index:10001;width:540px;text-align:center;box-shadow:0 0 30px rgba(255,0,85,.3);backdrop-filter:blur(15px)}
.modal-close{position:fixed;top:30px;right:30px;background:var(--accent);width:50px;height:50px;border-radius:50%;padding:0;justify-content:center;font-size:24px;z-index:99999;border:none;cursor:pointer;color:#fff;box-shadow:0 0 20px rgba(255,0,85,.8);transition:transform .2s;}
.modal-close:hover{transform:scale(1.1);background:#ff3366;}
.warn-box{background:rgba(255,0,85,.1);border-left:4px solid var(--accent);padding:16px;border-radius:0 4px 4px 0;margin-bottom:16px;font-size:15px;line-height:1.6;font-weight:500;backdrop-filter:blur(5px)}
.smart-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;margin-bottom:10px;backdrop-filter:blur(5px)}
.adv-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:16px}
.adv-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:16px;position:relative;backdrop-filter:blur(5px)}
.adv-card::before{content:'';position:absolute;top:0;left:0;width:4px;height:100%;background:var(--accent);}
.adv-card h3{font-size:14px;margin-bottom:8px;color:var(--accent2);font-family:'Orbitron';}
.adv-card p{font-size:13px;color:var(--muted);margin-bottom:12px;line-height:1.5}
.smart-item{display:flex;align-items:flex-start;gap:14px;padding:16px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:10px;transition:all .2s;backdrop-filter:blur(8px)}
.smart-item:hover{border-color:var(--accent2);box-shadow:0 0 12px rgba(0,240,255,.1)}
.smart-info{flex:1;min-width:0}
.smart-title{font-weight:700;font-size:16px;margin-bottom:6px;color:var(--text)}
.smart-badge{display:inline-block;padding:2px 8px;font-size:11px;border:1px solid;border-radius:10px;margin-left:6px;font-weight:700;text-transform:uppercase;vertical-align:middle}
.smart-size{display:inline-block;padding:2px 8px;font-size:11px;background:var(--surface);border:1px solid var(--border);border-radius:10px;margin-left:6px;font-weight:700;color:var(--accent2);vertical-align:middle}
.smart-desc{font-size:13px;color:var(--muted);line-height:1.5}
.smart-category{font-size:11px;font-weight:700;color:var(--accent);text-transform:uppercase;padding:4px 10px;border:1px solid var(--accent);border-radius:4px;flex-shrink:0;align-self:flex-start;white-space:nowrap}
.smart-total{padding:16px;background:rgba(0,240,255,.05);border:1px solid var(--accent2);border-radius:4px;font-size:16px;font-weight:700;text-align:center;margin:16px 0;color:var(--text);backdrop-filter:blur(5px)}
.smart-total b{color:var(--accent2);font-family:'Orbitron'}
.setting-group{padding:16px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:12px;backdrop-filter:blur(5px)}
.setting-group h3{font-size:13px;color:var(--accent2);margin-bottom:12px;font-family:'Orbitron';text-transform:uppercase}
.setting-row{display:flex;align-items:center;gap:12px;padding:8px 0;border-bottom:1px solid var(--border)}
.setting-row:last-child{border-bottom:none}
.setting-row label{flex:1;font-weight:600;font-size:14px}
.setting-row input[type=number]{width:80px}
.drive-current{padding:12px 16px;background:rgba(0,240,255,.08);border:1px solid var(--accent2);border-radius:4px;margin-bottom:12px;font-weight:700;color:var(--accent2);backdrop-filter:blur(5px)}
body[data-is-system="false"] .requires-system{display:none!important}
body[data-theme="void"] .smart-item,body[data-theme="void"] .setting-group{animation:float 8s ease-in-out infinite}
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
    <div class="nav-item active" data-tab="home"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 12l9-9 9 9M5 10v10h14V10"/></svg><span class="lbl" data-i18n="nav_dash">Дашборд</span></div>
    <div class="nav-item" data-tab="smart" style="color:var(--accent2)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg><span class="lbl" data-i18n="nav_smart">Умная оптимизация</span></div>
    <div class="nav-item" data-tab="autostart"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg><span class="lbl">Автозагрузка</span></div>
    <div class="nav-item" data-tab="games"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="6" width="20" height="12" rx="2"/><circle cx="7" cy="12" r="2"/><path d="M17 10v4M15 12h4"/></svg><span class="lbl" data-i18n="nav_games">Игры</span></div>
    <div class="nav-item" data-tab="overclock" style="color:var(--accent)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg><span class="lbl" data-i18n="nav_overclock">Разгон</span></div>
    <div class="nav-item requires-system" data-tab="lite" style="color:var(--error)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg><span class="lbl" data-i18n="nav_lite">Lite Windows</span></div>
    <div class="nav-item" data-tab="cleanup"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/></svg><span class="lbl" data-i18n="nav_cleanup">Очистка</span></div>
    <div class="nav-item" data-tab="apps"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/></svg><span class="lbl" data-i18n="nav_apps">Софт</span></div>
    <div class="nav-item" data-tab="overlay"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/></svg><span class="lbl" data-i18n="nav_overlay">Оверлей</span></div>
    <div class="nav-item requires-system" data-tab="services"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg><span class="lbl" data-i18n="nav_services">Службы</span></div>
    <div class="nav-item" data-tab="files"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/></svg><span class="lbl" data-i18n="nav_files">Файлы</span></div>
    <div class="nav-item" data-tab="tweaks"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg><span class="lbl" data-i18n="nav_tweaks">Твики</span></div>
    <div class="nav-item requires-system" data-tab="advanced" style="color:var(--warn)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 9v4M12 17h.01M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/></svg><span class="lbl" data-i18n="nav_advanced">Расширенное</span></div>
    <div class="nav-item requires-system" data-tab="power"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.36 6.64a9 9 0 1 1-12.73 0M12 2v10"/></svg><span class="lbl" data-i18n="nav_power">Питание</span></div>
    <div class="nav-item" data-tab="settings"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M12 16v-4M12 8h.01"/></svg><span class="lbl" data-i18n="nav_settings">Настройки</span></div>
    <div class="nav-item" data-tab="log"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><path d="M14 2v6h6"/></svg><span class="lbl" data-i18n="nav_log">Лог</span></div>
    <div class="nav-spacer"></div>
    <div class="nav-item" onclick="openDonate()" style="color:var(--warn)"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78z"/></svg><span class="lbl" data-i18n="nav_donate">Поддержать</span></div>
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
          <div class="card"><div class="card-label">↓ Скачать</div><div class="card-value" id="stat-down">0 Mbps</div></div>
          <div class="card"><div class="card-label">↑ Отдать</div><div class="card-value" id="stat-up">0 Mbps</div></div>
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
          <h2 data-i18n="info_title">Информация о системе</h2>
          <div style="display:grid;grid-template-columns:1fr 1fr;gap:12px">
            <div><div class="card-label" data-i18n="info_cpu">Процессор</div><div id="info-cpu" style="font-weight:600;font-size:13px">Загрузка...</div></div>
            <div><div class="card-label" data-i18n="info_gpu">Видеокарта</div><div id="info-gpu" style="font-weight:600;font-size:13px">Загрузка...</div></div>
            <div><div class="card-label" data-i18n="info_ram">ОЗУ (всего)</div><div id="info-ram" style="font-weight:600;font-size:13px">Загрузка...</div></div>
            <div><div class="card-label" data-i18n="info_temp">Температура</div><div id="info-temp" style="font-weight:600;font-size:13px">—</div></div>
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
          <h2 data-i18n="quick_title">Быстрые действия (безопасные)</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'cleanup_temp')">🧹 Temp</button>
            <button onclick="quickAction(this,'flush_dns')">🌐 Flush DNS</button>
            <button onclick="quickAction(this,'defrag_memory')">💾 Defrag RAM</button>
            <button onclick="quickAction(this,'clear_standby')" class="success">🧠 Standby Purger</button>
            <button onclick="quickAction(this,'one_click_maintenance')" class="success">✨ One-Click Maintenance</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-smart">
        <h1 data-i18n="smart_title">✨ Умная оптимизация</h1>
        <p class="subtitle" data-i18n="smart_sub">Автоматический анализ ПК и подбор безопасных улучшений</p>
        <div class="section">
          <div class="hint" data-i18n="smart_hint">Нажмите кнопку ниже, чтобы проанализировать систему. Программа подберёт только безопасные оптимизации (или осторожные — в малых дозах, обратимые), покажет что будет сделано, и вы сможете отредактировать список перед применением.</div>
          <div class="toolbar">
            <button onclick="loadSmartScan(this)" class="success">🔍 <span data-i18n="smart_scan_btn">Анализировать ПК</span></button>
          </div>
          <div id="smart-results"></div>
          <div class="toolbar" style="margin-top:16px">
            <button onclick="applySmart(this)" class="danger" id="smart-apply-btn" style="display:none">⚡ <span data-i18n="smart_apply_btn">Применить выбранное</span></button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-autostart">
        <h1>Автозагрузка</h1>
        <p class="subtitle">Управление программами при старте Windows</p>
        <div class="section">
          <div class="toolbar">
            <button onclick="loadAutostart(this)" class="secondary">🔄 Обновить</button>
          </div>
          <div id="autostart-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-games">
        <h1 data-i18n="nav_games">Игры</h1>
        <p class="subtitle" data-i18n="games_sub">Game Booster, сжатие, сканер</p>
        <div class="section requires-system">
          <h2>Game Booster</h2>
          <p style="color:var(--muted);font-size:14px;margin-bottom:12px" data-i18n="games_booster_desc">Временно останавливает SysMain, WSearch и DiagTrack для повышения FPS.</p>
          <div class="toolbar">
            <button onclick="quickAction(this,'game_booster_on')" class="success">▶ <span data-i18n="btn_boost_on">Включить буст</span></button>
            <button onclick="quickAction(this,'game_booster_off')" class="secondary">⏹ <span data-i18n="btn_boost_off">Выключить буст</span></button>
          </div>
          <div class="check-item" style="margin-top:12px;border:none">
            <input type="checkbox" id="auto-boost-chk"><label for="auto-boost-chk">Автоматический игровой режим (Auto-Boost daemon)</label>
          </div>
        </div>
        <div class="section">
          <h2>Оптимизация FSO</h2>
          <p style="color:var(--muted);font-size:14px;margin-bottom:12px">Отключает Fullscreen Optimizations для снижения задержки ввода.</p>
          <button onclick="quickAction(this,'disable_fso')" class="danger">Отключить FSO</button>
        </div>
        <div class="section">
          <h2 data-i18n="games_compress_title">Прозрачное сжатие игр (LZX)</h2>
          <div class="hint" data-i18n="games_compress_hint">Сжимает папку с игрой через Windows compact.exe. Игра работает как обычно, но занимает меньше места на SSD.</div>
          <div class="toolbar">
            <input type="text" id="compress-path" placeholder="C:\Games\Cyberpunk 2077" style="flex:1;min-width:200px">
            <button onclick="compressGame(this)" class="danger" data-i18n="btn_compress">🗜 Сжать папку</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="games_scan_title">Сканер установленных игр</h2>
          <button onclick="scanGames(this)" class="secondary">🔍 <span data-i18n="btn_scan_games">Сканировать</span></button>
          <div id="games-list" style="margin-top:15px"></div>
        </div>
      </div>

      <div class="tab" id="tab-overclock">
        <h1 data-i18n="nav_overclock">Разгон и Буст</h1>
        <p class="subtitle">GPU, CPU, RAM & System Tweaks</p>
        <div class="warn-box">
          ⚠️ <b>ВНИМАНИЕ!</b> Разгон может привести к нестабильности системы. MSI Afterburner будет запущен <b>скрытно</b> и применит ваш Профиль 1 (настройте его заранее).
        </div>
        <div class="section">
          <h2>GPU Overclock (Скрытый запуск MSI)</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'launch_afterburner')" class="danger">🎮 Применить профиль MSI (Скрыто)</button>
            <button onclick="openUrl('https://www.msi.com/Landing/afterburner/graphics-cards')" class="secondary">Скачать Afterburner</button>
          </div>
        </div>
        <div class="section">
          <h2>CPU & RAM Boost</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3>High Priority CPU</h3><p>Устанавливает максимальный приоритет процессора для игр (Win32PrioritySeparation = 0x26)</p><button onclick="quickAction(this,'adv_priority_sep')" class="secondary">Применить</button></div>
            <div class="adv-card"><h3>Unlock RAM (Disable Paging)</h3><p>Отключает файл подкачки (только если ОЗУ 32 ГБ и более, иначе BSOD)</p><button onclick="quickAction(this,'disable_paging')" class="danger">Отключить</button></div>
            <div class="adv-card"><h3>Memory Compression Off</h3><p>Отключает сжатие памяти Windows (снижает нагрузку на CPU, но увеличивает использование RAM)</p><button onclick="quickAction(this,'disable_mem_comp')" class="danger">Отключить</button></div>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-lite">
        <h1 data-i18n="lite_title">Lite Windows (Агрессивный деблоат)</h1>
        <p class="subtitle" data-i18n="lite_sub">Превращает систему в подобие сборок Tiny11 / 7tims OS</p>
        <div class="warn-box">
          <b>⚠️ <span data-i18n="lite_warn_title">ВНИМАНИЕ:</span></b> <span data-i18n="lite_warn_text">Эта функция удаляет UWP-приложения (Карты, Погода, Xbox и т.д.), отключает телеметрию и службы поиска Windows. Действие необратимо. Настоятельно рекомендуется протестировать это на виртуальной машине (VM).</span>
        </div>
        <div class="section">
          <h2 data-i18n="lite_what_does">Что будет сделано:</h2>
          <ul style="list-style:none;color:var(--muted);font-size:14px;line-height:1.8;margin-bottom:20px">
            <li>✓ <span data-i18n="lite_li1">Удаление встроенного мусора (Bing Weather, Xbox, Zune, People, Skype и т.д.)</span></li>
            <li>✓ <span data-i18n="lite_li2">Отключение служб телеметрии (DiagTrack)</span></li>
            <li>✓ <span data-i18n="lite_li3">Отключение Windows Search (индексатора)</span></li>
            <li>✓ <span data-i18n="lite_li4">Отключение рекламных ID и истории активности</span></li>
            <li>✓ <span data-i18n="lite_li5">Очистка временных файлов и кэша</span></li>
          </ul>
          <button onclick="runLiteDebloat(this)" class="danger" style="width:100%;justify-content:center;padding:15px;font-size:16px" data-i18n="lite_btn_run">
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
          <div class="check-item"><input type="checkbox" id="cl-do" checked><label for="cl-do" data-i18n="cl_do">Кэш обновлений (Delivery Optimization)</label></div>
          <div class="check-item"><input type="checkbox" id="cl-launchers" checked><label for="cl-launchers">Кэш игровых лаунчеров (Steam, Epic)</label></div>
          <div class="check-item"><input type="checkbox" id="cl-browsers" checked><label for="cl-browsers">Кэш браузеров (Chrome, Edge, Firefox)</label></div>
          <div class="check-item"><input type="checkbox" id="cl-chats" checked><label for="cl-chats">Кэш Discord и Telegram</label></div>
          <div class="check-item"><input type="checkbox" id="cl-dumps" checked><label for="cl-dumps">Дампы памяти (Memory Dumps)</label></div>
        </div>
        <div class="progress-bar"><div class="progress-fill" id="cleanup-progress"></div></div>
        <div class="toolbar">
          <button onclick="runCleanup(this)" data-i18n="btn_run_cleanup">▶ Очистить</button>
          <button onclick="runEnhancedCache(this)" class="success">🚀 Enhanced Cache (ISLC-класс)</button>
          <button onclick="runEnhancedRam(this)" class="success">🧠 Enhanced RAM (Mem Reduct-класс)</button>
        </div>
        <div class="hint">Enhanced Cache чистит: system file cache, standby list, DNS, font cache, Windows Store, update cache, старый prefetch, 0-byte файлы. Enhanced RAM: standby, modified page list, working sets всех процессов, system file cache.</div>
      </div>

      <div class="tab" id="tab-apps">
        <h1 data-i18n="nav_apps">Менеджер программ</h1>
        <p class="subtitle" data-i18n="apps_sub">Установка через winget</p>
        <div class="section">
          <h2>Обновление установленного софта</h2>
          <p style="color:var(--muted);font-size:14px;margin-bottom:12px">Обновляет весь установленный софт через winget.</p>
          <button onclick="quickAction(this,'winget_upgrade_all')" class="success">Обновить всё (Winget)</button>
        </div>
        <div class="section">
          <h2 data-i18n="apps_runtimes">Библиотеки Windows (Runtimes)</h2>
          <div class="app-grid">
            <div class="app-card"><span class="title">VC++ 2015-2022</span><button onclick="installApp(this,'Microsoft.VCRedist.2015+.x64')" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">.NET Runtime 8</span><button onclick="installApp(this,'Microsoft.DotNet.Runtime.8')" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">Java (Temurin 21)</span><button onclick="installApp(this,'EclipseAdoptium.Temurin.21.JRE')" data-i18n="btn_install">Установить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="apps_browsers">Браузеры</h2>
          <div class="app-grid">
            <div class="app-card"><span class="title">Google Chrome</span><button onclick="installApp(this,'Google.Chrome')" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">Mozilla Firefox</span><button onclick="installApp(this,'Mozilla.Firefox')" data-i18n="btn_install">Установить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="apps_utils">Утилиты</h2>
          <div class="app-grid">
            <div class="app-card"><span class="title">7-Zip</span><button onclick="installApp(this,'7zip.7zip')" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">Steam</span><button onclick="installApp(this,'Valve.Steam')" data-i18n="btn_install">Установить</button></div>
            <div class="app-card"><span class="title">MSI Afterburner</span><button onclick="installApp(this,'MSI.Afterburner')" data-i18n="btn_install">Установить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="apps_office">Microsoft Office (офлайн C2R)</h2>
          <div class="warn-box" data-i18n="apps_office_warn">
            ⚠️ <b>Офлайн-установщик Office C2R скачивается с официального CDN Microsoft.</b><br>
            Весит 3-5 ГБ. <b>Скачивание может занять очень долгое время</b> (зависит от скорости).<br>
            После скачивания — монтируйте образ и запустите setup.exe вручную.
          </div>
          <div class="toolbar">
            <select id="office-version" style="min-width:200px">
              <option value="O365ProPlusRetail">Microsoft 365 (ProPlus)</option>
              <option value="ProPlus2021Retail">Office 2021 ProPlus</option>
              <option value="ProPlus2019Retail">Office 2019 ProPlus</option>
            </select>
            <select id="office-lang" style="min-width:150px">
              <option value="ru-ru">Русский</option>
              <option value="en-us">English (US)</option>
            </select>
            <button onclick="downloadOffice(this)" class="secondary">⬇ <span data-i18n="btn_download_office">Скачать офлайн-установщик</span></button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-overlay">
        <h1 data-i18n="nav_overlay">Оверлей</h1>
        <p class="subtitle" data-i18n="ov_sub">Появляется только в полноэкранных играх</p>
        <div class="section">
          <div class="check-item"><input type="checkbox" id="ov-cpu" checked><label for="ov-cpu" data-i18n="ov_cpu">Показывать CPU</label></div>
          <div class="check-item"><input type="checkbox" id="ov-gpu" checked><label for="ov-gpu" data-i18n="ov_gpu">Показывать GPU</label></div>
          <div class="check-item"><input type="checkbox" id="ov-ram" checked><label for="ov-ram" data-i18n="ov_ram">Показывать ОЗУ</label></div>
          <div class="check-item"><input type="checkbox" id="ov-temp" checked><label for="ov-temp" data-i18n="ov_temp">Показывать температуру</label></div>
        </div>
        <div class="setting-group">
          <h3>Настройки положения и прозрачности</h3>
          <div class="setting-row">
            <label>Позиция X</label><input type="number" id="ov-pos-x" value="10">
            <label>Позиция Y</label><input type="number" id="ov-pos-y" value="10">
          </div>
          <div class="setting-row">
            <label>Прозрачность (50-255)</label>
            <input type="range" id="ov-alpha" min="50" max="255" value="255">
            <span id="ov-alpha-val" style="width:32px;text-align:center;font-weight:700;color:var(--accent2)">255</span>
          </div>
        </div>
        <div class="section">
          <button onclick="quickAction(this,'overlay_toggle')">🎯 <span data-i18n="btn_ov_toggle">Вкл/Выкл оверлей</span></button>
        </div>
      </div>

      <div class="tab" id="tab-services">
        <h1 data-i18n="nav_services">Службы Windows</h1>
        <div class="toolbar"><button onclick="refreshServices(this)" class="secondary">🔄 <span data-i18n="btn_refresh">Обновить</span></button></div>
        <div id="services-list"><div class="hint">Загрузка...</div></div>
      </div>

      <div class="tab" id="tab-files">
        <h1 data-i18n="nav_files">Файлы</h1>
        <div class="section">
          <h2 data-i18n="files_large">Поиск больших файлов</h2>
          <div class="toolbar">
            <input type="text" id="large-path" placeholder="Путь (пусто = профиль)" style="flex:1;min-width:200px">
            <input type="number" id="large-mb" value="100" min="1" style="width:100px">
            <button onclick="findLargeFiles(this)" data-i18n="btn_find">Найти</button>
          </div>
          <div id="large-results"></div>
        </div>
        <div class="section">
          <h2 data-i18n="files_dups">Поиск дубликатов</h2>
          <div class="toolbar">
            <input type="text" id="dup-path" placeholder="Путь (пусто = Downloads)" style="flex:1;min-width:200px">
            <button onclick="findDuplicates(this)" data-i18n="btn_scan">Сканировать</button>
          </div>
          <div id="dup-results"></div>
        </div>
        <div class="section">
          <h2 data-i18n="files_shred">Шредер файлов</h2>
          <div class="hint" data-i18n="files_shred_hint">Файл будет перезаписан 3 раза случайными данными и удалён.</div>
          <div class="toolbar">
            <input type="text" id="shred-path" placeholder="Полный путь к файлу" style="flex:1;min-width:200px">
            <button onclick="shredFile(this)" class="danger" data-i18n="btn_shred">Уничтожить</button>
          </div>
        </div>
        <div class="section requires-system">
          <h2 data-i18n="files_hosts">Редактор файла Hosts</h2>
          <textarea id="hosts-editor" placeholder="Загрузка..."></textarea>
          <div class="toolbar" style="margin-top:10px">
            <button onclick="loadHosts(this)" class="secondary">🔄 <span data-i18n="btn_load">Загрузить</span></button>
            <button onclick="saveHosts(this)" data-i18n="btn_save">💾 Сохранить</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-tweaks">
        <h1 data-i18n="nav_tweaks">Твики</h1>
        <div class="section">
          <h2 data-i18n="tw_gm">Game Mode</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'game_mode_on')" data-i18n="tw_gm_on">Включить</button>
            <button onclick="quickAction(this,'game_mode_off')" class="secondary" data-i18n="tw_gm_off">Выключить</button>
          </div>
        </div>
        <div class="section requires-system">
          <h2 data-i18n="tw_telemetry">Телеметрия и обновления</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'disable_telemetry')" class="danger" data-i18n="tw_disable_tel">Отключить телеметрию</button>
            <button onclick="quickAction(this,'disable_updates')" class="danger" data-i18n="tw_disable_upd">Отключить Windows Update</button>
            <button onclick="quickAction(this,'enable_updates')" class="secondary" data-i18n="tw_enable_upd">Включить обновления</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-advanced">
        <h1 data-i18n="nav_advanced">Расширенные настройки</h1>
        <div class="warn-box" data-i18n="adv_warn">
          ⚠️ <b>ВНИМАНИЕ!</b> Эти твики могут повлиять на стабильность системы.
        </div>
        <div class="section">
          <h2>Безопасность и Службы</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'disable_defender')" class="danger">Отключить Windows Defender</button>
            <button onclick="quickAction(this,'deep_services')" class="danger">Глубокое отключение служб</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_ctx">Менеджер контекстного меню Windows 11</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'ctx_menu_classic')" class="secondary" data-i18n="adv_ctx_classic">Классическое меню</button>
            <button onclick="quickAction(this,'ctx_menu_modern')" class="secondary" data-i18n="adv_ctx_modern">Новое меню Win11</button>
          </div>
        </div>
        <div class="section">
          <h2>Глубокий менеджер контекстного меню</h2>
          <button onclick="loadContextMenu(this)" class="secondary">Сканировать расширения</button>
          <div id="context-menu-list" style="margin-top:15px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_reg">Твики реестра</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3>Win32PrioritySeparation</h3><button onclick="quickAction(this,'adv_priority_sep')" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3>Dynamictick</h3><button onclick="quickAction(this,'adv_disable_dynamictick')" class="secondary" data-i18n="btn_disable">Отключить</button></div>
            <div class="adv-card"><h3>NetworkThrottlingIndex</h3><button onclick="quickAction(this,'adv_disable_net_throttle')" class="secondary" data-i18n="btn_disable">Отключить</button></div>
            <div class="adv-card"><h3>Nagle Algorithm</h3><button onclick="quickAction(this,'adv_disable_nagle')" class="secondary" data-i18n="btn_disable">Отключить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_deep">Глубокая очистка и оптимизация</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3>WinSxS</h3><button onclick="quickAction(this,'adv_clean_winsxs')" class="danger" data-i18n="btn_clean">Очистить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_do">Сброс кэша Dosvc</h3><button onclick="quickAction(this,'clean_do_cache')" class="danger" data-i18n="btn_clean">Очистить</button></div>
            <div class="adv-card"><h3>TRIM SSD</h3><button onclick="quickAction(this,'optimize_ssd_trim')" class="success">Оптимизировать</button></div>
            <div class="adv-card"><h3>Быстрый DNS (Cloudflare)</h3><button onclick="quickAction(this,'set_fast_dns')" class="success">Установить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_act">Активация Windows / Office</h2>
          <button onclick="quickAction(this,'activate_all')" class="danger" data-i18n="adv_act_btn">🔑 Активировать</button>
        </div>
      </div>

      <div class="tab" id="tab-power">
        <h1 data-i18n="nav_power">Питание</h1>
        <div class="section">
          <h2 data-i18n="power_scheme">Схема питания</h2>
          <div class="toolbar">
            <button onclick="quickAction(this,'power_ultimate')" data-i18n="power_ult">⚡ Максимальная</button>
            <button onclick="quickAction(this,'power_high')" data-i18n="power_high">Высокая</button>
            <button onclick="quickAction(this,'power_balanced')" class="secondary" data-i18n="power_bal">Сбалансированная</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-settings">
        <h1 data-i18n="nav_settings">Настройки</h1>
        <div class="section">
          <h2>💾 Активный диск</h2>
          <div class="drive-current" id="drive-current-label">C: — системный диск (все функции доступны)</div>
          <div class="toolbar">
            <select id="drive-select" onchange="setDrive(this.value)" style="min-width:280px"></select>
          </div>
          <div class="hint">При выборе не-системного диска функции, требующие Windows (Lite Windows, службы, WinSxS, твики реестра, телеметрия, hosts-редактор), будут скрыты. Корзина и другие операции очистки будут применены к выбранному диску. При возврате на системный диск всё восстановится.</div>
        </div>
        <div class="section">
          <h2>🎨 Тема оформления</h2>
          <div class="toolbar">
            <select id="theme-select" onchange="setTheme(this.value)" style="min-width:260px">
              <option value="black">⬛ Black (чистый чёрный)</option>
              <option value="white">⬜ White (чистый белый)</option>
              <option value="midnight">🌑 Midnight (по умолчанию)</option>
              <option value="charcoal">🪨 Charcoal</option>
              <option value="ocean">🌊 Ocean Deep</option>
              <option value="neon">💜 Neon Grid</option>
              <option value="void">🕳️ Void (космос + чёрная дыра)</option>
              <option value="forest">🌲 Forest</option>
              <option value="crimson">🔥 Crimson</option>
              <option value="ice">❄️ Ice</option>
              <option value="sunset">🌅 Sunset</option>
              <option value="muted-light">🌗 Muted Light (мягкая)</option>
            </select>
          </div>
        </div>
        <div class="section">
          <h2>⚙️ Тонкая настройка очистки</h2>
          <div class="setting-group">
            <h3>Очистка кэша (Enhanced Cache)</h3>
            <div class="setting-row"><label>Temp / Windows Temp</label><input type="checkbox" id="s-cleanTemp" checked></div>
            <div class="setting-row"><label>Prefetch</label><input type="checkbox" id="s-cleanPrefetch" checked></div>
            <div class="setting-row"><label>Старый Prefetch</label><input type="checkbox" id="s-cleanOldPrefetch"><input type="number" id="s-oldPrefetchDays" value="30" min="1" max="365"></div>
            <div class="setting-row"><label>Корзина (выбранный диск)</label><input type="checkbox" id="s-cleanRecycle" checked></div>
            <div class="setting-row"><label>Кэш миниатюр</label><input type="checkbox" id="s-cleanThumbs" checked></div>
            <div class="setting-row"><label>Кэш шейдеров (D3D, NVIDIA, AMD, Intel)</label><input type="checkbox" id="s-cleanShader" checked></div>
            <div class="setting-row"><label>DNS-кэш</label><input type="checkbox" id="s-cleanDns" checked></div>
            <div class="setting-row"><label>Delivery Optimization</label><input type="checkbox" id="s-cleanDO" checked></div>
            <div class="setting-row requires-system"><label>Font Cache</label><input type="checkbox" id="s-cleanFontCache" checked></div>
            <div class="setting-row requires-system"><label>Windows Store (wsreset)</label><input type="checkbox" id="s-cleanStoreCache" checked></div>
            <div class="setting-row requires-system"><label>Update Cache</label><input type="checkbox" id="s-cleanUpdateCache" checked></div>
            <div class="setting-row"><label>0-byte файлы (осторожно)</label><input type="checkbox" id="s-clean0ByteFiles"></div>
            <div class="setting-row"><label>Кэш лаунчеров</label><input type="checkbox" id="s-cleanLaunchers" checked></div>
            <div class="setting-row"><label>Кэш браузеров</label><input type="checkbox" id="s-cleanBrowsers" checked></div>
            <div class="setting-row"><label>Кэш Discord / Telegram</label><input type="checkbox" id="s-cleanChats" checked></div>
            <div class="setting-row"><label>Дампы памяти</label><input type="checkbox" id="s-cleanDumps" checked></div>
          </div>
          <div class="setting-group">
            <h3>Очистка ОЗУ (Enhanced RAM)</h3>
            <div class="setting-row"><label>Агрессивность (1-5)</label><input type="range" id="s-ramAggressiveness" min="1" max="5" value="3"><span id="s-ramAggressiveness-val" style="width:24px;text-align:center;font-weight:700;color:var(--accent2)">3</span></div>
            <div class="setting-row"><label>Очищать standby list</label><input type="checkbox" id="s-ramClearStandby" checked></div>
            <div class="setting-row"><label>Сбрасывать modified page list</label><input type="checkbox" id="s-ramFlushModified" checked></div>
            <div class="setting-row"><label>Trim working sets всех процессов</label><input type="checkbox" id="s-ramClearWorkingSets" checked></div>
            <div class="setting-row"><label>Очищать system file cache</label><input type="checkbox" id="s-ramClearFileCache" checked></div>
          </div>
          <div class="toolbar">
            <button onclick="saveFineSettings(this)" class="success">💾 Сохранить настройки</button>
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

<div id="donate-modal" class="modal-overlay" onclick="if(event.target===this) closeDonate()">
  <div class="modal-box">
    <button class="modal-close" onclick="closeDonate()">✕</button>
    <h2 style="margin-bottom:12px" data-i18n="donate_title">Поддержать проект</h2>
    <div style="position:relative; z-index: 1;">
        <iframe src="https://yoomoney.ru/quickpay/fundraise/widget?billNumber=1L4A2P0ICO6.261024&" width="100%" height="480" frameborder="0" allowtransparency="true" scrolling="no" style="border-radius:4px; background: #fff;"></iframe>
    </div>
    <p style="margin-top:12px;font-size:13px;color:var(--muted)">
      <span data-i18n="donate_fallback">Если виджет не загружается,</span>
      <a href="#" onclick="call('open_url', 'https://yoomoney.ru/fundraise/1L4A2P0ICO6.261024'); return false;" style="color:var(--accent2)" data-i18n="donate_link">откройте ссылку в браузере</a>.
    </p>
    <button class="secondary" style="margin-top: 15px; width: 100%; justify-content: center;" onclick="closeDonate()" data-i18n="btn_close">Закрыть</button>
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
    nav_dash: "Дашборд", nav_games: "Игры", nav_overclock: "Разгон", nav_lite: "Lite Windows", nav_cleanup: "Очистка", nav_apps: "Софт",
    nav_overlay: "Оверлей", nav_services: "Службы", nav_files: "Файлы", nav_tweaks: "Твики", nav_advanced: "Расширенное",
    nav_power: "Питание", nav_settings: "Настройки", nav_log: "Лог", nav_donate: "Поддержать",
    nav_smart: "Умная оптимизация",
    smart_title: "✨ Умная оптимизация",
    smart_sub: "Автоматический анализ ПК и подбор безопасных улучшений",
    smart_hint: "Нажмите кнопку ниже, чтобы проанализировать систему. Программа подберёт только безопасные оптимизации (или осторожные — в малых дозах, обратимые), покажет что будет сделано, и вы сможете отредактировать список перед применением.",
    smart_scan_btn: "Анализировать ПК", smart_apply_btn: "Применить выбранное",
    home_title: "Панель управления", home_sub: "Мониторинг в реальном времени",
    stat_ram: "ОЗУ свободно", stat_procs: "Процессов", chart_title: "Загрузка (последние 60 сек)",
    self_usage_title: "Использование ресурсов Optimizer Pro", disk_health_title: "Здоровье дисков (S.M.A.R.T.)",
    info_title: "Информация о системе", info_cpu: "Процессор", info_gpu: "Видеокарта", info_ram: "ОЗУ (всего)", info_temp: "Температура",
    quick_title: "Быстрые действия (безопасные)",
    games_sub: "Game Booster, сжатие, сканер", games_booster_desc: "Временно останавливает SysMain, WSearch и DiagTrack для повышения FPS.",
    btn_boost_on: "Включить буст", btn_boost_off: "Выключить буст", games_compress_title: "Прозрачное сжатие игр (LZX)",
    games_compress_hint: "Сжимает папку с игрой через Windows compact.exe. Игра работает как обычно, но занимает меньше места на SSD.",
    btn_compress: "🗜 Сжать папку", games_scan_title: "Сканер установленных игр", btn_scan_games: "Сканировать",
    lite_title: "Lite Windows (Агрессивный деблоат)", lite_sub: "Превращает систему в подобие сборок Tiny11 / 7tims OS",
    lite_warn_title: "ВНИМАНИЕ:", lite_warn_text: "Эта функция удаляет UWP-приложения (Карты, Погода, Xbox и т.д.), отключает телеметрию и службы поиска Windows. Действие необратимо. Настоятельно рекомендуется протестировать это на виртуальной машине (VM).",
    lite_what_does: "Что будет сделано:", lite_li1: "Удаление встроенного мусора (Bing Weather, Xbox, Zune, People, Skype и т.д.)",
    lite_li2: "Отключение служб телеметрии (DiagTrack)", lite_li3: "Отключение Windows Search (индексатора)",
    lite_li4: "Отключение рекламных ID и истории активности", lite_li5: "Очистка временных файлов и кэша",
    lite_btn_run: "⚡ СДЕЛАТЬ WINDOWS LITE",
    cleanup_sub: "Выберите компоненты", cl_recycle: "Корзина", cl_thumbs: "Кэш миниатюр", cl_do: "Кэш обновлений (Delivery Optimization)",
    btn_run_cleanup: "▶ Очистить", apps_sub: "Установка через winget", apps_runtimes: "Библиотеки Windows (Runtimes)",
    btn_install: "Установить", apps_browsers: "Браузеры", apps_utils: "Утилиты", apps_office: "Microsoft Office (офлайн C2R)",
    apps_office_warn: "⚠️ <b>Офлайн-установщик Office C2R скачивается с официального CDN Microsoft.</b><br>Весит 3-5 ГБ. <b>Скачивание может занять очень долгое время</b> (зависит от скорости).<br>После скачивания — монтируйте образ и запустите setup.exe вручную.",
    btn_download_office: "Скачать офлайн-установщик", ov_sub: "Появляется только в полноэкранных играх",
    ov_cpu: "Показывать CPU", ov_gpu: "Показывать GPU", ov_ram: "Показывать ОЗУ", ov_temp: "Показывать температуру",
    btn_ov_toggle: "Вкл/Выкл оверлей", btn_refresh: "Обновить", files_large: "Поиск больших файлов", btn_find: "Найти",
    files_dups: "Поиск дубликатов", btn_scan: "Сканировать", files_shred: "Шредер файлов", files_shred_hint: "Файл будет перезаписан 3 раза случайными данными и удалён.",
    btn_shred: "Уничтожить", files_hosts: "Редактор файла Hosts", btn_load: "Загрузить", btn_save: "Сохранить",
    tw_gm: "Game Mode", tw_gm_on: "Включить", tw_gm_off: "Выключить", tw_telemetry: "Телеметрия и обновления",
    tw_disable_tel: "Отключить телеметрию", tw_disable_upd: "Отключить Windows Update", tw_enable_upd: "Включить обновления",
    adv_warn: "⚠️ <b>ВНИМАНИЕ!</b> Эти твики могут повлиять на стабильность системы.",
    adv_ctx: "Менеджер контекстного меню Windows 11", adv_ctx_classic: "Классическое меню", adv_ctx_modern: "Новое меню Win11",
    adv_reg: "Твики реестра", btn_apply: "Применить", btn_disable: "Отключить", adv_deep: "Глубокая очистка и оптимизация",
    adv_do: "Сброс кэша Dosvc", btn_clean: "Очистить", adv_act: "Активация Windows / Office", adv_act_btn: "🔑 Активировать", power_scheme: "Схема питания",
    power_ult: "⚡ Максимальная", power_high: "Высокая", power_bal: "Сбалансированная", set_lang: "Язык",
    set_theme: "Тема", set_dark: "Тёмная", set_light: "Светлая", status_ready: "Готово", donate_title: "Поддержать проект",
    donate_fallback: "Если виджет не загружается,", donate_link: "откройте ссылку в браузере", btn_close: "Закрыть"
  },
  en: {
    nav_dash: "Dashboard", nav_games: "Games", nav_overclock: "Overclock", nav_lite: "Lite Windows", nav_cleanup: "Cleanup", nav_apps: "Apps",
    nav_overlay: "Overlay", nav_services: "Services", nav_files: "Files", nav_tweaks: "Tweaks", nav_advanced: "Advanced",
    nav_power: "Power", nav_settings: "Settings", nav_log: "Log", nav_donate: "Support",
    nav_smart: "Smart Optimize",
    smart_title: "✨ Smart Optimize",
    smart_sub: "Automatic PC analysis and safe improvements selection",
    smart_hint: "Click the button below to analyze the system. The program will pick only safe optimizations (or cautious ones in small reversible doses), show what will be done, and you can edit the list before applying.",
    smart_scan_btn: "Analyze PC", smart_apply_btn: "Apply Selected",
    home_title: "Control Panel", home_sub: "Real-time monitoring",
    stat_ram: "RAM Free", stat_procs: "Processes", chart_title: "Load (last 60s)",
    self_usage_title: "Optimizer Pro Resource Usage", disk_health_title: "Disk Health (S.M.A.R.T.)",
    info_title: "System Information", info_cpu: "Processor", info_gpu: "Video Card", info_ram: "RAM (Total)", info_temp: "Temperature",
    quick_title: "Quick Actions (Safe)",
    games_sub: "Game Booster, Compression, Scanner", games_booster_desc: "Temporarily stops SysMain, WSearch and DiagTrack to boost FPS.",
    btn_boost_on: "Enable Boost", btn_boost_off: "Disable Boost", games_compress_title: "Transparent Game Compression (LZX)",
    games_compress_hint: "Compresses the game folder via Windows compact.exe. The game runs as usual but takes less SSD space.",
    btn_compress: "🗜 Compress Folder", games_scan_title: "Installed Games Scanner", btn_scan_games: "Scan",
    lite_title: "Lite Windows (Aggressive Debloat)", lite_sub: "Turns the system into something like Tiny11 / 7tims OS",
    lite_warn_title: "WARNING:", lite_warn_text: "This function removes UWP apps (Maps, Weather, Xbox, etc.), disables telemetry and Windows Search services. Action is irreversible. It is highly recommended to test this on a Virtual Machine (VM).",
    lite_what_does: "What will be done:", lite_li1: "Removal of built-in bloatware (Bing Weather, Xbox, Zune, People, Skype, etc.)",
    lite_li2: "Disabling telemetry services (DiagTrack)", lite_li3: "Disabling Windows Search (indexer)",
    lite_li4: "Disabling Advertising ID and Activity History", lite_li5: "Cleaning temp files and cache",
    lite_btn_run: "⚡ MAKE WINDOWS LITE",
    cleanup_sub: "Select components", cl_recycle: "Recycle Bin", cl_thumbs: "Thumbnails", cl_do: "Delivery Optimization Cache",
    btn_run_cleanup: "▶ Clean", apps_sub: "Install via winget", apps_runtimes: "Windows Libraries (Runtimes)",
    btn_install: "Install", apps_browsers: "Browsers", apps_utils: "Utilities", apps_office: "Microsoft Office (Offline C2R)",
    apps_office_warn: "⚠️ <b>Office C2R offline installer is downloaded from official Microsoft CDN.</b><br>It weighs 3-5 GB. <b>Download may take a very long time</b> (depends on speed).<br>After download, mount the image and run setup.exe manually.",
    btn_download_office: "Download Offline Installer", ov_sub: "Appears only in fullscreen games",
    ov_cpu: "Show CPU", ov_gpu: "Show GPU", ov_ram: "Show RAM", ov_temp: "Show Temp",
    btn_ov_toggle: "Toggle Overlay", btn_refresh: "Refresh", files_large: "Find Large Files", btn_find: "Find",
    files_dups: "Find Duplicates", btn_scan: "Scan", files_shred: "File Shredder", files_shred_hint: "The file will be overwritten 3 times with random data and deleted.",
    btn_shred: "Shred", files_hosts: "Hosts File Editor", btn_load: "Load", btn_save: "Save",
    tw_gm: "Game Mode", tw_gm_on: "Enable", tw_gm_off: "Disable", tw_telemetry: "Telemetry & Updates",
    tw_disable_tel: "Disable Telemetry", tw_disable_upd: "Disable Windows Update", tw_enable_upd: "Enable Updates",
    adv_warn: "⚠️ <b>WARNING!</b> These tweaks may affect system stability.",
    adv_ctx: "Windows 11 Context Menu Manager", adv_ctx_classic: "Classic Menu", adv_ctx_modern: "Modern Win11 Menu",
    adv_reg: "Registry Tweaks", btn_apply: "Apply", btn_disable: "Disable", adv_deep: "Deep Clean & Optimize",
    adv_do: "Reset Dosvc Cache", btn_clean: "Clean", adv_act: "Windows / Office Activation", adv_act_btn: "🔑 Activate", power_scheme: "Power Plan",
    power_ult: "⚡ Ultimate", power_high: "High", power_bal: "Balanced", set_lang: "Language",
    set_theme: "Theme", set_dark: "Dark", set_light: "Light", status_ready: "Ready", donate_title: "Support Project",
    donate_fallback: "If the widget doesn't load,", donate_link: "open the link in a browser", btn_close: "Close"
  }
};

function loadLangConfig() {
  fetch('http://optpro.local/lang.json')
    .then(function(r) { if (!r.ok) throw new Error(); return r.json(); })
    .then(function(d) { i18n = d; setLang(currentLang); })
    .catch(function() { console.log('Using embedded lang'); });
}

function setLang(l) {
  currentLang = l;
  var dict = i18n[l] || i18n.ru;
  document.querySelectorAll('[data-i18n]').forEach(function(el) {
    var k = el.getAttribute('data-i18n');
    if (dict[k]) el.innerHTML = dict[k];
  });
  store.set('optpro-lang', l);
  var sel = document.getElementById('lang-select');
  if (sel) sel.value = l;
}

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
  document.body.style.cursor = d ? (d + '-resize') : '';
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
    var style = getComputedStyle(document.body);
    var border = style.getPropertyValue('--border').trim() || '#333';
    var muted = style.getPropertyValue('--muted').trim() || '#666';
    ctx.strokeStyle = border; ctx.lineWidth = 1;
    for (var i = 0; i <= 4; i++) {
      var y = (h / 4) * i;
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
      ctx.fillStyle = muted; ctx.font = '10px sans-serif';
      ctx.fillText((100 - i * 25) + '%', 4, y + 12);
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
  if (percent > 0 && percent < 100) {
    p.style.width = percent + '%';
    s.textContent = msg;
  } else {
    p.style.width = '0%';
    s.textContent = i18n[currentLang].status_ready;
  }
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
    if (chart) chart.push(cpu, Math.max(0, gpu));
    if (s.sys) {
      if ((e = document.getElementById('info-cpu'))) e.textContent = s.sys.cpu || '—';
      if ((e = document.getElementById('info-gpu'))) e.textContent = s.sys.gpu || '—';
      if ((e = document.getElementById('info-ram'))) e.textContent = s.sys.ram || '—';
    }
    if (s.temp && s.temp > 0 && (e = document.getElementById('info-temp'))) e.textContent = s.temp.toFixed(1) + ' °C';
    if ((e = document.getElementById('app-cpu'))) e.textContent = (s.appCpu || 0).toFixed(1) + '%';
    if ((e = document.getElementById('app-ram'))) e.textContent = (s.appRam || 0) + ' MB';
    if ((e = document.getElementById('app-gpu'))) e.textContent = (s.appGpu || 0).toFixed(1) + '%';
    if (s.disks && (e = document.getElementById('smart-list'))) {
      if (!s.disks.length) e.innerHTML = '<div class="hint">Нет данных SMART</div>';
      else {
        var html = '';
        s.disks.forEach(function(d) {
          var healthStr = d.status === 0 ? '✅ Healthy' : (d.status === 1 ? '⚠️ Warning' : '❌ Unhealthy');
          var tempStr = d.temp > 0 ? ' • Temp: ' + d.temp + '°C' : '';
          var wearStr = d.wear >= 0 ? ' • Wear: ' + d.wear + '%' : '';
          html += '<div class="smart-card"><div class="title">' + escapeHtml(d.model) + ' (' + escapeHtml(d.media) + ')</div>' +
                  '<div class="sub" style="color:' + (d.status===0?'var(--success)':'var(--accent)') + '">' + healthStr + tempStr + wearStr + '</div></div>';
        });
        e.innerHTML = html;
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
      el.innerHTML = k.toUpperCase() + ' <span>' + (supported ? '✔' : '✘') + '</span>';
    });
  });
}

function quickAction(btn, action) {
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
  var ap = document.getElementById('smart-apply-btn');
  if (ap) ap.style.display = 'none';
  
  var currentScan = ++smartScanId;
  
  call('smart_scan').then(function(r) {
    if (currentScan !== smartScanId) return;
    
    var c = document.getElementById('smart-results');
    if (!r || !r.items) { c.innerHTML = '<div class="hint">Ошибка анализа</div>'; return; }
    smartItems = r.items;
    if (!smartItems.length) { c.innerHTML = '<div class="hint">✅ ПК уже оптимизирован, дополнительных действий не найдено.</div>'; return; }
    var html = '';
    var totalBytes = 0;
    smartItems.forEach(function(it, i) {
      totalBytes += (it.size || 0);
      var safetyColor = it.safety === 'safe' ? 'var(--success)' : 'var(--warn)';
      var safetyText = it.safety === 'safe' ? '✓ Безопасно' : '⚠ Осторожно';
      var sizeStr = (it.size && it.size > 0) ? formatSize(it.size) : '';
      html += '<div class="smart-item" data-idx="' + i + '">' +
        '<input type="checkbox" data-idx="' + i + '"' + (it.defaultOn ? ' checked' : '') + '>' +
        '<div class="smart-info">' +
          '<div class="smart-title">' + escapeHtml(it.title) +
            ' <span class="smart-badge" style="color:' + safetyColor + ';border-color:' + safetyColor + '">' + safetyText + '</span>' +
            (sizeStr ? ' <span class="smart-size">' + sizeStr + '</span>' : '') +
          '</div>' +
          '<div class="smart-desc">' + escapeHtml(it.desc) + '</div>' +
        '</div>' +
        '<div class="smart-category">' + escapeHtml(it.category) + '</div>' +
      '</div>';
    });
    html += '<div class="smart-total">Итого можно освободить: <b>' + formatSize(totalBytes) + '</b></div>';
    c.innerHTML = html;
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
  if (!confirm('Применить ' + ids.length + ' оптимизаций?\nНекоторые изменения требуют перезагрузки для полного эффекта.\n\nПродолжить?')) return;
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
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет элементов</div>'; return; }
    var html = '';
    r.items.forEach(function(a, i) {
      html += '<div class="list-row"><div class="info"><div class="title">' + escapeHtml(a.name) + '</div>' +
              '<div class="sub">' + escapeHtml(a.command) + '</div></div>' +
              '<button class="secondary" onclick="toggleAutostart(this, \'' + escapeHtml(a.name) + '\', \'' + escapeHtml(a.location) + '\', \'' + escapeHtml(a.act) + '\')">' +
              (a.act === 'enable' ? 'Включить' : 'Отключить') + '</button></div>';
    });
    c.innerHTML = html;
  }).finally(function() { if (btn) setBusy(btn, false); });
}
function toggleAutostart(btn, name, loc, act) {
  setBusy(btn, true, '...');
  call('toggle_autostart', name, loc, act).then(function(r) {
    if (r.ok) { toast('Готово', 'success'); loadAutostart(null); }
    else toast('Ошибка', 'error');
  }).finally(function() { setBusy(btn, false); });
}

function loadContextMenu(btn) {
  if (btn) setBusy(btn, true, 'Сканирование...');
  call('get_context_menu').then(function(r) {
    var c = document.getElementById('context-menu-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Не найдено</div>'; return; }
    var html = '';
    r.items.forEach(function(a) {
      html += '<div class="list-row"><div class="info"><div class="title">' + escapeHtml(a.name) + '</div>' +
              '<div class="sub">' + escapeHtml(a.clsid) + '</div></div>' +
              '<button class="secondary" onclick="toggleContextMenu(this, \'' + escapeHtml(a.clsid) + '\', true)">Отключить</button></div>';
    });
    c.innerHTML = html;
  }).finally(function() { if (btn) setBusy(btn, false); });
}
function toggleContextMenu(btn, clsid, disable) {
  setBusy(btn, true, '...');
  call('toggle_context_menu', clsid, disable ? '1' : '0').then(function(r) {
    if (r.ok) { toast('Применено', 'success'); btn.disabled = true; }
    else toast('Ошибка', 'error');
  }).finally(function() { setBusy(btn, false); });
}

function runLiteDebloat(btn) {
  if (!confirm(i18n[currentLang].lite_warn_text)) return;
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
  ['temp', 'prefetch', 'recycle', 'thumbs', 'shader', 'dns', 'do', 'launchers', 'browsers', 'chats', 'dumps'].forEach(function(k) {
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

function downloadOffice(btn) {
  var ver = document.getElementById('office-version').value;
  var lang = document.getElementById('office-lang').value;
  if (!confirm('Офлайн-установщик Office C2R весит 3-5 ГБ.\nСкачивание может занять очень долгое время (зависит от скорости интернета).\n\nПродолжить?')) return;
  setBusy(btn, true, 'Скачивание (долго)...');
  call('download_office', ver, lang).then(function(r) {
    toast(r.ok ? 'Установщик скачан: ' + (r.path || '') : 'Ошибка', r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function compressGame(btn) {
  var path = document.getElementById('compress-path').value.trim();
  if (!path) { toast('Введите путь к папке', 'error'); return; }
  if (!confirm('Сжать папку?\n' + path + '\nЭто может занять длительное время.')) return;
  setBusy(btn, true, 'Сжатие...');
  call('compress_game', path).then(function(r) {
    toast(r.ok ? 'Сжатие завершено' : 'Ошибка сжатия', r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function scanGames(btn) {
  setBusy(btn, true, 'Сканирование...');
  call('scan_games').then(function(r) {
    var c = document.getElementById('games-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Игры не найдены</div>'; return; }
    var html = '';
    r.items.forEach(function(g) {
      html += '<div class="list-row"><div class="info"><div class="title">' + escapeHtml(g.name) + '</div>' +
              '<div class="sub">' + escapeHtml(g.launcher) + ' • ' + escapeHtml(g.path) + '</div></div>' +
              '<button class="secondary" onclick="document.getElementById(\'compress-path\').value=\'' + escapeHtml(g.path).replace(/\\/g, '\\\\') + '\'">Выбрать</button></div>';
    });
    c.innerHTML = html;
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
  if (!confirm('Сохранить файл hosts? Будет создана резервная копия.')) return;
  setBusy(btn, true, 'Сохранение...');
  call('write_hosts', content).then(function(r) {
    toast(r.ok ? 'Сохранено' : 'Ошибка', r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function refreshServices(btn) {
  if (btn) setBusy(btn, true, 'Загрузка...');
  call('list_services').then(function(r) {
    var c = document.getElementById('services-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет служб</div>'; return; }
    var html = '';
    r.items.slice(0, 200).forEach(function(s, i) {
      var stateCls = s.state === 'running' ? 'danger-low' : 'danger-high';
      html += '<div class="list-row"><div class="info"><div class="title ' + stateCls + '">' + escapeHtml(s.display) +
        '</div><div class="sub">' + escapeHtml(s.name) + ' • ' + s.startType + '</div></div>' +
        '<button class="secondary svc-btn" data-idx="' + i + '" data-action="' + (s.state === 'running' ? 'stop' : 'start') + '">' +
        (s.state === 'running' ? 'Стоп' : 'Старт') + '</button>' +
        '<button class="secondary svc-btn" data-idx="' + i + '" data-action="auto">A</button>' +
        '<button class="secondary svc-btn" data-idx="' + i + '" data-action="manual">M</button>' +
        '<button class="danger svc-btn" data-idx="' + i + '" data-action="disable">✕</button></div>';
    });
    c.innerHTML = html;
    var data = r.items;
    c.querySelectorAll('.svc-btn').forEach(function(btn) {
      btn.onclick = function() {
        var idx = parseInt(btn.dataset.idx);
        var s = data[idx];
        if (s) call('control_service', s.name, btn.dataset.action).then(function(res) {
            toast(res.ok ? 'OK' : 'Ошибка', res.ok ? 'success' : 'error');
            refreshServices(null);
        });
      };
    });
  }).catch(function(){}).finally(function() { if (btn) setBusy(btn, false); });
}

function findLargeFiles(btn) {
  setBusy(btn, true, 'Поиск...');
  var path = document.getElementById('large-path').value.trim();
  var mb = parseInt(document.getElementById('large-mb').value) || 100;
  call('find_large_files', path, mb).then(function(r) {
    var c = document.getElementById('large-results');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Не найдено</div>'; return; }
    c.innerHTML = '';
    r.items.forEach(function(f) {
      var sizeMB = (f.size / 1048576).toFixed(1);
      var row = document.createElement('div'); row.className = 'list-row';
      row.innerHTML = '<div class="info"><div class="title">' + sizeMB + ' MB</div><div class="sub">' + escapeHtml(f.path) + '</div></div>';
      c.appendChild(row);
    });
  }).finally(function() { setBusy(btn, false); });
}

function findDuplicates(btn) {
  setBusy(btn, true, 'Сканирование...');
  var path = document.getElementById('dup-path').value.trim();
  call('find_duplicates', path).then(function(r) {
    var c = document.getElementById('dup-results');
    if (!r.groups || !r.groups.length) { c.innerHTML = '<div class="hint">Нет дубликатов</div>'; return; }
    c.innerHTML = '';
    r.groups.forEach(function(g) {
      var div = document.createElement('div'); div.className = 'section';
      div.innerHTML = '<h2>Размер: ' + formatSize(g.size) + ' • ' + g.files.length + ' копий</h2>';
      g.files.forEach(function(f) { div.innerHTML += '<div class="sub" style="padding:6px 0;color:var(--muted)">' + escapeHtml(f) + '</div>'; });
      c.appendChild(div);
    });
  }).finally(function() { setBusy(btn, false); });
}

function shredFile(btn) {
  var p = document.getElementById('shred-path').value.trim();
  if (!p) { toast('Введите путь', 'error'); return; }
  if (!confirm('Уничтожить файл навсегда?\n' + p)) return;
  setBusy(btn, true, 'Уничтожение...');
  call('shred_file', p).then(function(r) { toast(r.ok ? 'Уничтожен' : 'Ошибка', r.ok ? 'success' : 'error'); }).finally(function() { setBusy(btn, false); });
}

function createBubbles() {
  var existing = document.querySelectorAll('.bubble');
  existing.forEach(function(b) { b.remove(); });
  if (document.body.dataset.theme === 'void') {
    for (var i = 0; i < 18; i++) {
      var b = document.createElement('div');
      b.className = 'bubble';
      var size = Math.random() * 70 + 15;
      b.style.width = size + 'px';
      b.style.height = size + 'px';
      b.style.left = (Math.random() * 100) + 'vw';
      b.style.animationDuration = (Math.random() * 18 + 12) + 's';
      b.style.animationDelay = (Math.random() * 10) + 's';
      document.body.appendChild(b);
    }
  }
}

function setTheme(t) {
  document.body.dataset.theme = t;
  store.set('optpro-theme', t);
  setTimeout(initChart, 100);
  createBubbles();
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
    lbl.textContent = isSystem 
      ? d.letter + ': — системный диск (все функции доступны)'
      : d.letter + ': — не-системный (функции Windows скрыты, очистка применяется к этому диску)';
  }
  var activeNav = document.querySelector('.nav-item.active');
  if (activeNav && activeNav.classList.contains('requires-system') && !isSystem) {
    document.querySelectorAll('.nav-item').forEach(function(e) { e.classList.remove('active'); });
    var home = document.querySelector('.nav-item[data-tab="home"]');
    if (home) home.classList.add('active');
    document.querySelectorAll('.tab').forEach(function(t) { t.classList.remove('active'); });
    var homeTab = document.getElementById('tab-home');
    if (homeTab) homeTab.classList.add('active');
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
    var map = ['cleanTemp','cleanPrefetch','cleanOldPrefetch','cleanRecycle','cleanThumbs','cleanShader','cleanDns','cleanDO','cleanFontCache','cleanStoreCache','cleanUpdateCache','clean0ByteFiles','cleanLaunchers','cleanBrowsers','cleanChats','cleanDumps','ramClearStandby','ramFlushModified','ramClearWorkingSets','ramClearFileCache','autoBoost'];
    map.forEach(function(k) {
      var el = document.getElementById('s-' + k);
      if (el) el.checked = !!st[k];
    });
    var ab = document.getElementById('auto-boost-chk'); if (ab) ab.checked = !!st.autoBoost;
    var days = document.getElementById('s-oldPrefetchDays'); if (days) days.value = st.oldPrefetchDays || 30;
    var agg = document.getElementById('s-ramAggressiveness'); if (agg) agg.value = st.ramAggressiveness || 3;
    var aggv = document.getElementById('s-ramAggressiveness-val'); if (aggv) aggv.textContent = st.ramAggressiveness || 3;
  });
}

function saveFineSettings(btn) {
  var s = {
    cleanTemp: getChk('s-cleanTemp'), cleanPrefetch: getChk('s-cleanPrefetch'),
    cleanOldPrefetch: getChk('s-cleanOldPrefetch'),
    oldPrefetchDays: parseInt((document.getElementById('s-oldPrefetchDays') || {}).value) || 30,
    cleanRecycle: getChk('s-cleanRecycle'), cleanThumbs: getChk('s-cleanThumbs'),
    cleanShader: getChk('s-cleanShader'), cleanDns: getChk('s-cleanDns'),
    cleanDO: getChk('s-cleanDO'), cleanFontCache: getChk('s-cleanFontCache'),
    cleanStoreCache: getChk('s-cleanStoreCache'), cleanUpdateCache: getChk('s-cleanUpdateCache'),
    clean0ByteFiles: getChk('s-clean0ByteFiles'),
    cleanLaunchers: getChk('s-cleanLaunchers'), cleanBrowsers: getChk('s-cleanBrowsers'),
    cleanChats: getChk('s-cleanChats'), cleanDumps: getChk('s-cleanDumps'),
    ramAggressiveness: parseInt((document.getElementById('s-ramAggressiveness') || {}).value) || 3,
    ramClearStandby: getChk('s-ramClearStandby'), ramFlushModified: getChk('s-ramFlushModified'),
    ramClearWorkingSets: getChk('s-ramClearWorkingSets'), ramClearFileCache: getChk('s-ramClearFileCache'),
    autoBoost: getChk('auto-boost-chk')
  };
  setBusy(btn, true, 'Сохранение...');
  call('set_settings', JSON.stringify(s)).then(function(r) {
    toast(r && r.ok ? 'Настройки сохранены' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

function getChk(id) { var e = document.getElementById(id); return e ? !!e.checked : false; }

document.getElementById('ov-cpu').addEventListener('change', function(e) { call('set_overlay_setting', 'cpu', e.target.checked ? '1' : '0'); });
document.getElementById('ov-gpu').addEventListener('change', function(e) { call('set_overlay_setting', 'gpu', e.target.checked ? '1' : '0'); });
document.getElementById('ov-ram').addEventListener('change', function(e) { call('set_overlay_setting', 'ram', e.target.checked ? '1' : '0'); });
document.getElementById('ov-temp').addEventListener('change', function(e) { call('set_overlay_setting', 'temp', e.target.checked ? '1' : '0'); });

document.getElementById('ov-pos-x').addEventListener('change', function(e) { call('set_overlay_setting', 'posx', e.target.value); });
document.getElementById('ov-pos-y').addEventListener('change', function(e) { call('set_overlay_setting', 'posy', e.target.value); });
document.getElementById('ov-alpha').addEventListener('input', function(e) {
  document.getElementById('ov-alpha-val').textContent = e.target.value;
  call('set_overlay_setting', 'alpha', e.target.value);
});

var ramAgg = document.getElementById('s-ramAggressiveness');
if (ramAgg) {
  ramAgg.addEventListener('input', function() {
    var v = document.getElementById('s-ramAggressiveness-val');
    if (v) v.textContent = ramAgg.value;
  });
}

function openDonate() { document.getElementById('donate-modal').classList.add('active'); }
function closeDonate() { 
    document.getElementById('donate-modal').classList.remove('active'); 
    var iframe = document.querySelector('#donate-modal iframe');
    if (iframe) iframe.src = iframe.src; 
}
document.addEventListener('keydown', function(e) { if (e.key === 'Escape') closeDonate(); });

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, function(c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}

function boot() {
  if (!initBridge()) {
    if (++bootTries < 100) { setTimeout(boot, 100); return; }
    return;
  }
  loadLangConfig();
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
    std::mutex autostartMutex;
    std::vector<AutoEntry> autostartCache;

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
        std::wstring html = Utf8ToWide(HTML_UI);
        auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, g_paths.WebView2Dir.c_str(), options.Get(),
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this, html](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result) || !env) return result;
                    HRESULT ch = env->CreateCoreWebView2Controller(hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this, html](HRESULT r, ICoreWebView2Controller* ctrl) -> HRESULT {
                                if (FAILED(r) || !ctrl) return r;
                                controller = ctrl;
                                HRESULT gh = controller->get_CoreWebView2(&webview);
                                if (FAILED(gh) || !webview) return gh;

                                ComPtr<ICoreWebView2_3> webview3;
                                if (SUCCEEDED(webview.As(&webview3))) {
                                    webview3->SetVirtualHostNameToFolderMapping(L"optpro.local", g_paths.AppDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
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
                                webview->NavigateToString(html.c_str());
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());
        return SUCCEEDED(hr);
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
        std::thread([self, id, fn]() {
            try {
                std::wstring result = fn();
                self->PostUI([self, id, result]() { if (!self->shuttingDown) self->SendResp(id, result); });
            }
            catch (...) {
                self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false,\"error\":\"exception\"}"); });
            }
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
                std::wstring j = L"{\"cpu\":" + DoubleToJsonStr(g_cpuUsage.load()) + L",\"gpu\":" + DoubleToJsonStr(g_gpuUsage.load()) +
                    L",\"ram_free\":" + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) + L",\"procs\":" + std::to_wstring(g_procCount.load()) +
                    L",\"temp\":" + DoubleToJsonStr(g_cpuTemp.load()) + L",\"appCpu\":" + DoubleToJsonStr(g_appCpuUsage.load()) +
                    L",\"appGpu\":" + DoubleToJsonStr(g_appGpuUsage.load()) + L",\"appRam\":" + std::to_wstring(g_appRamMB.load()) +
                    L",\"net_down\":" + DoubleToJsonStr(g_netDownMbps.load()) + L",\"net_up\":" + DoubleToJsonStr(g_netUpMbps.load()) +
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
                    + L",\"cleanOldPrefetch\":" + std::wstring(s.cleanOldPrefetch ? L"true" : L"false")
                    + L",\"oldPrefetchDays\":" + std::to_wstring(s.oldPrefetchDays)
                    + L",\"cleanRecycle\":" + std::wstring(s.cleanRecycle ? L"true" : L"false")
                    + L",\"cleanThumbs\":" + std::wstring(s.cleanThumbs ? L"true" : L"false")
                    + L",\"cleanShader\":" + std::wstring(s.cleanShader ? L"true" : L"false")
                    + L",\"cleanDns\":" + std::wstring(s.cleanDns ? L"true" : L"false")
                    + L",\"cleanDO\":" + std::wstring(s.cleanDO ? L"true" : L"false")
                    + L",\"cleanFontCache\":" + std::wstring(s.cleanFontCache ? L"true" : L"false")
                    + L",\"cleanStoreCache\":" + std::wstring(s.cleanStoreCache ? L"true" : L"false")
                    + L",\"cleanUpdateCache\":" + std::wstring(s.cleanUpdateCache ? L"true" : L"false")
                    + L",\"clean0ByteFiles\":" + std::wstring(s.clean0ByteFiles ? L"true" : L"false")
                    + L",\"cleanLaunchers\":" + std::wstring(s.cleanLaunchers ? L"true" : L"false")
                    + L",\"cleanBrowsers\":" + std::wstring(s.cleanBrowsers ? L"true" : L"false")
                    + L",\"cleanChats\":" + std::wstring(s.cleanChats ? L"true" : L"false")
                    + L",\"cleanDumps\":" + std::wstring(s.cleanDumps ? L"true" : L"false")
                    + L",\"ramAggressiveness\":" + std::to_wstring(s.ramAggressiveness)
                    + L",\"ramClearStandby\":" + std::wstring(s.ramClearStandby ? L"true" : L"false")
                    + L",\"ramFlushModified\":" + std::wstring(s.ramFlushModified ? L"true" : L"false")
                    + L",\"ramClearWorkingSets\":" + std::wstring(s.ramClearWorkingSets ? L"true" : L"false")
                    + L",\"ramClearFileCache\":" + std::wstring(s.ramClearFileCache ? L"true" : L"false")
                    + L",\"autoBoost\":" + std::wstring(s.autoBoost ? L"true" : L"false")
                    + L",\"disableFSO\":" + std::wstring(s.disableFSO ? L"true" : L"false")
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
                getBool("cleanOldPrefetch", s.cleanOldPrefetch);
                getInt("oldPrefetchDays", s.oldPrefetchDays);
                getBool("cleanRecycle", s.cleanRecycle);
                getBool("cleanThumbs", s.cleanThumbs);
                getBool("cleanShader", s.cleanShader);
                getBool("cleanDns", s.cleanDns);
                getBool("cleanDO", s.cleanDO);
                getBool("cleanFontCache", s.cleanFontCache);
                getBool("cleanStoreCache", s.cleanStoreCache);
                getBool("cleanUpdateCache", s.cleanUpdateCache);
                getBool("clean0ByteFiles", s.clean0ByteFiles);
                getBool("cleanLaunchers", s.cleanLaunchers);
                getBool("cleanBrowsers", s.cleanBrowsers);
                getBool("cleanChats", s.cleanChats);
                getBool("cleanDumps", s.cleanDumps);
                getInt("ramAggressiveness", s.ramAggressiveness);
                getBool("ramClearStandby", s.ramClearStandby);
                getBool("ramFlushModified", s.ramFlushModified);
                getBool("ramClearWorkingSets", s.ramClearWorkingSets);
                getBool("ramClearFileCache", s.ramClearFileCache);
                getBool("autoBoost", s.autoBoost);
                getBool("disableFSO", s.disableFSO);
                if (s.oldPrefetchDays < 1) s.oldPrefetchDays = 1;
                if (s.oldPrefetchDays > 365) s.oldPrefetchDays = 365;
                if (s.ramAggressiveness < 1) s.ramAggressiveness = 1;
                if (s.ramAggressiveness > 5) s.ramAggressiveness = 5;
                { std::lock_guard<std::mutex> lk(g_settingsMutex); g_userSettings = s; }
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
                        L",\"location\":" + JsonStr(v[i].location) + L",\"act\":\"disable\"}";
                }
                SendResp(id, L"{\"items\":" + j + L"]}"); return;
            }
            if (m == L"toggle_autostart") {
                std::wstring name = arg(4), loc = arg(5), act = arg(6);
                bool ok = ToggleAutostart(name, loc, act);
                SendResp(id, ok ? L"{\"ok\":true}" : L"{\"ok\":false}"); return;
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
                            L"ВНИМАНИЕ! Это удалит встроенные приложения и отключит службы.\n\nДействие необратимо. Продолжить?",
                            L"Подтверждение Lite Windows", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);
                        if (msgbox == IDYES) {
                            PerformLiteDebloat();
                            self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":true}"); });
                        }
                        else {
                            self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false,\"error\":\"Cancelled\"}"); });
                        }
                    }
                    catch (...) {
                        self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":false,\"error\":\"exception\"}"); });
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
            if (m == L"open_url") {
                std::wstring u = arg(4);
                ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                SendResp(id, L"{\"ok\":true}"); return;
            }
            if (m == L"download_office") {
                std::wstring ver = arg(4), lang = arg(5);
                if (ver.empty()) ver = L"O365ProPlusRetail";
                if (lang.empty()) lang = L"ru-ru";
                return AsyncData(id, [ver, lang]() -> std::wstring {
                    try {
                        std::wstring dir = g_paths.BackupDir + L"\\OfficeC2R";
                        std::error_code ec{};
                        fs::create_directories(dir, ec);
                        std::wstring url = L"https://officecdn.microsoft.com/db/492350f6-3a01-4f97-b9c0-c7c6ddf67d60/media/" + lang + L"/" + ver + L".img";
                        std::wstring dest = dir + L"\\" + ver + L"_" + lang + L".img";
                        Print(L"[Office] Скачивание: " + url);
                        Print(L"[Office] Это может занять очень долгое время (3-5 ГБ).");
                        WinHttpHandle hS(WinHttpOpen(L"OptPro/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
                        if (!hS) return L"{\"ok\":false}";
                        WinHttpSetTimeouts(hS.get(), 30000, 30000, 30000, 30000);
                        WinHttpHandle hC(WinHttpConnect(hS.get(), L"officecdn.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
                        if (!hC) return L"{\"ok\":false}";
                        std::wstring path = L"/db/492350f6-3a01-4f97-b9c0-c7c6ddf67d60/media/" + lang + L"/" + ver + L".img";
                        WinHttpHandle hR(WinHttpOpenRequest(hC.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
                        if (!hR) return L"{\"ok\":false}";
                        if (!WinHttpSendRequest(hR.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return L"{\"ok\":false}";
                        if (!WinHttpReceiveResponse(hR.get(), nullptr)) return L"{\"ok\":false}";
                        UniqueHandle hFile(CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                        if (!hFile) return L"{\"ok\":false}";
                        std::vector<char> buf(1 << 16); DWORD read = 0; uint64_t total = 0;
                        while (WinHttpReadData(hR.get(), buf.data(), (DWORD)buf.size(), &read) && read > 0) {
                            if (g_stopMonitor) break;
                            DWORD written = 0; WriteFile(hFile.get(), buf.data(), read, &written, nullptr); total += read;
                        }
                        hFile.reset();
                        return L"{\"ok\":true,\"path\":" + JsonStr(dest) + L"}";
                    }
                    catch (...) { return L"{\"ok\":false}"; }
                    });
            }
            if (m == L"set_overlay_setting") {
                std::wstring key = arg(4); std::wstring val = arg(5);
                bool valBool = val == L"1";
                if (key == L"cpu") g_overlay.showCpu = valBool;
                else if (key == L"gpu") g_overlay.showGpu = valBool;
                else if (key == L"ram") g_overlay.showRam = valBool;
                else if (key == L"temp") g_overlay.showTemp = valBool;
                else if (key == L"posx") { g_overlay.SetPosition(SafeStoi(val, 10), g_overlay.posY.load()); SaveOverlaySettings(); }
                else if (key == L"posy") { g_overlay.SetPosition(g_overlay.posX.load(), SafeStoi(val, 10)); SaveOverlaySettings(); }
                else if (key == L"alpha") { g_overlay.SetAlpha(SafeStoi(val, 255)); SaveOverlaySettings(); }
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
            if (m == L"read_hosts") {
                if (g_activeDrive.load() != g_systemDriveLetter) { SendResp(id, L"{\"ok\":false}"); return; }
                std::wstring path = g_paths.SystemRoot + L"\\System32\\drivers\\etc\\hosts";
                std::ifstream f(path, std::ios::binary);
                std::stringstream ss; ss << f.rdbuf();
                SendResp(id, L"{\"ok\":true,\"content\":" + JsonStr(Utf8ToWide(ss.str())) + L"}"); return;
            }
            if (m == L"write_hosts") {
                if (g_activeDrive.load() != g_systemDriveLetter) { SendResp(id, L"{\"ok\":false}"); return; }
                std::wstring content = arg(4);
                std::wstring path = g_paths.SystemRoot + L"\\System32\\drivers\\etc\\hosts";
                std::wstring backup = g_paths.BackupDir + L"\\hosts.bak";
                std::error_code ec{};
                fs::copy_file(path, backup, fs::copy_options::overwrite_existing, ec);
                std::ofstream f(path, std::ios::binary);
                if (f) { std::string utf8 = WideToUtf8(content); f.write(utf8.data(), utf8.size()); f.close(); }

                std::ifstream test(path, std::ios::binary);
                std::stringstream ss; ss << test.rdbuf();
                if (ss.str() != WideToUtf8(content)) {
                    std::error_code rollEc;
                    fs::copy_file(backup, path, fs::copy_options::overwrite_existing, rollEc);
                    SendResp(id, L"{\"ok\":false,\"error\":\"Hosts rolled back\"}");
                    return;
                }
                SendResp(id, L"{\"ok\":true}"); return;
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
                    std::error_code ec{};
                    if (fs::exists(path, ec) && !ec) {
                        for (auto& p : fs::directory_iterator(path, ec)) {
                            if (p.is_directory(ec) && !ec) {
                                std::wstring d = p.path().wstring();
                                futures.push_back(std::async(std::launch::async, [d, mb, &out, &outMutex]() {
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

                    for (auto& kv : bySize) {
                        if (kv.second.size() < 2) continue;
                        for (auto& f : kv.second) {
                            futures.push_back(std::async(std::launch::async, [f, sz = kv.first, &byHash, &mapMutex]() {
                                try {
                                    std::wstring h = HashFileHeader(f, sz);
                                    if (!h.empty()) {
                                        std::lock_guard<std::mutex> lk(mapMutex);
                                        byHash[sz][h].push_back(f);
                                    }
                                }
                                catch (...) {}
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
                        else if (act == L"one_click_maintenance") {
                            CleanCacheEnhanced(); PurgeStandbyEnhanced(); ok = true;
                        }
                        else if (act == L"game_booster_on") { if (!isSys) { ok = false; } else { GameBooster(true); ok = true; } }
                        else if (act == L"game_booster_off") { if (!isSys) { ok = false; } else { GameBooster(false); ok = true; } }
                        else if (act == L"game_mode_on") { SetGameMode(true); ok = true; }
                        else if (act == L"game_mode_off") { SetGameMode(false); ok = true; }
                        else if (act == L"disable_telemetry") { if (!isSys) { ok = false; } else { DisableTelemetry(); ok = true; } }
                        else if (act == L"disable_updates") { if (!isSys) { ok = false; } else { RunCmd(L"sc config wuauserv start= disabled", L"Disable WU"); ok = true; } }
                        else if (act == L"enable_updates") { if (!isSys) { ok = false; } else { RunCmd(L"sc config wuauserv start= auto", L"Enable WU"); ok = true; } }
                        else if (act == L"power_ultimate") { if (!isSys) { ok = false; } else { RunCmd(std::wstring(L"powercfg /setactive ") + ULTIMATE_GUID, L"Power Ultimate"); ok = true; } }
                        else if (act == L"power_high") { if (!isSys) { ok = false; } else { RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"Power High"); ok = true; } }
                        else if (act == L"power_balanced") { if (!isSys) { ok = false; } else { RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Power Balanced"); ok = true; } }
                        else if (act == L"ctx_menu_classic") { if (!isSys) { ok = false; } else { SetClassicContextMenu(true); ok = true; } }
                        else if (act == L"ctx_menu_modern") { if (!isSys) { ok = false; } else { SetClassicContextMenu(false); ok = true; } }
                        else if (act == L"disable_defender") { if (!isSys) { ok = false; } else { DisableDefender(); ok = true; } }
                        else if (act == L"deep_services") { if (!isSys) { ok = false; } else { DisableDeepServices(); ok = true; } }
                        else if (act == L"disable_fso") { if (!isSys) { ok = false; } else { OptimizeFSO(true); ok = true; } }
                        else if (act == L"winget_upgrade_all") { if (!isSys) { ok = false; } else { UpgradeAllWinget(); ok = true; } }
                        else if (act == L"set_fast_dns") { if (!isSys) { ok = false; } else { SetFastDNS(); ok = true; } }
                        else if (act == L"optimize_ssd_trim") { if (!isSys) { ok = false; } else { OptimizeSSDTrim(); ok = true; } }
                        else if (act == L"adv_priority_sep") { SetWin32PrioritySeparation(0x26); ok = true; }
                        else if (act == L"adv_disable_dynamictick") { if (!isSys) { ok = false; } else { DisableDynamictick(); ok = true; } }
                        else if (act == L"adv_disable_net_throttle") { if (!isSys) { ok = false; } else { DisableNetworkThrottling(); ok = true; } }
                        else if (act == L"adv_disable_nagle") { if (!isSys) { ok = false; } else { DisableNagle(); ok = true; } }
                        else if (act == L"adv_clean_winsxs") { if (!isSys) { ok = false; } else { CleanWinSxS(); ok = true; } }
                        else if (act == L"clean_do_cache") { if (!isSys) { ok = false; } else { CleanDeliveryOptimization(); ok = true; } }
                        else if (act == L"launch_afterburner") { LaunchAfterburnerHidden(); ok = true; }
                        else if (act == L"activate_all") { if (!isSys) { ok = false; } else { ActivateWindowsOffice(); ok = true; } }
                        else if (act == L"overlay_toggle") { g_overlay.Toggle(); ok = true; }
                        else if (act == L"disable_paging") {
                            if (!isSys) { ok = false; }
                            else {
                                MEMORYSTATUSEX ms{};
                                ms.dwLength = sizeof(ms);
                                GlobalMemoryStatusEx(&ms);
                                if (ms.ullTotalPhys < 32ULL * 1024 * 1024 * 1024) {
                                    Print(L"[-] RAM < 32GB. Отключение файла подкачки заблокировано для защиты от BSOD.");
                                    self->PostUI([self]() { MessageBoxW(self->hwnd, L"У вас меньше 32 ГБ ОЗУ.\nОтключение файла подкачки может привести к вылетам (BSOD).\nОперация отменена.", L"Защита от дурака", MB_ICONWARNING); });
                                }
                                else {
                                    RunCmd(L"wmic computersystem set AutomaticManagedPagefile=False", L"Disable Paging");
                                    RunCmd(L"wmic pagefileset where name=\"C:\\\\pagefile.sys\" delete", L"Delete pagefile");
                                    ok = true;
                                }
                            }
                        }
                        else if (act == L"disable_mem_comp") {
                            RunCmd(L"powershell -NoProfile -Command \"Disable-MMAgent -MemoryCompression\"", L"Disable Mem Comp");
                            ok = true;
                        }
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
                            else if (o == L"do") {
                                if (active == g_systemDriveLetter) CleanDeliveryOptimization();
                            }
                            else if (o == L"launchers") CleanGameLaunchers();
                            else if (o == L"browsers") CleanBrowserCaches();
                            else if (o == L"chats") CleanChatCaches();
                            else if (o == L"dumps") CleanMemoryDumps();
                        }
                    }
                    catch (...) { allOk = false; }
                    self->PostUI([self, id, allOk]() { if (!self->shuttingDown) self->SendResp(id, L"{\"ok\":" + std::wstring(allOk ? L"true" : L"false") + L"}"); });
                    }).detach();
                return;
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
            mmi->ptMaxPosition.x = mi.rcWork.left;
            mmi->ptMaxPosition.y = mi.rcWork.top;
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
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            g_overlay.Draw();
            QUERY_USER_NOTIFICATION_STATE st;
            bool inGame = false;
            if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
                if (st == QUNS_RUNNING_D3D_FULL_SCREEN || st == QUNS_PRESENTATION_MODE) inGame = true;
            }
            g_overlay.UpdateVisibility(inGame);
            return 0;
        }
        break;
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) { ShowWindow(hWnd, IsIconic(hWnd) ? SW_RESTORE : SW_SHOW); SetForegroundWindow(hWnd); }
        return 0;
    case WM_APP_RUNUI: {
        auto* fn = reinterpret_cast<std::function<void()>*>(lp);
        if (fn) { try { (*fn)(); } catch (...) {} delete fn; }
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hWnd); return 0;
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
static bool RelaunchAsAdmin() {
    wchar_t exe[MAX_PATH] = {}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; sei.lpFile = exe; sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool skipAdmin = false;
    bool startMinimized = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--no-admin") == 0) skipAdmin = true;
        else if (wcscmp(argv[i], L"--silent") == 0) startMinimized = true;
        else if (wcscmp(argv[i], L"--help") == 0) {
            MessageBoxW(nullptr, L"Optimizer Pro CLI Flags:\n--no-admin : Run without admin rights (limited functionality)\n--silent : Start minimized to tray\n--help : Show this message", L"Optimizer Pro Help", MB_OK | MB_ICONINFORMATION);
            if (argv) LocalFree(argv);
            return 0;
        }
    }
    if (argv) LocalFree(argv);

    if (!skipAdmin && !IsAdmin()) {
        if (RelaunchAsAdmin()) return 0;
        return 1;
    }

    EnablePrivilege(SE_TAKE_OWNERSHIP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    (void)CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    InitPaths();
    if (!ShowBootstrapper(hInst)) { CoUninitialize(); return 0; }

    g_overlay.Create(hInst);
    LoadOverlaySettings();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"OptProWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"OptProWnd", L"Optimizer Pro", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) { CoUninitialize(); return 1; }
    ShowWindow(hWnd, startMinimized ? SW_HIDE : SW_SHOW);

    InitTray(hWnd);
    g_stopMonitor = false;
    g_monThread = std::thread(MonitorThread);

    g_app = std::make_shared<App>();
    g_app->Initialize(hWnd);

    SetTimer(hWnd, 1, 1000, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }

    KillTimer(hWnd, 1);
    g_stopMonitor = true;
    if (g_monThread.joinable()) g_monThread.join();

    if (g_app) { g_app->Shutdown(); g_app.reset(); }
    g_overlay.Cleanup();
    CleanupTray();
    CoUninitialize();
    return 0;
}