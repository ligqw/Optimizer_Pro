// =============================================================================
//  Optimizer Pro — v14.2
//  Fixes:
//    - WebView2 WebMessage JSON unescape (команды из UI теперь доходят)
//    - try/catch в фоновых потоках (quick_action, run_cleanup)
//    - JS: увеличен таймаут call() до 1 часа
//    - JS: quickAction/runCleanup/exportSettings — снятие busy + JSON.stringify
//    - RegReadString: защита от граничного переполнения (+2)
//    - FindLargeFiles: корректный break по ecRoot
//    - GetGpuUsage: обработка PDH_CSTATUS_NO_INSTANCE
// =============================================================================
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

#include <cstdio>
#include <cwchar>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <exception>
#include <initializer_list>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <deque>
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
#pragma comment(lib, "WebView2LoaderStatic.lib")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

// ============================================================
// RAII PRIMITIVES & UTILS
// ============================================================
class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE h) noexcept : h_(h) {}
    UniqueHandle(UniqueHandle&& o) noexcept : h_(o.release()) {}
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) reset(o.release());
        return *this;
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    ~UniqueHandle() { reset(); }

    void reset(HANDLE h = nullptr) noexcept {
        if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
        h_ = h;
    }
    HANDLE get() const noexcept { return h_; }
    HANDLE release() noexcept { HANDLE t = h_; h_ = nullptr; return t; }
    bool valid() const noexcept { return h_ && h_ != INVALID_HANDLE_VALUE; }
    explicit operator bool() const noexcept { return valid(); }
private:
    HANDLE h_ = nullptr;
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
private:
    std::vector<std::uint64_t> w_;
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

// ============================================================
// CONSTANTS / GLOBALS
// ============================================================
static const wchar_t* ULTIMATE_GUID = L"e9a42b02-d5df-448d-aa00-03f14749eb61";
static const wchar_t* HIGH_PERF_GUID = L"8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
static const wchar_t* BALANCED_GUID = L"381b4222-f694-41f0-9685-ff5bb260df2e";
static const size_t LOG_MAX_SIZE = 1ull * 1024 * 1024;
static const size_t CMD_OUTPUT_LIMIT = 4ull * 1024 * 1024;

#define WM_TRAYICON    (WM_APP + 100)
#define WM_APP_RUNUI   (WM_APP + 42)

struct Paths { std::wstring SystemRoot, BackupDir, WebView2Dir, LogFile, SystemDrive; };
static Paths g_paths;
static std::mutex g_logMutex;

static std::atomic<double> g_cpuUsage{ 0.0 };
static std::atomic<double> g_gpuUsage{ 0.0 };
static std::atomic<double> g_cpuTemp{ -1.0 };
static std::atomic<double> g_gpuTemp{ -1.0 };
static std::atomic<DWORD>  g_procCount{ 0 };
static std::atomic<bool>   g_stopMonitor{ false };
static std::thread         g_monThread;

static std::wstring g_pcInfoJson = L"{\"cpu\":\"Loading...\",\"gpu\":\"Loading...\",\"ram\":\"Loading...\"}";
static std::mutex g_pcInfoMutex;

// ============================================================
// UTILITIES
// ============================================================
template <typename... Args>
static std::wstring Fmt(const wchar_t* fmt, Args... args) {
    wchar_t buf[512] = {};
    int n = _snwprintf_s(buf, _TRUNCATE, fmt, args...);
    if (n < 0) return std::wstring(buf);
    return std::wstring(buf, static_cast<size_t>(n));
}

static std::wstring DoubleToJsonStr(double v) {
    if (std::isnan(v) || std::isinf(v)) return L"0.0";
    wchar_t buf[64];
    int n = swprintf_s(buf, L"%.1f", v);
    if (n < 0) return L"0.0";
    for (int i = 0; i < n; ++i) if (buf[i] == L',') buf[i] = L'.'; // Fix locale comma
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
    std::wstring o;
    o.reserve(s.size() + 8);
    for (wchar_t c : s) {
        switch (c) {
        case L'"':  o += L"\\\""; break;
        case L'\\': o += L"\\\\"; break;
        case L'\n': o += L"\\n";  break;
        case L'\r': o += L"\\r";  break;
        case L'\t': o += L"\\t";  break;
        default:
            if (c >= 0x20) o += c;
            else o += Fmt(L"\\u%04x", (unsigned)c);
        }
    }
    return o;
}
static std::wstring JsonStr(const std::wstring& s) { return L"\"" + JsonEscape(s) + L"\""; }

// НОВОЕ: разворачивание JSON-строки, возвращённой WebView2
static std::wstring UnescapeJsonString(const std::wstring& j) {
    if (j.size() < 2 || j.front() != L'"' || j.back() != L'"') return j;
    std::wstring o;
    o.reserve(j.size() - 2);
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

static std::wstring EscapeCmdArg(const std::wstring& s) {
    std::wstring o;
    o.reserve(s.size() + 2);
    for (wchar_t c : s) {
        if (c == L'"') o += L"\\\"";
        else o += c;
    }
    return L"\"" + o + L"\"";
}

static std::wstring EscapePowerShellArg(const std::wstring& s) {
    std::wstring o;
    o.reserve(s.size() + 2);
    for (wchar_t c : s) {
        if (c == L'\'') o += L"''";
        else o += c;
    }
    return L"'" + o + L"'";
}

// ============================================================
// INIT PATHS
// ============================================================
static void InitPaths() {
    g_paths.SystemRoot = GetEnvW(L"SystemRoot");
    if (g_paths.SystemRoot.empty()) g_paths.SystemRoot = L"C:\\Windows";

    g_paths.SystemDrive = GetEnvW(L"SystemDrive");
    if (g_paths.SystemDrive.empty()) g_paths.SystemDrive = L"C:";
    g_paths.SystemDrive += L"\\";

    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    if (local.empty()) {
        wchar_t b[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, b))) local = b;
        else local = g_paths.SystemRoot + L"\\Temp";
    }
    g_paths.BackupDir = local + L"\\OptimizerPro";
    g_paths.WebView2Dir = g_paths.BackupDir + L"\\WebView2";
    std::error_code ec;
    fs::create_directories(g_paths.BackupDir, ec);
    fs::create_directories(g_paths.WebView2Dir, ec);
    g_paths.LogFile = g_paths.BackupDir + L"\\optimization_log.txt";
}

// ============================================================
// LOGGING
// ============================================================
static std::mutex g_sinkMutex;
static std::function<void(const std::wstring&)> g_uiLogSink;

static void WriteLogFile(const std::wstring& text) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    try {
        std::error_code ec;
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
    {
        std::lock_guard<std::mutex> lk(g_sinkMutex);
        sink = g_uiLogSink;
    }
    if (sink) { try { sink(text); } catch (...) {} }
}

// ============================================================
// CMD RUNNER
// ============================================================
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

    UniqueHandle hOut(CreateFileW(outFile.path, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    UniqueHandle hErr(CreateFileW(errFile.path, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    UniqueHandle hNul(CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr));

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
    BOOL ok = CreateProcessW(nullptr, clbuf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi);
    UniqueHandle hProcess(pi.hProcess);
    UniqueHandle hThread(pi.hThread);
    if (!ok) return res;

    if (job) AssignProcessToJobObject(job.get(), hProcess.get());
    ResumeThread(hThread.get());
    hThread.reset();

    hOut.reset();
    hErr.reset();
    hNul.reset();

    if (timeoutSec <= 0) timeoutSec = 1;
    DWORD waitMs = (DWORD)timeoutSec * 1000u;

    DWORD wr = WaitForSingleObject(hProcess.get(), waitMs);
    if (wr == WAIT_TIMEOUT) {
        res.timedOut = true;
        if (job) TerminateJobObject(job.get(), 1);
        else TerminateProcess(hProcess.get(), 1);
        WaitForSingleObject(hProcess.get(), 5000);
    }
    GetExitCodeProcess(hProcess.get(), &res.exitCode);

    res.out = OemToUtf8(ReadFileCapped(outFile.path, CMD_OUTPUT_LIMIT));
    res.err = OemToUtf8(ReadFileCapped(errFile.path, CMD_OUTPUT_LIMIT));
    return res;
}

static bool RunCmd(const std::wstring& cmd, const std::wstring& text, bool check = true,
    int timeoutSec = 300, std::initializer_list<DWORD> allowed = { 0 }) {
    if (!text.empty()) Print(L"  [+] " + text + L"...");
    CmdResult r = RunCmdImpl(cmd, timeoutSec);
    if (r.timedOut) { Print(L"      └─ [TIMEOUT]"); return false; }
    for (DWORD a : allowed) {
        if (r.exitCode == a) {
            if (!text.empty()) Print(L"      └─ [OK]");
            return true;
        }
    }
    std::string d = r.err.empty() ? r.out : r.err;
    if (d.empty()) d = "Code " + std::to_string(r.exitCode);
    if (d.size() > 200) d.resize(200);
    Print(std::wstring(L"      └─ [") + (check ? L"ERROR" : L"WARN") + L"] " + Utf8ToWide(d));
    return false;
}

// ============================================================
// PROCESSES
// ============================================================
struct ProcInfo { DWORD pid = 0; std::wstring name; SIZE_T workingSet = 0; };

static DWORD CountProcesses() {
    DWORD c = 0;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do { ++c; } while (Process32NextW(snap.get(), &pe));
    }
    return c;
}

static std::vector<ProcInfo> ListTopProcesses(int topN) {
    std::vector<ProcInfo> v;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return v;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            ProcInfo p;
            p.pid = pe.th32ProcessID;
            p.name = pe.szExeFile;
            p.workingSet = 0;
            UniqueHandle h(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, p.pid));
            if (h) {
                PROCESS_MEMORY_COUNTERS_EX pmc{};
                if (GetProcessMemoryInfo(h.get(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
                    p.workingSet = pmc.WorkingSetSize;
            }
            v.push_back(std::move(p));
        } while (Process32NextW(snap.get(), &pe));
    }
    std::sort(v.begin(), v.end(),
        [](const ProcInfo& a, const ProcInfo& b) { return a.workingSet > b.workingSet; });
    if ((int)v.size() > topN) v.resize((size_t)topN);
    return v;
}

static void EnableDebugPrivilege() {
    UniqueHandle tok;
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &raw)) return;
    tok.reset(raw);
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tok.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr);
}

static int ClearProcessWorkingSets() {
    EnableDebugPrivilege();
    int count = 0;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (pe.th32ProcessID <= 4) continue;
            UniqueHandle h(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pe.th32ProcessID));
            if (h && EmptyWorkingSet(h.get())) ++count;
        } while (Process32NextW(snap.get(), &pe));
    }
    return count;
}

static int DefragmentMemory(int& freedMB) {
    freedMB = 0;
    EnableDebugPrivilege();
    MEMORYSTATUSEX before{};
    before.dwLength = sizeof(before);
    GlobalMemoryStatusEx(&before);

    int count = 0;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (pe.th32ProcessID <= 4) continue;
            UniqueHandle h(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pe.th32ProcessID));
            if (h) {
                SetProcessWorkingSetSizeEx(h.get(), (SIZE_T)-1, (SIZE_T)-1, 0);
                if (EmptyWorkingSet(h.get())) ++count;
            }
        } while (Process32NextW(snap.get(), &pe));
    }
    MEMORYSTATUSEX after{};
    after.dwLength = sizeof(after);
    GlobalMemoryStatusEx(&after);
    freedMB = (int)(((int64_t)after.ullAvailPhys - (int64_t)before.ullAvailPhys) / (1024 * 1024));
    return count;
}

// ============================================================
// WMI HELPERS
// ============================================================
static const long kConnectMaxWait = 0x80;   // WBEM_FLAG_CONNECT_USE_MAX_WAIT

class WmiConnection {
public:
    bool Open(const wchar_t* ns) {
        ComPtr<IWbemLocator> loc;
        HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&loc));
        if (FAILED(hr)) return false;

        ComPtr<IWbemServices> svc;
        hr = loc->ConnectServer(_bstr_t(ns), nullptr, nullptr, nullptr, kConnectMaxWait, nullptr, nullptr, &svc);
        if (FAILED(hr)) return false;

        hr = CoSetProxyBlanket(svc.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) return false;

        svc_ = svc;
        return true;
    }
    IWbemServices* get() const noexcept { return svc_.Get(); }
private:
    ComPtr<IWbemServices> svc_;
};

static bool WmiQueryFirstString(IWbemServices* svc, const wchar_t* wql, const wchar_t* prop, std::wstring& out) {
    if (!svc) return false;
    ComPtr<IEnumWbemClassObject> en;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
    if (FAILED(hr) || !en) return false;

    ComPtr<IWbemClassObject> obj;
    ULONG ret = 0;
    hr = en->Next(4000, 1, &obj, &ret);
    if (FAILED(hr) || ret == 0 || !obj) return false;

    VARIANT v;
    VariantInit(&v);
    bool ok = false;
    if (SUCCEEDED(obj->Get(prop, 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR && v.bstrVal) {
        out = v.bstrVal;
        ok = true;
    }
    VariantClear(&v);
    return ok;
}

static bool WmiQueryMaxThermalZone(IWbemServices* svc, double& celsius) {
    celsius = -1;
    if (!svc) return false;
    ComPtr<IEnumWbemClassObject> en;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"),
        _bstr_t(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
    if (FAILED(hr) || !en) return false;

    double maxT = 0;
    for (;;) {
        ComPtr<IWbemClassObject> obj;
        ULONG ret = 0;
        hr = en->Next(4000, 1, &obj, &ret);
        if (FAILED(hr) || ret == 0 || !obj) break;

        VARIANT v;
        VariantInit(&v);
        if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &v, nullptr, nullptr))) {
            double c = -1;
            if (v.vt == VT_I4) c = (v.lVal / 10.0) - 273.15;
            else if (v.vt == VT_UI4) c = (v.ulVal / 10.0) - 273.15;
            if (c > maxT && c > 0 && c < 150) maxT = c;
        }
        VariantClear(&v);
    }
    if (maxT > 0) { celsius = maxT; return true; }
    return false;
}

// ============================================================
// HARDWARE MONITORING
// ============================================================
static ULONGLONG FileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

class CpuSampler {
public:
    double Sample() {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0;
        ULONGLONG i = FileTimeToULL(idle), k = FileTimeToULL(kernel), u = FileTimeToULL(user);
        if (first_) {
            prevIdle_ = i; prevKernel_ = k; prevUser_ = u;
            first_ = false;
            return 0.0;
        }
        ULONGLONG total = (k - prevKernel_) + (u - prevUser_);
        ULONGLONG idleDiff = i - prevIdle_;
        prevIdle_ = i; prevKernel_ = k; prevUser_ = u;
        if (total == 0) return 0.0;
        double val = (double)(total - idleDiff) * 100.0 / (double)total;
        if (val < 0.0) val = 0.0;
        if (val > 100.0) val = 100.0;
        return val;
    }
private:
    ULONGLONG prevIdle_ = 0, prevKernel_ = 0, prevUser_ = 0;
    bool first_ = true;
};

static double GetGpuUsage() {
    struct State {
        PdhQueryGuard guard;
        PDH_HCOUNTER counter = nullptr;
        bool ready = false;
    };
    static State st;
    static std::once_flag once;

    std::call_once(once, [] {
        PDH_HQUERY q = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS || !q) return;
        st.guard.q = q;

        if (PdhAddEnglishCounterW(q, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &st.counter) != ERROR_SUCCESS) {
            st.counter = nullptr;
        }
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
        AlignedBuffer buf(bufSize);
        DWORD bytes = bufSize, count = itemCount;
        s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bytes, &count,
            buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>());
        if (s == PDH_MORE_DATA) { bufSize = bytes; continue; }
        if (s != ERROR_SUCCESS) return -1.0;

        auto* items = buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>();
        double sum = 0.0;
        for (DWORD i = 0; i < count; ++i) {
            PDH_STATUS cs = items[i].FmtValue.CStatus;
            if (cs == PDH_CSTATUS_VALID_DATA || cs == PDH_CSTATUS_NEW_DATA) {
                sum += items[i].FmtValue.doubleValue;
            }
            // PDH_CSTATUS_NO_INSTANCE и прочие — просто пропускаем, не считаем как ошибку
        }
        if (sum < 0.0) sum = 0.0;
        if (sum > 100.0) sum = 100.0;
        return sum;
    }
    return -1.0;
}

static std::wstring GetPcInfo() {
    std::wstring cpuName = L"Unknown CPU", gpuName = L"Unknown GPU", ramTotal = L"0 GB";

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    ramTotal = Fmt(L"%.1f GB", (double)ms.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));

    try {
        WmiConnection conn;
        if (conn.Open(L"ROOT\\CIMV2")) {
            WmiQueryFirstString(conn.get(), L"SELECT Name FROM Win32_Processor", L"Name", cpuName);
            WmiQueryFirstString(conn.get(),
                L"SELECT Name FROM Win32_VideoController WHERE Name IS NOT NULL", L"Name", gpuName);
        }
    }
    catch (const _com_error&) {}
    catch (...) {}

    return L"{\"cpu\":" + JsonStr(cpuName) + L",\"gpu\":" + JsonStr(gpuName) +
        L",\"ram\":" + JsonStr(ramTotal) + L"}";
}

static bool GetTemperatures(double& cpuTemp, double& gpuTemp) {
    cpuTemp = -1;
    gpuTemp = -1;
    try {
        WmiConnection conn;
        if (!conn.Open(L"ROOT\\WMI")) return false;
        double t = -1;
        if (WmiQueryMaxThermalZone(conn.get(), t)) { cpuTemp = t; return true; }
    }
    catch (const _com_error&) {}
    catch (...) {}
    return false;
}

static void MonitorThread() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool ownCom = SUCCEEDED(hr);

    auto sleepStop = [](int ms) {
        for (int i = 0; i < ms && !g_stopMonitor; i += 100)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        };

    try {
        CpuSampler sampler;
        sleepStop(800);
        sampler.Sample();
        sleepStop(500);
        if (g_stopMonitor) { if (ownCom) CoUninitialize(); return; }

        g_cpuUsage = sampler.Sample();
        g_gpuUsage = GetGpuUsage();
        g_procCount = CountProcesses();

        std::thread([]() {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            try {
                std::wstring info = GetPcInfo();
                std::lock_guard<std::mutex> lk(g_pcInfoMutex);
                g_pcInfoJson = info;
            }
            catch (...) {}
            CoUninitialize();
            }).detach();

        int tick = 0;
        while (!g_stopMonitor) {
            sleepStop(1000);
            if (g_stopMonitor) break;
            g_cpuUsage = sampler.Sample();
            g_gpuUsage = GetGpuUsage();
            if ((++tick % 2) == 0) g_procCount = CountProcesses();
            if ((tick % 10) == 0) {
                double ct = -1, gt = -1;
                if (GetTemperatures(ct, gt)) { g_cpuTemp = ct; g_gpuTemp = gt; }
            }
        }
    }
    catch (...) {
        WriteLogFile(L"[MonitorThread] unhandled exception");
    }
    if (ownCom) CoUninitialize();
}

// ============================================================
// SYSTEM TWEAKS & CLEANUP
// ============================================================
static void CleanTemp() {
    RunCmd(L"del /f /s /q \"%TEMP%\\*\" >nul 2>&1", L"Temp", false);
    RunCmd(L"del /f /s /q \"%SystemRoot%\\Temp\\*\" >nul 2>&1", L"Windows Temp", false);
}
static void CleanPrefetch() { RunCmd(L"del /f /s /q \"%SystemRoot%\\Prefetch\\*\" >nul 2>&1", L"Prefetch", false); }
static void CleanRecycle() {
    RunCmd(L"PowerShell -NoProfile -Command \"Clear-RecycleBin -Force -ErrorAction SilentlyContinue\"", L"Recycle Bin", false, 120);
}
static void CleanThumbnails() {
    RunCmd(L"del /f /s /q \"%LOCALAPPDATA%\\Microsoft\\Windows\\Explorer\\thumbcache_*.db\" >nul 2>&1", L"Thumbnails", false);
}
static void CleanShaderCache() {
    RunCmd(L"del /f /s /q \"%LOCALAPPDATA%\\D3DSCache\\*\" >nul 2>&1", L"Shader cache", false);
    RunCmd(L"del /f /s /q \"%LOCALAPPDATA%\\NVIDIA\\DXCache\\*\" >nul 2>&1", L"NVIDIA cache", false);
}
static void FlushDns() { RunCmd(L"ipconfig /flushdns", L"Flush DNS", false, 30); }
static void RenewIp() {
    RunCmd(L"ipconfig /release", L"Release IP", false, 30);
    RunCmd(L"ipconfig /renew", L"Renew IP", false, 60);
}
static void ResetWinsock() { RunCmd(L"netsh winsock reset", L"Reset Winsock", false, 60); }
static void RunSfc() { RunCmd(L"sfc /scannow", L"SFC", false, 3600); }
static void RunDism() { RunCmd(L"DISM /Online /Cleanup-Image /RestoreHealth", L"DISM", false, 3600); }

static void DisableTelemetry() {
    RunCmd(L"sc config DiagTrack start= disabled >nul 2>&1", L"DiagTrack disabled", false);
    RunCmd(L"sc stop DiagTrack >nul 2>&1", L"DiagTrack stopped", false);
    RunCmd(L"sc config dmwappushservice start= disabled >nul 2>&1", L"dmwappushservice", false);
    RunCmd(L"sc stop dmwappushservice >nul 2>&1", L"dmwappushservice stopped", false);

    HKEY rawKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &rawKey, nullptr) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey);
        DWORD v = 0;
        RegSetValueExW(key.get(), L"AllowTelemetry", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
    RunCmd(L"schtasks /Change /TN \"Microsoft\\Windows\\Application Experience\\Microsoft Compatibility Appraiser\" /Disable >nul 2>&1", L"Appraiser", false);
    RunCmd(L"schtasks /Change /TN \"Microsoft\\Windows\\Customer Experience Improvement Program\\Consolidator\" /Disable >nul 2>&1", L"Consolidator", false);
    RunCmd(L"schtasks /Change /TN \"Microsoft\\Windows\\Feedback\\Siuf\\DmClient\" /Disable >nul 2>&1", L"DmClient", false);
}

static void DisableWindowsUpdate() {
    RunCmd(L"sc config wuauserv start= disabled >nul 2>&1", L"WU service", false);
    RunCmd(L"sc stop wuauserv >nul 2>&1", L"WU stopped", false);
    RunCmd(L"sc config bits start= disabled >nul 2>&1", L"BITS service", false);
    RunCmd(L"sc stop bits >nul 2>&1", L"BITS stopped", false);
}

static void EnableWindowsUpdate() {
    RunCmd(L"sc config wuauserv start= auto >nul 2>&1", L"WU enabled", false);
    RunCmd(L"sc config bits start= delayed-auto >nul 2>&1", L"BITS enabled", false);
}

static void SetGameMode(bool enabled) {
    HKEY rawKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &rawKey, nullptr) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey);
        DWORD v = enabled ? 1 : 0;
        RegSetValueExW(key.get(), L"AutoGameModeEnabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
}

// ============================================================
// AUTOSTART & SERVICES
// ============================================================
struct AutoEntry {
    std::wstring name, command, location;
    bool isRegistry = true;
    std::wstring keyPath, filePath;
    bool isHKLM = false;
};

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
            wchar_t name[512] = {};
            DWORD nameSize = 512;
            if (RegEnumValueW(key.get(), idx++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            DWORD type = 0, size = 0;
            if (RegQueryValueExW(key.get(), name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS)
                continue;
            if (type != REG_SZ || size == 0 || size > 1024 * 1024) continue;

            std::wstring cmd(size / sizeof(wchar_t) + 2, L'\0');
            if (RegQueryValueExW(key.get(), name, nullptr, &type, (LPBYTE)cmd.data(), &size) != ERROR_SUCCESS)
                continue;
            cmd.resize(wcslen(cmd.c_str()));

            AutoEntry e;
            e.name = name;
            e.command = cmd;
            e.location = loc.loc;
            e.isRegistry = true;
            e.keyPath = loc.path;
            e.isHKLM = loc.hklm;
            out.push_back(std::move(e));
        }
    }

    wchar_t startupDir[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupDir))) {
        std::error_code ec;
        for (fs::directory_iterator it(startupDir, fs::directory_options::skip_permission_denied, ec), end;
            !ec && it != end; it.increment(ec)) {
            std::error_code fec;
            if (!it->is_regular_file(fec) || fec) continue;
            AutoEntry e;
            e.name = it->path().filename().wstring();
            e.command = it->path().wstring();
            e.location = L"Startup";
            e.isRegistry = false;
            e.filePath = it->path().wstring();
            out.push_back(std::move(e));
        }
    }
    return out;
}

static bool DeleteAutostartEntry(const AutoEntry& e) {
    if (e.isRegistry) {
        HKEY hive = e.isHKLM ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        HKEY rawKey = nullptr;
        if (RegOpenKeyExW(hive, e.keyPath.c_str(), 0, KEY_SET_VALUE, &rawKey) != ERROR_SUCCESS)
            return false;
        UniqueRegKey key(rawKey);
        return RegDeleteValueW(key.get(), e.name.c_str()) == ERROR_SUCCESS;
    }
    std::error_code ec;
    return fs::remove(e.filePath, ec);
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
        ok = EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
            (LPBYTE)buf.data(), (DWORD)buf.size_bytes(), &needed, &count, &resume, nullptr);
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
        si.state = (st == SERVICE_RUNNING) ? L"running" :
            (st == SERVICE_STOPPED) ? L"stopped" :
            (st == SERVICE_PAUSED) ? L"paused" : L"other";

        ScHandle svc(OpenServiceW(scm.get(), si.name.c_str(), SERVICE_QUERY_CONFIG));
        if (svc) {
            DWORD cfgSize = 0;
            QueryServiceConfigW(svc.get(), nullptr, 0, &cfgSize);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && cfgSize > 0) {
                std::vector<BYTE> cfg(cfgSize);
                auto* qc = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(cfg.data());
                if (QueryServiceConfigW(svc.get(), qc, cfgSize, &cfgSize)) {
                    si.startType = (qc->dwStartType == SERVICE_AUTO_START) ? L"auto" :
                        (qc->dwStartType == SERVICE_DEMAND_START) ? L"manual" :
                        (qc->dwStartType == SERVICE_DISABLED) ? L"disabled" : L"other";
                }
            }
        }
        out.push_back(std::move(si));
    }

    std::sort(out.begin(), out.end(), [](const ServiceInfo& a, const ServiceInfo& b) {
        if (a.state != b.state) return a.state == L"running";
        return _wcsicmp(a.display.c_str(), b.display.c_str()) < 0;
        });
    return out;
}

static bool ControlService2(const std::wstring& name, const std::wstring& action, std::wstring& err) {
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) { err = L"OpenSCManager failed"; return false; }

    DWORD access = SERVICE_QUERY_STATUS;
    if (action == L"start")      access |= SERVICE_START;
    else if (action == L"stop")  access |= SERVICE_STOP;
    else                         access |= SERVICE_CHANGE_CONFIG;

    ScHandle svc(OpenServiceW(scm.get(), name.c_str(), access));
    if (!svc) {
        err = L"OpenService failed (" + std::to_wstring(GetLastError()) + L")";
        return false;
    }

    bool ok = false;
    if (action == L"start") {
        ok = StartServiceW(svc.get(), 0, nullptr) != FALSE;
        if (!ok && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) ok = true;
    }
    else if (action == L"stop") {
        SERVICE_STATUS ss{};
        ok = ControlService(svc.get(), SERVICE_CONTROL_STOP, &ss) != FALSE;
    }
    else {
        DWORD startType = SERVICE_NO_CHANGE;
        if (action == L"auto")         startType = SERVICE_AUTO_START;
        else if (action == L"manual")  startType = SERVICE_DEMAND_START;
        else if (action == L"disable") startType = SERVICE_DISABLED;
        else { err = L"Unknown action"; return false; }

        ok = ChangeServiceConfigW(svc.get(), SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != FALSE;
    }
    if (!ok) err = L"Error " + std::to_wstring(GetLastError());
    return ok;
}

// ============================================================
// FILES / BROWSER / TOOLS
// ============================================================
struct LargeFile { std::wstring path; uint64_t size = 0; };

static std::vector<LargeFile> FindLargeFiles(const std::wstring& root, uint64_t minBytes, int maxResults) {
    std::vector<LargeFile> out;
    std::error_code ecRoot;
    if (!fs::exists(root, ecRoot) || ecRoot) return out;
    try {
        for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
            it != fs::recursive_directory_iterator(); it.increment(ecRoot)) {
            if (g_stopMonitor) break;
            if (ecRoot) { ecRoot.clear(); continue; }
            std::error_code ec;
            if (!it->is_regular_file(ec) || ec) continue;
            auto sz = it->file_size(ec);
            if (ec || sz < minBytes) continue;
            out.push_back({ it->path().wstring(), sz });
            if (out.size() > (size_t)maxResults * 8) break;
        }
    }
    catch (...) {}
    std::sort(out.begin(), out.end(), [](const LargeFile& a, const LargeFile& b) { return a.size > b.size; });
    if ((int)out.size() > maxResults) out.resize((size_t)maxResults);
    return out;
}

static void CleanBrowserCaches() {
    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    struct B { std::wstring path; const wchar_t* name; };
    const B browsers[] = {
        { local + L"\\Google\\Chrome\\User Data\\Default\\Cache", L"Chrome cache" },
        { local + L"\\Google\\Chrome\\User Data\\Default\\Code Cache", L"Chrome code cache" },
        { local + L"\\Microsoft\\Edge\\User Data\\Default\\Cache", L"Edge cache" },
        { local + L"\\Microsoft\\Edge\\User Data\\Default\\Code Cache", L"Edge code cache" },
        { local + L"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Cache", L"Brave cache" },
        { local + L"\\Yandex\\YandexBrowser\\User Data\\Default\\Cache", L"Yandex cache" },
        { local + L"\\Opera Software\\Opera Stable\\Cache", L"Opera cache" },
    };
    for (const auto& b : browsers) {
        std::error_code ec;
        if (g_stopMonitor) break;
        if (fs::exists(b.path, ec) && !ec)
            RunCmd(L"del /f /s /q \"" + b.path + L"\\*\" >nul 2>&1", b.name, false, 120);
    }
}

static std::vector<std::wstring> ListBrowserAutostart() {
    std::vector<std::wstring> out;
    std::wstring local = GetEnvW(L"LOCALAPPDATA");
    struct E { std::wstring path; const wchar_t* label; };
    const E entries[] = {
        { local + L"\\Google\\Chrome\\User Data\\Default\\Extensions", L"Chrome extensions" },
        { local + L"\\Microsoft\\Edge\\User Data\\Default\\Extensions", L"Edge extensions" },
        { local + L"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Extensions", L"Brave extensions" },
    };
    for (const auto& e : entries) {
        std::error_code ec;
        if (!fs::exists(e.path, ec) || ec) continue;
        int n = 0;
        for (fs::directory_iterator it(e.path, fs::directory_options::skip_permission_denied, ec), end;
            !ec && it != end; it.increment(ec)) {
            std::error_code dec;
            if (it->is_directory(dec) && !dec) ++n;
            if (n > 500) break;
        }
        if (n > 0) out.push_back(std::wstring(e.label) + L" (" + std::to_wstring(n) + L" items)");
    }
    return out;
}

static bool CreateRestorePoint(const std::wstring& desc) {
    std::wstring safe = EscapePowerShellArg(desc);
    std::wstring cmd = L"PowerShell -NoProfile -ExecutionPolicy Bypass -Command \"Checkpoint-Computer "
        L"-Description " + safe + L" -RestorePointType 'MODIFY_SETTINGS'\"";
    return RunCmd(cmd, L"Restore point", false, 300, { 0 });
}

static bool TestDiskSpeed(const std::wstring& path, double& writeMBs, double& readMBs) {
    writeMBs = readMBs = 0;
    const size_t SZ = 32u * 1024u * 1024u;

    std::error_code ec;
    fs::path dir(path);
    if (!fs::exists(dir, ec) || ec) return false;
    fs::path testFile = dir / L"optpro_speedtest.tmp";

    std::vector<char> buf(SZ, 'x');
    auto t1 = std::chrono::high_resolution_clock::now();
    {
        std::ofstream f(testFile, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(buf.data(), (std::streamsize)SZ);
        f.flush();
        if (!f.good()) { f.close(); fs::remove(testFile, ec); return false; }
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    double wSec = std::chrono::duration<double>(t2 - t1).count();
    writeMBs = (SZ / 1048576.0) / (wSec > 0.001 ? wSec : 0.001);

    t1 = std::chrono::high_resolution_clock::now();
    {
        std::ifstream f(testFile, std::ios::binary);
        if (!f) { fs::remove(testFile, ec); return false; }
        f.read(buf.data(), (std::streamsize)SZ);
    }
    t2 = std::chrono::high_resolution_clock::now();
    double rSec = std::chrono::duration<double>(t2 - t1).count();
    readMBs = (SZ / 1048576.0) / (rSec > 0.001 ? rSec : 0.001);

    fs::remove(testFile, ec);
    return true;
}

static bool CreateScheduledCleanup(int hours) {
    RunCmd(L"schtasks /Delete /TN \"OptimizerProCleanup\" /F >nul 2>&1", L"", false);
    if (hours < 1) hours = 1;
    if (hours > 168) hours = 168;
    std::wstring cmd = L"schtasks /Create /TN \"OptimizerProCleanup\" /TR \"cmd.exe /c del /f /s /q %TEMP%\\*\" /SC HOURLY /MO " + std::to_wstring(hours) + L" /F /RL HIGHEST";
    return RunCmd(cmd, L"Scheduled cleanup", false, 300, { 0 });
}

static void RemoveScheduledCleanup() {
    RunCmd(L"schtasks /Delete /TN \"OptimizerProCleanup\" /F >nul 2>&1", L"Scheduler removed", false);
}

static bool ShredFile(const std::wstring& path, int passes, std::wstring& err) {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec) { err = L"File not found"; return false; }
    if (passes < 1) passes = 1;
    if (passes > 35) passes = 35;

    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);

    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!f) { err = L"Cannot open"; return false; }

        std::vector<char> buf(65536);
        std::mt19937 gen((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> dis(0, 255);

        for (int p = 0; p < passes; ++p) {
            f.seekp(0);
            for (uint64_t pos = 0; pos < sz; pos += buf.size()) {
                size_t chunk = (size_t)std::min<uint64_t>(buf.size(), sz - pos);
                for (size_t i = 0; i < chunk; ++i) buf[i] = (char)dis(gen);
                f.write(buf.data(), (std::streamsize)chunk);
            }
            f.flush();
        }
        UniqueHandle h(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (h) FlushFileBuffers(h.get());
    }
    fs::remove(path, ec);
    if (ec) { err = L"Remove failed"; return false; }
    return true;
}

struct InstalledApp { std::wstring name, version, uninstallString, location; };

static std::wstring RegReadString(HKEY key, const wchar_t* name) {
    DWORD sz = 0;
    if (RegQueryValueExW(key, name, nullptr, nullptr, nullptr, &sz) != ERROR_SUCCESS) return {};
    if (sz == 0) return {};
    std::wstring buf(sz / sizeof(wchar_t) + 2, L'\0');
    if (RegQueryValueExW(key, name, nullptr, nullptr, (LPBYTE)buf.data(), &sz) != ERROR_SUCCESS) return {};
    buf.resize(wcslen(buf.c_str()));
    return buf;
}

static std::vector<InstalledApp> ListInstalledApps() {
    std::vector<InstalledApp> out;
    struct R { HKEY root; const wchar_t* path; };
    const R roots[] = {
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
        { HKEY_CURRENT_USER,  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall" },
    };
    for (const auto& r : roots) {
        HKEY rawRoot = nullptr;
        if (RegOpenKeyExW(r.root, r.path, 0, KEY_READ, &rawRoot) != ERROR_SUCCESS) continue;
        UniqueRegKey rootKey(rawRoot);

        DWORD i = 0;
        for (;;) {
            wchar_t sub[512] = {};
            DWORD subLen = 512;
            if (RegEnumKeyExW(rootKey.get(), i++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            HKEY rawSub = nullptr;
            if (RegOpenKeyExW(rootKey.get(), sub, 0, KEY_READ, &rawSub) != ERROR_SUCCESS) continue;
            UniqueRegKey subKey(rawSub);

            InstalledApp app;
            app.name = RegReadString(subKey.get(), L"DisplayName");
            app.version = RegReadString(subKey.get(), L"DisplayVersion");
            app.uninstallString = RegReadString(subKey.get(), L"UninstallString");
            app.location = RegReadString(subKey.get(), L"InstallLocation");
            if (!app.name.empty() && !app.uninstallString.empty()) out.push_back(std::move(app));
        }
    }
    std::sort(out.begin(), out.end(), [](const InstalledApp& a, const InstalledApp& b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });
    return out;
}

static int ScanRegistryIssues(std::vector<std::wstring>& report) {
    int issues = 0;
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        0, KEY_READ, &rawKey) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey);

        DWORD i = 0;
        for (;;) {
            wchar_t sub[512] = {};
            DWORD subLen = 512;
            if (RegEnumKeyExW(key.get(), i++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
                break;

            HKEY rawSub = nullptr;
            if (RegOpenKeyExW(key.get(), sub, 0, KEY_READ, &rawSub) != ERROR_SUCCESS) continue;
            UniqueRegKey subKey(rawSub);

            std::wstring us = RegReadString(subKey.get(), L"UninstallString");
            if (us.size() <= 2) continue;
            if (us.find(L"MsiExec.exe") != std::wstring::npos) continue;

            std::wstring exe;
            if (us.front() == L'"') {
                auto end = us.find(L'"', 1);
                if (end != std::wstring::npos) exe = us.substr(1, end - 1);
            }
            else {
                auto sp = us.find(L' ');
                exe = (sp == std::wstring::npos) ? us : us.substr(0, sp);
            }

            std::error_code ec;
            if (!exe.empty() && exe.find(L'%') == std::wstring::npos && !fs::exists(exe, ec)) {
                report.push_back(L"Broken path: " + std::wstring(sub) + L" -> " + exe);
                ++issues;
            }
        }
    }

    std::wstring backup = g_paths.BackupDir + L"\\registry_backup.reg";
    RunCmd(L"reg export HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall \"" + backup +
        L"\" /y >nul 2>&1", L"Backup registry", false, 60);
    report.push_back(L"Backup: " + backup);
    return issues;
}

static std::vector<std::wstring> ListNetworkAdapters() {
    std::vector<std::wstring> out;
    ULONG sz = 0;
    ULONG flags = GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &sz) != ERROR_BUFFER_OVERFLOW || sz == 0)
        return out;

    AlignedBuffer buf(sz);
    auto* addr = buf.as<IP_ADAPTER_ADDRESSES>();
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addr, &sz) != NO_ERROR) return out;

    for (auto* a = addr; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->FriendlyName) out.push_back(a->FriendlyName);
    }
    return out;
}

static bool SetDns(const std::wstring& adapter, const std::vector<std::wstring>& servers) {
    if (servers.empty() || adapter.empty()) return false;
    std::wstring safeAdapter = EscapeCmdArg(adapter);

    CmdResult r1 = RunCmdImpl(L"netsh interface ipv4 set dns name=" + safeAdapter + L" source=static address=" + servers[0] + L" primary >nul 2>&1", 30);
    if (r1.exitCode != 0) return false;

    for (size_t i = 1; i < servers.size(); ++i) {
        RunCmdImpl(L"netsh interface ipv4 add dns name=" + safeAdapter + L" address=" + servers[i] + L" index=" + std::to_wstring(i + 1) + L" >nul 2>&1", 30);
    }
    RunCmdImpl(L"ipconfig /flushdns >nul 2>&1", 30);
    return true;
}

static bool MeasureDownloadSpeed(double& mbps, std::wstring& err) {
    mbps = 0;
    WinHttpHandle hS(WinHttpOpen(L"OptimizerPro/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hS) { err = L"WinHttpOpen"; return false; }
    WinHttpSetTimeouts(hS.get(), 8000, 8000, 15000, 15000);

    WinHttpHandle hC(WinHttpConnect(hS.get(), L"speed.cloudflare.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!hC) { err = L"Connect failed"; return false; }

    WinHttpHandle hR(WinHttpOpenRequest(hC.get(), L"GET", L"/__down?bytes=5000000", nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!hR) { err = L"OpenRequest"; return false; }

    bool ok = WinHttpSendRequest(hR.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE &&
        WinHttpReceiveResponse(hR.get(), nullptr) != FALSE;

    DWORD status = 0, ssz = sizeof(status);
    if (ok) {
        WinHttpQueryHeaders(hR.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &ssz, WINHTTP_NO_HEADER_INDEX);
    }
    if (ok && status != 200) { err = L"HTTP " + std::to_wstring(status); ok = false; }

    auto t1 = std::chrono::high_resolution_clock::now();
    uint64_t totalRead = 0;
    if (ok) {
        std::vector<char> buf(65536);
        DWORD rd = 0;
        while (WinHttpReadData(hR.get(), buf.data(), (DWORD)buf.size(), &rd) && rd > 0) {
            totalRead += rd;
            if (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t1).count() > 20.0)
                break;
        }
    }
    double sec = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t1).count();
    if (sec <= 0.001) sec = 0.001;
    mbps = ((double)totalRead * 8.0 / 1e6) / sec;

    if (!ok || totalRead == 0) {
        if (err.empty()) err = L"No data";
        return false;
    }
    return true;
}

static std::wstring ExportSettingsJson() {
    DWORD gameMode = 0;
    HKEY rawKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", 0, KEY_READ, &rawKey) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey);
        DWORD sz = sizeof(DWORD), type = 0;
        RegQueryValueExW(key.get(), L"AutoGameModeEnabled", nullptr, &type, (LPBYTE)&gameMode, &sz);
    }
    return L"{\"gameMode\":" + std::to_wstring(gameMode) +
        L",\"exported\":" + std::to_wstring((long long)std::time(nullptr)) + L"}";
}

static bool ImportSettingsJson(const std::wstring& json) {
    auto get = [&](const wchar_t* key) -> std::wstring {
        std::wstring k = L"\"" + std::wstring(key) + L"\":";
        auto p = json.find(k);
        if (p == std::wstring::npos) return {};
        p += k.size();
        while (p < json.size() && (json[p] == L' ' || json[p] == L'\t')) p++;
        auto end = json.find_first_of(L",}", p);
        if (end == std::wstring::npos) return {};
        return json.substr(p, end - p);
        };
    std::wstring gm = get(L"gameMode");
    if (!gm.empty()) SetGameMode(gm == L"1");
    return true;
}

static uint64_t HashFile(const std::wstring& path, uint64_t maxBytes = 100ull * 1024 * 1024) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    uint64_t h = 1469598103934665603ULL;
    std::vector<char> buf(65536);
    uint64_t total = 0;
    while (f && total < maxBytes) {
        f.read(buf.data(), buf.size());
        std::streamsize n = f.gcount();
        if (n <= 0) break;
        for (std::streamsize i = 0; i < n; ++i) { h ^= (unsigned char)buf[i]; h *= 1099511628211ULL; }
        total += n;
    }
    return h;
}

struct DupGroup { uint64_t size = 0; std::vector<std::wstring> files; };

static std::vector<DupGroup> FindDuplicates(const std::wstring& root, int maxGroups) {
    std::unordered_map<uint64_t, std::vector<std::wstring>> bySize;
    try {
        for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied);
            it != fs::recursive_directory_iterator(); ++it) {
            if (g_stopMonitor) break;
            std::error_code ec;
            if (!it->is_regular_file(ec) || ec) continue;
            auto sz = it->file_size(ec);
            if (ec || sz < 1024) continue;
            bySize[sz].push_back(it->path().wstring());
        }
    }
    catch (...) {}
    std::vector<DupGroup> result;
    for (auto& [sz, files] : bySize) {
        if (files.size() < 2) continue;
        std::unordered_map<uint64_t, std::vector<std::wstring>> byHash;
        for (auto& f : files) byHash[HashFile(f)].push_back(f);
        for (auto& [h, v] : byHash) {
            if (v.size() >= 2) {
                result.push_back({ sz, v });
                if ((int)result.size() >= maxGroups) return result;
            }
        }
    }
    return result;
}

// ============================================================
// TRAY / OVERLAY
// ============================================================
static NOTIFYICONDATAW g_nid = {};
static ULONGLONG g_lastHighLoadNotify = 0;

static void InitTray(HWND hwnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(g_nid.szTip, L"Optimizer Pro", _TRUNCATE);
    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        WriteLogFile(L"[Tray] Failed to add icon");
    }
}
static void CleanupTray() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }
static void ShowTrayBalloon(const std::wstring& title, const std::wstring& text, DWORD icon = NIIF_INFO) {
    NOTIFYICONDATAW nid = g_nid;
    nid.uFlags = NIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, text.c_str(), _TRUNCATE);
    nid.dwInfoFlags = icon;
    nid.uTimeout = 5000;
    if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) {
        WriteLogFile(L"[Tray] Failed to show balloon");
    }
}

class GameOverlay {
public:
    HWND hwnd = nullptr;
    std::atomic<bool> userEnabled{ true };
    std::atomic<bool> isVisible{ false };
    std::atomic<bool> showCpu{ true };
    std::atomic<bool> showGpu{ true };
    std::atomic<bool> showRam{ true };
    std::atomic<bool> showTemp{ true };
    HFONT hFont = nullptr;

    void Create(HINSTANCE hInst) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = hInst; wc.lpszClassName = L"GameOverlayWnd";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"GameOverlayWnd", L"Overlay", WS_POPUP, 10, 10, 240, 140, nullptr, nullptr, hInst, nullptr);
        if (!hwnd) return;
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        hFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        ShowWindow(hwnd, SW_HIDE);
    }
    void Cleanup() {
        if (hFont) { DeleteObject(hFont); hFont = nullptr; }
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
    }
    void Draw() {
        if (!hwnd || !isVisible) return;
        HDC hdc = GetDC(hwnd); if (!hdc) return;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(0, 255, 100));
        HGDIOBJ old = hFont ? SelectObject(hdc, hFont) : nullptr;
        int y = 10; wchar_t buf[128];

        if (showCpu) { swprintf_s(buf, L"CPU: %.1f%%", g_cpuUsage.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showGpu) { swprintf_s(buf, L"GPU: %.1f%%", g_gpuUsage.load()); TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22; }
        if (showRam) {
            MEMORYSTATUSEX ms = { sizeof(ms) }; GlobalMemoryStatusEx(&ms);
            swprintf_s(buf, L"RAM: %llu MB", ms.ullAvailPhys / (1024 * 1024));
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22;
        }
        if (showTemp && g_cpuTemp.load() > 0) {
            swprintf_s(buf, L"TEMP: %.1f C", g_cpuTemp.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf)); y += 22;
        }

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

// ============================================================
// HTML UI
// ============================================================
static const char* HTML_UI = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><title>Optimizer Pro</title><style>
:root{--bg:#0a0b10;--surface:#12151e;--border:#1f2433;--text:#e2e8f0;--muted:#64748b;--accent:#8b5cf6;--success:#10b981;--error:#ef4444;--warn:#f59e0b;--glass:rgba(18,21,30,.7)}
body[data-theme="light"]{--bg:#f8fafc;--surface:#fff;--border:#e2e8f0;--text:#1e293b;--muted:#64748b;--glass:rgba(255,255,255,.7)}
body[data-theme="amoled"]{--bg:#000;--surface:#0a0a0a;--border:#1a1a1a;--text:#e2e8f0;--muted:#94a3b8;--glass:rgba(0,0,0,.85)}
*{box-sizing:border-box;margin:0;padding:0}html,body{height:100%}
body{font-family:'Segoe UI',sans-serif;background:var(--bg);color:var(--text);overflow:hidden;font-size:14px;user-select:none;transition:background .3s,color .3s}
.titlebar{height:32px;background:var(--surface);display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--border);transition:background .3s}
.titlebar-text{padding-left:15px;font-weight:700;font-size:13px;display:flex;align-items:center;gap:8px;color:var(--accent)}
.titlebar-controls{display:flex;height:100%}
.tb-btn{width:46px;height:100%;background:transparent;border:none;color:#94a3b8;cursor:pointer;display:flex;align-items:center;justify-content:center;font-size:16px;transition:background .15s,color .15s}
.tb-btn:hover{background:var(--border);color:var(--text)}
.tb-btn.close:hover{background:var(--error);color:#fff}
.app{display:grid;grid-template-columns:220px 1fr;height:calc(100vh - 32px);transition:grid-template-columns .3s}
body.compact .app{grid-template-columns:64px 1fr}
.sidebar{backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);background:var(--glass);border-right:1px solid var(--border);padding:20px 0;display:flex;flex-direction:column;overflow-y:auto;overflow-x:hidden;transition:all .3s}
.logo{padding:0 20px 20px;font-size:18px;font-weight:900;display:flex;align-items:center;gap:10px;color:var(--text);white-space:nowrap;overflow:hidden}
body.compact .logo span.lbl{display:none}
.nav-item{padding:11px 20px;cursor:pointer;color:var(--muted);border-left:3px solid transparent;display:flex;gap:12px;align-items:center;transition:all .15s;white-space:nowrap;overflow:hidden}
.nav-item svg{flex-shrink:0;width:18px;height:18px}
.nav-item:hover,.nav-item.active{background:rgba(139,92,246,.1);color:var(--text);border-left-color:var(--accent)}
body.compact .nav-item{padding:11px 0;justify-content:center}
body.compact .nav-item span.lbl{display:none}
.nav-spacer{flex:1}
.main{display:flex;flex-direction:column;overflow:hidden}
.content{flex:1;overflow-y:auto;padding:30px 40px;scrollbar-width:thin}
::-webkit-scrollbar{width:10px;height:10px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:var(--border);border-radius:5px}
::-webkit-scrollbar-thumb:hover{background:var(--accent)}
.tab{display:none;animation:fadeIn .25s ease}
.tab.active{display:block}
@keyframes fadeIn{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:translateY(0)}}
h1{font-size:24px;margin-bottom:6px}h2{font-size:14px;margin:20px 0 12px;color:var(--muted);text-transform:uppercase;letter-spacing:1px}
.subtitle{color:var(--muted);margin-bottom:24px;font-size:13px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:16px;margin-bottom:24px}
@media(max-width:640px){.cards{grid-template-columns:repeat(2,1fr)}.content{padding:20px}}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:18px;position:relative;overflow:hidden;transition:transform .15s,border-color .15s}
.card:hover{border-color:var(--accent)}
.card::before{content:'';position:absolute;top:0;left:0;width:100%;height:2px;background:linear-gradient(90deg,#ff0000,#00ff00,#0000ff,#ff00ff,#ff0000)}
.card-label{color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
.card-value{font-size:22px;font-weight:700}.card-value.good{color:var(--success)}
.section{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:22px;margin-bottom:16px}
button{background:var(--accent);color:#fff;border:none;padding:11px 20px;border-radius:8px;font-weight:600;cursor:pointer;display:inline-flex;align-items:center;gap:8px;transition:background .15s,box-shadow .15s,transform .1s;font-size:13px}
button:hover{background:#7c3aed;box-shadow:0 0 20px rgba(139,92,246,.6)}
button:active{transform:scale(.97)}
button.secondary{background:var(--border);color:var(--text)}
button.secondary:hover{background:var(--muted);box-shadow:none}
button.danger{background:var(--error)}
button.danger:hover{background:#dc2626;box-shadow:0 0 20px rgba(239,68,68,.5)}
button.loading{opacity:.7;pointer-events:none}
.check-item{display:flex;align-items:center;padding:12px 0;border-bottom:1px solid var(--border);cursor:pointer}
.check-item label{margin-left:12px;cursor:pointer;flex:1}
input[type=checkbox]{-webkit-appearance:none;appearance:none;width:18px;height:18px;border:2px solid var(--border);border-radius:4px;cursor:pointer;position:relative;transition:all .15s;background:var(--surface);flex-shrink:0}
input[type=checkbox]:checked{background:var(--accent);border-color:var(--accent)}
input[type=checkbox]:checked::after{content:'';position:absolute;left:4px;top:1px;width:5px;height:9px;border:solid #fff;border-width:0 2px 2px 0;transform:rotate(45deg)}
input[type=text],input[type=number],select{padding:8px 12px;background:var(--surface);border:1px solid var(--border);border-radius:6px;color:var(--text);font-size:13px;outline:none}
input[type=text]:focus,input[type=number]:focus,select:focus{border-color:var(--accent)}
.log{background:#000;border:1px solid var(--border);border-radius:8px;padding:14px;font-family:'Consolas',monospace;font-size:12px;height:calc(100vh - 240px);color:#10b981;overflow-y:auto;user-select:text}
body[data-theme="light"] .log{background:#0f172a}
.status-bar{padding:10px 40px;background:var(--surface);border-top:1px solid var(--border);font-size:12px;color:var(--muted);display:flex;gap:12px;align-items:center;transition:background .3s}
.status-dot{width:9px;height:9px;border-radius:50%;background:var(--success);flex-shrink:0}
.status-dot.busy{background:var(--warn);animation:pulse 1s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
.hint{background:rgba(139,92,246,.1);border-left:3px solid var(--accent);padding:14px;border-radius:0 8px 8px 0;font-size:13px;margin:12px 0}
.list-row{display:flex;align-items:center;padding:12px;background:rgba(127,127,127,.04);border-radius:8px;margin-bottom:8px;gap:10px;transition:background .15s}
.list-row:hover{background:rgba(139,92,246,.08)}
.list-row .info{flex:1;min-width:0}
.list-row .title{font-weight:600;font-size:13px}
.list-row .sub{font-size:11px;color:var(--muted);font-family:Consolas,monospace;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#toasts{position:fixed;bottom:20px;right:20px;z-index:9999;display:flex;flex-direction:column;gap:8px;pointer-events:none}
.toast{background:var(--surface);color:var(--text);padding:12px 18px;border-radius:8px;border-left:3px solid var(--accent);box-shadow:0 8px 24px rgba(0,0,0,.5);max-width:340px;animation:slideIn .25s;pointer-events:auto}
@keyframes slideIn{from{transform:translateX(100%);opacity:0}to{transform:translateX(0);opacity:1}}
.toast.error{border-left-color:var(--error)}
.toast.success{border-left-color:var(--success)}
.skeleton{background:linear-gradient(90deg,var(--surface) 0%,rgba(139,92,246,.15) 50%,var(--surface) 100%);background-size:200% 100%;animation:shimmer 1.5s infinite;border-radius:8px}
@keyframes shimmer{0%{background-position:200% 0}100%{background-position:-200% 0}}
.skeleton-row{height:52px;margin-bottom:8px}
.progress-bar{height:6px;background:var(--border);border-radius:3px;overflow:hidden;margin:12px 0}
.progress-fill{height:100%;background:linear-gradient(90deg,var(--accent),#a78bfa);width:0%;transition:width .3s;border-radius:3px}
[data-tooltip]{position:relative}
[data-tooltip]:hover::after{content:attr(data-tooltip);position:absolute;bottom:calc(100% + 6px);left:50%;transform:translateX(-50%);background:#1e293b;color:#fff;padding:6px 10px;border-radius:6px;font-size:11px;white-space:nowrap;z-index:1000;pointer-events:none;box-shadow:0 4px 12px rgba(0,0,0,.5)}
.chart-wrap{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:16px;margin-bottom:16px}
#loadChart{width:100%;height:180px;display:block}
.theme-switch{display:flex;gap:6px;margin-bottom:20px}
.theme-btn{padding:6px 12px;background:var(--surface);border:1px solid var(--border);border-radius:6px;cursor:pointer;font-size:12px;color:var(--text)}
.theme-btn.active{background:var(--accent);color:#fff;border-color:var(--accent)}
.danger-high{color:var(--error);font-weight:700}
.danger-low{color:var(--success)}
.toolbar{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:15px}
</style></head><body data-theme="dark">
<div class="titlebar" id="titlebar">
<div class="titlebar-text">⚡ Optimizer Pro</div>
<div class="titlebar-controls">
<button class="tb-btn" data-wc="min" data-tooltip="Minimize">—</button>
<button class="tb-btn" data-wc="max" data-tooltip="Maximize">□</button>
<button class="tb-btn close" data-wc="close" data-tooltip="Close">✕</button>
</div>
</div>
<div class="app">
<div class="sidebar">
<div class="logo">⚡<span class="lbl">OPTIMIZER PRO</span></div>
<div class="nav-item active" data-tab="home"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 12l9-9 9 9M5 10v10h14V10"/></svg><span class="lbl" data-i18n="nav_dash">Dashboard</span></div>
<div class="nav-item" data-tab="overlay"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/></svg><span class="lbl" data-i18n="nav_overlay">Overlay</span></div>
<div class="nav-item" data-tab="cleanup"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/></svg><span class="lbl" data-i18n="nav_cleanup">Cleanup</span></div>
<div class="nav-item" data-tab="tweaks"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg><span class="lbl" data-i18n="nav_tweaks">Tweaks</span></div>
<div class="nav-item" data-tab="services"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg><span class="lbl" data-i18n="nav_services">Services</span></div>
<div class="nav-item" data-tab="files"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><path d="M14 2v6h6M16 13H8M16 17H8M10 9H8"/></svg><span class="lbl" data-i18n="nav_files">Files</span></div>
<div class="nav-item" data-tab="browser"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M2 12h20M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/></svg><span class="lbl" data-i18n="nav_browser">Browsers</span></div>
<div class="nav-item" data-tab="tools"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/></svg><span class="lbl" data-i18n="nav_tools">Tools</span></div>
<div class="nav-item" data-tab="autostart"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg><span class="lbl" data-i18n="nav_autostart">Autostart</span></div>
<div class="nav-item" data-tab="processes"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="4" y="4" width="16" height="16" rx="2"/><path d="M9 9h6v6H9zM9 1v3M15 1v3M9 20v3M15 20v3M1 9h3M1 15h3M20 9h3M20 15h3"/></svg><span class="lbl" data-i18n="nav_processes">Processes</span></div>
<div class="nav-item" data-tab="power"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18.36 6.64a9 9 0 1 1-12.73 0M12 2v10"/></svg><span class="lbl" data-i18n="nav_power">Power</span></div>
<div class="nav-item" data-tab="settings"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M12 16v-4M12 8h.01"/></svg><span class="lbl" data-i18n="nav_settings">Settings</span></div>
<div class="nav-item" data-tab="log"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><path d="M14 2v6h6"/></svg><span class="lbl" data-i18n="nav_log">Log</span></div>
<div class="nav-spacer"></div>
</div>
<div class="main">
<div class="content">
<div class="tab active" id="tab-home">
<h1 data-i18n="home_title">Control Panel</h1><p class="subtitle" data-i18n="home_sub">Real-time monitoring</p>
<div class="cards">
<div class="card"><div class="card-label" data-i18n="stat_cpu">CPU</div><div class="card-value" id="stat-cpu">—</div></div>
<div class="card"><div class="card-label" data-i18n="stat_gpu">GPU</div><div class="card-value" id="stat-gpu">—</div></div>
<div class="card"><div class="card-label" data-i18n="stat_ram">RAM Free</div><div class="card-value good" id="stat-ram">—</div></div>
<div class="card"><div class="card-label" data-i18n="stat_procs">Processes</div><div class="card-value" id="stat-procs">—</div></div>
</div>
<div class="chart-wrap"><h2 data-i18n="chart_title">Load (last 60s)</h2><canvas id="loadChart"></canvas></div>
<div class="section"><h2 data-i18n="info_title">System Information</h2>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:12px">
<div><div class="card-label" data-i18n="info_cpu">Processor</div><div id="info-cpu" style="font-weight:600;font-size:13px">Loading...</div></div>
<div><div class="card-label" data-i18n="info_gpu">Video Card</div><div id="info-gpu" style="font-weight:600;font-size:13px">Loading...</div></div>
<div><div class="card-label" data-i18n="info_ram">RAM (Total)</div><div id="info-ram" style="font-weight:600;font-size:13px">Loading...</div></div>
<div><div class="card-label" data-i18n="info_temp">Temperature</div><div id="info-temp" style="font-weight:600;font-size:13px">—</div></div>
</div></div>
<div class="section"><h2 data-i18n="quick_title">Quick Actions</h2>
<div class="toolbar">
<button onclick="quickAction(this,'cleanup_temp')" data-tooltip="Clear %TEMP%">🧹 <span data-i18n="btn_temp">Temp</span></button>
<button onclick="quickAction(this,'flush_dns')" data-tooltip="Reset DNS cache">🌐 <span data-i18n="btn_dns">Flush DNS</span></button>
<button onclick="quickAction(this,'defrag_memory')" data-tooltip="EmptyWorkingSet on all processes">💾 <span data-i18n="btn_defrag">Defrag RAM</span></button>
<button onclick="quickAction(this,'clear_memory')" class="secondary">🗜 <span data-i18n="btn_clear">Clear Memory</span></button>
</div></div>
</div>

<div class="tab" id="tab-overlay">
<h1 data-i18n="nav_overlay">Overlay Settings</h1><p class="subtitle" data-i18n="ov_sub">Appears only in fullscreen games</p>
<div class="section">
<div class="check-item"><input type="checkbox" id="ov-cpu" checked><label for="ov-cpu" data-i18n="ov_cpu">Show CPU</label></div>
<div class="check-item"><input type="checkbox" id="ov-gpu" checked><label for="ov-gpu" data-i18n="ov_gpu">Show GPU</label></div>
<div class="check-item"><input type="checkbox" id="ov-ram" checked><label for="ov-ram" data-i18n="ov_ram">Show RAM</label></div>
<div class="check-item"><input type="checkbox" id="ov-temp" checked><label for="ov-temp" data-i18n="ov_temp">Show Temp</label></div>
</div>
<div class="section">
<button onclick="quickAction(this,'overlay_toggle')">🎯 <span data-i18n="btn_ov_toggle">Toggle Overlay</span></button>
</div>
</div>

<div class="tab" id="tab-cleanup"><h1 data-i18n="nav_cleanup">Cleanup</h1><p class="subtitle" data-i18n="cl_sub">Select components</p>
<div class="section">
<div class="check-item"><input type="checkbox" id="cl-temp" checked><label for="cl-temp">Temp</label></div>
<div class="check-item"><input type="checkbox" id="cl-prefetch" checked><label for="cl-prefetch">Prefetch</label></div>
<div class="check-item"><input type="checkbox" id="cl-recycle" checked><label for="cl-recycle" data-i18n="cl_recycle">Recycle Bin</label></div>
<div class="check-item"><input type="checkbox" id="cl-thumbs" checked><label for="cl-thumbs" data-i18n="cl_thumbs">Thumbnails</label></div>
<div class="check-item"><input type="checkbox" id="cl-shader" checked><label for="cl-shader">Shader Cache</label></div>
<div class="check-item"><input type="checkbox" id="cl-dns" checked><label for="cl-dns">DNS Cache</label></div>
</div>
<div class="progress-bar"><div class="progress-fill" id="cleanup-progress"></div></div>
<button onclick="runCleanup(this)">▶ <span data-i18n="btn_run_cleanup">Run Cleanup</span></button>
</div>

<div class="tab" id="tab-tweaks"><h1 data-i18n="nav_tweaks">Tweaks</h1><p class="subtitle" data-i18n="tw_sub">Telemetry, updates, Game Mode, DNS</p>
<div class="section"><h2 data-i18n="tw_telemetry">Telemetry & Updates</h2>
<div class="toolbar">
<button onclick="quickAction(this,'disable_telemetry')" class="danger" data-i18n="tw_disable_tel">Disable Telemetry</button>
<button onclick="quickAction(this,'disable_updates')" class="danger" data-i18n="tw_disable_upd">Disable Windows Update</button>
<button onclick="quickAction(this,'enable_updates')" class="secondary" data-i18n="tw_enable_upd">Enable Updates</button>
</div></div>
<div class="section"><h2>Game Mode</h2>
<div class="toolbar">
<button onclick="quickAction(this,'game_mode_on')" data-i18n="tw_gm_on">Enable</button>
<button onclick="quickAction(this,'game_mode_off')" class="secondary" data-i18n="tw_gm_off">Disable</button>
</div></div>
<div class="section"><h2>DNS</h2>
<div style="margin-bottom:12px"><select id="dns-adapter" style="min-width:300px"></select></div>
<div class="toolbar">
<button onclick="setDnsPreset(this,'cloudflare')">Cloudflare (1.1.1.1)</button>
<button onclick="setDnsPreset(this,'google')">Google (8.8.8.8)</button>
<button onclick="setDnsPreset(this,'quad9')" class="secondary">Quad9 (9.9.9.9)</button>
</div></div>
</div>

<div class="tab" id="tab-services"><h1 data-i18n="nav_services">Windows Services</h1>
<div class="toolbar"><button onclick="refreshServices(this)" class="secondary">🔄 <span data-i18n="btn_refresh">Refresh</span></button></div>
<div id="services-list"><div class="skeleton skeleton-row"></div><div class="skeleton skeleton-row"></div><div class="skeleton skeleton-row"></div></div>
</div>

<div class="tab" id="tab-files"><h1 data-i18n="nav_files">Files</h1>
<div class="section"><h2 data-i18n="fl_large">Find Large Files</h2>
<div class="toolbar">
<input type="text" id="large-path" placeholder="Path (empty = profile)" style="flex:1;min-width:200px">
<input type="number" id="large-mb" value="100" min="1" style="width:100px">
<button onclick="findLargeFiles(this)" data-i18n="btn_find">Find</button>
</div>
<div id="large-results"></div></div>
<div class="section"><h2 data-i18n="fl_dups">Find Duplicates</h2>
<div class="toolbar">
<input type="text" id="dup-path" placeholder="Path (empty = Downloads)" style="flex:1;min-width:200px">
<button onclick="findDuplicates(this)" data-i18n="btn_scan">Scan</button>
</div>
<div id="dup-results"></div></div>
<div class="section"><h2 data-i18n="fl_shred">File Shredder</h2>
<div class="hint" data-i18n="fl_shred_hint">File will be overwritten 3 times with random data.</div>
<div class="toolbar">
<input type="text" id="shred-path" placeholder="Full path to file" style="flex:1;min-width:200px">
<button onclick="shredFile(this)" class="danger" data-i18n="btn_shred">Shred</button>
</div></div>
</div>

<div class="tab" id="tab-browser"><h1 data-i18n="nav_browser">Browsers</h1>
<div class="section"><h2 data-i18n="br_cache">Browser Cache</h2>
<button onclick="quickAction(this,'clean_browser_cache')">🧹 <span data-i18n="btn_clean_cache">Clean All Cache</span></button></div>
<div class="section"><h2 data-i18n="br_ext">Browser Extensions</h2>
<button onclick="refreshBrowserAutostart(this)" class="secondary">🔄 <span data-i18n="btn_check">Check</span></button>
<div id="browser-autostart" style="margin-top:15px"></div></div>
</div>

<div class="tab" id="tab-tools"><h1 data-i18n="nav_tools">Tools</h1>
<div class="section"><h2 data-i18n="tl_restore">Restore Point</h2>
<div class="toolbar">
<input type="text" id="rp-desc" value="Optimizer Pro checkpoint" style="flex:1;min-width:200px">
<button onclick="createRestorePoint(this)" data-i18n="btn_create">Create</button>
</div></div>
<div class="section"><h2 data-i18n="tl_disk">Disk Speed Test</h2>
<div class="toolbar">
<input type="text" id="disk-path" value="C:\" style="width:120px">
<button onclick="testDiskSpeed(this)" data-i18n="btn_run_test">Run Test</button>
</div><div id="disk-result"></div></div>
<div class="section"><h2 data-i18n="tl_speed">Internet Speed</h2>
<button onclick="measureSpeed(this)">▶ <span data-i18n="btn_measure">Measure Download</span></button><div id="speed-result"></div></div>
<div class="section"><h2 data-i18n="tl_sched">Scheduled Cleanup</h2>
<div class="toolbar">
<input type="number" id="sched-hours" value="6" min="1" max="168" style="width:80px">
<button onclick="createScheduler(this)" data-i18n="btn_sched_create">Create Task</button>
<button onclick="removeScheduler(this)" class="secondary" data-i18n="btn_sched_del">Delete</button>
</div></div>
<div class="section"><h2 data-i18n="tl_installed">Installed Software</h2>
<button onclick="refreshInstalled(this)" class="secondary">🔄 <span data-i18n="btn_list">List</span></button>
<div id="installed-list" style="margin-top:15px"></div></div>
<div class="section"><h2 data-i18n="tl_registry">Registry</h2>
<button onclick="fixRegistry(this)" class="secondary">🔍 <span data-i18n="btn_check_reg">Check Issues</span></button>
<div id="reg-result" style="margin-top:15px"></div></div>
</div>

<div class="tab" id="tab-autostart"><h1 data-i18n="nav_autostart">Autostart</h1>
<div class="toolbar"><button onclick="refreshAutostart(this)" class="secondary">🔄 <span data-i18n="btn_refresh">Refresh</span></button></div>
<div id="autostart-list"><div class="skeleton skeleton-row"></div><div class="skeleton skeleton-row"></div></div>
</div>
<div class="tab" id="tab-processes"><h1 data-i18n="nav_processes">Processes</h1>
<div class="toolbar"><button onclick="refreshProcesses(this)" class="secondary">🔄 <span data-i18n="btn_refresh">Refresh</span></button></div>
<div id="procs-list"><div class="skeleton skeleton-row"></div><div class="skeleton skeleton-row"></div><div class="skeleton skeleton-row"></div></div>
</div>
<div class="tab" id="tab-power"><h1 data-i18n="nav_power">Power Plan</h1>
<div class="section"><div class="toolbar">
<button onclick="quickAction(this,'power_ultimate')">⚡ Ultimate</button>
<button onclick="quickAction(this,'power_high')">High</button>
<button onclick="quickAction(this,'power_balanced')" class="secondary">Balanced</button>
</div></div></div>
<div class="tab" id="tab-settings"><h1 data-i18n="nav_settings">Settings</h1>
<div class="section"><h2 data-i18n="st_lang">Language</h2>
<div style="margin-bottom:12px">
<select id="lang-select" onchange="setLang(this.value)">
<option value="en">English</option>
<option value="ru">Русский</option>
</select>
</div>
<h2 data-i18n="st_theme">Theme</h2>
<div class="theme-switch">
<button class="theme-btn active" data-theme="dark" onclick="setTheme('dark')">🌙 Dark</button>
<button class="theme-btn" data-theme="light" onclick="setTheme('light')">☀️ Light</button>
<button class="theme-btn" data-theme="amoled" onclick="setTheme('amoled')">⬛ AMOLED</button>
</div></div>
<div class="section"><h2 data-i18n="st_ui">Interface</h2>
<div class="check-item"><input type="checkbox" id="opt-compact"><label for="opt-compact" data-i18n="st_compact">Compact Mode (Mini-UI)</label></div>
</div>
<div class="section"><h2 data-i18n="st_export">Settings Export / Import</h2>
<div class="toolbar">
<button onclick="exportSettings(this)" data-i18n="btn_export">📤 Export</button>
<button onclick="importSettings(this)" class="secondary" data-i18n="btn_import">📥 Import</button>
</div></div>
<div class="section"><h2 data-i18n="st_system">System</h2>
<div class="toolbar">
<button onclick="quickAction(this,'sfc')" class="secondary">SFC</button>
<button onclick="quickAction(this,'dism')" class="secondary">DISM</button>
</div></div>
</div>
<div class="tab" id="tab-log"><h1 data-i18n="nav_log">Log</h1>
<div class="log" id="log-container"></div>
</div>
</div>
<div class="status-bar"><div class="status-dot" id="status-dot"></div><span id="status-text" data-i18n="status_ready">Ready</span></div>
</div></div>
<div id="toasts"></div>
<script>
'use strict';
var msgId = 0, pending = {}, API = null, chart = null;
var bootTries = 0;
var currentLang = 'en';

var store = {
  get: function(k, d) { try { return localStorage.getItem(k) || d; } catch(e) { return d; } },
  set: function(k, v) { try { localStorage.setItem(k, v); } catch(e) {} }
};

var i18n = {
  en: {
    nav_dash: "Dashboard", nav_overlay: "Overlay", nav_cleanup: "Cleanup", nav_tweaks: "Tweaks",
    nav_services: "Services", nav_files: "Files", nav_browser: "Browsers", nav_tools: "Tools",
    nav_autostart: "Autostart", nav_processes: "Processes", nav_power: "Power", nav_settings: "Settings", nav_log: "Log",
    home_title: "Control Panel", home_sub: "Real-time monitoring", stat_cpu: "CPU", stat_gpu: "GPU", stat_ram: "RAM Free", stat_procs: "Processes",
    chart_title: "Load (last 60s)", info_title: "System Information", info_cpu: "Processor", info_gpu: "Video Card", info_ram: "RAM (Total)", info_temp: "Temperature",
    quick_title: "Quick Actions", btn_temp: "Temp", btn_dns: "Flush DNS", btn_defrag: "Defrag RAM", btn_clear: "Clear Memory",
    ov_sub: "Appears only in fullscreen games", ov_cpu: "Show CPU", ov_gpu: "Show GPU", ov_ram: "Show RAM", ov_temp: "Show Temp", btn_ov_toggle: "Toggle Overlay",
    cl_sub: "Select components", cl_recycle: "Recycle Bin", cl_thumbs: "Thumbnails", btn_run_cleanup: "Run Cleanup",
    tw_sub: "Telemetry, updates, Game Mode, DNS", tw_telemetry: "Telemetry & Updates", tw_disable_tel: "Disable Telemetry", tw_disable_upd: "Disable Windows Update", tw_enable_upd: "Enable Updates", tw_gm_on: "Enable", tw_gm_off: "Disable",
    btn_refresh: "Refresh", fl_large: "Find Large Files", btn_find: "Find", fl_dups: "Find Duplicates", btn_scan: "Scan", fl_shred: "File Shredder", fl_shred_hint: "File will be overwritten 3 times with random data.", btn_shred: "Shred",
    br_cache: "Browser Cache", btn_clean_cache: "Clean All Cache", br_ext: "Browser Extensions", btn_check: "Check",
    tl_restore: "Restore Point", btn_create: "Create", tl_disk: "Disk Speed Test", btn_run_test: "Run Test", tl_speed: "Internet Speed", btn_measure: "Measure Download", tl_sched: "Scheduled Cleanup", btn_sched_create: "Create Task", btn_sched_del: "Delete", tl_installed: "Installed Software", btn_list: "List", tl_registry: "Registry", btn_check_reg: "Check Issues",
    st_lang: "Language", st_theme: "Theme", st_ui: "Interface", st_compact: "Compact Mode (Mini-UI)", st_export: "Settings Export / Import", btn_export: "Export", btn_import: "Import", st_system: "System",
    status_ready: "Ready", status_running: "Running...", status_loading: "Loading..."
  },
  ru: {
    nav_dash: "Дашборд", nav_overlay: "Оверлей", nav_cleanup: "Очистка", nav_tweaks: "Твики",
    nav_services: "Службы", nav_files: "Файлы", nav_browser: "Браузеры", nav_tools: "Инструменты",
    nav_autostart: "Автозагрузка", nav_processes: "Процессы", nav_power: "Питание", nav_settings: "Настройки", nav_log: "Лог",
    home_title: "Панель управления", home_sub: "Мониторинг в реальном времени", stat_cpu: "CPU", stat_gpu: "GPU", stat_ram: "ОЗУ свободно", stat_procs: "Процессов",
    chart_title: "Загрузка (последние 60 сек)", info_title: "Информация о системе", info_cpu: "Процессор", info_gpu: "Видеокарта", info_ram: "ОЗУ (всего)", info_temp: "Температура",
    quick_title: "Быстрые действия", btn_temp: "Temp", btn_dns: "Flush DNS", btn_defrag: "Defrag RAM", btn_clear: "Очистить память",
    ov_sub: "Появляется только в полноэкранных играх", ov_cpu: "Показывать CPU", ov_gpu: "Показывать GPU", ov_ram: "Показывать ОЗУ", ov_temp: "Показывать температуру", btn_ov_toggle: "Вкл/Выкл оверлей",
    cl_sub: "Выберите компоненты", cl_recycle: "Корзина", cl_thumbs: "Кэш миниатюр", btn_run_cleanup: "Очистить",
    tw_sub: "Телеметрия, обновления, Game Mode, DNS", tw_telemetry: "Телеметрия и обновления", tw_disable_tel: "Отключить телеметрию", tw_disable_upd: "Отключить Windows Update", tw_enable_upd: "Включить обновления", tw_gm_on: "Включить", tw_gm_off: "Выключить",
    btn_refresh: "Обновить", fl_large: "Поиск больших файлов", btn_find: "Найти", fl_dups: "Поиск дубликатов", btn_scan: "Сканировать", fl_shred: "Шредер файлов", fl_shred_hint: "Файл будет перезаписан 3 раза случайными данными и удалён.", btn_shred: "Уничтожить",
    br_cache: "Кэш браузеров", btn_clean_cache: "Очистить весь кэш", br_ext: "Расширения браузеров", btn_check: "Проверить",
    tl_restore: "Точка восстановления", btn_create: "Создать", tl_disk: "Тест скорости диска", btn_run_test: "Запустить", tl_speed: "Скорость интернета", btn_measure: "Замерить загрузку", tl_sched: "Автоочистка по расписанию", btn_sched_create: "Создать задачу", btn_sched_del: "Удалить", tl_installed: "Установленный софт", btn_list: "Список", tl_registry: "Реестр", btn_check_reg: "Проверить ошибки",
    st_lang: "Язык", st_theme: "Тема", st_ui: "Интерфейс", st_compact: "Компактный режим (Mini-UI)", st_export: "Экспорт / Импорт настроек", btn_export: "Экспорт", btn_import: "Импорт", st_system: "Система",
    status_ready: "Готово", status_running: "Выполнение...", status_loading: "Загрузка..."
  }
};

function setLang(l) {
  currentLang = l;
  var dict = i18n[l] || i18n.en;
  document.querySelectorAll('[data-i18n]').forEach(function(el) {
    var k = el.getAttribute('data-i18n');
    if (dict[k]) el.textContent = dict[k];
  });
  store.set('optpro-lang', l);
  var sel = document.getElementById('lang-select');
  if (sel) sel.value = l;
  setTimeout(initChart, 50);
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
        else if (p[0] === 'D') { __done(JSON.parse(p[1])); }
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
    var flat = []; args.forEach(function(a) {
      if (Array.isArray(a)) a.forEach(function(x) { flat.push(String(x)); }); else flat.push(String(a));
    });
    postRaw('C\x1F' + id + '\x1F' + method + '\x1F' + flat.length + '\x1F' + flat.join('\x1F'));
    // ИСПРАВЛЕНО: 1 час — SFC/DISM могут длиться очень долго
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
  if (T) return 'top'; if (B) return 'bottom'; if (L) return 'left'; if (R) return 'right'; return null;
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

function hexToRgba(hex, alpha) {
  var r = parseInt(hex.slice(1, 3), 16),
      g = parseInt(hex.slice(3, 5), 16),
      b = parseInt(hex.slice(5, 7), 16);
  return "rgba(" + r + ", " + g + ", " + b + ", " + alpha + ")";
}

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
      var y = (h / 4) * i; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
      ctx.fillStyle = muted; ctx.font = '10px sans-serif'; ctx.fillText((100 - i * 25) + '%', 4, y + 12);
    }
    function line(arr, color) {
      if (arr.length < 2) return;
      ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath();
      for (var i = 0; i < arr.length; i++) {
        var x = (i / (MAX - 1)) * w;
        var y = h - (arr[i] / 100) * h;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath();
      ctx.fillStyle = hexToRgba(color, 0.12);
      ctx.fill();
    }
    line(cpu, '#8b5cf6');
    line(gpu, '#10b981');
  }
}
window.addEventListener('resize', function() { setTimeout(initChart, 100); });

function toast(m, t) {
  var c = document.getElementById('toasts');
  var e = document.createElement('div'); e.className = 'toast ' + (t || ''); e.textContent = m;
  c.appendChild(e); setTimeout(function() { e.remove(); }, 4000);
}
function __log(text) {
  var c = document.getElementById('log-container'); if (!c) return;
  var l = document.createElement('div'); l.textContent = text; c.appendChild(l); c.scrollTop = c.scrollHeight;
}
function setBusy(btn, busy, text) {
  document.getElementById('status-dot').className = 'status-dot ' + (busy ? 'busy' : '');
  document.getElementById('status-text').textContent = text || (i18n[currentLang].status_ready);
  if (btn) { btn.classList.toggle('loading', busy); btn.disabled = !!busy; }
}
function __done(r) {
  document.querySelectorAll('button').forEach(function(b) { b.disabled = false; b.classList.remove('loading'); });
  document.getElementById('status-dot').className = 'status-dot';
  document.getElementById('status-text').textContent = i18n[currentLang].status_ready;
  if (r && r.ok === false && r.error) toast('Error: ' + r.error, 'error');
  var p = document.getElementById('cleanup-progress');
  if (p) setTimeout(function() { p.style.width = '0%'; }, 800);
}

document.querySelectorAll('.nav-item').forEach(function(el) {
  el.addEventListener('click', function() {
    document.querySelectorAll('.nav-item').forEach(function(e) { e.classList.remove('active'); });
    el.classList.add('active');
    document.querySelectorAll('.tab').forEach(function(t) { t.classList.remove('active'); });
    var tab = document.getElementById('tab-' + el.dataset.tab);
    if (tab) tab.classList.add('active');
    if (el.dataset.tab === 'autostart') refreshAutostart(null);
    if (el.dataset.tab === 'processes') refreshProcesses(null);
    if (el.dataset.tab === 'services') refreshServices(null);
    if (el.dataset.tab === 'tweaks') loadNetworkAdapters();
    if (el.dataset.tab === 'home') setTimeout(initChart, 50);
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
    if (chart) chart.push(cpu, gpu);
    if (s.sys) {
      if ((e = document.getElementById('info-cpu'))) e.textContent = s.sys.cpu || 'Unknown';
      if ((e = document.getElementById('info-gpu'))) e.textContent = s.sys.gpu || 'Unknown';
      if ((e = document.getElementById('info-ram'))) e.textContent = s.sys.ram || 'Unknown';
    }
    if (s.temp && s.temp > 0 && (e = document.getElementById('info-temp'))) e.textContent = s.temp.toFixed(1) + ' °C';
  }).catch(function(err) { console.error('pollStats error:', err); });
}

// ИСПРАВЛЕНО: снимаем busy-state после ответа (через .finally)
function quickAction(btn, action) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('quick_action', action).then(function(r) {
    if (!r || r.started === false || r.ok === false) {
      toast('Failed: ' + (r && r.error ? r.error : 'unknown'), 'error');
    }
  }).catch(function() { toast('Timeout', 'error'); })
    .finally(function() { setBusy(btn, false); });
}
// ИСПРАВЛЕНО: снимаем busy-state и прогресс после завершения
function runCleanup(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  var p = document.getElementById('cleanup-progress');
  var prog = 0; var iv = setInterval(function() { prog += 8; if (prog > 90) prog = 90; if (p) p.style.width = prog + '%'; }, 100);
  var opts = [];
  ['temp', 'prefetch', 'recycle', 'thumbs', 'shader', 'dns'].forEach(function(k) {
    var e = document.getElementById('cl-' + k); if (e && e.checked) opts.push(k);
  });
  call('run_cleanup', opts).then(function(r) {
    if (p) p.style.width = '100%';
    clearInterval(iv);
    if (r && r.ok) toast('Done', 'success');
    else toast('Error during cleanup', 'error');
  }).catch(function() { clearInterval(iv); toast('Timeout', 'error'); })
    .finally(function() { setBusy(btn, false); });
}
function refreshAutostart(btn) {
  if (btn) setBusy(btn, true, i18n[currentLang].status_loading);
  call('list_autostart').then(function(r) {
    var c = document.getElementById('autostart-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">No entries</div>'; return; }
    c.innerHTML = '';
    r.items.forEach(function(it, i) {
      var danger = it.command.toLowerCase().indexOf('temp') >= 0;
      var cls = danger ? 'danger-high' : 'danger-low';
      var row = document.createElement('div'); row.className = 'list-row';
      row.innerHTML = '<div class="info"><div class="title ' + cls + '">' + escapeHtml(it.name) + '</div>' +
        '<div class="sub">' + escapeHtml(it.command) + '</div></div>' +
        '<button class="danger btn-del" data-idx="' + i + '">Delete</button>';
      row.querySelector('.btn-del').onclick = function() {
        if (!confirm('Delete entry?')) return;
        call('delete_autostart', i).then(function(res) {
            if (res.ok) refreshAutostart(null);
            else toast('Failed to delete', 'error');
        });
      };
      c.appendChild(row);
    });
  }).catch(function() {}).finally(function() { if (btn) setBusy(btn, false); });
}
function refreshProcesses(btn) {
  if (btn) setBusy(btn, true, i18n[currentLang].status_loading);
  call('list_processes').then(function(r) {
    var c = document.getElementById('procs-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">No data</div>'; return; }
    var sys = r.items.filter(function(p) { return p.system; });
    var usr = r.items.filter(function(p) { return !p.system; });
    var html = '';
    [['System', sys], ['User', usr]].forEach(function(g) {
      if (!g[1].length) return;
      html += '<h2>' + g[0] + ' (' + g[1].length + ')</h2>';
      g[1].forEach(function(p) {
        var mb = (p.rss / 1048576).toFixed(1);
        html += '<div class="list-row"><div class="info"><div class="title">' + escapeHtml(p.name) +
          ' <span style="color:var(--muted);font-size:11px">PID ' + p.pid + '</span></div>' +
          '<div class="sub">' + mb + ' MB</div></div></div>';
      });
    });
    c.innerHTML = html;
  }).catch(function() {}).finally(function() { if (btn) setBusy(btn, false); });
}
function refreshServices(btn) {
  if (btn) setBusy(btn, true, i18n[currentLang].status_loading);
  call('list_services').then(function(r) {
    var c = document.getElementById('services-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">No services</div>'; return; }
    var html = '';
    r.items.slice(0, 200).forEach(function(s, i) {
      var stateCls = s.state === 'running' ? 'danger-low' : 'danger-high';
      html += '<div class="list-row"><div class="info"><div class="title ' + stateCls + '">' + escapeHtml(s.display) +
        '</div><div class="sub">' + escapeHtml(s.name) + ' • ' + s.startType + '</div></div>' +
        '<button class="secondary svc-btn" data-idx="' + i + '" data-action="' + (s.state === 'running' ? 'stop' : 'start') + '">' +
        (s.state === 'running' ? 'Stop' : 'Start') + '</button>' +
        '<button class="secondary svc-btn" data-idx="' + i + '" data-action="auto">A</button>' +
        '<button class="secondary svc-btn" data-idx="' + i + '" data-action="manual">M</button>' +
        '<button class="danger svc-btn" data-idx="' + i + '" data-action="disable">✕</button></div>';
    });
    c.innerHTML = html;
    var tempSvcData = r.items;
    c.querySelectorAll('.svc-btn').forEach(function(btn) {
      btn.onclick = function() {
        var idx = parseInt(btn.dataset.idx);
        var s = tempSvcData[idx];
        if (s) svcAction(s.name, btn.dataset.action);
      };
    });
  }).catch(function() {}).finally(function() { if (btn) setBusy(btn, false); });
}
function svcAction(name, action) {
  call('control_service', name, action).then(function(r) {
    toast(r.ok ? 'OK' : 'Error: ' + (r.error || ''), r.ok ? 'success' : 'error');
    refreshServices(null);
  }).catch(function() { toast('Timeout', 'error'); });
}
function loadNetworkAdapters() {
  call('list_adapters').then(function(r) {
    var sel = document.getElementById('dns-adapter'); if (!sel) return;
    sel.innerHTML = '';
    (r.items || []).forEach(function(a) {
      var o = document.createElement('option'); o.value = a; o.textContent = a; sel.appendChild(o);
    });
  });
}
function setDnsPreset(btn, preset) {
  var sel = document.getElementById('dns-adapter');
  if (!sel || !sel.value) { toast('Select adapter', 'error'); return; }
  setBusy(btn, true, i18n[currentLang].status_running);
  call('set_dns', sel.value, preset).then(function(r) {
    toast(r.ok ? 'DNS set' : 'Error', r.ok ? 'success' : 'error');
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function findLargeFiles(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  var path = document.getElementById('large-path').value.trim();
  var mb = parseInt(document.getElementById('large-mb').value) || 100;
  call('find_large_files', path, mb).then(function(r) {
    var c = document.getElementById('large-results');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Not found</div>'; return; }
    c.innerHTML = '';
    r.items.forEach(function(f) {
      var sizeMB = (f.size / 1048576).toFixed(1);
      var row = document.createElement('div'); row.className = 'list-row';
      row.innerHTML = '<div class="info"><div class="title">' + sizeMB + ' MB</div><div class="sub">' + escapeHtml(f.path) + '</div></div>';
      c.appendChild(row);
    });
  }).catch(function() {}).finally(function() { setBusy(btn, false); });
}
function findDuplicates(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  var path = document.getElementById('dup-path').value.trim();
  call('find_duplicates', path).then(function(r) {
    var c = document.getElementById('dup-results');
    if (!r.groups || !r.groups.length) { c.innerHTML = '<div class="hint">No duplicates</div>'; return; }
    c.innerHTML = '';
    r.groups.forEach(function(g) {
      var div = document.createElement('div'); div.className = 'section';
      div.innerHTML = '<h2>Size: ' + (g.size / 1024).toFixed(1) + ' KB • ' + g.files.length + ' copies</h2>';
      g.files.forEach(function(f) { div.innerHTML += '<div class="sub" style="padding:6px 0;color:var(--muted)">' + escapeHtml(f) + '</div>'; });
      c.appendChild(div);
    });
  }).catch(function() {}).finally(function() { setBusy(btn, false); });
}
function shredFile(btn) {
  var p = document.getElementById('shred-path').value.trim();
  if (!p) { toast('Enter path', 'error'); return; }
  if (!confirm('Shred file permanently?\n' + p)) return;
  setBusy(btn, true, i18n[currentLang].status_running);
  call('shred_file', p).then(function(r) { toast(r.ok ? 'Shredded' : 'Error: ' + (r.error || ''), r.ok ? 'success' : 'error'); }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function refreshBrowserAutostart(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('list_browser_autostart').then(function(r) {
    var c = document.getElementById('browser-autostart');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">No extensions found</div>'; return; }
    c.innerHTML = '';
    r.items.forEach(function(x) { var d = document.createElement('div'); d.className = 'list-row'; d.innerHTML = '<div class="info"><div class="title">' + escapeHtml(x) + '</div></div>'; c.appendChild(d); });
  }).catch(function() {}).finally(function() { setBusy(btn, false); });
}
function createRestorePoint(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  var d = document.getElementById('rp-desc').value || 'Optimizer Pro';
  call('create_restore_point', d).then(function(r) {
      toast(r.ok ? 'Restore point created' : 'Failed to create restore point', r.ok ? 'success' : 'error');
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function testDiskSpeed(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  var p = document.getElementById('disk-path').value || 'C:\\';
  call('disk_test', p).then(function(r) {
    document.getElementById('disk-result').innerHTML =
      '<div class="hint">Write: ' + (r.write || 0).toFixed(1) + ' MB/s • Read: ' + (r.read || 0).toFixed(1) + ' MB/s</div>';
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function measureSpeed(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('internet_speed').then(function(r) {
    document.getElementById('speed-result').innerHTML =
      r.ok ? '<div class="hint">Download speed: ' + (r.mbps || 0).toFixed(2) + ' Mbps</div>'
           : '<div class="hint" style="border-left-color:var(--error)">Error: ' + (r.error || '') + '</div>';
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function createScheduler(btn) {
  var h = parseInt(document.getElementById('sched-hours').value) || 6;
  setBusy(btn, true, i18n[currentLang].status_running);
  call('create_scheduler', h).then(function(r) {
      toast(r.ok ? 'Task created' : 'Failed to create task', r.ok ? 'success' : 'error');
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function removeScheduler(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('remove_scheduler').then(function(r) {
      toast(r.ok ? 'Task removed' : 'Failed to remove task', r.ok ? 'success' : 'error');
  }).catch(function() { toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}
function refreshInstalled(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('list_installed').then(function(r) {
    var c = document.getElementById('installed-list');
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Not found</div>'; return; }
    c.innerHTML = '';
    r.items.slice(0, 200).forEach(function(a) {
      var row = document.createElement('div'); row.className = 'list-row';
      row.innerHTML = '<div class="info"><div class="title">' + escapeHtml(a.name) + ' <span style="color:var(--muted);font-size:11px">' + escapeHtml(a.version) + '</span></div>' +
        '<div class="sub">' + escapeHtml(a.uninstallString) + '</div></div>';
      c.appendChild(row);
    });
  }).catch(function() {}).finally(function() { setBusy(btn, false); });
}
function fixRegistry(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('fix_registry').then(function(r) {
    var c = document.getElementById('reg-result');
    c.innerHTML = '<div class="hint">Issues found: ' + (r.issues || 0) + '</div>';
    (r.report || []).forEach(function(s) { c.innerHTML += '<div class="sub" style="padding:4px 0;color:var(--muted)">' + escapeHtml(s) + '</div>'; });
  }).catch(function() {}).finally(function() { setBusy(btn, false); });
}
function setTheme(t) {
  document.body.dataset.theme = t;
  document.querySelectorAll('.theme-btn').forEach(function(b) { b.classList.toggle('active', b.dataset.theme === t); });
  store.set('optpro-theme', t);
  setTimeout(initChart, 100);
}
function toggleCompact(on) {
  document.body.classList.toggle('compact', on);
  store.set('optpro-compact', on ? '1' : '0');
  setTimeout(initChart, 100);
}
document.getElementById('opt-compact').addEventListener('change', function(e) { toggleCompact(e.target.checked); });

function setOverlaySetting(key, val) {
  call('set_overlay_setting', key, val ? '1' : '0');
}
document.getElementById('ov-cpu').addEventListener('change', function(e) { setOverlaySetting('cpu', e.target.checked); });
document.getElementById('ov-gpu').addEventListener('change', function(e) { setOverlaySetting('gpu', e.target.checked); });
document.getElementById('ov-ram').addEventListener('change', function(e) { setOverlaySetting('ram', e.target.checked); });
document.getElementById('ov-temp').addEventListener('change', function(e) { setOverlaySetting('temp', e.target.checked); });

// ИСПРАВЛЕНО: используем JSON.stringify(r) вместо несуществующего r.json
function exportSettings(btn) {
  setBusy(btn, true, i18n[currentLang].status_running);
  call('export_settings').then(function(r) {
    prompt("Copy your settings:", JSON.stringify(r, null, 2));
  }).catch(function() { toast('Timeout', 'error'); })
    .finally(function() { setBusy(btn, false); });
}
function importSettings(btn) {
  var j = prompt('Paste JSON:'); if (!j) return;
  call('import_settings', j).then(function() { toast('Imported', 'success'); });
}
function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, function(c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c];
  });
}

function boot() {
  if (!initBridge()) {
    if (++bootTries < 100) { setTimeout(boot, 100); return; }
    document.getElementById('status-text').textContent = 'WebView2 API Error';
    document.getElementById('status-dot').className = 'status-dot busy';
    return;
  }
  var lang = store.get('optpro-lang', 'en');
  setLang(lang);
  var savedTheme = store.get('optpro-theme', 'dark');
  setTheme(savedTheme);
  if (store.get('optpro-compact', '0') === '1') { toggleCompact(true); document.getElementById('opt-compact').checked = true; }
  setInterval(pollStats, 2000);
  pollStats();
  setTimeout(initChart, 200);
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', boot);
} else {
  boot();
}
</script></body></html>)HTML";

// ============================================================
// APP CLASS
// ============================================================
class App : public std::enable_shared_from_this<App> {
public:
    HWND hwnd = nullptr;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::atomic<bool> shuttingDown{ false };
    std::mutex busyLock;
    std::vector<AutoEntry> autostartCache;
    std::mutex autostartMutex;

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
        }
        std::wstring html = Utf8ToWide(HTML_UI);
        auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, g_paths.WebView2Dir.c_str(), options.Get(),
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this, html](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result) || !env) {
                        WriteLogFile(L"[WebView2] env failed: 0x" + std::to_wstring(result));
                        MessageBoxW(hwnd, L"WebView2 runtime not installed.\nhttps://developer.microsoft.com/microsoft-edge/webview2/",
                            L"Optimizer Pro", MB_ICONERROR);
                        return result;
                    }
                    HRESULT ch = env->CreateCoreWebView2Controller(hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this, html](HRESULT r, ICoreWebView2Controller* ctrl) -> HRESULT {
                                if (FAILED(r) || !ctrl) {
                                    WriteLogFile(L"[WebView2] controller failed: 0x" + std::to_wstring(r));
                                    MessageBoxW(hwnd, L"Failed to create WebView2 controller.", L"Error", MB_ICONERROR);
                                    return r;
                                }
                                controller = ctrl;
                                HRESULT gh = controller->get_CoreWebView2(&webview);
                                if (FAILED(gh) || !webview) return gh;
                                RECT rc; GetClientRect(hwnd, &rc);
                                controller->put_Bounds(rc);
                                // ИСПРАВЛЕНО: WebView2 при postMessage(string) сериализует
                                // строку как JSON. get_WebMessageAsJson возвращает "\"W\\u001Fmin\"".
                                // Разворачиваем эту JSON-строку обратно в сырой текст.
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
                                            LPWSTR json = nullptr;
                                            if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json) {
                                                std::wstring s(json); CoTaskMemFree(json);
                                                if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"')
                                                    s = UnescapeJsonString(s);
                                                OnMessage(s);
                                            }
                                            return S_OK;
                                        }).Get(), nullptr);
                                webview->NavigateToString(html.c_str());
                                return S_OK;
                            }).Get());
                    if (FAILED(ch)) WriteLogFile(L"[WebView2] CreateController failed");
                    return S_OK;
                }).Get());
        return SUCCEEDED(hr);
    }

    void Resize(int w, int h) {
        if (w <= 0 || h <= 0) return;
        if (controller) { RECT rc = { 0, 0, w, h }; controller->put_Bounds(rc); }
    }
    void Send(const std::wstring& msg) {
        if (webview && !shuttingDown) {
            HRESULT hr = webview->PostWebMessageAsString(msg.c_str());
            if (FAILED(hr)) WriteLogFile(L"[WebView2] PostWebMessage failed: " + std::to_wstring(hr));
        }
    }
    void SendLog(const std::wstring& s) { Send(L"L\x1F" + s); }
    void SendDone(bool ok, const std::wstring& error = L"") {
        std::wstring j = L"{\"ok\":" + std::wstring(ok ? L"true" : L"false");
        if (!error.empty()) j += L",\"error\":" + JsonStr(error);
        j += L"}";
        Send(L"D\x1F" + j);
    }
    void SendResp(int id, const std::wstring& json) { Send(L"R\x1F" + std::to_wstring(id) + L"\x1F" + json); }

    void Shutdown() {
        shuttingDown = true;
        { std::lock_guard<std::mutex> lk(g_sinkMutex); g_uiLogSink = nullptr; }
        if (controller) { controller->Close(); controller = nullptr; }
        webview = nullptr;
    }

    void HandleWinCtrl(const std::wstring& act) {
        if (!hwnd) return;
        if (act == L"min") ShowWindow(hwnd, SW_MINIMIZE);
        else if (act == L"max") {
            if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
            else ShowWindow(hwnd, SW_MAXIMIZE);
        }
        else if (act == L"close") PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    void HandleDrag() {
        if (!hwnd) return;
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    void HandleResize(const std::wstring& dir) {
        if (!hwnd) return;
        UINT hit = HTCLIENT;
        if (dir == L"topleft") hit = HTTOPLEFT;
        else if (dir == L"topright") hit = HTTOPRIGHT;
        else if (dir == L"bottomleft") hit = HTBOTTOMLEFT;
        else if (dir == L"bottomright") hit = HTBOTTOMRIGHT;
        else if (dir == L"left") hit = HTLEFT;
        else if (dir == L"right") hit = HTRIGHT;
        else if (dir == L"top") hit = HTTOP;
        else if (dir == L"bottom") hit = HTBOTTOM;
        if (hit == HTCLIENT) return;
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, hit, 0);
    }

    void AsyncData(int id, std::function<std::wstring()> fn) {
        auto self = shared_from_this();
        std::thread([self, id, fn]() {
            std::wstring result;
            try { result = fn(); }
            catch (...) { result = L"{\"ok\":false,\"error\":\"exception\"}"; }
            self->PostUI([self, id, result]() { if (!self->shuttingDown) self->SendResp(id, result); });
            }).detach();
    }

    void OnMessage(const std::wstring& raw) {
        int id = -1;
        try {
            if (raw.size() >= 3 && raw[0] == L'W' && raw[1] == L'\x1F') {
                std::wstring payload = raw.substr(2);
                auto p = payload.find(L'\x1F');
                if (p == std::wstring::npos) {
                    if (payload == L"drag") HandleDrag();
                    else HandleWinCtrl(payload);
                }
                else {
                    std::wstring cmd = payload.substr(0, p);
                    std::wstring arg = payload.substr(p + 1);
                    if (cmd == L"resize") HandleResize(arg);
                }
                return;
            }
            if (raw.size() < 4 || raw[0] != L'C') return;
            std::vector<std::wstring> parts;
            std::wstring cur;
            for (wchar_t c : raw) { if (c == L'\x1F') { parts.push_back(cur); cur.clear(); } else cur += c; }
            parts.push_back(cur);

            if (parts.size() < 4) return;
            id = SafeStoi(parts[1], -1);
            if (id < 0) return;

            const std::wstring& m = parts[2];
            auto arg = [&](size_t i) -> std::wstring { return i < parts.size() ? parts[i] : L""; };

            if (m == L"get_status") {
                MEMORYSTATUSEX ms = { sizeof(ms) }; GlobalMemoryStatusEx(&ms);
                DWORD procs = g_procCount.load();
                std::wstring sys;
                { std::lock_guard<std::mutex> lk(g_pcInfoMutex); sys = g_pcInfoJson; }

                std::wstring j = L"{\"cpu\":" + DoubleToJsonStr(g_cpuUsage.load()) +
                    L",\"gpu\":" + DoubleToJsonStr(g_gpuUsage.load()) +
                    L",\"ram_free\":" + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) +
                    L",\"procs\":" + std::to_wstring(procs) +
                    L",\"temp\":" + DoubleToJsonStr(g_cpuTemp.load()) +
                    L",\"sys\":" + sys + L"}";
                SendResp(id, j); return;
            }
            if (m == L"set_overlay_setting") {
                std::wstring key = arg(4);
                bool val = arg(5) == L"1";
                if (key == L"cpu") g_overlay.showCpu = val;
                else if (key == L"gpu") g_overlay.showGpu = val;
                else if (key == L"ram") g_overlay.showRam = val;
                else if (key == L"temp") g_overlay.showTemp = val;
                SendResp(id, L"{\"ok\":true}"); return;
            }
            if (m == L"list_autostart") {
                std::vector<AutoEntry> items;
                { std::lock_guard<std::mutex> lk(autostartMutex); autostartCache = ListAutostart(); items = autostartCache; }
                std::wstring j = L"[";
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i) j += L",";
                    j += L"{\"name\":" + JsonStr(items[i].name) + L",\"command\":" + JsonStr(items[i].command) + L"}";
                }
                j += L"]";
                SendResp(id, L"{\"items\":" + j + L"}"); return;
            }
            if (m == L"list_processes") {
                auto v = ListTopProcesses(40);
                std::wstring j = L"[";
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i) j += L",";
                    bool isSys = v[i].pid < 1000 ||
                        _wcsicmp(v[i].name.c_str(), L"svchost.exe") == 0 ||
                        _wcsicmp(v[i].name.c_str(), L"csrss.exe") == 0 ||
                        _wcsicmp(v[i].name.c_str(), L"wininit.exe") == 0 ||
                        _wcsicmp(v[i].name.c_str(), L"winlogon.exe") == 0 ||
                        _wcsicmp(v[i].name.c_str(), L"services.exe") == 0 ||
                        _wcsicmp(v[i].name.c_str(), L"lsass.exe") == 0 ||
                        _wcsicmp(v[i].name.c_str(), L"smss.exe") == 0;
                    j += L"{\"pid\":" + std::to_wstring(v[i].pid) + L",\"name\":" + JsonStr(v[i].name) +
                        L",\"rss\":" + std::to_wstring((unsigned long long)v[i].workingSet) +
                        L",\"system\":" + (isSys ? L"true" : L"false") + L"}";
                }
                j += L"]";
                SendResp(id, L"{\"items\":" + j + L"}"); return;
            }
            if (m == L"delete_autostart") {
                int idx = SafeStoi(arg(4), -1);
                AutoEntry e; bool have = false;
                {
                    std::lock_guard<std::mutex> lk(autostartMutex);
                    if (idx >= 0 && idx < (int)autostartCache.size()) { e = autostartCache[idx]; have = true; }
                }
                bool ok = have ? DeleteAutostartEntry(e) : false;
                SendResp(id, L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}"); return;
            }
            if (m == L"quick_action") {
                std::wstring act = arg(4);
                auto self = shared_from_this();
                // ИСПРАВЛЕНО: try/catch вокруг всей операции — раньше любое исключение
                // в фоновом потоке приводило к std::terminate() и краху приложения.
                std::thread([self, id, act]() {
                    bool ok = false;
                    try {
                        if (act == L"cleanup_temp") { CleanTemp(); ok = true; }
                        else if (act == L"flush_dns") { FlushDns(); ok = true; }
                        else if (act == L"clear_memory") { ClearProcessWorkingSets(); ok = true; }
                        else if (act == L"defrag_memory") { int freed = 0; DefragmentMemory(freed); ok = true; }
                        else if (act == L"renew_ip") { RenewIp(); ok = true; }
                        else if (act == L"reset_winsock") { ResetWinsock(); ok = true; }
                        else if (act == L"power_ultimate") { RunCmd(std::wstring(L"powercfg /setactive ") + ULTIMATE_GUID, L"Ultimate", false); ok = true; }
                        else if (act == L"power_high") { RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"High", false); ok = true; }
                        else if (act == L"power_balanced") { RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Balanced", false); ok = true; }
                        else if (act == L"sfc") { RunSfc(); ok = true; }
                        else if (act == L"dism") { RunDism(); ok = true; }
                        else if (act == L"overlay_toggle") { g_overlay.Toggle(); ok = true; }
                        else if (act == L"disable_telemetry") { DisableTelemetry(); ok = true; }
                        else if (act == L"disable_updates") { DisableWindowsUpdate(); ok = true; }
                        else if (act == L"enable_updates") { EnableWindowsUpdate(); ok = true; }
                        else if (act == L"game_mode_on") { SetGameMode(true); ok = true; }
                        else if (act == L"game_mode_off") { SetGameMode(false); ok = true; }
                        else if (act == L"clean_browser_cache") { CleanBrowserCaches(); ok = true; }
                    }
                    catch (...) { ok = false; }

                    self->PostUI([self, id, ok]() {
                        if (!self->shuttingDown)
                            self->SendResp(id, L"{\"started\":true,\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}");
                        });
                    }).detach();
                return;
            }
            if (m == L"run_cleanup") {
                std::vector<std::wstring> ops;
                for (size_t i = 4; i < parts.size(); ++i) ops.push_back(parts[i]);
                auto self = shared_from_this();
                // ИСПРАВЛЕНО: try/catch — при исключении UI получал вечный busy-state.
                std::thread([self, id, ops]() {
                    bool allOk = true;
                    try {
                        for (auto& o : ops) {
                            if (o == L"temp") CleanTemp();
                            else if (o == L"prefetch") CleanPrefetch();
                            else if (o == L"recycle") CleanRecycle();
                            else if (o == L"thumbs") CleanThumbnails();
                            else if (o == L"shader") CleanShaderCache();
                            else if (o == L"dns") FlushDns();
                        }
                    }
                    catch (...) { allOk = false; }
                    self->PostUI([self, id, allOk]() {
                        if (!self->shuttingDown)
                            self->SendResp(id, L"{\"started\":true,\"ok\":" + std::wstring(allOk ? L"true" : L"false") + L"}");
                        });
                    }).detach();
                return;
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
                    j += L"]";
                    return L"{\"items\":" + j + L"}";
                    });
            }
            if (m == L"control_service") {
                std::wstring name = arg(4), action = arg(5);
                return AsyncData(id, [name, action]() -> std::wstring {
                    std::wstring err;
                    bool ok = ControlService2(name, action, err);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") +
                        (ok ? L"" : L",\"error\":" + JsonStr(err)) + L"}";
                    });
            }
            if (m == L"list_adapters") {
                return AsyncData(id, []() -> std::wstring {
                    auto a = ListNetworkAdapters();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < a.size(); ++i) { if (i) j += L","; j += JsonStr(a[i]); }
                    j += L"]";
                    return L"{\"items\":" + j + L"}";
                    });
            }
            if (m == L"set_dns") {
                std::wstring adapter = arg(4), preset = arg(5);
                return AsyncData(id, [adapter, preset]() -> std::wstring {
                    std::vector<std::wstring> servers;
                    if (preset == L"cloudflare") servers = { L"1.1.1.1", L"1.0.0.1" };
                    else if (preset == L"google") servers = { L"8.8.8.8", L"8.8.4.4" };
                    else if (preset == L"quad9") servers = { L"9.9.9.9", L"149.112.112.112" };
                    bool ok = SetDns(adapter, servers);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"find_large_files") {
                std::wstring path = arg(4);
                if (path.empty()) path = GetEnvW(L"USERPROFILE");
                int mb = SafeStoi(arg(5), 100);
                return AsyncData(id, [path, mb]() -> std::wstring {
                    auto files = FindLargeFiles(path, (uint64_t)mb * 1024 * 1024, 100);
                    std::wstring j = L"[";
                    for (size_t i = 0; i < files.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"path\":" + JsonStr(files[i].path) + L",\"size\":" + std::to_wstring(files[i].size) + L"}";
                    }
                    j += L"]";
                    return L"{\"items\":" + j + L"}";
                    });
            }
            if (m == L"find_duplicates") {
                std::wstring path = arg(4);
                if (path.empty()) path = GetEnvW(L"USERPROFILE") + L"\\Downloads";
                return AsyncData(id, [path]() -> std::wstring {
                    auto groups = FindDuplicates(path, 20);
                    std::wstring j = L"[";
                    for (size_t i = 0; i < groups.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"size\":" + std::to_wstring(groups[i].size) + L",\"files\":[";
                        for (size_t k = 0; k < groups[i].files.size(); ++k) {
                            if (k) j += L",";
                            j += JsonStr(groups[i].files[k]);
                        }
                        j += L"]}";
                    }
                    j += L"]";
                    return L"{\"groups\":" + j + L"}";
                    });
            }
            if (m == L"shred_file") {
                std::wstring path = arg(4);
                return AsyncData(id, [path]() -> std::wstring {
                    std::wstring err;
                    bool ok = ShredFile(path, 3, err);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") +
                        (ok ? L"" : L",\"error\":" + JsonStr(err)) + L"}";
                    });
            }
            if (m == L"list_installed") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ListInstalledApps();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(v[i].name) + L",\"version\":" + JsonStr(v[i].version) +
                            L",\"uninstallString\":" + JsonStr(v[i].uninstallString) + L"}";
                    }
                    j += L"]";
                    return L"{\"items\":" + j + L"}";
                    });
            }
            if (m == L"fix_registry") {
                return AsyncData(id, []() -> std::wstring {
                    std::vector<std::wstring> report;
                    int issues = ScanRegistryIssues(report);
                    std::wstring j = L"[";
                    for (size_t i = 0; i < report.size(); ++i) {
                        if (i) j += L","; j += JsonStr(report[i]);
                    }
                    j += L"]";
                    return L"{\"issues\":" + std::to_wstring(issues) + L",\"report\":" + j + L"}";
                    });
            }
            if (m == L"list_browser_autostart") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ListBrowserAutostart();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) { if (i) j += L","; j += JsonStr(v[i]); }
                    j += L"]";
                    return L"{\"items\":" + j + L"}";
                    });
            }
            if (m == L"create_restore_point") {
                std::wstring desc = arg(4);
                return AsyncData(id, [desc]() -> std::wstring {
                    bool ok = CreateRestorePoint(desc);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"disk_test") {
                std::wstring path = arg(4);
                if (path.empty()) path = L"C:\\";
                return AsyncData(id, [path]() -> std::wstring {
                    double w = 0, r = 0;
                    bool ok = TestDiskSpeed(path, w, r);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") +
                        L",\"write\":" + DoubleToJsonStr(w) +
                        L",\"read\":" + DoubleToJsonStr(r) + L"}";
                    });
            }
            if (m == L"internet_speed") {
                return AsyncData(id, []() -> std::wstring {
                    double mbps = 0; std::wstring err;
                    bool ok = MeasureDownloadSpeed(mbps, err);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") +
                        L",\"mbps\":" + DoubleToJsonStr(mbps) +
                        L",\"error\":" + JsonStr(err) + L"}";
                    });
            }
            if (m == L"create_scheduler") {
                int h = SafeStoi(arg(4), 6);
                return AsyncData(id, [h]() -> std::wstring {
                    bool ok = CreateScheduledCleanup(h);
                    return L"{\"ok\":" + std::wstring(ok ? L"true" : L"false") + L"}";
                    });
            }
            if (m == L"remove_scheduler") {
                return AsyncData(id, []() -> std::wstring {
                    RemoveScheduledCleanup();
                    return L"{\"ok\":true}";
                    });
            }
            if (m == L"export_settings") {
                std::wstring j = ExportSettingsJson();
                SendResp(id, j);
                return;
            }
            if (m == L"import_settings") {
                ImportSettingsJson(arg(4));
                SendResp(id, L"{\"ok\":true}"); return;
            }
            SendResp(id, L"{\"ok\":false}");
        }
        catch (const std::exception& e) {
            WriteLogFile(L"[OnMessage] " + Utf8ToWide(e.what()));
            if (id >= 0) { SendResp(id, L"{\"ok\":false}"); SendDone(false, L"error"); }
        }
        catch (...) {
            if (id >= 0) { SendResp(id, L"{\"ok\":false}"); SendDone(false, L"error"); }
        }
    }
};

// ============================================================
// MAIN WINDOW
// ============================================================
static std::shared_ptr<App> g_app;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp == TRUE) {
            WINDOWPLACEMENT wp_ = { sizeof(wp_) };
            GetWindowPlacement(hWnd, &wp_);
            if (wp_.showCmd == SW_MAXIMIZE) {
                return DefWindowProcW(hWnd, msg, wp, lp);
            }
            return 0;
        }
        break;

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = 800;
        mmi->ptMinTrackSize.y = 600;
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
            if (!inGame) {
                HWND fg = GetForegroundWindow();
                if (fg && fg != hWnd && fg != g_overlay.hwnd) {
                    RECT r; GetWindowRect(fg, &r);
                    HMONITOR hm = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfo(hm, &mi);
                    if (r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top &&
                        r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom) inGame = true;
                }
            }
            g_overlay.UpdateVisibility(inGame);

            if (g_cpuUsage.load() > 90.0) {
                ULONGLONG now = GetTickCount64();
                if (now - g_lastHighLoadNotify > 5 * 60 * 1000) {
                    g_lastHighLoadNotify = now;
                    wchar_t msg[128];
                    swprintf_s(msg, L"CPU usage is %.0f%%", g_cpuUsage.load());
                    ShowTrayBalloon(L"High Load", msg, NIIF_WARNING);
                }
            }
            return 0;
        }
        break;

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, IsIconic(hWnd) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(hWnd);
        }
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
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &id)) {
        CheckTokenMembership(nullptr, id, &a); FreeSid(id);
    }
    return a != FALSE;
}
static bool RelaunchAsAdmin() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; sei.lpFile = exe; sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        wchar_t b[256];
        swprintf_s(b, L"[CRASH] Code=0x%08X Addr=%p",
            ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
        WriteLogFile(b);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// ============================================================
// ENTRY POINT
// ============================================================
int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    SetUnhandledExceptionFilter(CrashHandler);
    if (!IsAdmin()) { if (RelaunchAsAdmin()) return 0; return 1; }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    (void)CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);

    InitPaths();
    g_overlay.Create(hInst);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"OptProWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"OptProWnd", L"Optimizer Pro",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) { CoUninitialize(); return 1; }
    ShowWindow(hWnd, SW_SHOW);

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