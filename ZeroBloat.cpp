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
#define WEBVIEW2_STATIC
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wincrypt.h>
#include <dpapi.h>
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
#include <dbghelp.h>
#include <objbase.h>
#include <objidl.h>
#include <sddl.h>
#include <mscat.h>
#include <winternl.h>

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <cstdlib>
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

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

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
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")
#pragma comment(linker, "/NODEFAULTLIB:WebView2Loader.lib")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")
#pragma comment(linker, "/DYNAMICBASE /HIGHENTROPYVA /guard:cf")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

#define APP_VERSION L"15.0.0"

static HICON g_hIconBig = nullptr;
static HICON g_hIconSmall = nullptr;

constexpr UINT WM_TRAYICON = WM_APP + 100;
constexpr UINT WM_APP_RUNUI = WM_APP + 42;
constexpr UINT WM_BOOT_DONE = WM_APP + 43;
constexpr UINT WM_APP_MSG = WM_APP + 44;
constexpr UINT WM_APP_PING = WM_APP + 45;

struct Paths {
    std::wstring SystemRoot, BackupDir, WebView2Dir, ThemesDir, AppDir, LogFile, SystemDrive, ConfigFile, AuditFile, UiDir, AiConfigFile, DevAccessFile, HistoryFile, ProfilesFile, RollbackFile, SdeDir;
};
static Paths g_paths;

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
    return std::wstring(buf, static_cast<size_t>(n));
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
    catch (const std::exception&) { return def; }
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
        default: if (c >= 0x20) o += c; else o += Fmt(L"\\u%04x", (unsigned)c);
        }
    }
    return o;
}
static std::wstring JsonStr(const std::wstring& s) { return L"\"" + JsonEscape(s) + L"\""; }

static std::wstring ErrCode(const wchar_t* code, const std::wstring& msg) {
    return L"{\"ok\":false,\"code\":" + JsonStr(code) + L",\"message\":" + JsonStr(msg) + L"}";
}
static std::wstring OkJson() { return L"{\"ok\":true}"; }

static std::mutex g_logMutex;
static std::mutex g_cleanMutex;
static std::mutex g_boostMutex;
static std::mutex g_auditMutex;
static std::mutex g_rollbackMutex;

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
static std::thread         g_pingThread;
static std::thread         g_bsodMonitorThread;

static std::atomic<float> g_appCpuUsage{ 0.0f };
static std::atomic<float> g_appGpuUsage{ 0.0f };
static std::atomic<uint64_t> g_appRamMB{ 0 };

static std::atomic<float> g_netDownMbps{ 0.0f };
static std::atomic<float> g_netUpMbps{ 0.0f };
static std::atomic<int>    g_pingMs{ -1 };
static std::atomic<float> g_batteryPercent{ -1.0f };
static std::atomic<bool>  g_isAcConnected{ true };
static std::atomic<float> g_dpcIsrTime{ 0.0f };

static std::atomic<bool>   g_portableMode{ false };
static std::atomic<bool>   g_cacheTimerEnabled{ false };
static std::atomic<bool>   g_betaAccessEnabled{ false };
static std::atomic<bool>   g_isBetaBuild{ false };
static std::atomic<bool>   g_dryRunMode{ false };
static std::wstring g_betaUpdateUrl = L"";

static std::wstring g_pcInfoJson = L"{\"cpu\":\"Loading...\",\"gpu\":\"Loading...\",\"ram\":\"Loading...\"}";
static std::mutex g_pcInfoMutex;

static std::wstring g_diskHealthJson = L"[]";
static std::mutex g_diskHealthMutex;

static std::atomic<wchar_t> g_activeDrive{ L'C' };
static wchar_t g_systemDriveLetter = L'C';

static std::atomic<int> g_activeAsyncTasks = 0;
static std::condition_variable g_asyncCv;
static std::mutex g_asyncMutex;

struct AiConfig {
    std::wstring endpoint = L"https://api.openai.com/v1/chat/completions";
    std::wstring apiKey;
    std::wstring model = L"gpt-4o-mini";
    bool enabled = false;
    bool useLocal = false;
};
static AiConfig g_aiConfig;
static std::mutex g_aiMutex;

struct CpuFeatures {
    bool sse41 = false, sse42 = false, popcnt = false;
    bool avx = false, fma3 = false, avx2 = false;
    bool bmi1 = false, bmi2 = false, avx512 = false;
};

static CpuFeatures GetCpuFeatures() {
    CpuFeatures f;
    int info[4];
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

static const std::vector<std::wstring> kCriticalProcesses = {
    L"system", L"idle", L"registry", L"memory compression",
    L"csrss.exe", L"smss.exe", L"wininit.exe", L"services.exe",
    L"lsass.exe", L"winlogon.exe", L"svchost.exe", L"fontdrvhost.exe",
    L"dwm.exe", L"explorer.exe", L"audiodg.exe"
};

static bool IsCriticalProcess(const std::wstring& name) {
    std::wstring lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    for (const auto& c : kCriticalProcesses) if (lower == c) return true;
    return false;
}

static bool IsWindowsVersionSupported() {
    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    typedef struct _RTL_OSVERSIONINFOW_LOCAL {
        ULONG dwOSVersionInfoSize;
        ULONG dwMajorVersion;
        ULONG dwMinorVersion;
        ULONG dwBuildNumber;
        ULONG dwPlatformId;
        WCHAR szCSDVersion[128];
    } RTL_OSVERSIONINFOW_LOCAL, * PRTL_OSVERSIONINFOW_LOCAL;

    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        auto RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (RtlGetVersion) {
            RTL_OSVERSIONINFOW_LOCAL rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (RtlGetVersion((PRTL_OSVERSIONINFOW)&rovi) == 0) {
                if (rovi.dwMajorVersion > 10) return true;
                if (rovi.dwMajorVersion == 10 && rovi.dwBuildNumber >= 17763) return true;
                return false;
            }
        }
    }
    return true;
}

static LONG WINAPI AppUnhandledExceptionFilter(EXCEPTION_POINTERS* pExceptionInfo) {
    wchar_t dumpPath[MAX_PATH];
    swprintf_s(dumpPath, L"%s\\crash_%lld.dmp", g_paths.BackupDir.c_str(), (long long)time(nullptr));

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = pExceptionInfo;
        mdei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &mdei, nullptr, nullptr);
        CloseHandle(hFile);
    }
    MessageBoxW(nullptr, L"Критическая ошибка! Приложение будет закрыто.\nДамп сохранен в папке с программой.", L"ZeroBloat Crash", MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

class JsonParser {
public:
    static std::map<std::wstring, std::wstring> ParseObject(const std::wstring& json) {
        std::map<std::wstring, std::wstring> result;
        size_t pos = 0;
        auto skipWs = [&]() { while (pos < json.size() && iswspace(json[pos])) pos++; };
        skipWs();
        if (pos >= json.size() || json[pos] != L'{') return result;
        pos++;
        while (pos < json.size()) {
            skipWs();
            if (pos >= json.size() || json[pos] == L'}') break;
            if (json[pos] != L'"') break;
            pos++;
            std::wstring key;
            while (pos < json.size() && json[pos] != L'"') {
                if (json[pos] == L'\\' && pos + 1 < json.size()) {
                    pos++;
                    switch (json[pos]) {
                    case L'n': key += L'\n'; break;
                    case L't': key += L'\t'; break;
                    case L'r': key += L'\r'; break;
                    default: key += json[pos];
                    }
                }
                else key += json[pos];
                pos++;
            }
            if (pos >= json.size()) break;
            pos++;
            skipWs();
            if (pos >= json.size() || json[pos] != L':') break;
            pos++;
            skipWs();
            std::wstring val;
            if (pos < json.size() && json[pos] == L'"') {
                pos++;
                while (pos < json.size() && json[pos] != L'"') {
                    if (json[pos] == L'\\' && pos + 1 < json.size()) {
                        pos++;
                        switch (json[pos]) {
                        case L'n': val += L'\n'; break;
                        case L't': val += L'\t'; break;
                        case L'r': val += L'\r'; break;
                        case L'u':
                            if (pos + 4 < json.size()) {
                                std::wstring hex = json.substr(pos + 1, 4);
                                try {
                                    int code = std::stoi(WideToUtf8(hex), nullptr, 16);
                                    val += static_cast<wchar_t>(code);
                                }
                                catch (const std::exception&) {}
                                pos += 4;
                            }
                            break;
                        default: val += json[pos];
                        }
                    }
                    else val += json[pos];
                    pos++;
                }
                if (pos < json.size()) pos++;
            }
            else {
                while (pos < json.size() && json[pos] != L',' && json[pos] != L'}') { val += json[pos]; pos++; }
            }
            result[key] = val;
            skipWs();
            if (pos < json.size() && json[pos] == L',') { pos++; continue; }
            if (pos < json.size() && json[pos] == L'}') break;
        }
        return result;
    }
    static bool GetBool(const std::map<std::wstring, std::wstring>& obj, const std::wstring& key, bool def) {
        auto it = obj.find(key);
        if (it == obj.end()) return def;
        return it->second == L"true";
    }
    static int GetInt(const std::map<std::wstring, std::wstring>& obj, const std::wstring& key, int def) {
        auto it = obj.find(key);
        if (it == obj.end()) return def;
        try { return std::stoi(it->second); }
        catch (const std::exception&) { return def; }
    }
};

static bool IsSafePath(const std::wstring& path, const std::wstring& baseDir = L"", bool allowAbsolute = false) {
    (void)allowAbsolute;
    if (path.empty() || path.size() > MAX_PATH * 2) return false;

    std::wstring checkPath = path;
    if (checkPath.rfind(L"\\\\?\\", 0) == 0) checkPath = checkPath.substr(4);
    if (checkPath.rfind(L"\\\\.\\", 0) == 0) return false;

    std::error_code ec;
    fs::path abs = fs::weakly_canonical(fs::absolute(checkPath, ec), ec);
    if (ec) return false;
    std::wstring canon = abs.wstring();

    if (canon.find(L"..") != std::wstring::npos) return false;

    if (!baseDir.empty()) {
        std::wstring canonicalBase = fs::weakly_canonical(fs::absolute(baseDir, ec), ec).wstring();
        if (ec) return false;
        auto lower = [](std::wstring s) {
            std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
            return s;
            };
        std::wstring c = lower(canon);
        std::wstring b = lower(canonicalBase);
        if (!b.empty() && b.back() != L'\\' && b.back() != L'/') b += L'\\';
        if (c.size() < b.size()) return false;
        if (c.compare(0, b.size(), b) != 0) {
            if (c.size() + 1 != b.size()) return false;
            if (c + L"\\" != b) return false;
        }
    }

    fs::path p = abs;
    std::vector<fs::path> components;
    while (p.has_relative_path()) {
        components.push_back(p);
        p = p.parent_path();
    }
    components.push_back(p);
    for (const auto& comp : components) {
        DWORD attrs = GetFileAttributesW(comp.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    }
    return true;
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

struct RollbackEntry {
    std::wstring id;
    std::wstring type;
    std::wstring target;
    std::wstring oldValue;
    std::wstring newValue;
    std::wstring description;
    long long timestamp = 0;
};
static std::vector<RollbackEntry> g_rollbackStack;

static bool ApplyRollbackEntry(const RollbackEntry& e, std::wstring& errOut);
static std::wstring BuildRollbackListJson();

static std::wstring ComputeSha256(const std::wstring& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string utf8 = WideToUtf8(input);
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return L"";
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return L"";
    }
    CryptHashData(hHash, (BYTE*)utf8.data(), (DWORD)utf8.size(), 0);
    BYTE rgbHash[32];
    DWORD cbHash = 32;
    CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    wchar_t hex[65] = {};
    for (int i = 0; i < 32; i++) {
        swprintf_s(hex + (i * 2), 3, L"%02x", rgbHash[i]);
    }
    return hex;
}

static std::wstring ComputeFileSha256(const std::wstring& path) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return L"";
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return L"";
    }
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return L"";
    }
    std::vector<BYTE> buf(65536);
    DWORD read = 0;
    while (ReadFile(hFile, buf.data(), (DWORD)buf.size(), &read, nullptr) && read > 0) {
        CryptHashData(hHash, buf.data(), read, 0);
    }
    CloseHandle(hFile);
    BYTE hash[32];
    DWORD hashLen = 32;
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    wchar_t hex[65] = {};
    for (int i = 0; i < 32; ++i) swprintf_s(hex + (i * 2), 3, L"%02x", hash[i]);
    return hex;
}

static bool ValidateDeveloperCode(const std::wstring& input) {
    static const unsigned char enc[] = {
        0xFD, 0xE2, 0xF5, 0xE8, 0xE5, 0xEB, 0xE8, 0xE6,
        0xF3, 0x8A, 0xE5, 0xE2, 0xF3, 0xE6, 0x8A, 0x95,
        0x97, 0x95, 0x92, 0x8A, 0xEC, 0x9E, 0xFF, 0x90
    };
    static const size_t N = sizeof(enc);
    std::string utf8 = WideToUtf8(input);
    if (utf8.size() != N) return false;
    unsigned diff = 0;
    for (size_t i = 0; i < N; ++i) {
        diff |= (unsigned char)(utf8[i] ^ 0xA7 ^ enc[i]);
    }
    return diff == 0;
}

static std::wstring EncryptString(const std::wstring& plainText) {
    if (plainText.empty()) return L"";
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    std::string utf8 = WideToUtf8(plainText);
    DataIn.pbData = (BYTE*)utf8.data();
    DataIn.cbData = (DWORD)utf8.size();

    if (CryptProtectData(&DataIn, L"ZeroBloatAI", NULL, NULL, NULL, 0, &DataOut)) {
        std::string enc((char*)DataOut.pbData, DataOut.cbData);
        LocalFree(DataOut.pbData);
        std::string base64;
        DWORD base64Size = 0;
        CryptBinaryToStringA((BYTE*)enc.data(), (DWORD)enc.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &base64Size);
        if (base64Size > 0) {
            base64.resize(base64Size);
            CryptBinaryToStringA((BYTE*)enc.data(), (DWORD)enc.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64[0], &base64Size);
            base64.resize(base64Size);
            return Utf8ToWide(base64);
        }
    }
    return L"";
}

static std::wstring DecryptString(const std::wstring& encrypted) {
    if (encrypted.empty()) return L"";
    std::string base64 = WideToUtf8(encrypted);
    DWORD binSize = 0;
    CryptStringToBinaryA(base64.c_str(), (DWORD)base64.size(), CRYPT_STRING_BASE64, NULL, &binSize, NULL, NULL);
    if (binSize == 0) return L"";

    std::string bin(binSize, 0);
    CryptStringToBinaryA(base64.c_str(), (DWORD)base64.size(), CRYPT_STRING_BASE64, (BYTE*)&bin[0], &binSize, NULL, NULL);

    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    DataIn.pbData = (BYTE*)bin.data();
    DataIn.cbData = (DWORD)bin.size();

    if (CryptUnprotectData(&DataIn, NULL, NULL, NULL, NULL, 0, &DataOut)) {
        std::string dec((char*)DataOut.pbData, DataOut.cbData);
        LocalFree(DataOut.pbData);
        return Utf8ToWide(dec);
    }
    return L"";
}

static void CheckBetaBuild() {
    std::wstring marker = g_paths.AppDir + L"\\.beta_build";
    std::error_code ec;
    g_isBetaBuild = fs::exists(marker, ec) && !ec;
    std::wstring ver(APP_VERSION);
    std::wstring lowerVer = ver;
    std::transform(lowerVer.begin(), lowerVer.end(), lowerVer.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
    if (lowerVer.find(L"beta") != std::wstring::npos) {
        g_isBetaBuild = true;
    }
}

static void SetBetaMarker(bool beta) {
    std::wstring marker = g_paths.AppDir + L"\\.beta_build";
    if (beta) {
        std::ofstream f(marker.c_str());
        if (f) {
            f << "beta";
            f.close();
        }
        g_isBetaBuild = true;
    }
    else {
        std::error_code ec;
        fs::remove(marker, ec);
        g_isBetaBuild = false;
    }
}

static void SaveDevAccess(bool granted) {
    std::wstring path = g_paths.DevAccessFile;
    std::wstring j = L"{\"betaAccess\":" + std::wstring(granted ? L"true" : L"false") + L"}";
    std::ofstream f(path.c_str(), std::ios::binary);
    if (f) {
        std::string u = WideToUtf8(j);
        f.write(u.data(), (std::streamsize)u.size());
    }
}

static void LoadDevAccess() {
    std::wstring path = g_paths.DevAccessFile;
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string utf8 = ss.str();
    auto pos = utf8.find("\"betaAccess\":");
    if (pos != std::string::npos) {
        pos += 13;
        g_betaAccessEnabled = (utf8.compare(pos, 4, "true") == 0);
    }
}

static HICON CreateLightningIcon(int size) {
    HDC hdc = CreateCompatibleDC(nullptr);
    HDC hScreen = GetDC(nullptr);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, size, size);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
    RECT rc = { 0, 0, size, size };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

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
    ReleaseDC(nullptr, hScreen);

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

static HICON LoadAppIcon(int size) {
    std::wstring iconPath = g_paths.AppDir + L"\\bolt.ico";
    HICON hIcon = (HICON)LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, size, size, LR_LOADFROMFILE);
    if (!hIcon) {
        hIcon = CreateLightningIcon(size);
    }
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

class CoInitScope {
public:
    CoInitScope(DWORD coInit = COINIT_MULTITHREADED) { hr_ = CoInitializeEx(nullptr, coInit); }
    ~CoInitScope() { if (SUCCEEDED(hr_)) CoUninitialize(); }
    bool IsInitialized() const { return SUCCEEDED(hr_); }
private: HRESULT hr_;
};

class SingleInstanceMutex {
public:
    SingleInstanceMutex(const wchar_t* name) {
        hMutex_ = CreateMutexW(nullptr, FALSE, name);
        if (hMutex_ && GetLastError() == ERROR_ALREADY_EXISTS) alreadyRunning_ = true;
        else alreadyRunning_ = false;
    }
    ~SingleInstanceMutex() { if (hMutex_) CloseHandle(hMutex_); }
    bool IsAlreadyRunning() const { return alreadyRunning_; }
private:
    HANDLE hMutex_ = nullptr;
    bool alreadyRunning_ = false;
};

struct CmdResult {
    DWORD exitCode = (DWORD)-1;
    std::string out, err;
    bool timedOut = false;
};

static const wchar_t* ULTIMATE_GUID = L"e9a42b02-d5df-448d-aa00-03f14749eb61";
static const wchar_t* HIGH_PERF_GUID = L"8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
static const wchar_t* BALANCED_GUID = L"381b4222-f694-41f0-9685-ff5bb260df2e";
static const size_t LOG_MAX_SIZE = 1ull * 1024 * 1024;
static const size_t CMD_OUTPUT_LIMIT = 4ull * 1024 * 1024;
static const size_t MAX_ARG_LENGTH = 1024 * 1024;
static const size_t MAX_ARGS_COUNT = 64;
static const size_t MAX_IPC_MESSAGE_SIZE = 4 * 1024 * 1024;

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
        std::wstring programData = GetEnvW(L"ProgramData");
        if (programData.empty()) {
            wchar_t b[MAX_PATH] = {};
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, b))) programData = b;
            else programData = g_paths.SystemRoot + L"\\ProgramData";
        }
        g_paths.BackupDir = programData + L"\\ZeroBloat";
        g_paths.WebView2Dir = g_paths.BackupDir + L"\\WebView2";
    }
    fs::create_directories(g_paths.BackupDir, ec);
    fs::create_directories(g_paths.WebView2Dir, ec);
    if (!g_portableMode.load()) {
        SetFileAttributesW(g_paths.BackupDir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }
    g_paths.UiDir = g_paths.BackupDir + L"\\ui";
    fs::create_directories(g_paths.UiDir, ec);
    g_paths.LogFile = g_paths.BackupDir + L"\\zerobloat_log.txt";
    g_paths.ConfigFile = g_paths.BackupDir + L"\\config.json";
    g_paths.AuditFile = g_paths.BackupDir + L"\\audit.log";
    g_paths.AiConfigFile = g_paths.BackupDir + L"\\ai.json";
    g_paths.DevAccessFile = g_paths.BackupDir + L"\\dev.json";
    g_paths.HistoryFile = g_paths.BackupDir + L"\\history.json";
    g_paths.ProfilesFile = g_paths.BackupDir + L"\\profiles.json";
    g_paths.RollbackFile = g_paths.BackupDir + L"\\rollback.json";
    g_paths.SdeDir = g_paths.BackupDir + L"\\sde";
    fs::create_directories(g_paths.SdeDir, ec);
}

static std::mutex g_sinkMutex;
static std::function<void(const std::wstring&)> g_uiLogSink;
static std::function<void(int, const std::wstring&)> g_uiProgressSink;

enum class LogLevel { INFO, WARN, ERR };

static void WriteAuditLog(const std::wstring& action, const std::wstring& details) {
    std::lock_guard<std::mutex> lk(g_auditMutex);
    try {
        std::error_code ec{};
        auto sz = fs::file_size(g_paths.AuditFile, ec);
        if (!ec && sz > 4ULL * 1024 * 1024) {
            fs::remove(g_paths.AuditFile + L".old", ec);
            fs::rename(g_paths.AuditFile, g_paths.AuditFile + L".old", ec);
        }
        std::ofstream f(g_paths.AuditFile.c_str(), std::ios::app | std::ios::binary);
        if (!f) return;
        time_t t = time(nullptr);
        struct tm tm;
        localtime_s(&tm, &t);
        wchar_t ts[64];
        swprintf_s(ts, L"[%04d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        std::string s = WideToUtf8(ts) + WideToUtf8(action) + " | " + WideToUtf8(details) + "\r\n";
        f.write(s.data(), (std::streamsize)s.size());
    }
    catch (const std::exception&) {}
}

static void WriteLogFile(LogLevel level, const std::wstring& text) {
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
        time_t t = time(nullptr);
        struct tm tm;
        localtime_s(&tm, &t);
        wchar_t ts[64];
        swprintf_s(ts, L"[%04d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        std::wstring lvl = (level == LogLevel::INFO) ? L"INFO" : (level == LogLevel::WARN ? L"WARN" : L"ERROR");
        std::string s = WideToUtf8(ts) + "[" + WideToUtf8(lvl) + "] " + WideToUtf8(text) + "\r\n";
        f.write(s.data(), (std::streamsize)s.size());
    }
    catch (const std::exception&) {}
}

static void Print(const std::wstring& text) {
    WriteLogFile(LogLevel::INFO, text);
    std::function<void(const std::wstring&)> sink;
    { std::lock_guard<std::mutex> lk(g_sinkMutex); sink = g_uiLogSink; }
    if (sink) { try { sink(text); } catch (const std::exception&) {} }
}
static void PrintWarn(const std::wstring& text) { WriteLogFile(LogLevel::WARN, text); Print(text); }
static void PrintErr(const std::wstring& text) { WriteLogFile(LogLevel::ERR, text); Print(text); }

static void SendProgress(int percent, const std::wstring& msg) {
    std::function<void(int, const std::wstring&)> sink;
    { std::lock_guard<std::mutex> lk(g_sinkMutex); sink = g_uiProgressSink; }
    if (sink) { try { sink(percent, msg); } catch (const std::exception&) {} }
}

static void SaveRollbackStack() {
    std::wstring j = L"[";
    for (size_t i = 0; i < g_rollbackStack.size(); ++i) {
        if (i) j += L",";
        const auto& e = g_rollbackStack[i];
        j += L"{\"id\":" + JsonStr(e.id)
            + L",\"type\":" + JsonStr(e.type)
            + L",\"target\":" + JsonStr(e.target)
            + L",\"oldValue\":" + JsonStr(e.oldValue)
            + L",\"newValue\":" + JsonStr(e.newValue)
            + L",\"description\":" + JsonStr(e.description)
            + L",\"timestamp\":" + std::to_wstring(e.timestamp) + L"}";
    }
    j += L"]";
    std::ofstream f(g_paths.RollbackFile.c_str(), std::ios::binary);
    if (f) {
        std::string u = WideToUtf8(j);
        f.write(u.data(), (std::streamsize)u.size());
    }
}

static void LoadRollbackStack() {
    std::ifstream f(g_paths.RollbackFile.c_str(), std::ios::binary);
    if (!f) return;
    std::stringstream ss;
    ss << f.rdbuf();
    std::wstring content = Utf8ToWide(ss.str());
    std::lock_guard<std::mutex> lk(g_rollbackMutex);
    g_rollbackStack.clear();
    size_t pos = 0;
    while ((pos = content.find(L"{\"id\":", pos)) != std::wstring::npos) {
        size_t end = content.find(L"}", pos);
        if (end == std::wstring::npos) break;
        std::wstring obj = content.substr(pos, end - pos + 1);
        auto m = JsonParser::ParseObject(obj);
        if (m.count(L"id") && m.count(L"type") && m.count(L"target")) {
            RollbackEntry e;
            e.id = m[L"id"];
            e.type = m[L"type"];
            e.target = m[L"target"];
            e.oldValue = m.count(L"oldValue") ? m[L"oldValue"] : L"";
            e.newValue = m.count(L"newValue") ? m[L"newValue"] : L"";
            e.description = m.count(L"description") ? m[L"description"] : L"";
            e.timestamp = (long long)SafeStoi(m.count(L"timestamp") ? m[L"timestamp"] : L"0");
            g_rollbackStack.push_back(e);
        }
        pos = end + 1;
    }
}

static void RecordRollback(const std::wstring& type, const std::wstring& target,
    const std::wstring& oldValue, const std::wstring& newValue,
    const std::wstring& desc) {
        {
            std::lock_guard<std::mutex> lk(g_rollbackMutex);
            RollbackEntry e;
            e.id = Fmt(L"%lld_%d", (long long)time(nullptr), (int)g_rollbackStack.size());
            e.type = type;
            e.target = target;
            e.oldValue = oldValue;
            e.newValue = newValue;
            e.description = desc;
            e.timestamp = (long long)time(nullptr);
            g_rollbackStack.push_back(e);
            if (g_rollbackStack.size() > 500) {
                g_rollbackStack.erase(g_rollbackStack.begin(), g_rollbackStack.begin() + 100);
            }
        }
        SaveRollbackStack();
}

static bool RunProcessSafe(const std::wstring& exePath, const std::vector<std::wstring>& args, DWORD& exitCode, int timeoutSec = 300) {
    std::wstring cmdLine;
    if (exePath.find(L' ') != std::wstring::npos) cmdLine += L"\"" + exePath + L"\"";
    else cmdLine += exePath;

    for (const auto& arg : args) {
        cmdLine += L" ";
        bool needQuote = (arg.find(L' ') != std::wstring::npos || arg.find(L'\t') != std::wstring::npos || arg.empty());
        if (needQuote) {
            cmdLine += L"\"";
            for (wchar_t c : arg) {
                if (c == L'"') cmdLine += L"\\\"";
                else cmdLine += c;
            }
            cmdLine += L"\"";
        }
        else {
            cmdLine += arg;
        }
    }

    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> clbuf(cmdLine.begin(), cmdLine.end());
    clbuf.push_back(L'\0');

    if (!CreateProcessW(exePath.c_str(), clbuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi)) {
        return false;
    }

    UniqueHandle hProcess(pi.hProcess);
    UniqueHandle hThread(pi.hThread);

    DWORD waitMs = (timeoutSec <= 0 ? 1 : timeoutSec) * 1000;
    DWORD wr = WaitForSingleObject(hProcess.get(), waitMs);

    if (wr == WAIT_TIMEOUT) {
        TerminateProcess(hProcess.get(), 1);
        WaitForSingleObject(hProcess.get(), 5000);
        exitCode = (DWORD)-1;
        return false;
    }

    GetExitCodeProcess(hProcess.get(), &exitCode);
    return true;
}

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

    std::wstring cl = L"cmd.exe /d /s /c \"" + cmd + L"\"";

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    UniqueHandle hOut(CreateFileW(outFile.path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    UniqueHandle hErr(CreateFileW(errFile.path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    UniqueHandle hNul(CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr));

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

    std::vector<wchar_t> clbuf(cl.begin(), cl.end());
    clbuf.push_back(L'\0');
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, clbuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi);
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
    res.out = ReadFileCapped(outFile.path, CMD_OUTPUT_LIMIT);
    res.err = ReadFileCapped(errFile.path, CMD_OUTPUT_LIMIT);
    return res;
}

static bool RunCmd(const std::wstring& cmd, const std::wstring& text = L"", bool check = true, int timeoutSec = 300, std::initializer_list<DWORD> allowed = { 0 }) {
    if (!text.empty()) Print(L"  [+] " + text + L"...");
    if (g_dryRunMode.load()) {
        if (!text.empty()) Print(L"      └─ [DRY RUN]");
        return true;
    }
    CmdResult r = RunCmdImpl(cmd, timeoutSec);
    if (r.timedOut) { PrintErr(L"      └─ [TIMEOUT]"); return false; }
    for (DWORD a : allowed) {
        if (r.exitCode == a) { if (!text.empty()) Print(L"      └─ [OK]"); return true; }
    }
    std::string d = r.err.empty() ? r.out : r.err;
    if (d.empty()) d = "Code " + std::to_string(r.exitCode);
    if (d.size() > 200) d.resize(200);
    if (check) PrintErr(std::wstring(L"      └─ [ERROR] ") + Utf8ToWide(d));
    else PrintWarn(std::wstring(L"      └─ [WARN] ") + Utf8ToWide(d));
    return false;
}

static bool RunCmdArgs(const std::wstring& exe, const std::vector<std::wstring>& args, const std::wstring& text, int timeoutSec = 300) {
    if (!text.empty()) Print(L"  [+] " + text + L"...");
    if (g_dryRunMode.load()) { Print(L"      └─ [DRY RUN]"); return true; }
    DWORD code = 0;
    bool ok = RunProcessSafe(exe, args, code, timeoutSec);
    if (!ok) { PrintErr(L"      └─ [ERROR] process failed"); return false; }
    if (code == 0) { if (!text.empty()) Print(L"      └─ [OK]"); return true; }
    PrintErr(L"      └─ [ERROR] code " + std::to_wstring(code));
    return false;
}

static DWORD CountProcesses() {
    DWORD c = 0;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) { do { ++c; } while (Process32NextW(snap.get(), &pe)); }
    return c;
}

static DWORD CountServices() {
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (!scm) return 0;
    DWORD needed = 0, count = 0;
    (void)EnumServicesStatusExW(scm.get(), SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &needed, &count, nullptr, nullptr);
    return count;
}

static bool EnablePrivilege(const wchar_t* priv) {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &raw)) return false;
    UniqueHandle tok(raw);
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    LookupPrivilegeValueW(nullptr, priv, &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    return AdjustTokenPrivileges(tok.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr) != FALSE;
}

static void ClearKernelPools() {
    if (g_dryRunMode.load()) { Print(L"  [+] Clear Kernel Pools [DRY RUN]"); return; }
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
    freedMB = 0;
    if (g_dryRunMode.load()) return 0;
    EnablePrivilege(SE_DEBUG_NAME);
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
            if (pe.th32ProcessID <= 4 || IsCriticalProcess(pe.szExeFile)) continue;
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

static uint64_t PurgeStandbyEnhanced() {
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
    if (g_dryRunMode.load()) return 0;

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
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
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
        k.LowPart = kernel.dwLowDateTime;
        k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime;
        u.HighPart = user.dwHighDateTime;
        ULONGLONG procTime = k.QuadPart + u.QuadPart;
        ULONGLONG sysTime = GetTickCount64() * 10000;
        if (lastSysTime > 0 && sysTime > lastSysTime) {
            ULONGLONG procDiff = procTime - lastProcTime;
            ULONGLONG sysDiff = sysTime - lastSysTime;
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            float val = (float)((double)procDiff / (double)sysDiff / si.dwNumberOfProcessors * 100.0);
            g_appCpuUsage = std::clamp(val, 0.0f, 100.0f);
        }
        lastSysTime = sysTime;
        lastProcTime = procTime;
    }
}

static const long kConnectMaxWait = 0x80;
class WmiConnection {
public:
    bool Open(const wchar_t* ns) {
        for (int attempt = 0; attempt < 3; ++attempt) {
            CoInitScope com;
            ComPtr<IWbemLocator> loc;
            HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&loc));
            if (FAILED(hr)) return false;
            ComPtr<IWbemServices> svc;
            hr = loc->ConnectServer(_bstr_t(ns), nullptr, nullptr, nullptr, kConnectMaxWait, nullptr, nullptr, &svc);
            if (SUCCEEDED(hr)) {
                hr = CoSetProxyBlanket(svc.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
                if (SUCCEEDED(hr)) {
                    svc_ = svc;
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1)));
        }
        return false;
    }
    IWbemServices* get() const noexcept { return svc_.Get(); }
private: ComPtr<IWbemServices> svc_;
};

static bool WmiQueryFirstString(IWbemServices* svc, const wchar_t* wql, const wchar_t* prop, std::wstring& out) {
    if (!svc) return false;
    ComPtr<IEnumWbemClassObject> en;
    HRESULT hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
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
    obj.Reset();
    return ok;
}

static std::wstring GetDiskHealthJson(IWbemServices* svcStorage, IWbemServices* svcCim) {
    std::wstring result = L"[";
    if (svcStorage) {
        ComPtr<IEnumWbemClassObject> en;
        HRESULT hr = svcStorage->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"SELECT FriendlyName, HealthStatus, MediaType, Size, DeviceId FROM MSFT_PhysicalDisk"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
        if (SUCCEEDED(hr) && en) {
            bool first = true;
            for (;;) {
                ComPtr<IWbemClassObject> obj;
                ULONG ret = 0;
                hr = en->Next(4000, 1, &obj, &ret);
                if (FAILED(hr) || ret == 0 || !obj) break;

                std::wstring name = L"Unknown Disk", media = L"Unknown", devId = L"";
                int health = 0;
                uint64_t sz = 0;
                float temp = -1.0f, wear = -1.0f;

                VARIANT vName, vHealth, vMedia, vSize, vDevId;
                VariantInit(&vName);
                VariantInit(&vHealth);
                VariantInit(&vMedia);
                VariantInit(&vSize);
                VariantInit(&vDevId);

                if (SUCCEEDED(obj->Get(L"FriendlyName", 0, &vName, nullptr, nullptr)) && vName.vt == VT_BSTR) name = vName.bstrVal;
                if (SUCCEEDED(obj->Get(L"HealthStatus", 0, &vHealth, nullptr, nullptr))) {
                    if (vHealth.vt == VT_I4) health = vHealth.lVal;
                    else if (vHealth.vt == VT_UI4) health = (int)vHealth.ulVal;
                }
                if (SUCCEEDED(obj->Get(L"MediaType", 0, &vMedia, nullptr, nullptr))) {
                    int mt = (vMedia.vt == VT_I4) ? vMedia.lVal : (vMedia.vt == VT_UI4 ? (int)vMedia.ulVal : 0);
                    media = (mt == 3) ? L"HDD" : (mt == 4) ? L"SSD" : (mt == 5) ? L"SCM" : L"Unspecified";
                }
                if (SUCCEEDED(obj->Get(L"Size", 0, &vSize, nullptr, nullptr)) && vSize.vt == VT_UI8) sz = vSize.ullVal;
                if (SUCCEEDED(obj->Get(L"DeviceId", 0, &vDevId, nullptr, nullptr)) && vDevId.vt == VT_BSTR) devId = vDevId.bstrVal;

                if (svcCim && !devId.empty()) {
                    std::wstring wql = L"SELECT Wear, Temperature FROM MSFT_StorageReliabilityCounter WHERE DeviceId='" + devId + L"'";
                    ComPtr<IEnumWbemClassObject> enRel;
                    if (SUCCEEDED(svcCim->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enRel))) {
                        ComPtr<IWbemClassObject> objRel;
                        ULONG rRet = 0;
                        if (SUCCEEDED(enRel->Next(0, 1, &objRel, &rRet)) && rRet > 0) {
                            VARIANT vWear, vTemp;
                            VariantInit(&vWear);
                            VariantInit(&vTemp);
                            if (SUCCEEDED(objRel->Get(L"Wear", 0, &vWear, nullptr, nullptr)) && vWear.vt == VT_UI1) wear = (float)vWear.bVal;
                            if (SUCCEEDED(objRel->Get(L"Temperature", 0, &vTemp, nullptr, nullptr)) && vTemp.vt == VT_UI1) temp = (float)vTemp.bVal;
                            VariantClear(&vWear);
                            VariantClear(&vTemp);
                        }
                    }
                }

                if (!first) result += L",";
                result += L"{\"model\":" + JsonStr(name) + L",\"media\":" + JsonStr(media) + L",\"status\":" + std::to_wstring(health) +
                    L",\"size\":" + std::to_wstring(sz) + L",\"temp\":" + FloatToJsonStr(temp) + L",\"wear\":" + FloatToJsonStr(wear) + L"}";
                first = false;
                VariantClear(&vName);
                VariantClear(&vHealth);
                VariantClear(&vMedia);
                VariantClear(&vSize);
                VariantClear(&vDevId);
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
        ComPtr<IWbemClassObject> obj;
        ULONG ret = 0;
        hr = en->Next(4000, 1, &obj, &ret);
        if (FAILED(hr) || ret == 0 || !obj) break;
        VARIANT v{};
        VariantInit(&v);
        if (SUCCEEDED(obj->Get(L"CurrentTemperature", 0, &v, nullptr, nullptr))) {
            float c = -1;
            if (v.vt == VT_I4) c = (float)((v.lVal / 10.0) - 273.15);
            else if (v.vt == VT_UI4) c = (float)((v.ulVal / 10.0) - 273.15);
            if (c > maxT && c > 0 && c < 150) maxT = c;
        }
        VariantClear(&v);
        obj.Reset();
    }
    if (maxT > 0) {
        celsius = maxT;
        return true;
    }
    return false;
}

static ULONGLONG FileTimeToULL(const FILETIME& ft) {
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

class CpuSampler {
public:
    float Sample() {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0f;
        ULONGLONG i = FileTimeToULL(idle), k = FileTimeToULL(kernel), u = FileTimeToULL(user);
        if (first_) {
            prevIdle_ = i;
            prevKernel_ = k;
            prevUser_ = u;
            first_ = false;
            return 0.0f;
        }
        ULONGLONG total = (k - prevKernel_) + (u - prevUser_);
        ULONGLONG idleDiff = i - prevIdle_;
        prevIdle_ = i;
        prevKernel_ = k;
        prevUser_ = u;
        if (total == 0) return 0.0f;
        float val = (float)((double)(total - idleDiff) * 100.0 / (double)total);
        return std::clamp(val, 0.0f, 100.0f);
    }
private:
    ULONGLONG prevIdle_ = 0, prevKernel_ = 0, prevUser_ = 0;
    bool first_ = true;
};

static float GetGpuUsage() {
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
            if (PdhAddCounterW(q, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &st.counter) != ERROR_SUCCESS) {
                st.counter = nullptr;
            }
        }
        if (!st.counter) {
            PdhCloseQuery(st.guard.q);
            st.guard.q = nullptr;
            return;
        }
        PdhCollectQueryData(q);
        st.ready = true;
        });
    if (!st.ready) return -1.0f;
    if (PdhCollectQueryData(st.guard.q) != ERROR_SUCCESS) return -1.0f;
    DWORD bufSize = 0, itemCount = 0;
    PDH_STATUS s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
    if (s == ERROR_SUCCESS) return 0.0f;
    if (s != (PDH_STATUS)0x800007D2L || bufSize == 0) return -1.0f;
    for (int attempt = 0; attempt < 3; ++attempt) {
        AlignedBuffer buf(bufSize);
        DWORD bytes = bufSize, count = itemCount;
        s = PdhGetFormattedCounterArrayW(st.counter, PDH_FMT_DOUBLE, &bytes, &count, buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>());
        if (s == (PDH_STATUS)0x800007D2L) {
            bufSize = bytes;
            continue;
        }
        if (s != ERROR_SUCCESS) return -1.0f;
        auto* items = buf.as<PDH_FMT_COUNTERVALUE_ITEM_W>();
        double sum = 0.0;
        for (DWORD i = 0; i < count; ++i) {
            PDH_STATUS cs = items[i].FmtValue.CStatus;
            if (cs == (PDH_STATUS)0x00000000L || cs == (PDH_STATUS)0x00000001L) sum += items[i].FmtValue.doubleValue;
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
        std::wstring cmd = L"\"" + nvSmiPath + L"\" --id=0 --query-gpu=temperature.gpu,fan.speed,utilization.gpu,memory.used,memory.total --format=csv,noheader,nounits";
        CmdResult r = RunCmdImpl(cmd, 5);
        if (r.exitCode == 0 && !r.out.empty()) {
            std::istringstream iss(r.out);
            std::string line;
            double maxTemp = -1, maxFan = -1, maxUtil = -1;
            uint64_t maxUsed = 0, maxTotal = 0;
            if (std::getline(iss, line)) {
                if (line.empty() || (line[0] >= '0' && line[0] <= '9') || line.find("N/A") != std::string::npos) {
                    iss.seekg(0);
                }
            }
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
                        if (parts[0] != "N/A" && !parts[0].empty()) {
                            double t = std::stod(parts[0]);
                            if (t > maxTemp) maxTemp = t;
                        }
                        if (parts[1] != "N/A" && !parts[1].empty()) {
                            double f = std::stod(parts[1]);
                            if (f > maxFan) maxFan = f;
                        }
                        if (parts[2] != "N/A" && !parts[2].empty()) {
                            double u = std::stod(parts[2]);
                            if (u > maxUtil) maxUtil = u;
                        }
                        if (parts[3] != "N/A" && !parts[3].empty()) maxUsed = (uint64_t)std::stoll(parts[3]);
                        if (parts[4] != "N/A" && !parts[4].empty()) maxTotal = (uint64_t)std::stoll(parts[4]);
                    }
                    catch (const std::exception&) {}
                }
            }
            if (maxTemp > 0) g_gpuTemp = (float)maxTemp;
            if (maxFan >= 0) g_gpuFan = (float)maxFan;
            if (maxUtil >= 0) g_gpuUsage = (float)maxUtil;
            if (maxTotal > 0) {
                g_vramUsedMB = maxUsed;
                g_vramTotalMB = maxTotal;
            }
        }
    }
    if (g_batteryPercent.load() < 0 || g_isAcConnected.load()) {
        SYSTEM_POWER_STATUS sps{};
        if (GetSystemPowerStatus(&sps) && sps.BatteryLifePercent != 255) {
            g_batteryPercent = (float)sps.BatteryLifePercent;
            g_isAcConnected = (sps.ACLineStatus == 1);
        }
    }
}

static void UpdateNetworkSpeed() {
    static ULONGLONG lastTick = 0;
    static ULONGLONG lastIn = 0, lastOut = 0;
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR) {
        ULONGLONG totalIn = 0, totalOut = 0;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            if (table->Table[i].Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (table->Table[i].OperStatus == IfOperStatusUp && (table->Table[i].Type == IF_TYPE_ETHERNET_CSMACD || table->Table[i].Type == IF_TYPE_IEEE80211)) {
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
        lastTick = now;
        lastIn = totalIn;
        lastOut = totalOut;
        FreeMibTable(table);
    }
}

static void PingWorker() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    while (!g_stopMonitor) {
        try {
            HANDLE hIcmp = IcmpCreateFile();
            if (hIcmp != INVALID_HANDLE_VALUE) {
                IPAddr ip = 0;
                InetPtonA(AF_INET, "8.8.8.8", &ip);
                char SendData[32] = "DataBuffer";
                DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData) + 8;
                LPVOID ReplyBuffer = malloc(replySize);
                if (ReplyBuffer) {
                    DWORD dwRet = IcmpSendEcho(hIcmp, ip, SendData, sizeof(SendData), NULL, ReplyBuffer, replySize, 1000);
                    if (dwRet != 0) {
                        PICMP_ECHO_REPLY pIpe = (PICMP_ECHO_REPLY)ReplyBuffer;
                        g_pingMs = (int)pIpe->RoundTripTime;
                    }
                    else {
                        g_pingMs = -1;
                    }
                    free(ReplyBuffer);
                }
                IcmpCloseHandle(hIcmp);
            }
        }
        catch (const std::exception&) {}

        for (int i = 0; i < 50 && !g_stopMonitor; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (SUCCEEDED(hr)) CoUninitialize();
}

static void UpdateThrottleDetection() {
    struct State {
        PdhQueryGuard q;
        PDH_HCOUNTER counter = nullptr;
        bool ready = false;
    };
    static State st;
    static std::once_flag once;
    std::call_once(once, [] {
        PDH_HQUERY q = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) == ERROR_SUCCESS) {
            st.q.q = q;
            if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% Processor Performance", 0, &st.counter) != ERROR_SUCCESS) {
                PdhAddCounterW(q, L"\\Processor Information(_Total)\\% Processor Performance", 0, &st.counter);
            }
            PdhCollectQueryData(q);
            st.ready = true;
        }
        });
    if (!st.ready || !st.counter) return;
    PdhCollectQueryData(st.q.q);
    PDH_FMT_COUNTERVALUE val;
    if (PdhGetFormattedCounterValue(st.counter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
        if (val.doubleValue < 60.0 && g_cpuUsage.load() > 50.0f) g_throttling = true;
        else g_throttling = false;
    }
}

static void UpdateDpcIsrMetrics() {
    struct State {
        PdhQueryGuard q;
        PDH_HCOUNTER dpc = nullptr;
        PDH_HCOUNTER isr = nullptr;
        bool ready = false;
    };
    static State st;
    static std::once_flag once;
    std::call_once(once, [] {
        PDH_HQUERY q = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &q) != ERROR_SUCCESS) return;
        st.q.q = q;
        if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% DPC Time", 0, &st.dpc) != ERROR_SUCCESS) PdhAddCounterW(q, L"\\Processor Information(_Total)\\% DPC Time", 0, &st.dpc);
        if (PdhAddEnglishCounterW(q, L"\\Processor Information(_Total)\\% Interrupt Time", 0, &st.isr) != ERROR_SUCCESS) PdhAddCounterW(q, L"\\Processor Information(_Total)\\% Interrupt Time", 0, &st.isr);
        PdhCollectQueryData(q);
        st.ready = true;
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
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    ramTotal = Fmt(L"%.1f GB", (double)ms.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
    try {
        if (svc) {
            WmiQueryFirstString(svc, L"SELECT Name FROM Win32_Processor", L"Name", cpuName);
            WmiQueryFirstString(svc, L"SELECT Name FROM Win32_VideoController WHERE Name IS NOT NULL", L"Name", gpuName);
        }
    }
    catch (const std::exception&) {}
    return L"{\"cpu\":" + JsonStr(cpuName) + L",\"gpu\":" + JsonStr(gpuName) + L",\"ram\":" + JsonStr(ramTotal) + L"}";
}

static bool GetTemperatures(IWbemServices* svc, float& cpuTemp, float& gpuTemp) {
    cpuTemp = -1.0f;
    gpuTemp = g_gpuTemp.load();
    try {
        if (svc) {
            float t = -1;
            if (WmiQueryMaxThermalZone(svc, t)) {
                cpuTemp = t;
                return true;
            }
        }
    }
    catch (const std::exception&) {}
    return false;
}

static void GameBooster(bool enable);
static void ApplySmartGameMode(bool enable) {
    static std::map<DWORD, DWORD> originalPriorities;
    std::lock_guard<std::mutex> lk(g_boostMutex);
    if (g_dryRunMode.load()) return;
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
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
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
    CoInitScope com;
    WmiConnection wmiCim, wmiStorage, wmiWmi;
    wmiCim.Open(L"ROOT\\CIMV2");
    wmiStorage.Open(L"ROOT\\Microsoft\\Windows\\Storage");
    wmiWmi.Open(L"ROOT\\WMI");

    {
        std::wstring info = GetPcInfo(wmiCim.get());
        std::lock_guard<std::mutex> lk(g_pcInfoMutex);
        g_pcInfoJson = info;
    }
    {
        std::wstring disks = GetDiskHealthJson(wmiStorage.get(), wmiCim.get());
        std::lock_guard<std::mutex> lk(g_diskHealthMutex);
        g_diskHealthJson = disks;
    }

    auto sleepStop = [](int ms) {
        for (int i = 0; i < ms && !g_stopMonitor; i += 100) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        };
    try {
        CpuSampler sampler;
        sleepStop(800);
        sampler.Sample();
        sleepStop(500);
        if (g_stopMonitor) return;
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
                MEMORYSTATUSEX ms{};
                ms.dwLength = sizeof(ms);
                GlobalMemoryStatusEx(&ms);
                int usedPct = (int)((ms.ullTotalPhys - ms.ullAvailPhys) * 100 / ms.ullTotalPhys);
                if (usedPct >= s.ramAutoCleanThresholdPct) shouldClean = true;

                if (s.ramAutoCleanOnIdle) {
                    LASTINPUTINFO lii = { sizeof(lii) };
                    if (GetLastInputInfo(&lii)) {
                        ULONGLONG idleMs = GetTickCount64() - lii.dwTime;
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

                std::wstring dh = GetDiskHealthJson(wmiStorage.get(), wmiCim.get());
                { std::lock_guard<std::mutex> lk(g_diskHealthMutex); g_diskHealthJson = dh; }

                float ct = -1.0f, gt = -1.0f;
                if (GetTemperatures(wmiWmi.get(), ct, gt)) {
                    g_cpuTemp = ct;
                    g_gpuTemp = gt;
                }
            }

            QUERY_USER_NOTIFICATION_STATE st;
            bool inGame = false;
            if (SUCCEEDED(SHQueryUserNotificationState(&st))) {
                if (st == QUNS_RUNNING_D3D_FULL_SCREEN || st == QUNS_PRESENTATION_MODE) inGame = true;
            }
            if (s.autoBoost && inGame != wasInGame) {
                std::thread([](bool en) {
                    try { GameBooster(en); }
                    catch (const std::exception& e) { WriteLogFile(LogLevel::ERR, L"GameBooster: " + Utf8ToWide(e.what())); }
                    }, inGame).detach();
                wasInGame = inGame;
            }
        }
    }
    catch (const std::exception& e) {
        WriteLogFile(LogLevel::ERR, L"[MonitorThread] " + Utf8ToWide(e.what()));
    }
}

static void CleanTemp() {
    RunCmd(L"del /f /s /q \"%TEMP%\\*\" >nul 2>&1", L"Temp", false);
    RunCmd(L"del /f /s /q \"%SystemRoot%\\Temp\\*\" >nul 2>&1", L"Windows Temp", false);
}
static void CleanPrefetch() { RunCmd(L"del /f /s /q \"%SystemRoot%\\Prefetch\\*\" >nul 2>&1", L"Prefetch", false); }
static void CleanRecycleForDrive(wchar_t letter) {
    if (letter < L'A' || letter > L'Z') return;
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
    if (driveLetter < L'A' || driveLetter > L'Z') return;
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
                        if (!g_dryRunMode.load()) fs::remove(it->path(), ec);
                        if (!ec) ++removed;
                    }
                }
                catch (const std::exception&) {}
            }
        }
    }
    catch (const std::exception&) {}
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

static bool RequireDangerousConfirm(const std::wstring& name, const std::wstring& details) {
    std::wstring msg = L"⚠️ ОПАСНАЯ ОПЕРАЦИЯ: " + name + L"\n\n" + details +
        L"\n\nБудет автоматически создана точка восстановления. Продолжить?";
    if (MessageBoxW(nullptr, msg.c_str(), L"ZeroBloat - Подтверждение", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
        return false;
    }
    return true;
}

static void DisableTelemetry() {
    WriteAuditLog(L"DisableTelemetry", L"User disabled Windows telemetry services");
    RunCmd(L"sc config DiagTrack start= disabled >nul 2>&1", L"DiagTrack disabled", false);
    RunCmd(L"sc stop DiagTrack >nul 2>&1", L"DiagTrack stopped", false);
    RunCmd(L"sc config dmwappushservice start= disabled >nul 2>&1", L"dmwappushservice", false);
    RunCmd(L"sc stop dmwappushservice >nul 2>&1", L"dmwappushservice stopped", false);
    HKEY rawKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &rawKey, nullptr) == ERROR_SUCCESS) {
        UniqueRegKey key(rawKey);
        DWORD v = 0;
        RegSetValueExW(key.get(), L"AllowTelemetry", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
    RecordRollback(L"service", L"DiagTrack", L"auto", L"disabled", L"Disable Telemetry (DiagTrack)");
}

static void DisableDeepServices() {
    WriteAuditLog(L"DisableDeepServices", L"User disabled deep Windows services");
    const wchar_t* svcs[] = { L"MapsBroker", L"RetailDemo", L"DiagTrack", L"dmwappushservice", L"WSearch", L"SysMain", L"XblAuthManager", L"XblGameSave", L"XboxNetApiSvc", L"XboxGipSvc" };
    for (auto s : svcs) {
        RunCmd(L"sc config " + std::wstring(s) + L" start= disabled", L"", false);
        RunCmd(L"sc stop " + std::wstring(s), L"", false);
        RecordRollback(L"service", s, L"auto", L"disabled", L"Disable Deep Services");
    }
}

static void DisableDefender() {
    WriteAuditLog(L"DisableDefender", L"User attempted to disable Windows Defender");
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows Defender", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = 1;
        RegSetValueExW(hKey, L"DisableAntiSpyware", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    RecordRollback(L"registry", L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\DisableAntiSpyware", L"0", L"1", L"Disable Defender");
}

static void OptimizeFSO(bool disable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = disable ? 2 : 0;
        RegSetValueExW(hKey, L"GameDVR_FSEBehaviorMode", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegSetValueExW(hKey, L"GameDVR_HonorUserFSEBehaviorMode", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    RecordRollback(L"registry", L"HKCU\\System\\GameConfigStore\\GameDVR_FSEBehaviorMode", disable ? L"0" : L"2", disable ? L"2" : L"0", L"Optimize FSO");
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
        UniqueRegKey key(rawKey);
        DWORD v = enabled ? 1 : 0;
        RegSetValueExW(key.get(), L"AutoGameModeEnabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    }
    RecordRollback(L"registry", L"HKCU\\Software\\Microsoft\\GameBar\\AutoGameModeEnabled", enabled ? L"0" : L"1", enabled ? L"1" : L"0", L"Game Mode");
}

static void SetClassicContextMenu(bool enableClassic) {
    WriteAuditLog(L"SetClassicContextMenu", enableClassic ? L"Enabled classic context menu" : L"Restored modern context menu");
    if (g_dryRunMode.load()) { Print(L"[DRY RUN] Toggle Context Menu"); return; }
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
    RecordRollback(L"registry", L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\PriorityControl\\Win32PrioritySeparation", L"2", std::to_wstring(value), L"Win32PrioritySeparation");
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
    RecordRollback(L"registry", L"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\NetworkThrottlingIndex", L"10", L"4294967295", L"Disable NetworkThrottling");
}
static void DisableNagle() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces", 0, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hKey) != ERROR_SUCCESS) return;
    DWORD idx = 0;
    wchar_t sub[256] = {};
    DWORD subLen = 256;
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
    if (g_dryRunMode.load()) return true;
    HKEY hKey = nullptr;
    DWORD sessionInterval = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\SystemRestore", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD sz = sizeof(sessionInterval);
        RegQueryValueExW(hKey, L"RPSessionInterval", nullptr, nullptr, (LPBYTE)&sessionInterval, &sz);
        RegCloseKey(hKey);
    }
    if (sessionInterval == 0) {
        PrintWarn(L"Восстановление системы отключено в Windows.");
        RunCmd(L"powershell -NoProfile -Command \"Enable-ComputerRestore -Drive 'C:\\'\"", L"Enable System Restore", false, 60);
    }

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
    std::error_code ec;
    fs::create_directories(dir, ec);
    RunCmd(L"reg export \"HKLM\\SYSTEM\\CurrentControlSet\\Control\\PriorityControl\" \"" + dir + L"\\PriorityControl.reg\" /y", L"", false);
    RunCmd(L"reg export \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\" \"" + dir + L"\\SystemProfile.reg\" /y", L"", false);
    RunCmd(L"reg export \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\" \"" + dir + L"\\Advanced.reg\" /y", L"", false);
}

static void RestoreRegistryBackup(const std::wstring& regFile) {
    if (!fs::exists(regFile)) {
        PrintErr(L"Файл бэкапа реестра не найден: " + regFile);
        return;
    }
    RunCmd(L"reg import \"" + regFile + L"\"", L"Импорт реестра", false);
}

static void PerformLiteDebloat() {
    WriteAuditLog(L"PerformLiteDebloat", L"User initiated Lite Windows debloat");
    Print(L"=== STARTING LITE WINDOWS DEBLOAT ===");
    Print(L"[+] Creating restore point...");
    CreateRestorePoint(L"ZeroBloat - Lite Debloat");
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
    CleanTemp();
    CleanPrefetch();
    CleanThumbnails();
    Print(L"=== LITE WINDOWS DEBLOAT COMPLETE ===");
}

static void GameBooster(bool enable) {
    std::lock_guard<std::mutex> lk(g_boostMutex);
    if (g_dryRunMode.load()) return;
    static std::map<std::wstring, DWORD> originalStartTypes;
    const wchar_t* svcs[] = { L"SysMain", L"WSearch", L"DiagTrack" };

    if (enable) {
        ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (scm) {
            for (auto s : svcs) {
                ScHandle svc(OpenServiceW(scm.get(), s, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_STOP));
                if (svc) {
                    DWORD cfgSize = 0;
                    (void)QueryServiceConfigW(svc.get(), nullptr, 0, &cfgSize);
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
    std::vector<GameInfo> games;
    std::error_code ec{};
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
                    std::stringstream ss;
                    ss << f.rdbuf();
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
    if (path.empty() || path.size() > MAX_PATH) return false;
    if (!IsSafePath(path, L"", true)) {
        PrintErr(L"[compress_game] Unsafe path rejected");
        return false;
    }
    std::vector<std::wstring> args = { L"/c", L"/s", L"/a", L"/i", L"/f", L"/exe:LZX", path };
    Print(L"[+] Сжатие папки: " + path + L" (может занять время)...");
    return RunCmdArgs(L"compact.exe", args, L"Сжатие LZX", 3600);
}

static bool InstallAppWinget(const std::wstring& packageId) {
    if (packageId.empty() || packageId.size() > 256) return false;
    for (wchar_t c : packageId) {
        if (!(iswalnum(c) || c == L'.' || c == L'-' || c == L'_')) {
            PrintErr(L"Неверный ID пакета. Разрешены только буквы, цифры, точки, дефисы, подчёркивания.");
            return false;
        }
    }
    std::vector<std::wstring> args = { L"install", L"--id", packageId, L"-e", L"--accept-package-agreements", L"--accept-source-agreements", L"--silent" };
    DWORD exitCode = 0;
    bool ok = RunProcessSafe(L"winget.exe", args, exitCode, 1200);
    return ok && (exitCode == 0 || exitCode == (DWORD)-1978335189);
}

static void LaunchAfterburnerHidden() {
    std::wstring path = L"C:\\Program Files (x86)\\MSI Afterburner\\MSIAfterburner.exe";
    std::error_code ec{};
    if (fs::exists(path, ec) && !ec) {
        ShellExecuteW(nullptr, L"open", path.c_str(), L"/Profile1 /StartMinimized", nullptr, SW_HIDE);
    }
    else {
        std::vector<std::wstring> args = { L"install", L"--id", L"MSI.Afterburner", L"-e", L"--silent", L"--accept-package-agreements", L"--accept-source-agreements" };
        DWORD exitCode = 0;
        RunProcessSafe(L"winget.exe", args, exitCode, 600);
        ShellExecuteW(nullptr, L"open", path.c_str(), L"/Profile1 /StartMinimized", nullptr, SW_HIDE);
    }
}

static bool ShredFileW(const std::wstring& path) {
    if (path.empty() || path.size() > MAX_PATH) return false;
    if (!IsSafePath(path, L"", true)) {
        PrintErr(L"[shred_file] Unsafe path rejected");
        return false;
    }
    if (g_dryRunMode.load()) return true;
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

struct UwpAppInfo {
    std::wstring packageName;
    std::wstring displayName;
    std::wstring category;
    std::wstring description;
};

static std::vector<UwpAppInfo> GetUwpAppList() {
    return {
        { L"Microsoft.BingWeather", L"Погода", L"safe", L"Легко заменяется сайтом." },
        { L"Microsoft.GetHelp", L"Справка", L"safe", L"Не критично." },
        { L"Microsoft.Getstarted", L"Советы", L"safe", L"Бесполезно." },
        { L"Microsoft.MicrosoftOfficeHub", L"Office Hub", L"safe", L"Без Office не нужен." },
        { L"Microsoft.MicrosoftSolitaireCollection", L"Косынка", L"safe", L"Игра." },
        { L"Microsoft.MicrosoftStickyNotes", L"Записки", L"caution", L"Может использоваться." },
        { L"Microsoft.People", L"Люди", L"safe", L"Не критично." },
        { L"Microsoft.SkypeApp", L"Skype", L"caution", L"Если не нужен — можно." },
        { L"Microsoft.WindowsAlarms", L"Будильник", L"caution", L"Может использоваться." },
        { L"Microsoft.WindowsCamera", L"Камера", L"caution", L"Может использоваться." },
        { L"Microsoft.WindowsMaps", L"Карты", L"safe", L"Легко заменяется." },
        { L"Microsoft.WindowsFeedbackHub", L"Отзывы", L"safe", L"Только для отзывов MS." },
        { L"Microsoft.Xbox.TCUI", L"Xbox TCUI", L"donottouch", L"Требуется для Xbox игр." },
        { L"Microsoft.XboxApp", L"Xbox App", L"caution", L"Только для Xbox." },
        { L"Microsoft.XboxGameOverlay", L"Xbox Overlay", L"caution", L"Для геймеров." },
        { L"Microsoft.XboxGamingOverlay", L"Xbox Gaming Overlay", L"caution", L"Для геймеров." },
        { L"Microsoft.XboxIdentityProvider", L"Xbox Identity", L"donottouch", L"Нужен для Game Pass." },
        { L"Microsoft.XboxSpeechToTextOverlay", L"Xbox STT", L"caution", L"Для геймеров." },
        { L"Microsoft.ZuneMusic", L"Groove Music", L"caution", L"Музыка." },
        { L"Microsoft.ZuneVideo", L"Films & TV", L"caution", L"Видео." },
        { L"Microsoft.YourPhone", L"Phone Link", L"caution", L"Связь с телефоном." },
        { L"Microsoft.MixedReality.Portal", L"Mixed Reality", L"safe", L"Только для VR." },
    };
}

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
            catch (const std::exception&) {}
        }
    }
    catch (const std::exception&) {}
    return total;
}

static uint64_t GetRecycleBinSizeForDrive(wchar_t letter) {
    if (letter < L'A' || letter > L'Z') return 0;
    std::wstring path = std::wstring(1, letter) + L":\\$Recycle.Bin";
    std::error_code ec{};
    if (!fs::exists(path, ec) || ec) return 0;
    return GetFolderSizeBytes(path);
}

static std::vector<SmartItem> SmartScan() {
    std::vector<SmartItem> items;
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    uint64_t ramFree = ms.ullAvailPhys;
    wchar_t activeLetter = g_activeDrive.load();

    std::wstring tempPath = GetEnvW(L"TEMP");
    uint64_t tempSize = tempPath.empty() ? 0 : GetFolderSizeBytes(tempPath);
    if (tempSize > 5ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_temp";
        it.category = L"Очистка";
        it.title = L"Временные файлы пользователя";
        it.desc = L"Файлы из %TEMP%. Полностью безопасно удалять — приложения создадут их заново.";
        it.safety = L"safe";
        it.impact = tempSize > 500ULL * 1024 * 1024 ? L"high" : L"low";
        it.sizeBytes = tempSize;
        it.defaultOn = true;
        items.push_back(it);
    }
    uint64_t winTempSize = GetFolderSizeBytes(g_paths.SystemRoot + L"\\Temp");
    if (winTempSize > 5ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_win_temp";
        it.category = L"Очистка";
        it.title = L"Системные временные файлы";
        it.desc = L"Файлы из Windows\\Temp. Безопасно удалять.";
        it.safety = L"safe";
        it.impact = winTempSize > 500ULL * 1024 * 1024 ? L"high" : L"low";
        it.sizeBytes = winTempSize;
        it.defaultOn = true;
        items.push_back(it);
    }
    uint64_t rbSize = GetRecycleBinSizeForDrive(activeLetter);
    if (rbSize > 1024ULL * 1024) {
        SmartItem it;
        it.id = L"clean_recycle";
        it.category = L"Очистка";
        it.title = L"Корзина (" + std::wstring(1, activeLetter) + L":)";
        it.desc = L"Файлы из корзины выбранного диска.";
        it.safety = L"safe";
        it.impact = rbSize > 1024ULL * 1024 * 1024 ? L"high" : L"medium";
        it.sizeBytes = rbSize;
        it.defaultOn = true;
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
    catch (const std::exception&) {}
    if (thumbSize > 5ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_thumbs";
        it.category = L"Очистка";
        it.title = L"Кэш миниатюр";
        it.desc = L"Кэш эскизов Проводника. Windows пересоздаст его автоматически.";
        it.safety = L"safe";
        it.impact = L"low";
        it.sizeBytes = thumbSize;
        it.defaultOn = true;
        items.push_back(it);
    }
    std::wstring localApp = GetEnvW(L"LOCALAPPDATA");
    uint64_t shaderSize = GetFolderSizeBytes(localApp + L"\\D3DSCache") + GetFolderSizeBytes(localApp + L"\\NVIDIA\\DXCache") +
        GetFolderSizeBytes(localApp + L"\\AMD\\DxCache") + GetFolderSizeBytes(localApp + L"\\Intel\\ShaderCache");
    if (shaderSize > 50ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_shader";
        it.category = L"Очистка";
        it.title = L"Кэш шейдеров";
        it.desc = L"Кэш графических шейдеров. Игры пересоздадут его.";
        it.safety = L"safe";
        it.impact = shaderSize > 500ULL * 1024 * 1024 ? L"medium" : L"low";
        it.sizeBytes = shaderSize;
        it.defaultOn = true;
        items.push_back(it);
    }
    uint64_t doSize = GetFolderSizeBytes(L"C:\\Windows\\ServiceProfiles\\NetworkService\\AppData\\Local\\Microsoft\\Windows\\DeliveryOptimization\\Cache");
    if (doSize > 50ULL * 1024 * 1024) {
        SmartItem it;
        it.id = L"clean_do";
        it.category = L"Очистка";
        it.title = L"Кэш обновлений Windows";
        it.desc = L"Временный кэш Delivery Optimization. Безопасно очищать.";
        it.safety = L"safe";
        it.impact = doSize > 1024ULL * 1024 * 1024 ? L"high" : L"medium";
        it.sizeBytes = doSize;
        it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"clean_dns";
        it.category = L"Сеть";
        it.title = L"Сброс DNS-кэша";
        it.desc = L"Очищает кэш DNS.";
        it.safety = L"safe";
        it.impact = L"low";
        it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"clean_standby";
        it.category = L"Память";
        it.title = L"Очистка списка ожидания (Standby)";
        it.desc = L"Освобождает память, занятую кэшем.";
        it.safety = L"safe";
        it.impact = ramFree < 4ULL * 1024 * 1024 * 1024 ? L"medium" : L"low";
        it.defaultOn = true;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"power_high";
        it.category = L"Производительность";
        it.title = L"Схема питания: Высокая производительность";
        it.desc = L"Отключает энергосбережение CPU.";
        it.safety = L"safe";
        it.impact = L"medium";
        it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"game_mode";
        it.category = L"Игры";
        it.title = L"Включить Game Mode";
        it.desc = L"Приоритизация игр в Windows.";
        it.safety = L"safe";
        it.impact = L"low";
        it.defaultOn = true;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"disable_net_throttle";
        it.category = L"Сеть";
        it.title = L"Отключить NetworkThrottling";
        it.desc = L"Отключает ограничение сети Windows для мультимедиа.";
        it.safety = L"caution";
        it.impact = L"low";
        it.defaultOn = true;
        items.push_back(it);
    }
    {
        SmartItem it;
        it.id = L"priority_sep";
        it.category = L"Производительность";
        it.title = L"Win32PrioritySeparation = 0x26";
        it.desc = L"Приоритет CPU для активного приложения.";
        it.safety = L"caution";
        it.impact = L"low";
        it.defaultOn = false;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"disable_nagle";
        it.category = L"Сеть";
        it.title = L"Отключить алгоритм Nagle";
        it.desc = L"Снижает задержку в онлайн-играх.";
        it.safety = L"caution";
        it.impact = L"low";
        it.defaultOn = false;
        items.push_back(it);
    }
    if (activeLetter == g_systemDriveLetter) {
        SmartItem it;
        it.id = L"disable_telemetry";
        it.category = L"Приватность";
        it.title = L"Отключить телеметрию Windows";
        it.desc = L"Отключает службы сбора телеметрии.";
        it.safety = L"caution";
        it.impact = L"low";
        it.defaultOn = false;
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
        return RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"Power High", false);
    }
    if (id == L"game_mode") { SetGameMode(true); return true; }
    if (id == L"disable_net_throttle") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        DisableNetworkThrottling();
        return true;
    }
    if (id == L"priority_sep") { SetWin32PrioritySeparation(0x26); return true; }
    if (id == L"disable_nagle") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        DisableNagle();
        return true;
    }
    if (id == L"disable_telemetry") {
        if (g_activeDrive.load() != g_systemDriveLetter) return false;
        DisableTelemetry();
        return true;
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
        { HKEY_CURRENT_USER,  L"Software\\Microsoft\\Windows\\CurrentVersion\\Run-ZeroBloatBackup", L"HKCU", false },
        { HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run-ZeroBloatBackup", L"HKLM", true },
    };
    for (const auto& loc : locs) {
        HKEY rawKey = nullptr;
        if (RegOpenKeyExW(loc.hive, loc.path, 0, KEY_READ, &rawKey) != ERROR_SUCCESS) continue;
        UniqueRegKey key(rawKey);
        DWORD idx = 0;
        for (;;) {
            wchar_t name[512] = {};
            DWORD nameSize = 512;
            if (RegEnumValueW(key.get(), idx++, name, &nameSize, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            DWORD type = 0, size = 0;
            if (RegQueryValueExW(key.get(), name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) continue;
            if ((type != REG_SZ && type != REG_EXPAND_SZ) || size == 0 || size > 1024 * 1024) continue;
            std::wstring cmd(size / sizeof(wchar_t) + 2, L'\0');
            if (RegQueryValueExW(key.get(), name, nullptr, &type, (LPBYTE)cmd.data(), &size) != ERROR_SUCCESS) continue;
            cmd.resize(wcslen(cmd.c_str()));
            AutoEntry e;
            e.name = name;
            e.command = cmd;
            e.location = loc.loc;
            e.isRegistry = true;
            e.keyPath = loc.path;
            e.isHKLM = loc.hklm;
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
                            AutoEntry e;
                            e.name = p.path().filename().wstring();
                            e.command = p.path().wstring();
                            e.location = (id == CSIDL_STARTUP) ? L"User Startup" : L"Common Startup";
                            e.isRegistry = false;
                            e.isEnabled = true;
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
    if (loc != L"HKCU" && loc != L"HKLM") return false;
    if (act != L"enable" && act != L"disable") return false;
    if (g_dryRunMode.load()) return true;
    HKEY hive = (loc == L"HKCU") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring runPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::wstring bakPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run-ZeroBloatBackup";

    if (act == L"disable") {
        HKEY hRun = nullptr;
        if (RegOpenKeyExW(hive, runPath.c_str(), 0, KEY_READ | KEY_WRITE, &hRun) != ERROR_SUCCESS) return false;
        wchar_t data[2048] = {};
        DWORD size = sizeof(data);
        DWORD type = 0;
        if (RegQueryValueExW(hRun, name.c_str(), nullptr, &type, (LPBYTE)data, &size) == ERROR_SUCCESS) {
            HKEY hBak = nullptr;
            if (RegCreateKeyExW(hive, bakPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hBak, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(hBak, name.c_str(), 0, type, (const BYTE*)data, size);
                RegCloseKey(hBak);
            }
            RegDeleteValueW(hRun, name.c_str());
            RegCloseKey(hRun);
            RecordRollback(L"autostart", name, data, L"", L"Disable autostart");
            return true;
        }
        RegCloseKey(hRun);
        return false;
    }
    else if (act == L"enable") {
        HKEY hBak = nullptr;
        if (RegOpenKeyExW(hive, bakPath.c_str(), 0, KEY_READ | KEY_WRITE, &hBak) != ERROR_SUCCESS) return false;
        wchar_t data[2048] = {};
        DWORD size = sizeof(data);
        DWORD type = 0;
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
        needed = 0;
        count = 0;
        resume = 0;
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
            DWORD cfgSize = 0;
            (void)QueryServiceConfigW(svc.get(), nullptr, 0, &cfgSize);
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
    if (name.empty() || name.size() > 256) { err = L"Invalid service name"; return false; }
    for (wchar_t c : name) {
        if (!(iswalnum(c) || c == L'_' || c == L'-' || c == L'.')) { err = L"Invalid service name"; return false; }
    }
    if (action != L"start" && action != L"stop" && action != L"auto" && action != L"manual" && action != L"disable") {
        err = L"Invalid action";
        return false;
    }
    if (g_dryRunMode.load()) return true;
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) { err = L"OpenSCManager failed"; return false; }
    DWORD access = SERVICE_QUERY_STATUS;
    if (action == L"start") access |= SERVICE_START;
    else if (action == L"stop") access |= SERVICE_STOP;
    else access |= SERVICE_CHANGE_CONFIG;
    ScHandle svc(OpenServiceW(scm.get(), name.c_str(), access));
    if (!svc) { err = L"OpenService failed"; return false; }
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
        if (action == L"auto") startType = SERVICE_AUTO_START;
        else if (action == L"manual") startType = SERVICE_DEMAND_START;
        else if (action == L"disable") startType = SERVICE_DISABLED;
        ok = ChangeServiceConfigW(svc.get(), SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != FALSE;
        if (ok) RecordRollback(L"service", name, L"auto", action, L"Service " + name + L" -> " + action);
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
            wchar_t sub[256] = {};
            DWORD subLen = 256;
            while (true) {
                subLen = 256;
                if (RegEnumKeyExW(hKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                HKEY hSub = nullptr;
                if (RegOpenKeyExW(hKey, sub, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                    wchar_t clsid[256];
                    DWORD clsidSize = sizeof(clsid);
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
    if (g_dryRunMode.load()) return true;
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
            RegCloseKey(hKey);
            return true;
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
    WinHttpHandle hSession(WinHttpOpen(L"ZeroBloat/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
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
    std::vector<char> buf(65536);
    DWORD read = 0;
    while (WinHttpReadData(hRequest.get(), buf.data(), (DWORD)buf.size(), &read) && read > 0) {
        DWORD written = 0;
        WriteFile(hFile.get(), buf.data(), read, &written, nullptr);
    }
    hFile.reset();

    if (!VerifySignature(tmp.path)) {
        MessageBoxW(nullptr, L"Signature verification failed for WebView2 installer.", L"Security Error", MB_ICONERROR);
        return false;
    }

    std::wstring cmd = L"\"" + std::wstring(tmp.path) + L"\" /silent /install";
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cl(cmd.begin(), cmd.end());
    cl.push_back(L'\0');
    if (!CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, 300000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static LRESULT CALLBACK BootProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RGB(230, 230, 240));
        SetBkColor(hdc, RGB(18, 21, 30));
        static HBRUSH hbr = CreateSolidBrush(RGB(18, 21, 30));
        return (LRESULT)hbr;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        static HBRUSH br = CreateSolidBrush(RGB(18, 21, 30));
        FillRect(hdc, &rc, br);
        return 1;
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
                        catch (const std::exception&) {
                            PostMessageW(hwnd, WM_BOOT_DONE, 0, 0);
                        }
                        }).detach();
                }
                else {
                    MessageBoxW(hwnd, L"Установка уже идет. Пожалуйста, подождите.", L"ZeroBloat", MB_ICONINFORMATION);
                }
                return 0;
            }
            g_bootstrapperDone = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool ShowBootstrapper(HINSTANCE hInst) {
    if (IsWebView2Installed()) g_webviewReady = true;
    if (g_webviewReady) {
        g_bootstrapperDone = true;
        return true;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = BootProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"OptBootWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = g_hIconBig ? g_hIconBig : LoadAppIcon(32);
    wc.hIconSm = g_hIconSmall ? g_hIconSmall : LoadAppIcon(16);
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 560, 340 };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"OptBootWnd", L"ZeroBloat - Подготовка",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        g_bootstrapperDone = true;
        return false;
    }

    HFONT font = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT fontBold = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    HWND hTitle = CreateWindowW(L"STATIC", L"⚡ ZeroBloat — Подготовка", WS_CHILD | WS_VISIBLE, 20, 15, 500, 30, hwnd, nullptr, hInst, nullptr);
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
            if (g_webviewReady) {
                DestroyWindow(hwnd);
                g_bootstrapperDone = true;
                break;
            }
            else {
                MessageBoxW(hwnd, L"Не удалось установить WebView2.\nСкачайте вручную: https://developer.microsoft.com/microsoft-edge/webview2/", L"Ошибка", MB_ICONERROR);
                HWND hBtnFail = GetDlgItem(hwnd, 100);
                SetWindowTextW(hBtnFail, L"Попробовать снова");
                EnableWindow(hBtnFail, TRUE);
                g_isInstalling = false;
            }
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    DeleteObject(font);
    DeleteObject(fontBold);
    return g_webviewReady;
}

static NOTIFYICONDATAW g_nid = {};
static ULONGLONG g_lastHighLoadNotify = 0;

static void InitTray(HWND hwnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_hIconSmall ? g_hIconSmall : LoadAppIcon(16);
    wcsncpy_s(g_nid.szTip, L"ZeroBloat", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void CleanupTray() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

class GameOverlay {
public:
    HWND hwnd = nullptr;
    std::atomic<bool> userEnabled{ true }, isVisible{ false };
    std::atomic<bool> showCpu{ true }, showGpu{ true }, showRam{ true }, showTemp{ true };
    std::atomic<bool> showVram{ true }, showFan{ false }, showPing{ true };
    std::atomic<bool> darkTheme{ true };
    std::atomic<int> posX{ 10 }, posY{ 10 }, alpha{ 255 };
    HFONT hFont = nullptr;

    void Create(HINSTANCE hInst) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = hInst;
        wc.lpszClassName = L"GameOverlayWnd";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"GameOverlayWnd", L"Overlay", WS_POPUP, posX.load(), posY.load(), 320, 240, nullptr, nullptr, hInst, nullptr);
        if (!hwnd) return;
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), (BYTE)alpha.load(), LWA_COLORKEY | LWA_ALPHA);
        hFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        ShowWindow(hwnd, SW_HIDE);
    }

    void Cleanup() {
        if (hFont) {
            DeleteObject(hFont);
            hFont = nullptr;
        }
        if (hwnd) {
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
    }

    void SetPosition(int x, int y) {
        HMONITOR hMon = MonitorFromPoint(POINT{ x, y }, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMon, &mi)) {
            if (x < mi.rcWork.left) x = mi.rcWork.left;
            if (y < mi.rcWork.top) y = mi.rcWork.top;
            if (x > mi.rcWork.right - 320) x = mi.rcWork.right - 320;
            if (y > mi.rcWork.bottom - 240) y = mi.rcWork.bottom - 240;
        }
        posX = x;
        posY = y;
        if (hwnd) SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    void SetAlpha(int a) {
        alpha = a;
        if (hwnd) SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), (BYTE)a, LWA_COLORKEY | LWA_ALPHA);
    }

    void Draw() const {
        if (!hwnd || !isVisible) return;
        HDC hdc = GetDC(hwnd);
        if (!hdc) return;
        RECT rc;
        GetClientRect(hwnd, &rc);
        static HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bg);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, darkTheme.load() ? RGB(0, 255, 100) : RGB(0, 120, 50));
        HGDIOBJ old = hFont ? SelectObject(hdc, hFont) : nullptr;
        int y = 10;
        wchar_t buf[128] = {};
        if (showCpu) {
            swprintf_s(buf, L"CPU: %.1f%%%s", g_cpuUsage.load(), g_throttling.load() ? L" THR" : L"");
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showGpu) {
            swprintf_s(buf, L"GPU: %.1f%%", g_gpuUsage.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showRam) {
            MEMORYSTATUSEX ms = { sizeof(ms) };
            GlobalMemoryStatusEx(&ms);
            swprintf_s(buf, L"RAM: %llu MB", static_cast<unsigned long long>(ms.ullAvailPhys / (1024 * 1024)));
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showVram && g_vramTotalMB.load() > 0) {
            swprintf_s(buf, L"VRAM: %llu/%llu MB", (unsigned long long)g_vramUsedMB.load(), (unsigned long long)g_vramTotalMB.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showTemp && g_cpuTemp.load() > 0) {
            swprintf_s(buf, L"CPU T: %.1fC", g_cpuTemp.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showTemp && g_gpuTemp.load() > 0) {
            swprintf_s(buf, L"GPU T: %.1fC", g_gpuTemp.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showFan && g_gpuFan.load() >= 0) {
            swprintf_s(buf, L"GPU Fan: %.0f%%", g_gpuFan.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (showPing && g_pingMs.load() >= 0) {
            swprintf_s(buf, L"Ping: %d ms", g_pingMs.load());
            TextOutW(hdc, 10, y, buf, (int)wcslen(buf));
            y += 22;
        }
        if (old) SelectObject(hdc, old);
        ReleaseDC(hwnd, hdc);
    }
    void Toggle() {
        userEnabled = !userEnabled;
        if (!userEnabled && hwnd) {
            ShowWindow(hwnd, SW_HIDE);
            isVisible = false;
        }
    }
    void UpdateVisibility(bool inGame) {
        bool should = userEnabled && inGame;
        if (isVisible != should) {
            isVisible = should;
            if (hwnd) ShowWindow(hwnd, isVisible ? SW_SHOWNA : SW_HIDE);
        }
    }
};

static GameOverlay g_overlay;

static void LoadOverlaySettings() {
    if (g_portableMode.load()) {
        std::wstring path = g_paths.BackupDir + L"\\overlay.json";
        std::ifstream f(path.c_str());
        if (f) {
            std::stringstream ss;
            ss << f.rdbuf();
            std::string utf8 = ss.str();
            auto getInt = [&](const char* key, std::atomic<int>& out) {
                std::string needle = std::string("\"") + key + "\":";
                auto pos = utf8.find(needle);
                if (pos != std::string::npos) {
                    try { out = std::stoi(utf8.substr(pos + needle.size())); }
                    catch (const std::exception&) {}
                }
                };
            auto getBool = [&](const char* key, std::atomic<bool>& out) {
                std::string needle = std::string("\"") + key + "\":";
                auto pos = utf8.find(needle);
                if (pos != std::string::npos) {
                    out = (utf8.compare(pos + needle.size(), 4, "true") == 0);
                }
                };
            getInt("posX", g_overlay.posX);
            getInt("posY", g_overlay.posY);
            getInt("alpha", g_overlay.alpha);
            getBool("darkTheme", g_overlay.darkTheme);
            getBool("showCpu", g_overlay.showCpu);
            getBool("showGpu", g_overlay.showGpu);
            getBool("showRam", g_overlay.showRam);
            getBool("showTemp", g_overlay.showTemp);
            getBool("showVram", g_overlay.showVram);
            getBool("showFan", g_overlay.showFan);
            getBool("showPing", g_overlay.showPing);
        }
    }
    else {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ZeroBloat\\Overlay", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0;
            DWORD sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"PosX", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.posX = val;
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"PosY", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.posY = val;
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"Alpha", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.alpha = val;
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"DarkTheme", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.darkTheme = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowCpu", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showCpu = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowGpu", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showGpu = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowRam", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showRam = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowTemp", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showTemp = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowVram", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showVram = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowFan", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showFan = (val != 0);
            sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"ShowPing", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) g_overlay.showPing = (val != 0);
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
        j += L"\"showPing\":" + std::wstring(g_overlay.showPing.load() ? L"true" : L"false");
        j += L"}";
        std::ofstream f(path.c_str(), std::ios::binary);
        if (f) {
            std::string u = WideToUtf8(j);
            f.write(u.data(), (std::streamsize)u.size());
        }
    }
    else {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\ZeroBloat\\Overlay", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            DWORD px = (DWORD)g_overlay.posX.load();
            RegSetValueExW(hKey, L"PosX", 0, REG_DWORD, (const BYTE*)&px, sizeof(px));
            DWORD py = (DWORD)g_overlay.posY.load();
            RegSetValueExW(hKey, L"PosY", 0, REG_DWORD, (const BYTE*)&py, sizeof(py));
            DWORD al = (DWORD)g_overlay.alpha.load();
            RegSetValueExW(hKey, L"Alpha", 0, REG_DWORD, (const BYTE*)&al, sizeof(al));
            DWORD dt = g_overlay.darkTheme.load() ? 1 : 0;
            RegSetValueExW(hKey, L"DarkTheme", 0, REG_DWORD, (const BYTE*)&dt, sizeof(dt));
            DWORD c = g_overlay.showCpu.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowCpu", 0, REG_DWORD, (const BYTE*)&c, sizeof(c));
            DWORD g = g_overlay.showGpu.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowGpu", 0, REG_DWORD, (const BYTE*)&g, sizeof(g));
            DWORD r = g_overlay.showRam.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowRam", 0, REG_DWORD, (const BYTE*)&r, sizeof(r));
            DWORD t = g_overlay.showTemp.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowTemp", 0, REG_DWORD, (const BYTE*)&t, sizeof(t));
            DWORD v = g_overlay.showVram.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowVram", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
            DWORD f = g_overlay.showFan.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowFan", 0, REG_DWORD, (const BYTE*)&f, sizeof(f));
            DWORD p = g_overlay.showPing.load() ? 1 : 0;
            RegSetValueExW(hKey, L"ShowPing", 0, REG_DWORD, (const BYTE*)&p, sizeof(p));
            RegCloseKey(hKey);
        }
    }
}

static void EnumerateTasksRecursive(ITaskFolder* folder, std::vector<std::wstring>& result, int& count) {
    if (!folder || count > 500) return;
    ComPtr<IRegisteredTaskCollection> coll;
    if (SUCCEEDED(folder->GetTasks(TASK_ENUM_HIDDEN, &coll))) {
        LONG c = 0;
        coll->get_Count(&c);
        for (LONG i = 1; i <= c && count < 500; ++i) {
            ComPtr<IRegisteredTask> task;
            if (FAILED(coll->get_Item(_variant_t(i), &task))) continue;
            _bstr_t name;
            task->get_Name(name.GetAddress());
            _bstr_t path;
            task->get_Path(path.GetAddress());
            VARIANT_BOOL enabled = VARIANT_FALSE;
            task->get_Enabled(&enabled);
            const wchar_t* pName = (const wchar_t*)name;
            const wchar_t* pPath = (const wchar_t*)path;
            std::wstring entry = (pName ? pName : L"") + std::wstring(L"\x1F") + (pPath ? pPath : L"") + L"\x1F" + (enabled ? L"1" : L"0");
            result.push_back(entry);
            count++;
        }
    }

    ComPtr<ITaskFolderCollection> subColl;
    if (SUCCEEDED(folder->GetFolders(0, &subColl))) {
        LONG sc = 0;
        subColl->get_Count(&sc);
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

static bool CreateAutoCleanupTask(const std::wstring& schedule) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring taskName = L"ZeroBloat_AutoCleanup";
    std::wstring sc;
    if (schedule == L"daily") sc = L"DAILY";
    else if (schedule == L"weekly") sc = L"WEEKLY";
    else if (schedule == L"onidle") sc = L"ONIDLE";
    else return false;
    std::vector<std::wstring> args = { L"/Create", L"/TN", taskName, L"/TR", L"\"" + std::wstring(exePath) + L"\" --auto-clean", L"/SC", sc, L"/F", L"/RL", L"HIGHEST" };
    if (schedule == L"daily") { args.push_back(L"/ST"); args.push_back(L"03:00"); }
    else if (schedule == L"weekly") { args.push_back(L"/D"); args.push_back(L"SUN"); args.push_back(L"/ST"); args.push_back(L"03:00"); }
    return RunCmdArgs(L"schtasks.exe", args, L"", 60);
}

static bool DeleteAutoCleanupTask() {
    std::vector<std::wstring> args = { L"/Delete", L"/TN", L"ZeroBloat_AutoCleanup", L"/F" };
    return RunCmdArgs(L"schtasks.exe", args, L"", 30);
}

static std::vector<std::wstring> ScanLeftovers(const std::wstring& programName) {
    std::vector<std::wstring> result;
    if (programName.empty()) return result;
    std::wstring compact;
    for (wchar_t c : programName) if (iswalnum(c)) compact += static_cast<wchar_t>(::towlower(c));
    if (compact.size() < 4) return result;
    std::vector<std::wstring> dirs = {
        GetEnvW(L"APPDATA"), GetEnvW(L"LOCALAPPDATA"), GetEnvW(L"ProgramData"),
        L"C:\\Program Files", L"C:\\Program Files (x86)"
    };
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (dir.empty() || !fs::exists(dir, ec)) continue;
        try {
            for (auto& p : fs::directory_iterator(dir, ec)) {
                if (ec) { ec.clear(); continue; }
                if (!p.is_directory(ec)) continue;
                std::wstring name = p.path().filename().wstring();
                std::wstring nCompact;
                for (wchar_t c : name) if (iswalnum(c)) nCompact += static_cast<wchar_t>(::towlower(c));
                if (nCompact == compact || (nCompact.size() > 6 && compact.find(nCompact) != std::wstring::npos)) {
                    result.push_back(p.path().wstring());
                }
            }
        }
        catch (const std::exception&) {}
    }
    return result;
}

struct BenchmarkData {
    long long timestamp = 0;
    uint64_t freeRamMB = 0;
    DWORD processCount = 0;
    DWORD serviceCount = 0;
    double diskLatencyMs = 0;
    double cpuScore = 0;
};
static std::vector<BenchmarkData> g_benchHistory;

static BenchmarkData CaptureBenchmark() {
    BenchmarkData b;
    b.timestamp = (long long)time(nullptr);
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    b.freeRamMB = ms.ullAvailPhys / (1024 * 1024);
    b.processCount = CountProcesses();
    b.serviceCount = CountServices();

    auto start = std::chrono::high_resolution_clock::now();
    TempFile tf;
    if (tf.Create(L"bench")) {
        UniqueHandle h(CreateFileW(tf.path, GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (h) {
            std::vector<char> buf(1024 * 1024, 'X');
            DWORD written = 0;
            WriteFile(h.get(), buf.data(), (DWORD)buf.size(), &written, nullptr);
            FlushFileBuffers(h.get());
            SetFilePointer(h.get(), 0, nullptr, FILE_BEGIN);
            DWORD rd = 0;
            (void)ReadFile(h.get(), buf.data(), (DWORD)buf.size(), &rd, nullptr);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    b.diskLatencyMs = std::chrono::duration<double, std::milli>(end - start).count();

    auto cstart = std::chrono::high_resolution_clock::now();
    volatile double acc = 0.0;
    for (int i = 0; i < 5000000; ++i) acc += std::sqrt((double)i);
    auto cend = std::chrono::high_resolution_clock::now();
    double cms = std::chrono::duration<double, std::milli>(cend - cstart).count();
    b.cpuScore = cms > 0 ? 5000000.0 / cms : 0;
    return b;
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
            wchar_t sub[256] = {};
            DWORD subLen = 256;
            if (RegEnumKeyExW(hKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY hSub = nullptr;
            if (RegOpenKeyExW(hKey, sub, 0, loc.sam, &hSub) != ERROR_SUCCESS) continue;

            ProgramInfo p;
            p.is64 = loc.is64;

            auto readStr = [&](const wchar_t* name, std::wstring& out) {
                wchar_t buf[1024] = {};
                DWORD size = sizeof(buf);
                DWORD type = 0;
                if (RegQueryValueExW(hSub, name, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
                    out = buf;
                }
                };
            auto readDword = [&](const wchar_t* name, uint64_t& out) {
                DWORD val = 0;
                DWORD size = sizeof(val);
                DWORD type = 0;
                if (RegQueryValueExW(hSub, name, nullptr, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                    out = val;
                }
                };

            readStr(L"DisplayName", p.name);
            if (p.name.empty()) {
                RegCloseKey(hSub);
                continue;
            }
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

static std::wstring GenerateHtmlReport() {
    std::error_code ec{};
    fs::create_directories(g_paths.BackupDir, ec);
    std::wstring html = L"<html><head><meta charset='utf-8'><title>ZeroBloat Report</title><style>body{font-family:Segoe UI;background:#0f111a;color:#e2e8f0;padding:20px}h1{color:#ff0055}h2{color:#00f0ff;border-bottom:1px solid #22283a;padding-bottom:8px}table{border-collapse:collapse;width:100%;margin:10px 0}td,th{border:1px solid #22283a;padding:8px;text-align:left}th{background:#1a1f2e}</style></head><body>";
    html += L"<h1>ZeroBloat — System Report</h1>";
    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_s(&tmNow, &now);
    html += Fmt(L"<p>Date: %04d-%02d-%02d %02d:%02d:%02d</p>", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
    html += L"<h2>System Info</h2><table>";
    std::wstring pcInfo;
    { std::lock_guard<std::mutex> lk(g_pcInfoMutex); pcInfo = g_pcInfoJson; }
    html += L"<tr><td>PC Info</td><td>" + pcInfo + L"</td></tr>";
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    html += L"<tr><td>RAM Total</td><td>" + std::to_wstring(ms.ullTotalPhys / (1024 * 1024)) + L" MB</td></tr>";
    html += L"<tr><td>RAM Free</td><td>" + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) + L" MB</td></tr>";
    html += L"<tr><td>Processes</td><td>" + std::to_wstring(g_procCount.load()) + L"</td></tr>";
    html += L"<tr><td>CPU Usage</td><td>" + FloatToJsonStr(g_cpuUsage.load()) + L"%</td></tr>";
    html += L"<tr><td>GPU Usage</td><td>" + FloatToJsonStr(g_gpuUsage.load()) + L"%</td></tr>";
    html += L"<tr><td>CPU Temp</td><td>" + FloatToJsonStr(g_cpuTemp.load()) + L" C</td></tr>";
    html += L"<tr><td>GPU Temp</td><td>" + FloatToJsonStr(g_gpuTemp.load()) + L" C</td></tr>";
    html += L"</table>";
    {
        std::wstring disks;
        { std::lock_guard<std::mutex> lk(g_diskHealthMutex); disks = g_diskHealthJson; }
        html += L"<h2>Disks</h2><pre>" + disks + L"</pre>";
    }
    html += L"</body></html>";

    std::wstring outPath = g_paths.AppDir + L"\\report.html";
    std::ofstream f(outPath.c_str(), std::ios::binary);
    if (f) {
        std::string u = WideToUtf8(html);
        f.write(u.data(), (std::streamsize)u.size());
    }
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
    std::stringstream ss;
    ss << f.rdbuf();
    std::string utf8 = ss.str();
    auto getBool = [&](const char* key, bool& out) {
        std::string needle = std::string("\"") + key + "\":";
        auto pos = utf8.find(needle);
        if (pos == std::string::npos) return;
        auto start = pos + needle.size();
        out = (utf8.compare(start, 4, "true") == 0);
        };
    auto getInt = [&](const char* key, int& out) {
        std::string needle = std::string("\"") + key + "\":";
        auto pos = utf8.find(needle);
        if (pos == std::string::npos) return;
        auto start = pos + needle.size();
        try { out = std::stoi(utf8.substr(start)); }
        catch (const std::exception&) {}
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
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER u;
        u.LowPart = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        u.QuadPart += (ULONGLONG)days * 24ULL * 60ULL * 60ULL * 10000000ULL;
        FILETIME ftOut;
        ftOut.dwLowDateTime = u.LowPart;
        ftOut.dwHighDateTime = u.HighPart;
        SYSTEMTIME st;
        FileTimeToSystemTime(&ftOut, &st);
        std::wstring iso = Fmt(L"%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        RegSetValueExW(hKey, L"PauseUpdatesExpiryTime", 0, REG_SZ, (const BYTE*)iso.c_str(), (DWORD)((iso.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"PauseFeatureUpdatesStartTime", 0, REG_SZ, (const BYTE*)iso.c_str(), (DWORD)((iso.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(hKey, L"PauseQualityUpdatesStartTime", 0, REG_SZ, (const BYTE*)iso.c_str(), (DWORD)((iso.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return L"{\"ok\":true,\"until\":" + JsonStr(iso) + L"}";
    }
    return ErrCode(L"ERR_REG_OPEN", L"Failed to open Windows Update registry key");
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
    return OkJson();
}

static void ResetNetwork() {
    WriteAuditLog(L"ResetNetwork", L"User reset network stack");
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
        RecordRollback(L"registry", L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers\\HwSchMode",
            enable ? L"1" : L"2", enable ? L"2" : L"1", L"ToggleHAGS");
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
    RecordRollback(L"registry", L"HKCU\\Control Panel\\Mouse\\MouseSpeed",
        enable ? L"0" : L"1", enable ? L"1" : L"0", L"MouseAcceleration");
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
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    uint64_t totalRamMB = ms.ullTotalPhys / (1024 * 1024);
    uint64_t recommendedMin = totalRamMB / 2;
    uint64_t recommendedMax = totalRamMB;
    if (totalRamMB >= 32ULL * 1024) {
        recommendedMin = 2048;
        recommendedMax = 8192;
    }
    else if (totalRamMB >= 16ULL * 1024) {
        recommendedMin = 4096;
        recommendedMax = 16384;
    }
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
        wchar_t sub[256];
        DWORD subLen = 256;
        while (true) {
            subLen = 256;
            if (RegEnumKeyExW(hNetKey, idx++, sub, &subLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY hCard = nullptr;
            if (RegOpenKeyExW(hNetKey, sub, 0, KEY_READ, &hCard) == ERROR_SUCCESS) {
                wchar_t guid[256] = {};
                DWORD sz = sizeof(guid);
                DWORD type;
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
    WriteAuditLog(L"DisableETW", L"User disabled ETW/WPP logging");
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
    WriteAuditLog(L"DisableVBS", L"User disabled VBS/HVCI");
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD v = 0;
        RegSetValueExW(hKey, L"Enabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    RunCmd(L"bcdedit /set hypervisorlaunchtype off", L"Disable Hypervisor", false);
    RecordRollback(L"registry", L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity\\Enabled", L"1", L"0", L"DisableVBS");
}

static void ApplyNvidiaPState(int state) {
    wchar_t path[MAX_PATH] = {};
    if (SearchPathW(nullptr, L"nvidia-smi.exe", nullptr, MAX_PATH, path, nullptr) > 0) {
        std::vector<std::wstring> args;
        if (state == 0) args = { L"-pm", L"1" };
        else if (state == 1) args = { L"-pm", L"0" };
        else if (state == 2) args = { L"-lgc", L"0,2100" };
        if (!args.empty()) RunCmdArgs(path, args, L"NVAPI P-State", 30);
    }
}

static std::wstring RunDDU() {
    std::wstring dduPath = g_paths.AppDir + L"\\DDU\\Display Driver Uninstaller.exe";
    if (fs::exists(dduPath)) {
        ShellExecuteW(nullptr, L"open", dduPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return L"{\"ok\":true,\"msg\":\"DDU launched\"}";
    }
    return ErrCode(L"ERR_NOT_FOUND", L"DDU not found. Place DDU folder next to exe.");
}

static void ApplyS50Settings() {
    UserSettings s;
    { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
    bool isSystem = (g_activeDrive.load() == g_systemDriveLetter);
    if (!isSystem && (s.s50_telemetry || s.s50_defender || s.s50_vbs || s.s50_etw || s.s50_winupd || s.s50_services || s.s50_uac || s.s50_smartscreen)) {
        PrintWarn(L"[apply_s50] Dangerous tweaks skipped — non-system drive active.");
    }
    else {
        if (s.s50_telemetry) DisableTelemetry();
        if (s.s50_cortana) {
            HKEY k;
            if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
                DWORD v = 0;
                RegSetValueExW(k, L"AllowCortana", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
                RegCloseKey(k);
            }
        }
        if (s.s50_adid) {
            HKEY k;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
                DWORD v = 0;
                RegSetValueExW(k, L"Enabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
                RegCloseKey(k);
            }
        }
        if (s.s50_actfeed) {
            HKEY k;
            if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
                DWORD v = 0;
                RegSetValueExW(k, L"EnableActivityFeed", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
                RegCloseKey(k);
            }
        }
        if (s.s50_smartscreen) {
            HKEY k;
            if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\System", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
                DWORD v = 0;
                RegSetValueExW(k, L"EnableSmartScreen", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
                RegCloseKey(k);
            }
        }
        if (s.s50_uac) {
            HKEY k;
            if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
                DWORD v = 0;
                RegSetValueExW(k, L"EnableLUA", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
                RegCloseKey(k);
                RecordRollback(L"registry", L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\EnableLUA", L"1", L"0", L"DisableUAC");
            }
        }
        if (s.s50_defender) DisableDefender();
        if (s.s50_vbs) DisableVBS();
        if (s.s50_etw) DisableETW();
        if (s.s50_winupd) {
            RunCmd(L"sc config wuauserv start= disabled", L"Disable WU", false);
            RecordRollback(L"service", L"wuauserv", L"auto", L"disabled", L"Disable Windows Update");
        }
        if (s.s50_services) DisableDeepServices();
    }
    if (s.s50_gamemode) SetGameMode(true);
    if (s.s50_gamedvr) {
        HKEY k;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"System\\GameConfigStore", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
            DWORD v = 0;
            RegSetValueExW(k, L"GameDVR_Enabled", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
            RegCloseKey(k);
        }
    }
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
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        if (ms.ullTotalPhys >= 32ULL * 1024 * 1024 * 1024) {
            RunCmd(L"wmic computersystem set AutomaticManagedPagefile=False", L"Disable Paging");
            RunCmd(L"wmic pagefileset where name=\"C:\\\\pagefile.sys\" delete", L"Delete pagefile");
        }
    }
    if (s.s50_memcomp) RunCmd(L"powershell -NoProfile -Command \"Disable-MMAgent -MemoryCompression\"", L"Disable Mem Comp");
    if (s.s50_fastdns) SetFastDNS();
    if (s.s50_rss) ApplyNetworkTweaks(true);
    if (s.s50_resetnet) ResetNetwork();
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

static void LoadAiConfig() {
    std::lock_guard<std::mutex> lk(g_aiMutex);
    std::ifstream f(g_paths.AiConfigFile.c_str(), std::ios::binary);
    if (!f) return;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string utf8 = ss.str();
    auto getStr = [&](const char* key, std::wstring& out) {
        std::string needle = std::string("\"") + key + "\":\"";
        auto pos = utf8.find(needle);
        if (pos == std::string::npos) return;
        auto start = pos + needle.size();
        auto end = utf8.find("\"", start);
        if (end == std::string::npos) return;
        out = Utf8ToWide(utf8.substr(start, end - start));
        };
    auto getBool = [&](const char* key, bool& out) {
        std::string needle = std::string("\"") + key + "\":";
        auto pos = utf8.find(needle);
        if (pos == std::string::npos) return;
        auto start = pos + needle.size();
        out = (utf8.compare(start, 4, "true") == 0);
        };
    std::wstring encKey;
    getStr("endpoint", g_aiConfig.endpoint);
    getStr("apiKey", encKey);
    if (!encKey.empty()) g_aiConfig.apiKey = DecryptString(encKey);
    getStr("model", g_aiConfig.model);
    getBool("enabled", g_aiConfig.enabled);
    getBool("useLocal", g_aiConfig.useLocal);
}

static void SaveAiConfig() {
    std::lock_guard<std::mutex> lk(g_aiMutex);
    std::wstring j = L"{";
    j += L"\"endpoint\":" + JsonStr(g_aiConfig.endpoint) + L",";
    j += L"\"apiKey\":" + JsonStr(EncryptString(g_aiConfig.apiKey)) + L",";
    j += L"\"model\":" + JsonStr(g_aiConfig.model) + L",";
    j += L"\"enabled\":" + std::wstring(g_aiConfig.enabled ? L"true" : L"false") + L",";
    j += L"\"useLocal\":" + std::wstring(g_aiConfig.useLocal ? L"true" : L"false");
    j += L"}";
    std::ofstream f(g_paths.AiConfigFile.c_str(), std::ios::binary);
    if (f) {
        std::string u = WideToUtf8(j);
        f.write(u.data(), (std::streamsize)u.size());
    }
}

static std::wstring HttpGet(const std::wstring& url, int timeoutSec, std::wstring& errMsg) {
    errMsg.clear();
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    uc.lpszHostName = hostName;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        errMsg = L"Invalid URL";
        return L"";
    }
    bool useTls = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    int port = uc.nPort;

    WinHttpHandle session(WinHttpOpen(L"ZeroBloatUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) { errMsg = L"WinHttpOpen failed"; return L""; }
    WinHttpSetTimeouts(session.get(), 15000, 15000, timeoutSec * 1000, timeoutSec * 1000);

    WinHttpHandle connect(WinHttpConnect(session.get(), hostName, (INTERNET_PORT)port, 0));
    if (!connect) { errMsg = L"WinHttpConnect failed"; return L""; }

    DWORD flags = useTls ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connect.get(), L"GET", urlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) { errMsg = L"WinHttpOpenRequest failed"; return L""; }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        errMsg = L"WinHttpSendRequest failed";
        return L"";
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        errMsg = L"WinHttpReceiveResponse failed";
        return L"";
    }

    DWORD statusCode = 0, sz = sizeof(statusCode);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX);

    std::string response;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(request.get(), &dwSize)) break;
        if (dwSize == 0) break;
        std::string buf(dwSize, 0);
        DWORD dwDownloaded = 0;
        if (WinHttpReadData(request.get(), &buf[0], dwSize, &dwDownloaded)) {
            buf.resize(dwDownloaded);
            response += buf;
        }
        else break;
    } while (dwSize > 0);

    if (statusCode != 200) {
        errMsg = L"HTTP " + std::to_wstring(statusCode);
        return L"";
    }
    return Utf8ToWide(response);
}

static std::wstring HttpDownloadFile(const std::wstring& url, const std::wstring& outPath, std::wstring& errMsg) {
    errMsg.clear();
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    uc.lpszHostName = hostName;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { errMsg = L"Invalid URL"; return L""; }
    bool useTls = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    int port = uc.nPort;

    WinHttpHandle session(WinHttpOpen(L"ZeroBloatUpdater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) { errMsg = L"WinHttpOpen failed"; return L""; }
    WinHttpHandle connect(WinHttpConnect(session.get(), hostName, (INTERNET_PORT)port, 0));
    if (!connect) { errMsg = L"WinHttpConnect failed"; return L""; }

    DWORD flags = useTls ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connect.get(), L"GET", urlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) { errMsg = L"WinHttpOpenRequest failed"; return L""; }

    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) { errMsg = L"WinHttpSendRequest failed"; return L""; }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) { errMsg = L"WinHttpReceiveResponse failed"; return L""; }

    UniqueHandle hFile(CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!hFile) { errMsg = L"Failed to create file"; return L""; }

    std::vector<char> buf(65536);
    DWORD read = 0;
    while (WinHttpReadData(request.get(), buf.data(), (DWORD)buf.size(), &read) && read > 0) {
        DWORD written = 0;
        WriteFile(hFile.get(), buf.data(), read, &written, nullptr);
    }
    return outPath;
}

static std::wstring HttpPostJson(const std::wstring& url, const std::wstring& bearer, const std::wstring& jsonBody, int timeoutSec, std::wstring& errMsg) {
    errMsg.clear();
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    uc.lpszHostName = hostName;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
        errMsg = L"Invalid URL";
        return L"";
    }
    bool useTls = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    int port = uc.nPort;

    WinHttpHandle session(WinHttpOpen(L"ZeroBloatAI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        errMsg = L"WinHttpOpen failed";
        return L"";
    }
    WinHttpSetTimeouts(session.get(), 30000, 30000, timeoutSec * 1000, timeoutSec * 1000);

    WinHttpHandle connect(WinHttpConnect(session.get(), hostName, (INTERNET_PORT)port, 0));
    if (!connect) {
        errMsg = L"WinHttpConnect failed";
        return L"";
    }

    DWORD flags = useTls ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connect.get(), L"POST", urlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
        errMsg = L"WinHttpOpenRequest failed";
        return L"";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!bearer.empty()) headers += L"Authorization: Bearer " + bearer + L"\r\n";

    std::string bodyUtf8 = WideToUtf8(jsonBody);

    if (!WinHttpSendRequest(request.get(), headers.c_str(), (DWORD)-1L,
        (LPVOID)bodyUtf8.data(), (DWORD)bodyUtf8.size(),
        (DWORD)bodyUtf8.size(), 0)) {
        errMsg = L"WinHttpSendRequest failed: " + std::to_wstring(GetLastError());
        return L"";
    }
    if (!WinHttpReceiveResponse(request.get(), nullptr)) {
        errMsg = L"WinHttpReceiveResponse failed: " + std::to_wstring(GetLastError());
        return L"";
    }

    DWORD statusCode = 0, sz = sizeof(statusCode);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX);

    std::string response;
    DWORD dwSize = 0;
    do {
        if (!WinHttpQueryDataAvailable(request.get(), &dwSize)) break;
        if (dwSize == 0) break;
        std::string buf(dwSize, 0);
        DWORD dwDownloaded = 0;
        if (WinHttpReadData(request.get(), &buf[0], dwSize, &dwDownloaded)) {
            buf.resize(dwDownloaded);
            response += buf;
        }
        else break;
    } while (dwSize > 0);

    if (statusCode != 200 && statusCode != 201) {
        errMsg = L"HTTP " + std::to_wstring(statusCode) + L": " + Utf8ToWide(response.substr(0, 500));
    }
    return Utf8ToWide(response);
}

static std::wstring ExtractAiContent(const std::wstring& responseJson) {
    size_t pos = responseJson.find(L"\"content\"");
    if (pos == std::wstring::npos) return L"";
    pos = responseJson.find(L"\"", pos + 9);
    if (pos == std::wstring::npos) return L"";
    pos++;
    std::wstring result;
    while (pos < responseJson.size()) {
        wchar_t c = responseJson[pos];
        if (c == L'"') break;
        if (c == L'\\' && pos + 1 < responseJson.size()) {
            pos++;
            wchar_t esc = responseJson[pos];
            switch (esc) {
            case L'n': result += L'\n'; break;
            case L'r': result += L'\r'; break;
            case L't': result += L'\t'; break;
            case L'"': result += L'"'; break;
            case L'\\': result += L'\\'; break;
            case L'/': result += L'/'; break;
            case L'u': {
                if (pos + 4 < responseJson.size()) {
                    std::wstring hex = responseJson.substr(pos + 1, 4);
                    try {
                        int code = std::stoi(WideToUtf8(hex), nullptr, 16);
                        result += (wchar_t)code;
                    }
                    catch (const std::exception&) {}
                    pos += 4;
                }
                break;
            }
            default: result += esc;
            }
        }
        else {
            result += c;
        }
        pos++;
    }
    return result;
}

static std::vector<std::wstring> ExtractAiActions(const std::wstring& content) {
    std::vector<std::wstring> actions;
    size_t pos = content.find(L"\"actions\"");
    if (pos == std::wstring::npos) return actions;
    pos = content.find(L'[', pos);
    if (pos == std::wstring::npos) return actions;
    size_t end = content.find(L']', pos);
    if (end == std::wstring::npos) return actions;
    std::wstring arr = content.substr(pos, end - pos + 1);
    size_t p = 0;
    while ((p = arr.find(L"\"action\"", p)) != std::wstring::npos) {
        p = arr.find(L'"', p + 8);
        if (p == std::wstring::npos) break;
        p++;
        size_t e = arr.find(L'"', p);
        if (e == std::wstring::npos) break;
        actions.push_back(arr.substr(p, e - p));
        p = e;
    }
    return actions;
}

static std::wstring ExtractAiReply(const std::wstring& content) {
    size_t pos = content.find(L"\"reply\"");
    if (pos == std::wstring::npos) return content;
    pos = content.find(L'"', pos + 7);
    if (pos == std::wstring::npos) return content;
    pos++;
    std::wstring result;
    while (pos < content.size()) {
        wchar_t c = content[pos];
        if (c == L'"') break;
        if (c == L'\\' && pos + 1 < content.size()) {
            pos++;
            wchar_t esc = content[pos];
            switch (esc) {
            case L'n': result += L'\n'; break;
            case L'r': result += L'\r'; break;
            case L't': result += L'\t'; break;
            case L'"': result += L'"'; break;
            case L'\\': result += L'\\'; break;
            default: result += esc;
            }
        }
        else {
            result += c;
        }
        pos++;
    }
    return result;
}

static bool ExecuteAiAction(const std::wstring& action, std::wstring& result) {
    result.clear();
    try {
        bool isSys = (g_activeDrive.load() == g_systemDriveLetter);
        if (action == L"clean_temp") { CleanTemp(); result = L"Temp cleaned"; }
        else if (action == L"clean_prefetch") { CleanPrefetch(); result = L"Prefetch cleaned"; }
        else if (action == L"clean_recycle") { CleanRecycleForDrive(g_activeDrive.load()); result = L"Recycle bin cleaned"; }
        else if (action == L"clean_dns") { FlushDns(); result = L"DNS flushed"; }
        else if (action == L"clean_shader") { CleanShaderCache(); result = L"Shader cache cleaned"; }
        else if (action == L"clean_thumbs") { CleanThumbnails(); result = L"Thumbnails cleaned"; }
        else if (action == L"clean_do") { if (!isSys) return false; CleanDeliveryOptimization(); result = L"DO cache cleaned"; }
        else if (action == L"clean_browsers") { CleanBrowserCaches(); result = L"Browser caches cleaned"; }
        else if (action == L"clean_chats") { CleanChatCaches(); result = L"Chat caches cleaned"; }
        else if (action == L"clean_dumps") { CleanMemoryDumps(); result = L"Dumps cleaned"; }
        else if (action == L"clean_launchers") { CleanGameLaunchers(); result = L"Game launchers cleaned"; }
        else if (action == L"clear_standby") { uint64_t f = PurgeStandbyEnhanced(); result = L"Standby cleared: " + std::to_wstring(f / (1024 * 1024)) + L" MB"; }
        else if (action == L"defrag_memory") { int f = 0; DefragmentMemory(f); result = L"Memory defragmented: " + std::to_wstring(f) + L" MB"; }
        else if (action == L"clear_kernel_pools") { ClearKernelPools(); result = L"Kernel pools cleared"; }
        else if (action == L"set_power_high") { if (!isSys) return false; RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"Power High"); result = L"Power plan: High"; }
        else if (action == L"set_power_balanced") { if (!isSys) return false; RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Power Balanced"); result = L"Power plan: Balanced"; }
        else if (action == L"set_power_ultimate") { if (!isSys) return false; RunCmd(std::wstring(L"powercfg /setactive ") + ULTIMATE_GUID, L"Power Ultimate"); result = L"Power plan: Ultimate"; }
        else if (action == L"disable_telemetry") { if (!isSys) return false; DisableTelemetry(); result = L"Telemetry disabled"; }
        else if (action == L"disable_net_throttle") { if (!isSys) return false; DisableNetworkThrottling(); result = L"Network throttling disabled"; }
        else if (action == L"disable_nagle") { if (!isSys) return false; DisableNagle(); result = L"Nagle disabled"; }
        else if (action == L"enable_hags") { ToggleHAGS(true); result = L"HAGS enabled"; }
        else if (action == L"disable_hags") { ToggleHAGS(false); result = L"HAGS disabled"; }
        else if (action == L"enable_game_mode") { SetGameMode(true); result = L"Game Mode enabled"; }
        else if (action == L"disable_game_mode") { SetGameMode(false); result = L"Game Mode disabled"; }
        else if (action == L"disable_mouse_accel") { ToggleMouseAcceleration(false); result = L"Mouse acceleration disabled"; }
        else if (action == L"enable_mouse_accel") { ToggleMouseAcceleration(true); result = L"Mouse acceleration enabled"; }
        else if (action == L"set_fast_dns") { if (!isSys) return false; SetFastDNS(); result = L"Fast DNS set"; }
        else if (action == L"enable_rss") { if (!isSys) return false; ApplyNetworkTweaks(true); result = L"RSS enabled"; }
        else if (action == L"disable_rss") { if (!isSys) return false; ApplyNetworkTweaks(false); result = L"RSS disabled"; }
        else if (action == L"disable_mem_comp") { RunCmd(L"powershell -NoProfile -Command \"Disable-MMAgent -MemoryCompression\"", L"Disable Mem Comp"); result = L"Memory compression disabled"; }
        else if (action == L"enable_mem_comp") { RunCmd(L"powershell -NoProfile -Command \"Enable-MMAgent -MemoryCompression\"", L"Enable Mem Comp"); result = L"Memory compression enabled"; }
        else if (action == L"game_booster_on") { if (!isSys) return false; GameBooster(true); result = L"Game Booster enabled"; }
        else if (action == L"game_booster_off") { if (!isSys) return false; GameBooster(false); result = L"Game Booster disabled"; }
        else if (action == L"clean_winsxs") { if (!isSys) return false; CleanWinSxS(); result = L"WinSxS cleaned"; }
        else if (action == L"trim_ssd") { if (!isSys) return false; OptimizeSSDTrim(); result = L"SSD trimmed"; }
        else {
            result = L"Unknown action: " + action;
            return false;
        }
        return true;
    }
    catch (const std::exception& e) {
        result = L"Exception: " + Utf8ToWide(e.what());
        return false;
    }
}

static std::wstring BuildAiSystemPrompt() {
    return L"You are ZeroBloat AI, a Windows system optimization assistant with full administrative access. "
        L"The user will ask you to optimize their system. You must respond with valid JSON only, no markdown, no extra text. "
        L"Format: {\"reply\": \"short explanation in user's language\", \"actions\": [{\"action\": \"action_name\"}]}. "
        L"Available actions: clean_temp, clean_prefetch, clean_recycle, clean_dns, clean_shader, clean_thumbs, "
        L"clean_do, clean_browsers, clean_chats, clean_dumps, clean_launchers, clear_standby, defrag_memory, "
        L"clear_kernel_pools, set_power_high, set_power_balanced, set_power_ultimate, disable_telemetry, "
        L"disable_net_throttle, disable_nagle, enable_hags, disable_hags, enable_game_mode, disable_game_mode, "
        L"disable_mouse_accel, enable_mouse_accel, set_fast_dns, enable_rss, disable_rss, disable_mem_comp, "
        L"enable_mem_comp, game_booster_on, game_booster_off, clean_winsxs, trim_ssd. "
        L"Only use actions that fit the user's request. If you don't need actions, return empty actions array. "
        L"Be conservative - do not do destructive changes unless explicitly requested.";
}

static std::wstring BuildAiUserPrompt(const std::wstring& userMessage) {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    std::wstring pcInfo;
    { std::lock_guard<std::mutex> lk(g_pcInfoMutex); pcInfo = g_pcInfoJson; }
    std::wstring sysInfo = L"System info: " + pcInfo +
        L", RAM total MB: " + std::to_wstring(ms.ullTotalPhys / (1024 * 1024)) +
        L", RAM free MB: " + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) +
        L", CPU usage %: " + FloatToJsonStr(g_cpuUsage.load()) +
        L", GPU usage %: " + FloatToJsonStr(g_gpuUsage.load()) +
        L", CPU temp: " + FloatToJsonStr(g_cpuTemp.load()) +
        L", GPU temp: " + FloatToJsonStr(g_gpuTemp.load()) +
        L", active drive: " + std::wstring(1, g_activeDrive.load()) + L":";
    return sysInfo + L"\n\nUser request: " + userMessage;
}

static std::wstring AiChat(const std::wstring& userMessage, std::wstring& err) {
    AiConfig cfg;
    { std::lock_guard<std::mutex> lk(g_aiMutex); cfg = g_aiConfig; }
    if (cfg.endpoint.empty()) { err = L"API endpoint not configured"; return L""; }
    if (!cfg.useLocal && cfg.apiKey.empty()) { err = L"API key not configured"; return L""; }
    if (cfg.model.empty()) { err = L"Model not configured"; return L""; }

    std::wstring systemPrompt = BuildAiSystemPrompt();
    std::wstring userPrompt = BuildAiUserPrompt(userMessage);

    std::wstring body = L"{\"model\":" + JsonStr(cfg.model) + L",\"messages\":[{\"role\":\"system\",\"content\":" + JsonStr(systemPrompt) + L"},{\"role\":\"user\",\"content\":" + JsonStr(userPrompt) + L"}]";

    std::wstring response = HttpPostJson(cfg.endpoint, cfg.useLocal ? L"" : cfg.apiKey, body, 120, err);
    if (!err.empty()) return L"";

    std::wstring content = ExtractAiContent(response);
    if (content.empty()) {
        err = L"Empty AI response";
        return L"";
    }

    std::wstring reply = ExtractAiReply(content);
    std::vector<std::wstring> actions = ExtractAiActions(content);

    std::vector<std::wstring> executed;
    for (const auto& a : actions) {
        std::wstring r;
        if (ExecuteAiAction(a, r)) executed.push_back(r);
        else executed.push_back(a + L": " + r);
    }

    std::wstring resultJson = L"{\"ok\":true,\"reply\":" + JsonStr(reply) + L",\"executed\":[";
    for (size_t i = 0; i < executed.size(); ++i) {
        if (i) resultJson += L",";
        resultJson += JsonStr(executed[i]);
    }
    resultJson += L"]}";
    return resultJson;
}

static const wchar_t* HTML_UI = LR"HTML(<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'none';">
<title>ZeroBloat v15.0.0</title><style>
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
body{font-family:'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;background-color:var(--bg);color:var(--text);overflow:hidden;font-size:15px;user-select:none;transition:background-color .3s,color .3s; background-image:var(--bg-img); position:relative}
h1,h2,.logo,.titlebar-text{font-family:'Consolas', 'Courier New', monospace;text-transform:uppercase;letter-spacing:1px;}
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
.card-value{font-size:26px;font-weight:700;font-family:'Consolas',monospace;}
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
input[type=text],input[type=number],input[type=password],select,textarea{padding:10px 14px;background:var(--bg);border:1px solid var(--border);border-radius:4px;color:var(--text);font-size:15px;outline:none;font-family:'Segoe UI',sans-serif;}
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
.list-row .title{font-weight:700;font-size:16px}.list-row .sub{font-size:13px;color:var(--muted);font-family:'Consolas',monospace;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
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
.feat-item{background:var(--bg);border:1px solid var(--border);padding:12px;border-radius:4px;display:flex;justify-content:space-between;align-items:center;font-family:'Consolas',monospace;font-size:12px;}
.feat-item.supported{border-color:var(--success);color:var(--success);}
.feat-item.unsupported{border-color:var(--border);color:var(--muted)}
.self-usage{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-bottom:20px}
.self-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:16px;text-align:center;}
.self-card .lbl{font-size:12px;color:var(--muted);text-transform:uppercase;font-weight:700;margin-bottom:6px}
.self-card .val{font-size:20px;font-weight:700;color:var(--accent2);font-family:'Consolas',monospace;}
.app-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(220px,1fr));gap:12px;margin-top:12px}
.app-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;display:flex;justify-content:space-between;align-items:center;}
.app-card .title{font-weight:700;font-size:15px;color:var(--text)}
.app-card button{padding:8px 14px;font-size:12px}
.modal-overlay{display:none;position:fixed;inset:0;z-index:10000;background:rgba(0,0,0,.85);align-items:center;justify-content:center}
.modal-overlay.active{display:flex}
.modal-box{background:var(--surface);border:2px solid var(--accent);border-radius:8px;padding:24px;position:relative;max-width:90vw;max-height:90vh;overflow:auto;z-index:10001;width:540px;text-align:center;}
.warn-box{background:rgba(255,0,85,.1);border-left:4px solid var(--accent);padding:16px;border-radius:0 4px 4px 0;margin-bottom:16px;font-size:15px;line-height:1.6;font-weight:500}
.smart-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;margin-bottom:10px;}
.adv-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:16px}
.adv-card{background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:16px;}
.adv-card h3{font-size:14px;margin-bottom:8px;color:var(--accent2);font-family:'Consolas',monospace;}
.adv-card p{font-size:13px;color:var(--muted);margin-bottom:12px;line-height:1.5}
.smart-item{display:flex;align-items:flex-start;gap:14px;padding:16px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:10px;}
.smart-info{flex:1;min-width:0}
.smart-title{font-weight:700;font-size:16px;margin-bottom:6px;color:var(--text)}
.smart-badge{display:inline-block;padding:2px 8px;font-size:11px;border:1px solid;border-radius:10px;margin-left:6px;font-weight:700;text-transform:uppercase;}
.smart-size{display:inline-block;padding:2px 8px;font-size:11px;background:var(--surface);border:1px solid var(--border);border-radius:10px;margin-left:6px;font-weight:700;color:var(--accent2);}
.smart-desc{font-size:13px;color:var(--muted);line-height:1.5}
.smart-category{font-size:11px;font-weight:700;color:var(--accent);text-transform:uppercase;padding:4px 10px;border:1px solid var(--accent);border-radius:4px;flex-shrink:0;align-self:flex-start;white-space:nowrap}
.smart-total{padding:16px;background:rgba(0,240,255,.05);border:1px solid var(--accent2);border-radius:4px;font-size:16px;font-weight:700;text-align:center;margin:16px 0;color:var(--text);}
.smart-total b{color:var(--accent2);font-family:'Consolas',monospace}
.setting-group{padding:16px;background:var(--bg);border:1px solid var(--border);border-radius:4px;margin-bottom:12px;}
.setting-group h3{font-size:13px;color:var(--accent2);margin-bottom:12px;font-family:'Consolas',monospace;text-transform:uppercase}
.setting-row{display:flex;align-items:center;gap:12px;padding:8px 0;border-bottom:1px solid var(--border)}
.setting-row:last-child{border-bottom:none}
.setting-row label{flex:1;font-weight:600;font-size:14px}
.setting-row input[type=number]{width:80px}
.drive-current{padding:12px 16px;background:rgba(0,240,255,.08);border:1px solid var(--accent2);border-radius:4px;margin-bottom:12px;font-weight:700;color:var(--accent2);}
body[data-is-system="false"] .requires-system{display:none!important}
.badge-safe{background:rgba(16,185,129,.2);color:#10b981;padding:2px 6px;border-radius:4px;font-size:11px;margin-left:8px}
.badge-danger{background:rgba(239,68,68,.2);color:#ef4444;padding:2px 6px;border-radius:4px;font-size:11px;margin-left:8px}
.beta-badge{color:var(--warn);font-weight:900;margin-left:10px;padding:2px 8px;border:1px solid var(--warn);border-radius:4px;font-size:12px;letter-spacing:1px;}
#ai-chat-messages div{animation:fadein .3s}@keyframes fadein{from{opacity:0}to{opacity:1}}
.footer-info{text-align:center;font-size:12px;color:var(--muted);margin-top:20px;padding-top:20px;border-top:1px solid var(--border)}
table.bench{width:100%;border-collapse:collapse;font-family:'Consolas',monospace;font-size:13px}
table.bench th,table.bench td{border:1px solid var(--border);padding:8px;text-align:left}
table.bench th{background:var(--surface);color:var(--accent2)}
</style></head><body data-theme="midnight" data-is-system="true">

<div class="titlebar" id="titlebar">
  <div class="titlebar-text">⚡ ZEROBLOAT v15.0.0</div>
  <div class="titlebar-controls">
    <button class="tb-btn" data-wc="min">—</button>
    <button class="tb-btn" data-wc="max">□</button>
    <button class="tb-btn close" data-wc="close">✕</button>
  </div>
</div>

<div class="app">
  <div class="sidebar">
    <div class="logo">⚡<span class="lbl">ZB</span></div>
    <div class="nav-item active" data-tab="home">🏠 <span data-i18n="nav_dash">Дашборд</span></div>
    <div class="nav-item" data-tab="cpu">🧠 <span data-i18n="nav_cpu">Инструкции CPU</span></div>
    <div class="nav-item" data-tab="ai">🤖 <span data-i18n="nav_ai">AI Ассистент</span></div>
    <div class="nav-item" data-tab="profiles">🎮 <span data-i18n="nav_profiles">Профили</span></div>
    <div class="nav-item" data-tab="history">📜 <span data-i18n="nav_history">История</span></div>
    <div class="nav-item" data-tab="smart">✨ <span data-i18n="nav_smart">Умная оптимизация</span></div>
    <div class="nav-item" data-tab="autostart">🔌 <span data-i18n="nav_autostart">Автозагрузка</span></div>
    <div class="nav-item" data-tab="freeze">❄️ <span data-i18n="nav_freeze">Заморозка</span></div>
    <div class="nav-item" data-tab="games">🎮 <span data-i18n="nav_games">Игры</span></div>
    <div class="nav-item" data-tab="overclock">🔥 <span data-i18n="nav_overclock">Разгон</span></div>
    <div class="nav-item" data-tab="intel_sde">🔬 <span data-i18n="nav_intel_sde">Intel SDE</span></div>
    <div class="nav-item" data-tab="drivers">🔧 <span data-i18n="nav_drivers">Драйверы</span></div>
    <div class="nav-item" data-tab="drive_scanner">💾 <span data-i18n="nav_drive">Сканер диска</span></div>
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
    <div class="nav-item" data-tab="updates">🔄 <span data-i18n="nav_updates">Обновления</span></div>
    <div class="nav-item" data-tab="settings">⚙️ <span data-i18n="nav_settings">Настройки</span></div>
    <div class="nav-item" data-tab="log">📜 <span data-i18n="nav_log">Лог</span></div>
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
          <h2 data-i18n="self_usage_title">Использование ресурсов ZeroBloat</h2>
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
          <h2 data-i18n="quick_title">Быстрые действия</h2>
          <div class="toolbar">
            <button id="btn-qa-temp">🧹 <span data-i18n="btn_temp">Temp</span></button>
            <button id="btn-qa-dns">🌐 <span data-i18n="btn_dns">Flush DNS</span></button>
            <button id="btn-qa-ram">💾 <span data-i18n="btn_ram">Defrag RAM</span></button>
            <button id="btn-qa-standby" class="success">🧠 <span data-i18n="btn_standby">Standby Purger</span></button>
            <button id="btn-qa-maintenance" class="success">✨ <span data-i18n="btn_maintenance">One-Click Maintenance</span></button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-cpu">
        <h1 data-i18n="nav_cpu">Инструкции CPU</h1>
        <p class="subtitle">Подробная информация о поддерживаемых наборах инструкций</p>
        <div class="section">
          <h2>Обнаруженные наборы инструкций</h2>
          <div class="feat-grid" id="cpu-features-grid-full">
            <div class="feat-item" id="feat-full-sse41">SSE4.1 <span>—</span></div>
            <div class="feat-item" id="feat-full-sse42">SSE4.2 <span>—</span></div>
            <div class="feat-item" id="feat-full-popcnt">POPCNT <span>—</span></div>
            <div class="feat-item" id="feat-full-avx">AVX <span>—</span></div>
            <div class="feat-item" id="feat-full-fma3">FMA3 <span>—</span></div>
            <div class="feat-item" id="feat-full-avx2">AVX2 <span>—</span></div>
            <div class="feat-item" id="feat-full-bmi1">BMI1 <span>—</span></div>
            <div class="feat-item" id="feat-full-bmi2">BMI2 <span>—</span></div>
            <div class="feat-item" id="feat-full-avx512">AVX-512 <span>—</span></div>
          </div>
        </div>
        <div class="section">
          <h2>Описание</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3>SSE4.1 / SSE4.2</h3><p>Улучшают производительность мультимедиа и строковых операций.</p></div>
            <div class="adv-card"><h3>AVX / AVX2</h3><p>Векторные инструкции для вычислений с плавающей точкой. Ускоряют игры и рендеринг.</p></div>
            <div class="adv-card"><h3>FMA3</h3><p>Fused Multiply-Add — ускоряет математические операции.</p></div>
            <div class="adv-card"><h3>BMI1 / BMI2</h3><p>Bit Manipulation Instructions — ускоряют работу с битами.</p></div>
            <div class="adv-card"><h3>AVX-512</h3><p>Расширенные векторные инструкции для серверных и профессиональных задач.</p></div>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-ai">
        <h1 data-i18n="nav_ai">AI Ассистент</h1>
        <p class="subtitle" data-i18n="ai_sub">Подключите LLM API (OpenAI/Ollama/LM Studio/GPT-4o) и опишите задачу оптимизации</p>
        <div class="section">
          <h2 data-i18n="ai_settings">Настройки API</h2>
          <div class="setting-row"><label data-i18n="ai_local">Использовать локальную модель</label><input type="checkbox" id="ai-local-chk"></div>
          <div class="setting-row"><label data-i18n="ai_endpoint">Endpoint URL</label><input type="text" id="ai-endpoint" placeholder="https://api.openai.com/v1/chat/completions" style="flex:1;max-width:600px"></div>
          <div class="setting-row"><label data-i18n="ai_key">API Key</label><input type="password" id="ai-key" placeholder="sk-..." style="flex:1;max-width:600px"></div>
          <div class="setting-row"><label data-i18n="ai_model">Model</label><input type="text" id="ai-model" placeholder="gpt-4o-mini" style="flex:1;max-width:600px"></div>
          <div class="hint" style="margin-top:16px" data-i18n="ai_hint">Ключ хранится в зашифрованном виде (Windows DPAPI). Для локальных моделей (Ollama, LM Studio) укажите endpoint (например, http://localhost:11434/v1/chat/completions).</div>
          <div class="toolbar" style="margin-top:16px">
            <button id="btn-ai-save-config" class="success">💾 <span data-i18n="btn_save">Сохранить</span></button>
            <button id="btn-ai-load-config" class="secondary">🔄 <span data-i18n="btn_load">Загрузить</span></button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="ai_chat">Диалог</h2>
          <div id="ai-chat-messages" style="max-height:450px;overflow-y:auto;background:var(--bg);border:1px solid var(--border);border-radius:4px;padding:14px;margin-bottom:12px;font-size:14px;min-height:120px"></div>
          <div class="toolbar">
            <input type="text" id="ai-user-input" data-i18n-placeholder="ai_placeholder" placeholder="Например: удали весь мусор и оптимизируй ПК под игры" style="flex:1">
            <button id="btn-ai-send" class="success">📤 <span data-i18n="btn_send">Отправить</span></button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-profiles">
        <h1 data-i18n="nav_profiles">Профили</h1>
        <p class="subtitle" data-i18n="profiles_sub">Управление профилями оптимизации (Игры, Работа, Ноутбук, Безопасный)</p>
        <div class="section">
          <h2 data-i18n="profiles_available">Доступные профили</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3 data-i18n="prof_gaming">Игровой</h3><p data-i18n="prof_gaming_desc">Максимальный FPS, отключение фоновых служб.</p><button id="btn-profile-gaming" class="success" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3 data-i18n="prof_work">Работа</h3><p data-i18n="prof_work_desc">Сбалансированная система для стабильной работы.</p><button id="btn-profile-work" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3 data-i18n="prof_laptop">Ноутбук</h3><p data-i18n="prof_laptop_desc">Энергосбережение, минимальный нагрев.</p><button id="btn-profile-laptop" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3 data-i18n="prof_safe">Безопасный</h3><p data-i18n="prof_safe_desc">Только 100% безопасные оптимизации без отключения защиты.</p><button id="btn-profile-safe" class="success" data-i18n="btn_apply">Применить</button></div>
          </div>
          <div class="toolbar" style="margin-top:20px">
            <button id="btn-profile-export" class="secondary">📤 <span data-i18n="btn_export">Экспорт профилей</span></button>
            <button id="btn-profile-import" class="secondary">📥 <span data-i18n="btn_import">Импорт профилей</span></button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-history">
        <h1 data-i18n="nav_history">История изменений</h1>
        <p class="subtitle" data-i18n="hist_sub">Журнал всех внесенных изменений с возможностью отката</p>
        <div class="section">
          <div class="toolbar">
            <button id="btn-rollback-last" class="danger">↶ <span data-i18n="btn_rollback_last">Откатить последнее</span></button>
            <button id="btn-rollback-refresh" class="secondary">🔄 <span data-i18n="btn_refresh">Обновить</span></button>
          </div>
          <div id="rollback-list"></div>
          <h2 style="margin-top:24px">Audit log</h2>
          <div id="history-list"><div class="hint">Загрузка истории...</div></div>
        </div>
      </div>

      <div class="tab" id="tab-smart">
        <h1 data-i18n="smart_title">✨ Умная оптимизация</h1>
        <p class="subtitle" data-i18n="smart_sub">Автоматический анализ ПК и подбор безопасных улучшений</p>
        <div class="section">
          <div class="hint" data-i18n="smart_hint">Нажмите кнопку ниже, чтобы проанализировать систему.</div>
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

      <div class="tab" id="tab-freeze">
        <h1 data-i18n="nav_freeze">Заморозка процессов</h1>
        <p class="subtitle">Приостановка фоновых приложений для освобождения ресурсов во время игр</p>
        <div class="section">
          <div class="hint">Выберите приложения, которые нужно «заморозить» (приостановить), чтобы они не потребляли ресурсы CPU и диска. Во время игры это может повысить FPS.</div>
          <div class="toolbar">
            <button id="btn-freeze-refresh" class="secondary">🔄 Обновить список</button>
            <button id="btn-freeze-all" class="danger">❄️ Заморозить выбранные</button>
            <button id="btn-unfreeze-all" class="success">🔥 Разморозить все</button>
          </div>
          <div id="freeze-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-games">
        <h1 data-i18n="nav_games">Игры</h1>
        <p class="subtitle" data-i18n="games_sub">Game Booster, сжатие, сканер, пресеты</p>
        <div class="section requires-system">
          <h2 data-i18n="game_booster_title">Game Booster</h2>
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
            <div class="adv-card"><h3 data-i18n="preset_comp">Competitive FPS</h3><p data-i18n="preset_comp_desc">Максимальный FPS, минимум задержек</p><button id="btn-preset-comp" class="success" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3 data-i18n="preset_bal">Balanced</h3><p data-i18n="preset_bal_desc">Компромисс между FPS и качеством</p><button id="btn-preset-bal" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3 data-i18n="preset_qual">Quality</h3><p data-i18n="preset_qual_desc">Максимальное качество картинки</p><button id="btn-preset-qual" class="secondary" data-i18n="btn_apply">Применить</button></div>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-overclock">
        <h1 data-i18n="nav_overclock">Разгон и Буст</h1>
        <p class="subtitle" data-i18n="oc_sub">GPU, CPU, RAM & System Tweaks</p>
        <div class="warn-box" data-i18n="oc_warn">⚠️ ВНИМАНИЕ! Разгон может привести к нестабильности системы.</div>
        <div class="section">
          <h2 data-i18n="oc_afterburner_title">GPU Overclock (скрытый запуск MSI)</h2>
          <div class="toolbar">
            <button id="btn-msi-afterburner" class="danger" data-i18n="btn_apply_msi">🎮 Применить профиль MSI</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="oc_cpu_ram_boost">CPU & RAM Boost</h2>
          <div class="adv-grid">
            <div class="adv-card"><h3 data-i18n="oc_prio">High Priority CPU</h3><p>Win32PrioritySeparation = 0x26</p><button id="btn-priority-sep" class="secondary" data-i18n="btn_apply">Применить</button></div>
            <div class="adv-card"><h3 data-i18n="oc_paging">Disable Paging</h3><p data-i18n="oc_paging_desc">Только для 32GB+ RAM</p><button id="btn-disable-paging" class="danger" data-i18n="btn_disable">Отключить</button></div>
            <div class="adv-card"><h3 data-i18n="oc_memcomp">Memory Compression Off</h3><p data-i18n="oc_memcomp_desc">Отключает сжатие памяти</p><button id="btn-disable-mem-comp" class="danger" data-i18n="btn_disable">Отключить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="oc_bench_title">Бенчмарк до/после</h2>
          <div class="hint" data-i18n="oc_bench_hint">Снимает показатели: свободная RAM, число процессов/служб, задержка диска, CPU score. Сравнивайте результат ДО и ПОСЛЕ оптимизации.</div>
          <div class="toolbar">
            <button id="btn-bench-capture" class="success">📸 <span data-i18n="btn_bench_capture">Снять бенчмарк</span></button>
            <button id="btn-bench-compare" class="secondary">📊 <span data-i18n="btn_bench_compare">Сравнить</span></button>
          </div>
          <div id="bench-results" style="margin-top:16px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="oc_stress_title">Стресс-тест CPU (30 сек)</h2>
          <div class="hint" data-i18n="oc_stress_hint">Нагружает все ядра CPU для проверки стабильности и температур.</div>
          <div class="toolbar">
            <button id="btn-stress-test" class="danger">🔥 <span data-i18n="btn_stress">Запустить стресс-тест</span></button>
            <button id="btn-benchmark" class="success">📊 <span data-i18n="btn_bench">Быстрый бенчмарк</span></button>
          </div>
          <div id="stress-results"></div>
        </div>
      </div>

      <div class="tab" id="tab-intel_sde">
        <h1 data-i18n="nav_intel_sde">Intel Software Development Emulator</h1>
        <p class="subtitle">Эмуляция инструкций для тестирования и отладки</p>
        <div class="section">
          <div class="hint">Intel SDE позволяет запускать приложения с эмуляцией новых наборов инструкций (AVX-512 и др.) на процессорах, которые их не поддерживают. ZeroBloat может автоматически скачать и настроить SDE.</div>
          <div class="toolbar">
            <button id="btn-sde-install" class="success">📥 Установить / Обновить SDE</button>
            <button id="btn-sde-remove" class="danger">🗑 Удалить SDE</button>
          </div>
          <div class="setting-row" style="margin-top:16px">
            <label>Включить эмуляцию</label>
            <input type="checkbox" id="sde-enable-chk">
          </div>
          <div class="setting-row">
            <label>Целевое приложение (exe)</label>
            <input type="text" id="sde-target-path" placeholder="C:\Games\Game.exe" style="flex:1;max-width:600px">
          </div>
          <div class="toolbar" style="margin-top:16px">
            <button id="btn-sde-launch" class="success">🚀 Запустить через SDE</button>
          </div>
          <div id="sde-status" style="margin-top:16px"></div>
        </div>
      </div>

      <div class="tab" id="tab-drivers">
        <h1 data-i18n="nav_drivers">Менеджер драйверов</h1>
        <p class="subtitle">Сканирование подключенных устройств и обновление драйверов</p>
        <div class="section">
          <div class="hint">Будут показаны устройства, для которых доступны обновления драйверов через Windows Update или безопасные источники.</div>
          <div class="toolbar">
            <button id="btn-scan-drivers" class="success">🔍 Сканировать устройства</button>
            <button id="btn-update-all-drivers" class="danger">🔄 Обновить все (Windows Update)</button>
          </div>
          <div id="drivers-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-drive_scanner">
        <h1 data-i18n="nav_drive">Сканер диска</h1>
        <div class="section">
          <h2 data-i18n="drive_treemap_title">Treemap дискового пространства</h2>
          <div class="hint" data-i18n="drive_hint">Нажмите кнопку ниже, чтобы проанализировать, какие папки занимают больше всего места на диске (диаграмма покажет это в виде цветных блоков).</div>
          <div class="toolbar">
            <button id="btn-scan-drive" class="success">🚀 <span data-i18n="btn_scan_drive">Начать сканирование</span></button>
          </div>
          <canvas id="treemapCanvas" width="800" height="300"></canvas>
          <div id="drive-legend" style="margin-top:16px;font-size:13px;color:var(--muted);"></div>
        </div>
      </div>

      <div class="tab" id="tab-bulk_uninstall">
        <h1 data-i18n="nav_bulk">Массовое удаление</h1>
        <div class="section">
          <div class="toolbar">
            <button id="btn-load-programs-detailed" class="secondary">📋 <span data-i18n="btn_load_programs_detailed">Загрузить программы</span></button>
            <button id="btn-bulk-uninstall" class="danger" style="display:none">🗑 <span data-i18n="btn_bulk_uninstall">Удалить выбранные</span></button>
          </div>
          <div id="programs-detailed-list"></div>
          <h2 style="margin-top:24px" data-i18n="bulk_leftovers">Поиск остатков</h2>
          <div class="toolbar">
            <input type="text" id="leftover-name" data-i18n-placeholder="bulk_leftovers_placeholder" placeholder="Название программы" style="flex:1">
            <button id="btn-scan-leftovers" class="secondary">🔍 <span data-i18n="btn_scan_leftovers">Найти остатки</span></button>
          </div>
          <div id="leftover-results"></div>
        </div>
      </div>

      <div class="tab" id="tab-security">
        <h1 data-i18n="nav_sec">Безопасность</h1>
        <div class="section">
          <div class="toolbar">
            <button id="btn-load-security" class="secondary">🔄 <span data-i18n="btn_load_security">Обновить статус</span></button>
          </div>
          <div id="security-status-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-lite">
        <h1 data-i18n="lite_title">Lite Windows</h1>
        <p class="subtitle" data-i18n="lite_sub">Агрессивный деблоат</p>
        <div class="warn-box"><b>⚠️ <span data-i18n="lite_warn_title">ВНИМАНИЕ:</span></b> <span data-i18n="lite_warn_text">Удаляет UWP-приложения, отключает телеметрию и службы. Необратимо!</span></div>
        <div class="section">
          <button id="btn-lite-debloat" class="danger" style="width:100%;justify-content:center;padding:15px;font-size:16px">
            ⚡ <span data-i18n="lite_btn_run">СДЕЛАТЬ WINDOWS LITE</span>
          </button>
        </div>
        <div class="section">
          <h2 data-i18n="lite_uwp_title">Список UWP-приложений</h2>
          <div class="hint" data-i18n="lite_uwp_hint">Можно выбрать, что удалять. Значения «safe/caution/donottouch» показывают степень риска.</div>
          <div id="uwp-list"></div>
          <div class="toolbar">
            <button id="btn-uwp-refresh" class="secondary">🔄 <span data-i18n="btn_refresh">Обновить</span></button>
            <button id="btn-uwp-apply" class="danger" data-i18n="btn_uwp_apply">🗑 Удалить выбранные</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-cleanup">
        <h1 data-i18n="nav_cleanup">Очистка</h1>
        <div class="section">
          <div class="check-item"><input type="checkbox" id="cl-temp" checked><label for="cl-temp">Temp</label></div>
          <div class="check-item"><input type="checkbox" id="cl-prefetch" checked><label for="cl-prefetch">Prefetch</label></div>
          <div class="check-item"><input type="checkbox" id="cl-recycle" checked><label for="cl-recycle" data-i18n="cl_recycle">Корзина</label></div>
          <div class="check-item"><input type="checkbox" id="cl-thumbs" checked><label for="cl-thumbs" data-i18n="cl_thumbs">Кэш миниатюр</label></div>
          <div class="check-item"><input type="checkbox" id="cl-shader" checked><label for="cl-shader" data-i18n="cl_shader">Shader Cache</label></div>
          <div class="check-item"><input type="checkbox" id="cl-dns" checked><label for="cl-dns">DNS Cache</label></div>
          <div class="check-item"><input type="checkbox" id="cl-do" checked><label for="cl-do" data-i18n="cl_do">Delivery Optimization</label></div>
          <div class="check-item"><input type="checkbox" id="cl-launchers" checked><label for="cl-launchers" data-i18n="cl_launchers">Кэш лаунчеров</label></div>
          <div class="check-item"><input type="checkbox" id="cl-browsers" checked><label for="cl-browsers" data-i18n="cl_browsers">Кэш браузеров</label></div>
          <div class="check-item"><input type="checkbox" id="cl-chats" checked><label for="cl-chats" data-i18n="cl_chats">Discord/Telegram</label></div>
          <div class="check-item"><input type="checkbox" id="cl-dumps" checked><label for="cl-dumps" data-i18n="cl_dumps">Дампы памяти</label></div>
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
          <div class="toolbar">
            <button id="btn-load-installed-programs" class="secondary" data-i18n="btn_load_programs">📋 Список программ</button>
          </div>
          <div id="programs-list"></div>
        </div>
      </div>

      <div class="tab" id="tab-overlay">
        <h1 data-i18n="nav_overlay">Оверлей</h1>
        <div class="section">
          <div class="check-item"><input type="checkbox" id="ov-cpu" checked><label for="ov-cpu">CPU</label></div>
          <div class="check-item"><input type="checkbox" id="ov-gpu" checked><label for="ov-gpu">GPU</label></div>
          <div class="check-item"><input type="checkbox" id="ov-ram" checked><label for="ov-ram">RAM</label></div>
          <div class="check-item"><input type="checkbox" id="ov-temp" checked><label for="ov-temp" data-i18n="ov_temp">Температура</label></div>
          <div class="check-item"><input type="checkbox" id="ov-vram" checked><label for="ov-vram">VRAM</label></div>
          <div class="check-item"><input type="checkbox" id="ov-fan"><label for="ov-fan" data-i18n="ov_fan">Вентилятор</label></div>
          <div class="check-item"><input type="checkbox" id="ov-ping" checked><label for="ov-ping">Ping</label></div>
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
            <input type="text" id="large-path" data-i18n-placeholder="files_large_path" placeholder="Путь (пусто = профиль)" style="flex:1">
            <input type="number" id="large-mb" value="100" min="1" style="width:100px">
            <button id="btn-find-large" data-i18n="btn_find">Найти</button>
          </div>
          <div id="large-results"></div>
        </div>
        <div class="section">
          <h2 data-i18n="files_dups">Поиск дубликатов</h2>
          <div class="toolbar">
            <input type="text" id="dup-path" data-i18n-placeholder="files_dup_path" placeholder="Путь (пусто = Downloads)" style="flex:1">
            <button id="btn-find-dups" data-i18n="btn_scan">Сканировать</button>
          </div>
          <div id="dup-results"></div>
        </div>
        <div class="section">
          <h2 data-i18n="files_shred">Шредер файлов</h2>
          <div class="toolbar">
            <input type="text" id="shred-path" data-i18n-placeholder="files_shred_path" placeholder="Полный путь к файлу" style="flex:1">
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
        <div class="warn-box" data-i18n="adv_warn">⚠️ Осторожно! Некоторые функции могут повлиять на стабильность системы. Все опасные твики требуют подтверждения и создают точку восстановления.</div>
        <div class="section">
          <h2 data-i18n="adv_safe_title">Безопасные оптимизации <span class="badge-safe">Safe</span></h2>
          <div class="adv-grid">
            <div class="adv-card"><h3 data-i18n="adv_cache_timer">Кэш-таймер (5 мин)</h3><p data-i18n="adv_cache_timer_desc">Автоматическая очистка кэша ФС каждые 5 минут.</p><button id="btn-cache-timer" class="success" data-i18n="btn_toggle">Включить/Выключить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_dpc">Анализатор DPC/ISR</h3><p data-i18n="adv_dpc_desc">Мониторинг задержек драйверов.</p><button id="btn-check-dpc" class="secondary" data-i18n="btn_check">Проверить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_restore">Точки восстановления</h3><p data-i18n="adv_restore_desc">Создание бэкапа системы перед изменениями.</p><button id="btn-create-restore" class="success" data-i18n="btn_create_restore">💾 Создать</button></div>
            <div class="adv-card"><h3 data-i18n="adv_net_reset">Сброс сети</h3><p data-i18n="adv_net_reset_desc">Winsock/TCP reset.</p><button id="btn-reset-network" class="secondary" data-i18n="btn_reset">🔄 Сбросить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_rss">Твикер RSS / MSI-X</h3><p data-i18n="adv_rss_desc">Включает прерывания сетевых адаптеров.</p><button id="btn-rss-on" class="success" data-i18n="btn_enable">Включить</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_caution_title">Использовать с осторожностью <span class="badge-danger">Caution</span></h2>
          <div class="adv-grid">
            <div class="adv-card"><h3 data-i18n="adv_vbs">Отключить VBS / HVCI</h3><p data-i18n="adv_vbs_desc">Отключает виртуализацию безопасности.</p><button id="btn-disable-vbs" class="danger" data-i18n="btn_disable">Отключить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_etw">Отключить ETW / WPP</h3><p data-i18n="adv_etw_desc">Останавливает системное логирование.</p><button id="btn-disable-etw" class="danger" data-i18n="btn_disable">Отключить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_ctx_menu">Агрессивный Debloat меню</h3><p data-i18n="adv_ctx_menu_desc">Удаляет сторонние CLSID.</p><button id="btn-load-ctx-menu" class="danger" data-i18n="btn_scan">Сканировать</button></div>
            <div class="adv-card"><h3 data-i18n="adv_spyware">Удаление шпионского ПО</h3><p data-i18n="adv_spyware_desc">Полное удаление UWP-мусора.</p><button id="btn-remove-spyware" class="danger" data-i18n="btn_clean">Очистить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_nvidia">NVIDIA P-States</h3><p data-i18n="adv_nvidia_desc">Принудительный P0.</p><button id="btn-nvidia-max-perf" class="danger" data-i18n="btn_p0">P0 Mode</button></div>
            <div class="adv-card"><h3 data-i18n="adv_kernel_pools">Сжатие пулов ядра</h3><p data-i18n="adv_kernel_pools_desc">Очистка Non-Paged и Paged Pool.</p><button id="btn-clear-kernel-pools" class="danger" data-i18n="btn_clean">Очистить</button></div>
            <div class="adv-card"><h3 data-i18n="adv_ddu">Интеграция DDU</h3><p data-i18n="adv_ddu_desc">Запуск Display Driver Uninstaller.</p><button id="btn-run-ddu" class="danger" data-i18n="btn_run_ddu">Запустить DDU</button></div>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_dangerous_title">Опасные зоны <span class="badge-danger">Dangerous</span></h2>
          <div class="toolbar">
            <button id="btn-disable-defender" class="danger" data-i18n="btn_disable_defender">Отключить Defender</button>
            <button id="btn-deep-services" class="danger" data-i18n="btn_deep_services">Глубокое отключение служб</button>
            <button id="btn-clean-winsxs" class="danger" data-i18n="btn_clean_winsxs">Очистить WinSxS</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="adv_restore_points_title">Восстановление системы</h2>
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
          <div class="toolbar">
            <button id="btn-run-sfc" class="success" data-i18n="btn_sfc">🛠 SFC /scannow</button>
            <button id="btn-run-dism" class="success" data-i18n="btn_dism">🔧 DISM /RestoreHealth</button>
          </div>
          <div id="sfc-output"></div>
        </div>
        <div class="section">
          <h2 data-i18n="maint_updates_title">Windows Update</h2>
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
          <h2 data-i18n="maint_sched_title">Автоочистка через Task Scheduler</h2>
          <div class="hint" data-i18n="maint_sched_hint">Создаёт задание schtasks.exe для запуска очистки по расписанию.</div>
          <div class="toolbar">
            <button id="btn-sched-daily" class="success" data-i18n="btn_sched_daily">📅 Ежедневно 03:00</button>
            <button id="btn-sched-weekly" class="secondary" data-i18n="btn_sched_weekly">📅 Еженедельно (Вс 03:00)</button>
            <button id="btn-sched-onidle" class="secondary" data-i18n="btn_sched_onidle">📅 При простое</button>
            <button id="btn-sched-delete" class="danger" data-i18n="btn_sched_delete">🗑 Удалить задание</button>
          </div>
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
          <button id="btn-reset-network-2" class="danger" data-i18n="btn_reset_net">🔄 Сбросить сетевые настройки</button>
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
          <h2 data-i18n="diag_hags_title">HAGS</h2>
          <div class="toolbar">
            <button id="btn-hags-on" class="success" data-i18n="btn_enable_hags">Включить HAGS</button>
            <button id="btn-hags-off" class="secondary" data-i18n="btn_disable_hags">Выключить HAGS</button>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="diag_mouse_title">Акселерация мыши</h2>
          <div class="toolbar">
            <button id="btn-mouse-accel-off" class="success" data-i18n="btn_mouse_accel_off">Отключить</button>
            <button id="btn-mouse-accel-on" class="secondary" data-i18n="btn_mouse_accel_on">Включить</button>
          </div>
        </div>
      </div>

      <div class="tab" id="tab-report">
        <h1 data-i18n="nav_report">Отчёты</h1>
        <div class="section">
          <h2 data-i18n="report_html_title">HTML-отчёт о системе</h2>
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

      <div class="tab" id="tab-updates">
        <h1 data-i18n="updates_title">🔄 Обновления</h1>
        <p class="subtitle" data-i18n="updates_sub">Центр обновлений (GitHub + Бета)</p>
        <div class="section">
          <div class="hint">
            <b data-i18n="updates_hint_title">Обновление:</b> <span data-i18n="updates_hint_text">Нажмите «Проверить обновления». Программа сама найдет последнюю версию на GitHub и предложит её установить. Для бета-версий настройте URL в настройках.</span><br><br>
            <span data-i18n="updates_current_version">Текущая версия:</span> <b id="current-version-text" style="color:var(--accent2)">—</b>
            <span id="beta-version-label" style="display:none;color:var(--warn);font-weight:700;margin-left:8px;">BETA</span>
          </div>
          <div class="toolbar" style="margin-top:20px">
            <button id="btn-check-updates" class="success" data-i18n="btn_check_updates">🔍 Проверить обновления (GitHub)</button>
            <button id="btn-check-beta" class="danger" style="display:none;" data-i18n="btn_check_beta">🧪 Проверить Beta</button>
          </div>
          <div id="updates-list" style="margin-top:20px"></div>
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
              <option value="glass">🪟 Liquid Glass</option>
            </select>
          </div>
        </div>
        <div class="section">
          <h2 data-i18n="settings_mode">🛠 Режим работы</h2>
          <div class="check-item"><input type="checkbox" id="dry-run-chk"><label for="dry-run-chk" data-i18n="settings_dry_run">Режим "Только чтение" (предпросмотр без изменений)</label></div>
        </div>
        <div class="section">
          <h2 data-i18n="settings_fine_title">⚙️ Тонкая настройка очистки</h2>
          <div class="setting-group">
            <h3 data-i18n="settings_cache_clean">Очистка кэша</h3>
            <div class="setting-row"><label data-i18n="settings_clean_temp">Temp / Windows Temp</label><input type="checkbox" id="s-cleanTemp" checked></div>
            <div class="setting-row"><label data-i18n="settings_clean_prefetch">Prefetch</label><input type="checkbox" id="s-cleanPrefetch" checked></div>
            <div class="setting-row"><label data-i18n="cl_recycle">Корзина</label><input type="checkbox" id="s-cleanRecycle" checked></div>
            <div class="setting-row"><label data-i18n="cl_thumbs">Кэш миниатюр</label><input type="checkbox" id="s-cleanThumbs" checked></div>
            <div class="setting-row"><label data-i18n="cl_shader">Кэш шейдеров</label><input type="checkbox" id="s-cleanShader" checked></div>
            <div class="setting-row"><label data-i18n="settings_clean_dns">DNS-кэш</label><input type="checkbox" id="s-cleanDns" checked></div>
            <div class="setting-row"><label data-i18n="cl_do">Delivery Optimization</label><input type="checkbox" id="s-cleanDO" checked></div>
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
        <div class="section">
          <h2 data-i18n="settings_dev_code_title">🔑 Код разработчика (Beta)</h2>
          <div class="hint" data-i18n="settings_dev_code_hint">Введите код разработчика для получения доступа к бета-версиям.</div>
          <div class="toolbar">
            <input type="password" id="dev-code-input" data-i18n-placeholder="settings_dev_code_placeholder" placeholder="Введите код разработчика" style="flex:1;max-width:600px">
            <button id="btn-dev-code-submit" class="secondary" data-i18n="btn_dev_code_submit">Войти</button>
          </div>
          <div id="dev-code-status" style="margin-top:12px"></div>
        </div>
        <div class="section">
          <h2 data-i18n="settings_beta_title">🧪 Бета-обновления (Termux)</h2>
          <div class="hint" data-i18n="settings_beta_hint">URL вашего локального сервера с бета-версиями (например, https://xxx.trycloudflare.com/latest.json).</div>
          <div class="toolbar">
            <input type="text" id="beta-url-input" placeholder="https://..." style="flex:1;max-width:600px">
            <button id="btn-save-beta-url" class="secondary" data-i18n="btn_save_beta_url">Сохранить URL</button>
          </div>
        </div>
        <div class="footer-info">
          <p>ZeroBloat v15.0.0</p>
          <p>Поддержка: <b>i.love.pedro.67@gmail.com</b></p>
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
      <div style="flex:1"></div>
      <span id="app-version" style="color:var(--accent2);font-weight:700;">v15.0.0</span>
      <span id="beta-badge" class="beta-badge" style="display:none;">BETA</span>
      <div id="global-progress"></div>
    </div>
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
    nav_dash: "Дашборд", nav_ai: "AI Ассистент", nav_profiles: "Профили", nav_history: "История",
    nav_cpu: "Инструкции CPU", nav_smart: "Умная оптимизация", nav_autostart: "Автозагрузка", nav_games: "Игры", nav_overclock: "Разгон",
    nav_freeze: "Заморозка", nav_intel_sde: "Intel SDE", nav_drivers: "Драйверы",
    nav_drive: "Сканер диска", nav_bulk: "Массовое удаление", nav_sec: "Безопасность",
    nav_lite: "Lite Windows", nav_cleanup: "Очистка", nav_apps: "Софт", nav_overlay: "Оверлей",
    nav_services: "Службы", nav_files: "Файлы", nav_tweaks: "Твики", nav_advanced: "Расширенное",
    nav_power: "Питание", nav_maint: "Обслуживание", nav_network: "Сеть", nav_diag: "Диагностика",
    nav_report: "Отчёт", nav_updates: "Обновления", nav_settings: "Настройки", nav_log: "Лог",
    home_title: "Панель управления", home_sub: "Мониторинг в реальном времени",
    stat_ram: "ОЗУ свободно", stat_procs: "Процессов", stat_battery: "Батарея", chart_title: "Загрузка (последние 60 сек)",
    self_usage_title: "Использование ресурсов ZeroBloat", disk_health_title: "Здоровье дисков (S.M.A.R.T.)",
    info_title: "Информация о системе", info_cpu: "Процессор", info_gpu: "Видеокарта", info_ram: "ОЗУ (всего)",
    info_temp: "Температура CPU", info_throttle: "Троттлинг", info_boot: "Boot Time",
    quick_title: "Быстрые действия", btn_temp: "Temp", btn_dns: "Flush DNS",
    btn_ram: "Defrag RAM", btn_standby: "Standby Purger", btn_maintenance: "One-Click Maintenance",
    ai_sub: "Подключите LLM API (OpenAI/Ollama/LM Studio/GPT-4o) и опишите задачу оптимизации",
    ai_settings: "Настройки API", ai_local: "Использовать локальную модель", ai_endpoint: "Endpoint URL",
    ai_key: "API Key", ai_model: "Model", ai_hint: "Ключ хранится в зашифрованном виде (Windows DPAPI). Для локальных моделей (Ollama, LM Studio) укажите endpoint (например, http://localhost:11434/v1/chat/completions).",
    btn_save: "Сохранить", btn_load: "Загрузить", ai_chat: "Диалог", ai_placeholder: "Например: удали весь мусор и оптимизируй ПК под игры",
    btn_send: "Отправить", profiles_sub: "Управление профилями оптимизации (Игры, Работа, Ноутбук, Безопасный)",
    profiles_available: "Доступные профили", prof_gaming: "Игровой", prof_gaming_desc: "Максимальный FPS, отключение фоновых служб.",
    prof_work: "Работа", prof_work_desc: "Сбалансированная система для стабильной работы.", prof_laptop: "Ноутбук", prof_laptop_desc: "Энергосбережение, минимальный нагрев.",
    prof_safe: "Безопасный", prof_safe_desc: "Только 100% безопасные оптимизации без отключения защиты.", btn_apply: "Применить",
    btn_export: "Экспорт профилей", btn_import: "Импорт профилей", hist_sub: "Журнал всех внесенных изменений с возможностью отката",
    btn_rollback_last: "Откатить последнее", btn_refresh: "Обновить", smart_title: "✨ Умная оптимизация",
    smart_sub: "Автоматический анализ ПК и подбор безопасных улучшений", smart_hint: "Нажмите кнопку ниже, чтобы проанализировать систему.",
    smart_scan_btn: "Анализировать ПК", smart_apply_btn: "Применить выбранное", autostart_sub: "Управление программами при старте Windows",
    btn_boot_impact: "Анализ влияния на старт", games_sub: "Game Booster, сжатие, сканер, пресеты", game_booster_title: "Game Booster",
    btn_boost_on: "Включить буст", btn_boost_off: "Выключить буст", auto_boost_chk: "Автоматический игровой режим",
    games_fso_title: "Оптимизация FSO", btn_disable_fso: "Отключить Fullscreen Optimizations", games_compress_title: "Прозрачное сжатие игр (LZX)",
    btn_compress: "🗜 Сжать папку", games_scan_title: "Сканер установленных игр", btn_scan_games: "Сканировать",
    games_presets_title: "Игровые пресеты", preset_comp: "Competitive FPS", preset_comp_desc: "Максимальный FPS, минимум задержек",
    preset_bal: "Balanced", preset_bal_desc: "Компромисс между FPS и качеством", preset_qual: "Quality", preset_qual_desc: "Максимальное качество картинки",
    oc_sub: "GPU, CPU, RAM & System Tweaks", oc_warn: "⚠️ ВНИМАНИЕ! Разгон может привести к нестабильности системы.",
    oc_afterburner_title: "GPU Overclock (скрытый запуск MSI)", btn_apply_msi: "🎮 Применить профиль MSI", oc_cpu_ram_boost: "CPU & RAM Boost",
    oc_prio: "High Priority CPU", oc_paging: "Disable Paging", oc_paging_desc: "Только для 32GB+ RAM", oc_memcomp: "Memory Compression Off",
    oc_memcomp_desc: "Отключает сжатие памяти", btn_disable: "Отключить", oc_bench_title: "Бенчмарк до/после",
    oc_bench_hint: "Снимает показатели: свободная RAM, число процессов/служб, задержка диска, CPU score. Сравнивайте результат ДО и ПОСЛЕ оптимизации.",
    btn_bench_capture: "Снять бенчмарк", btn_bench_compare: "Сравнить", oc_stress_title: "Стресс-тест CPU (30 сек)",
    oc_stress_hint: "Нагружает все ядра CPU для проверки стабильности и температур.", btn_stress: "Запустить стресс-тест",
    btn_bench: "Быстрый бенчмарк", drive_treemap_title: "Treemap дискового пространства",
    drive_hint: "Нажмите кнопку ниже, чтобы проанализировать, какие папки занимают больше всего места на диске (диаграмма покажет это в виде цветных блоков).",
    btn_scan_drive: "Начать сканирование", btn_load_programs_detailed: "Загрузить программы", btn_bulk_uninstall: "Удалить выбранные",
    bulk_leftovers: "Поиск остатков", bulk_leftovers_placeholder: "Название программы", btn_scan_leftovers: "Найти остатки",
    btn_load_security: "Обновить статус", lite_title: "Lite Windows", lite_sub: "Агрессивный деблоат",
    lite_warn_title: "ВНИМАНИЕ:", lite_warn_text: "Удаляет UWP-приложения, отключает телеметрию и службы. Необратимо!",
    lite_btn_run: "СДЕЛАТЬ WINDOWS LITE", lite_uwp_title: "Список UWP-приложений",
    lite_uwp_hint: "Можно выбрать, что удалять. Значения «safe/caution/donottouch» показывают степень риска.", btn_uwp_apply: "🗑 Удалить выбранные",
    cl_recycle: "Корзина", cl_thumbs: "Кэш миниатюр", cl_shader: "Shader Cache", cl_do: "Delivery Optimization",
    cl_launchers: "Кэш лаунчеров", cl_browsers: "Кэш браузеров", cl_chats: "Discord/Telegram", cl_dumps: "Дампы памяти",
    btn_run_cleanup: "▶ Очистить", btn_enh_cache: "🚀 Enhanced Cache", btn_enh_ram: "🧠 Enhanced RAM",
    apps_update_title: "Обновление софта", btn_update_all: "Обновить всё (Winget)", apps_uninstaller_title: "Деинсталлятор",
    btn_load_programs: "📋 Список программ", ov_temp: "Температура", ov_fan: "Вентилятор", ov_dark: "Тёмная тема оверлея",
    btn_ov_toggle: "🎯 Вкл/Выкл оверлей", files_large: "Поиск больших файлов", files_large_path: "Путь (пусто = профиль)",
    btn_find: "Найти", files_dups: "Поиск дубликатов", files_dup_path: "Путь (пусто = Downloads)", btn_scan: "Сканировать",
    files_shred: "Шредер файлов", files_shred_path: "Полный путь к файлу", btn_shred: "Уничтожить", files_hosts: "Редактор файла Hosts",
    btn_save_settings: "💾 Сохранить настройки", tw_gm: "Game Mode", tw_gm_on: "Включить", tw_gm_off: "Выключить",
    tw_telemetry: "Телеметрия и обновления", tw_disable_tel: "Отключить телеметрию", tw_disable_upd: "Отключить Windows Update",
    tw_enable_upd: "Включить обновления", adv_warn: "⚠️ Осторожно! Некоторые функции могут повлиять на стабильность системы. Все опасные твики требуют подтверждения и создают точку восстановления.",
    adv_safe_title: "Безопасные оптимизации", adv_cache_timer: "Кэш-таймер (5 мин)", adv_cache_timer_desc: "Автоматическая очистка кэша ФС каждые 5 минут.",
    btn_toggle: "Включить/Выключить", adv_dpc: "Анализатор DPC/ISR", adv_dpc_desc: "Мониторинг задержек драйверов.", btn_check: "Проверить",
    adv_restore: "Точки восстановления", adv_restore_desc: "Создание бэкапа системы перед изменениями.", btn_create_restore: "💾 Создать",
    adv_net_reset: "Сброс сети", adv_net_reset_desc: "Winsock/TCP reset.", btn_reset: "🔄 Сбросить", adv_rss: "Твикер RSS / MSI-X",
    adv_rss_desc: "Включает прерывания сетевых адаптеров.", btn_enable: "Включить", adv_caution_title: "Использовать с осторожностью",
    adv_vbs: "Отключить VBS / HVCI", adv_vbs_desc: "Отключает виртуализацию безопасности.", adv_etw: "Отключить ETW / WPP",
    adv_etw_desc: "Останавливает системное логирование.", adv_ctx_menu: "Агрессивный Debloat меню", adv_ctx_menu_desc: "Удаляет сторонние CLSID.",
    adv_spyware: "Удаление шпионского ПО", adv_spyware_desc: "Полное удаление UWP-мусора.", btn_clean: "Очистить",
    adv_nvidia: "NVIDIA P-States", adv_nvidia_desc: "Принудительный P0.", btn_p0: "P0 Mode", adv_kernel_pools: "Сжатие пулов ядра",
    adv_kernel_pools_desc: "Очистка Non-Paged и Paged Pool.", adv_ddu: "Интеграция DDU", adv_ddu_desc: "Запуск Display Driver Uninstaller.",
    btn_run_ddu: "Запустить DDU", adv_dangerous_title: "Опасные зоны", btn_disable_defender: "Отключить Defender",
    btn_deep_services: "Глубокое отключение служб", btn_clean_winsxs: "Очистить WinSxS", adv_restore_points_title: "Восстановление системы",
    btn_list_rp: "📋 Список точек", power_scheme: "Схема питания", power_ult: "⚡ Максимальная", power_high: "Высокая",
    power_bal: "Сбалансированная", power_usb_pcie: "USB / PCIe Power Management", btn_usb_no_suspend: "USB без suspend",
    btn_usb_suspend_ok: "USB suspend OK", btn_pcie_max_perf: "PCIe max perf", btn_pcie_power_save: "PCIe power save",
    btn_sfc: "🛠 SFC /scannow", btn_dism: "🔧 DISM /RestoreHealth", maint_updates_title: "Windows Update",
    btn_pause_7: "⏸ Пауза 7 дней", btn_pause_35: "⏸ Пауза 35 дней", btn_resume_upd: "▶ Возобновить",
    maint_tasks_title: "Планировщик заданий", btn_load_tasks: "📋 Загрузить задачи", maint_sched_title: "Автоочистка через Task Scheduler",
    maint_sched_hint: "Создаёт задание schtasks.exe для запуска очистки по расписанию.", btn_sched_daily: "📅 Ежедневно 03:00",
    btn_sched_weekly: "📅 Еженедельно (Вс 03:00)", btn_sched_onidle: "📅 При простое", btn_sched_delete: "🗑 Удалить задание",
    maint_pagefile_title: "Файл подкачки", btn_pagefile_info: "📊 Информация", maint_pagefile_initial: "Initial (MB)",
    maint_pagefile_max: "Max (MB)", net_ping_title: "Ping-монитор", btn_ping: "📡 Ping", net_reset_title: "Сброс сети",
    btn_reset_net: "🔄 Сбросить сетевые настройки", diag_events_title: "Анализ журнала событий", btn_analyze_events: "📋 Анализ (7 дней)",
    diag_battery_title: "Батарея", btn_batt_health: "🔋 Проверить", diag_hags_title: "HAGS", btn_enable_hags: "Включить HAGS",
    btn_disable_hags: "Выключить HAGS", diag_mouse_title: "Акселерация мыши", btn_mouse_accel_off: "Отключить", btn_mouse_accel_on: "Включить",
    report_html_title: "HTML-отчёт о системе", btn_generate_report: "📊 Сгенерировать отчёт", report_backup_title: "Резервное копирование настроек",
    btn_backup_settings: "💾 Сохранить", btn_restore_settings: "🔄 Восстановить", updates_title: "🔄 Обновления",
    updates_sub: "Центр обновлений (GitHub + Бета)", updates_hint_title: "Обновление:", updates_hint_text: "Нажмите «Проверить обновления». Программа сама найдет последнюю версию на GitHub и предложит её установить. Для бета-версий настройте URL в настройках.",
    updates_current_version: "Текущая версия:", btn_check_updates: "🔍 Проверить обновления (GitHub)", btn_check_beta: "🧪 Проверить Beta",
    settings_lang_title: "🌐 Язык / Language", settings_drive_title: "💾 Активный диск", settings_theme_title: "🎨 Тема",
    settings_mode: "🛠 Режим работы", settings_dry_run: "Режим \"Только чтение\" (предпросмотр без изменений)", settings_fine_title: "⚙️ Тонкая настройка очистки",
    settings_cache_clean: "Очистка кэша", settings_clean_temp: "Temp / Windows Temp", settings_clean_prefetch: "Prefetch",
    settings_clean_dns: "DNS-кэш", settings_ram_clean: "Очистка ОЗУ", settings_ram_standby: "Очищать standby list",
    settings_ram_modified: "Сбрасывать modified page list", settings_ram_ws: "Trim working sets", settings_ram_cache: "Очищать system file cache",
    settings_auto_ram: "Автоочистка ОЗУ", settings_auto_ram_enable: "Включить автоочистку", settings_auto_ram_threshold: "Порог использования (%)",
    settings_auto_ram_interval: "Интервал (мин)", settings_auto_ram_idle: "Очищать при простое", settings_ui: "Интерфейс",
    settings_hotkeys: "Горячие клавиши (Ctrl+Alt+O)", settings_deep: "Глубокая настройка (50+ параметров)",
    settings_sec_privacy: "Безопасность и Приватность", settings_perf_games: "Производительность и Игры",
    settings_clean_disk: "Очистка и Диск", settings_ram_mem: "ОЗУ и Память", settings_network: "Сеть", settings_system: "Система",
    btn_apply_s50: "Применить 50+ настроек", settings_dev_code_title: "🔑 Код разработчика (Beta)",
    settings_dev_code_hint: "Введите код разработчика для получения доступа к бета-версиям.", settings_dev_code_placeholder: "Введите код разработчика",
    btn_dev_code_submit: "Войти", settings_beta_title: "🧪 Бета-обновления (Termux)", settings_beta_hint: "URL вашего локального сервера с бета-версиями (например, https://xxx.trycloudflare.com/latest.json).",
    btn_save_beta_url: "Сохранить URL", status_ready: "Готово"
  },
  en: {
    nav_dash: "Dashboard", nav_ai: "AI Assistant", nav_profiles: "Profiles", nav_history: "History",
    nav_cpu: "CPU Instructions", nav_smart: "Smart Optimize", nav_autostart: "Autostart", nav_games: "Games", nav_overclock: "Overclock",
    nav_freeze: "Freeze", nav_intel_sde: "Intel SDE", nav_drivers: "Drivers",
    nav_drive: "Drive Scanner", nav_bulk: "Bulk Uninstall", nav_sec: "Security",
    nav_lite: "Lite Windows", nav_cleanup: "Cleanup", nav_apps: "Apps", nav_overlay: "Overlay",
    nav_services: "Services", nav_files: "Files", nav_tweaks: "Tweaks", nav_advanced: "Advanced",
    nav_power: "Power", nav_maint: "Maintenance", nav_network: "Network", nav_diag: "Diagnostics",
    nav_report: "Report", nav_updates: "Updates", nav_settings: "Settings", nav_log: "Log",
    home_title: "Control Panel", home_sub: "Real-time monitoring",
    stat_ram: "RAM Free", stat_procs: "Processes", stat_battery: "Battery", chart_title: "Load (last 60s)",
    self_usage_title: "ZeroBloat Resource Usage", disk_health_title: "Disk Health (S.M.A.R.T.)",
    info_title: "System Information", info_cpu: "Processor", info_gpu: "Video Card", info_ram: "RAM (Total)",
    info_temp: "CPU Temp", info_throttle: "Throttling", info_boot: "Boot Time",
    quick_title: "Quick Actions", btn_temp: "Temp", btn_dns: "Flush DNS",
    btn_ram: "Defrag RAM", btn_standby: "Standby Purger", btn_maintenance: "One-Click Maintenance",
    ai_sub: "Connect LLM API (OpenAI/Ollama/LM Studio/GPT-4o) and describe your optimization task",
    ai_settings: "API Settings", ai_local: "Use local model", ai_endpoint: "Endpoint URL",
    ai_key: "API Key", ai_model: "Model", ai_hint: "Key is stored encrypted (Windows DPAPI). For local models (Ollama, LM Studio) specify endpoint (e.g. http://localhost:11434/v1/chat/completions).",
    btn_save: "Save", btn_load: "Load", ai_chat: "Chat", ai_placeholder: "Example: clean up junk and optimize PC for gaming",
    btn_send: "Send", profiles_sub: "Manage optimization profiles (Gaming, Work, Laptop, Safe)",
    profiles_available: "Available Profiles", prof_gaming: "Gaming", prof_gaming_desc: "Max FPS, disable background services.",
    prof_work: "Work", prof_work_desc: "Balanced system for stable work.", prof_laptop: "Laptop", prof_laptop_desc: "Power saving, minimal heat.",
    prof_safe: "Safe", prof_safe_desc: "Only 100% safe optimizations without disabling protection.", btn_apply: "Apply",
    btn_export: "Export Profiles", btn_import: "Import Profiles", hist_sub: "Log of all changes with rollback option",
    btn_rollback_last: "Rollback Last", btn_refresh: "Refresh", smart_title: "✨ Smart Optimize",
    smart_sub: "Automatic PC analysis and safe improvements selection", smart_hint: "Click the button below to analyze the system.",
    smart_scan_btn: "Analyze PC", smart_apply_btn: "Apply Selected", autostart_sub: "Manage programs at Windows startup",
    btn_boot_impact: "Analyze boot impact", games_sub: "Game Booster, compression, scanner, presets", game_booster_title: "Game Booster",
    btn_boost_on: "Enable Boost", btn_boost_off: "Disable Boost", auto_boost_chk: "Automatic game mode",
    games_fso_title: "FSO Optimization", btn_disable_fso: "Disable Fullscreen Optimizations", games_compress_title: "Transparent Game Compression (LZX)",
    btn_compress: "🗜 Compress Folder", games_scan_title: "Installed Games Scanner", btn_scan_games: "Scan",
    games_presets_title: "Game Presets", preset_comp: "Competitive FPS", preset_comp_desc: "Maximum FPS, minimum latency",
    preset_bal: "Balanced", preset_bal_desc: "Balance between FPS and quality", preset_qual: "Quality", preset_qual_desc: "Maximum image quality",
    oc_sub: "GPU, CPU, RAM & System Tweaks", oc_warn: "⚠️ WARNING! Overclocking can cause system instability.",
    oc_afterburner_title: "GPU Overclock (hidden MSI launch)", btn_apply_msi: "🎮 Apply MSI Profile", oc_cpu_ram_boost: "CPU & RAM Boost",
    oc_prio: "High Priority CPU", oc_paging: "Disable Paging", oc_paging_desc: "Only for 32GB+ RAM", oc_memcomp: "Memory Compression Off",
    oc_memcomp_desc: "Disables memory compression", btn_disable: "Disable", oc_bench_title: "Before/After Benchmark",
    oc_bench_hint: "Captures metrics: free RAM, process/service count, disk latency, CPU score. Compare results BEFORE and AFTER optimization.",
    btn_bench_capture: "Capture Benchmark", btn_bench_compare: "Compare", oc_stress_title: "CPU Stress Test (30s)",
    oc_stress_hint: "Loads all CPU cores to check stability and temperatures.", btn_stress: "Start Stress Test",
    btn_bench: "Quick Benchmark", drive_treemap_title: "Disk Space Treemap",
    drive_hint: "Click the button below to analyze which folders take up the most space on the disk (the diagram will show this as colored blocks).",
    btn_scan_drive: "Start Scan", btn_load_programs_detailed: "Load Programs", btn_bulk_uninstall: "Uninstall Selected",
    bulk_leftovers: "Find Leftovers", bulk_leftovers_placeholder: "Program name", btn_scan_leftovers: "Find Leftovers",
    btn_load_security: "Refresh Status", lite_title: "Lite Windows", lite_sub: "Aggressive debloat",
    lite_warn_title: "WARNING:", lite_warn_text: "Removes UWP apps, disables telemetry and services. Irreversible!",
    lite_btn_run: "MAKE WINDOWS LITE", lite_uwp_title: "UWP Apps List",
    lite_uwp_hint: "Select what to remove. Values «safe/caution/donottouch» indicate risk level.", btn_uwp_apply: "🗑 Remove Selected",
    cl_recycle: "Recycle Bin", cl_thumbs: "Thumbnails", cl_shader: "Shader Cache", cl_do: "Delivery Optimization",
    cl_launchers: "Launchers Cache", cl_browsers: "Browsers Cache", cl_chats: "Discord/Telegram", cl_dumps: "Memory Dumps",
    btn_run_cleanup: "▶ Clean", btn_enh_cache: "🚀 Enhanced Cache", btn_enh_ram: "🧠 Enhanced RAM",
    apps_update_title: "Software Update", btn_update_all: "Update All (Winget)", apps_uninstaller_title: "Uninstaller",
    btn_load_programs: "📋 Program List", ov_temp: "Temperature", ov_fan: "Fan", ov_dark: "Overlay Dark Theme",
    btn_ov_toggle: "🎯 Toggle Overlay", files_large: "Find Large Files", files_large_path: "Path (empty = profile)",
    btn_find: "Find", files_dups: "Find Duplicates", files_dup_path: "Path (empty = Downloads)", btn_scan: "Scan",
    files_shred: "File Shredder", files_shred_path: "Full file path", btn_shred: "Shred", files_hosts: "Hosts File Editor",
    btn_save_settings: "💾 Save Settings", tw_gm: "Game Mode", tw_gm_on: "Enable", tw_gm_off: "Disable",
    tw_telemetry: "Telemetry & Updates", tw_disable_tel: "Disable Telemetry", tw_disable_upd: "Disable Windows Update",
    tw_enable_upd: "Enable Updates", adv_warn: "⚠️ Caution! Some functions may affect system stability. All dangerous tweaks require confirmation and create a restore point.",
    adv_safe_title: "Safe Optimizations", adv_cache_timer: "Cache Timer (5 min)", adv_cache_timer_desc: "Automatic FS cache cleanup every 5 minutes.",
    btn_toggle: "Toggle", adv_dpc: "DPC/ISR Analyzer", adv_dpc_desc: "Driver latency monitoring.", btn_check: "Check",
    adv_restore: "Restore Points", adv_restore_desc: "Create system backup before changes.", btn_create_restore: "💾 Create",
    adv_net_reset: "Network Reset", adv_net_reset_desc: "Winsock/TCP reset.", btn_reset: "🔄 Reset", adv_rss: "RSS / MSI-X Tweaker",
    adv_rss_desc: "Enables network adapter interrupts.", btn_enable: "Enable", adv_caution_title: "Use with Caution",
    adv_vbs: "Disable VBS / HVCI", adv_vbs_desc: "Disables security virtualization.", adv_etw: "Disable ETW / WPP",
    adv_etw_desc: "Stops system logging.", adv_ctx_menu: "Aggressive Menu Debloat", adv_ctx_menu_desc: "Removes third-party CLSIDs.",
    adv_spyware: "Remove Spyware", adv_spyware_desc: "Full removal of UWP junk.", btn_clean: "Clean",
    adv_nvidia: "NVIDIA P-States", adv_nvidia_desc: "Force P0.", btn_p0: "P0 Mode", adv_kernel_pools: "Kernel Pools Compression",
    adv_kernel_pools_desc: "Clean Non-Paged and Paged Pool.", adv_ddu: "DDU Integration", adv_ddu_desc: "Launch Display Driver Uninstaller.",
    btn_run_ddu: "Run DDU", adv_dangerous_title: "Danger Zones", btn_disable_defender: "Disable Defender",
    btn_deep_services: "Deep Service Disable", btn_clean_winsxs: "Clean WinSxS", adv_restore_points_title: "System Restore",
    btn_list_rp: "📋 List Points", power_scheme: "Power Plan", power_ult: "⚡ Ultimate", power_high: "High",
    power_bal: "Balanced", power_usb_pcie: "USB / PCIe Power Management", btn_usb_no_suspend: "USB no suspend",
    btn_usb_suspend_ok: "USB suspend OK", btn_pcie_max_perf: "PCIe max perf", btn_pcie_power_save: "PCIe power save",
    btn_sfc: "🛠 SFC /scannow", btn_dism: "🔧 DISM /RestoreHealth", maint_updates_title: "Windows Update",
    btn_pause_7: "⏸ Pause 7 days", btn_pause_35: "⏸ Pause 35 days", btn_resume_upd: "▶ Resume",
    maint_tasks_title: "Task Scheduler", btn_load_tasks: "📋 Load Tasks", maint_sched_title: "Auto-Cleanup via Task Scheduler",
    maint_sched_hint: "Creates schtasks.exe task to run cleanup on schedule.", btn_sched_daily: "📅 Daily 03:00",
    btn_sched_weekly: "📅 Weekly (Sun 03:00)", btn_sched_onidle: "📅 On Idle", btn_sched_delete: "🗑 Delete Task",
    maint_pagefile_title: "Pagefile", btn_pagefile_info: "📊 Info", maint_pagefile_initial: "Initial (MB)",
    maint_pagefile_max: "Max (MB)", net_ping_title: "Ping Monitor", btn_ping: "📡 Ping", net_reset_title: "Network Reset",
    btn_reset_net: "🔄 Reset Network Settings", diag_events_title: "Event Log Analysis", btn_analyze_events: "📋 Analyze (7 days)",
    diag_battery_title: "Battery", btn_batt_health: "🔋 Check", diag_hags_title: "HAGS", btn_enable_hags: "Enable HAGS",
    btn_disable_hags: "Disable HAGS", diag_mouse_title: "Mouse Acceleration", btn_mouse_accel_off: "Disable", btn_mouse_accel_on: "Enable",
    report_html_title: "HTML System Report", btn_generate_report: "📊 Generate Report", report_backup_title: "Settings Backup",
    btn_backup_settings: "💾 Backup", btn_restore_settings: "🔄 Restore", updates_title: "🔄 Updates",
    updates_sub: "Update Center (GitHub + Beta)", updates_hint_title: "Update:", updates_hint_text: "Click 'Check for updates'. The program will find the latest version on GitHub and offer to install it. For beta versions, configure the URL in settings.",
    updates_current_version: "Current version:", btn_check_updates: "🔍 Check for Updates (GitHub)", btn_check_beta: "🧪 Check Beta",
    settings_lang_title: "🌐 Language", settings_drive_title: "💾 Active Drive", settings_theme_title: "🎨 Theme",
    settings_mode: "🛠 Operation Mode", settings_dry_run: "Read-Only Mode (preview without changes)", settings_fine_title: "⚙️ Fine Tuning",
    settings_cache_clean: "Cache Cleaning", settings_clean_temp: "Temp / Windows Temp", settings_clean_prefetch: "Prefetch",
    settings_clean_dns: "DNS Cache", settings_ram_clean: "RAM Cleaning", settings_ram_standby: "Clear standby list",
    settings_ram_modified: "Flush modified page list", settings_ram_ws: "Trim working sets", settings_ram_cache: "Clear system file cache",
    settings_auto_ram: "Auto RAM Clean", settings_auto_ram_enable: "Enable auto clean", settings_auto_ram_threshold: "Threshold (%)",
    settings_auto_ram_interval: "Interval (min)", settings_auto_ram_idle: "Clean on idle", settings_ui: "Interface",
    settings_hotkeys: "Hotkeys (Ctrl+Alt+O)", settings_deep: "Deep Customization (50+ parameters)",
    settings_sec_privacy: "Security & Privacy", settings_perf_games: "Performance & Games",
    settings_clean_disk: "Cleanup & Disk", settings_ram_mem: "RAM & Memory", settings_network: "Network", settings_system: "System",
    btn_apply_s50: "Apply 50+ Settings", settings_dev_code_title: "🔑 Developer Code (Beta)",
    settings_dev_code_hint: "Enter developer code to access beta versions.", settings_dev_code_placeholder: "Enter developer code",
    btn_dev_code_submit: "Login", settings_beta_title: "🧪 Beta Updates (Termux)", settings_beta_hint: "URL of your local server with beta versions (e.g. https://xxx.trycloudflare.com/latest.json).",
    btn_save_beta_url: "Save URL", status_ready: "Ready"
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
    var legend = document.getElementById('drive-legend');
    if (legend) legend.innerHTML = '';
    
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
        if (legend) {
            var span = document.createElement('div');
            span.style.marginBottom = '4px';
            span.innerHTML = '<span style="display:inline-block;width:12px;height:12px;background:' + colors[i % colors.length] + ';margin-right:8px;"></span>' + items[i].name + ' (' + (items[i].size / (1024*1024)).toFixed(1) + ' MB)';
            legend.appendChild(span);
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
  document.getElementById('status-text').textContent = text || i18n[currentLang].status_ready;
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
    if (el.dataset.tab === 'updates') loadUpdates(null);
    if (el.dataset.tab === 'ai') loadAiConfig();
    if (el.dataset.tab === 'settings') loadDevCodeStatus();
    if (el.dataset.tab === 'history') loadHistory();
    if (el.dataset.tab === 'lite') loadUwpList();
    if (el.dataset.tab === 'freeze') loadFreezeList();
    if (el.dataset.tab === 'intel_sde') loadSdeStatus();
    if (el.dataset.tab === 'drivers') scanDrivers();
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
    if ((e = document.getElementById('stat-battery'))) e.textContent = (s.battery >= 0) ? s.battery + '%' + (s.ac ? ' (AC)' : ' (BAT)') : '—';
    if ((e = document.getElementById('stat-dpc'))) e.textContent = (s.dpc || 0).toFixed(1) + '%';
    if (chart) chart.push(cpu, Math.max(0, gpu));
    if (s.version) {
      var vel = document.getElementById('app-version');
      if (vel) vel.textContent = 'v' + s.version;
      var vtext = document.getElementById('current-version-text');
      if (vtext) vtext.textContent = 'v' + s.version;
    }
    var betaBadge = document.getElementById('beta-badge');
    if (betaBadge) betaBadge.style.display = s.isBeta ? 'inline-block' : 'none';
    var betaVerLabel = document.getElementById('beta-version-label');
    if (betaVerLabel) betaVerLabel.style.display = s.isBeta ? 'inline-block' : 'none';
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
          if (d.wear >= 0) healthStr += ' | Wear: ' + d.wear + '%';
          if (d.temp >= 0) healthStr += ' | Temp: ' + d.temp + '°C';
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
      var el = document.getElementById('feat-full-' + k); if (!el) return;
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
  setBusy(btn, true, 'Выполнение...');
  call('quick_action', action).then(function(r) {
    if (!r || r.ok === false) toast('Ошибка: ' + (r && r.message ? r.message : 'unknown'), 'error');
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

function loadFreezeList() {
  call('get_freeze_list').then(function(r) {
    var c = document.getElementById('freeze-list');
    if (!c) return;
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет процессов для заморозки</div>'; return; }
    r.items.forEach(function(p) {
      var row = document.createElement('div');
      row.className = 'list-row';
      var cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.dataset.pid = p.pid;
      cb.checked = p.frozen;
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = p.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = 'PID: ' + p.pid;
      info.appendChild(title);
      info.appendChild(sub);
      row.appendChild(cb);
      row.appendChild(info);
      c.appendChild(row);
    });
  });
}

function freezeSelected() {
  var cbs = document.querySelectorAll('#freeze-list input[type=checkbox]:checked');
  if (!cbs.length) { toast('Ничего не выбрано', 'error'); return; }
  var pids = [];
  cbs.forEach(function(cb) { pids.push(parseInt(cb.dataset.pid, 10)); });
  call('freeze_processes', pids).then(function(r) {
    toast(r.ok ? 'Заморожено: ' + (r.count || 0) : 'Ошибка', r.ok ? 'success' : 'error');
    loadFreezeList();
  });
}

function unfreezeAll() {
  call('unfreeze_all').then(function(r) {
    toast(r.ok ? 'Разморожено' : 'Ошибка', r.ok ? 'success' : 'error');
    loadFreezeList();
  });
}

function runLiteDebloat(btn) {
  if (!confirm('Это удалит встроенные приложения и отключит службы. Необратимо. Продолжить?')) return;
  setBusy(btn, true, 'Debloat...');
  call('lite_debloat').then(function(r) {
    toast(r.ok ? 'Готово!' : 'Ошибка', r.ok ? 'success' : 'error');
  }).catch(function(){ toast('Timeout', 'error'); }).finally(function() { setBusy(btn, false); });
}

function loadUwpList() {
  call('uwp_list').then(function(r) {
    var c = document.getElementById('uwp-list');
    if (!c) return;
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет данных</div>'; return; }
    r.items.forEach(function(a) {
      var row = document.createElement('div');
      row.className = 'list-row';
      var cb = document.createElement('input');
      cb.type = 'checkbox';
      cb.dataset.pkg = a.packageName;
      cb.checked = a.category === 'safe';
      if (a.category === 'donottouch') cb.disabled = true;
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = a.displayName + ' (' + a.packageName + ')';
      var badge = document.createElement('span');
      badge.style.marginLeft = '8px';
      badge.style.fontSize = '11px';
      badge.textContent = a.category === 'safe' ? '✓ safe' : (a.category === 'caution' ? '⚠ caution' : '✕ donottouch');
      badge.style.color = a.category === 'safe' ? 'var(--success)' : (a.category === 'caution' ? 'var(--warn)' : 'var(--accent)');
      title.appendChild(badge);
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = a.description;
      info.appendChild(title);
      info.appendChild(sub);
      row.appendChild(cb);
      row.appendChild(info);
      c.appendChild(row);
    });
  });
}

function applyUwpRemoval(btn) {
  var cbs = document.querySelectorAll('#uwp-list input[type=checkbox]:checked');
  if (!cbs.length) { toast('Ничего не выбрано', 'error'); return; }
  if (!confirm('Удалить ' + cbs.length + ' приложений? Это необратимо.')) return;
  var pkgs = [];
  cbs.forEach(function(cb) { pkgs.push(cb.dataset.pkg); });
  setBusy(btn, true, 'Удаление...');
  call('uwp_remove', pkgs).then(function(r) {
    toast(r.ok ? 'Удалено: ' + (r.count || 0) : 'Ошибка', r.ok ? 'success' : 'error');
    loadUwpList();
  }).finally(function() { setBusy(btn, false); });
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
    r.items.slice(0, 200).forEach(function(s) {
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
        if (!confirm('Отключить службу ' + s.name + '? Будет создана точка восстановления.')) return;
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
    r.items.forEach(function(p) {
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
    if (!confirm('Удалить ' + cbs.length + ' программ? Действие необратимо.')) return;
    setBusy(btn, true, 'Удаление...');
    var progs = [];
    cbs.forEach(function(cb) {
        progs.push({ name: cb.dataset.name, silentCmd: cb.dataset.silent });
    });
    call('bulk_uninstall', JSON.stringify(progs)).then(function(r) {
        if (r.ok) {
            toast('Удаление завершено', 'success');
            loadProgramsDetailed(null);
        } else toast('Ошибка', 'error');
    }).finally(function() { setBusy(btn, false); });
}

function scanLeftovers(btn) {
  var name = document.getElementById('leftover-name').value.trim();
  if (!name) { toast('Введите название', 'error'); return; }
  setBusy(btn, true, 'Поиск...');
  call('scan_leftovers', name).then(function(r) {
    var c = document.getElementById('leftover-results');
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Остатки не найдены</div>'; return; }
    r.items.forEach(function(p) {
      var row = document.createElement('div');
      row.className = 'list-row';
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = p;
      info.appendChild(title);
      row.appendChild(info);
      c.appendChild(row);
    });
  }).finally(function() { setBusy(btn, false); });
}

function scanDriveAsync(btn) {
    setBusy(btn, true, 'Сканирование...');
    document.getElementById('drive-legend').innerHTML = '<div class="hint">Пожалуйста, подождите. Сканирование диска может занять несколько минут в зависимости от количества файлов.</div>';
    call('scan_drive_fast', 'C').then(function(r) {
        if (r && r.items) {
            drawTreemap(r.items);
            toast('Сканирование завершено', 'success');
        } else {
            document.getElementById('drive-legend').innerHTML = '';
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

function scanDrivers() {
  call('scan_drivers').then(function(r) {
    var c = document.getElementById('drivers-list');
    if (!c) return;
    c.innerHTML = '';
    if (!r.items || !r.items.length) { c.innerHTML = '<div class="hint">Нет устройств, требующих обновления</div>'; return; }
    r.items.forEach(function(d) {
      var row = document.createElement('div');
      row.className = 'list-row';
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = d.name;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = 'Текущая версия: ' + d.version;
      info.appendChild(title);
      info.appendChild(sub);
      var btnUpdate = document.createElement('button');
      btnUpdate.className = 'success';
      btnUpdate.textContent = 'Обновить';
      btnUpdate.addEventListener('click', function() { updateDriver(d.deviceId); });
      row.appendChild(info);
      row.appendChild(btnUpdate);
      c.appendChild(row);
    });
  });
}

function updateDriver(deviceId) {
  call('update_driver', deviceId).then(function(r) {
    toast(r.ok ? 'Обновление запущено' : 'Ошибка', r.ok ? 'success' : 'error');
    scanDrivers();
  });
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
    lbl.textContent = d.letter + ': — ' + (isSystem ? 'Системный диск' : 'Не-системный диск');
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

function getChk(id) { var e = document.getElementById(id); return e ? !!e.checked : false; }

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
  if (!confirm('Применить 50+ настроек? Опасные твики будут предварительно подтверждены и создадут точку восстановления.')) return;
  setBusy(btn, true, 'Применение 50+ настроек...');
  call('apply_s50').then(function(r) {
    toast(r && r.ok ? 'Применено!' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

document.getElementById('ov-cpu').addEventListener('change', function(e) { call('set_overlay_setting', 'cpu', e.target.checked ? '1' : '0'); });
document.getElementById('ov-gpu').addEventListener('change', function(e) { call('set_overlay_setting', 'gpu', e.target.checked ? '1' : '0'); });
document.getElementById('ov-ram').addEventListener('change', function(e) { call('set_overlay_setting', 'ram', e.target.checked ? '1' : '0'); });
document.getElementById('ov-temp').addEventListener('change', function(e) { call('set_overlay_setting', 'temp', e.target.checked ? '1' : '0'); });
document.getElementById('ov-vram').addEventListener('change', function(e) { call('set_overlay_setting', 'vram', e.target.checked ? '1' : '0'); });
document.getElementById('ov-fan').addEventListener('change', function(e) { call('set_overlay_setting', 'fan', e.target.checked ? '1' : '0'); });
document.getElementById('ov-ping').addEventListener('change', function(e) { call('set_overlay_setting', 'ping', e.target.checked ? '1' : '0'); });
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

function benchCapture(btn) {
  setBusy(btn, true, 'Снятие...');
  call('bench_capture').then(function(r) {
    toast(r && r.ok ? 'Бенчмарк снят' : 'Ошибка', r && r.ok ? 'success' : 'error');
    benchCompare(null);
  }).finally(function() { setBusy(btn, false); });
}

function benchCompare(btn) {
  if (btn) setBusy(btn, true, '...');
  call('bench_get_history').then(function(r) {
    var c = document.getElementById('bench-results');
    if (!c) return;
    c.innerHTML = '';
    if (!r.items || r.items.length < 1) { c.innerHTML = '<div class="hint">Нет данных бенчмарка</div>'; return; }
    var last = r.items[r.items.length - 1];
    var prev = r.items.length > 1 ? r.items[r.items.length - 2] : null;
    var table = document.createElement('table');
    table.className = 'bench';
    table.innerHTML = '<tr><th>Метрика</th><th>До</th><th>После</th><th>Δ</th></tr>';
    function row(name, a, b, invert) {
      var tr = document.createElement('tr');
      tr.innerHTML = '<td>' + name + '</td><td>' + a + '</td><td>' + b + '</td><td>' + (b - a > 0 ? '+' : '') + (b - a).toFixed(2) + '</td>';
      table.appendChild(tr);
    }
    if (prev) {
      row('Free RAM (MB)', prev.freeRamMB, last.freeRamMB);
      row('Processes', prev.processCount, last.processCount);
      row('Services', prev.serviceCount, last.serviceCount);
      row('Disk latency (ms)', prev.diskLatencyMs, last.diskLatencyMs);
      row('CPU score', prev.cpuScore, last.cpuScore);
    } else {
      row('Free RAM (MB)', 0, last.freeRamMB);
      row('Processes', 0, last.processCount);
      row('Services', 0, last.serviceCount);
      row('Disk latency (ms)', 0, last.diskLatencyMs);
      row('CPU score', 0, last.cpuScore);
    }
    c.appendChild(table);
  }).finally(function() { if (btn) setBusy(btn, false); });
}

function createRestorePoint(btn) {
  var name = prompt('Название точки восстановления:', 'ZeroBloat');
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
  document.querySelectorAll('[data-i18n-placeholder]').forEach(function(el) {
    var k = el.getAttribute('data-i18n-placeholder');
    if (dict[k]) el.placeholder = dict[k];
  });
  store.set('optpro-lang', currentLang);
  var sel = document.getElementById('lang-select');
  if (sel) sel.value = currentLang;
  call('get_drives').then(function(r) {
     if (r && r.drives) {
         var activeDriveStr = document.getElementById('drive-select').value || 'C';
         var isSys = (activeDriveStr === r.system);
         applyDriveUI({ letter: activeDriveStr, isSystem: isSys });
     }
  });
}

function loadUpdates(btn) {
  if (btn) setBusy(btn, true, 'Поиск обновлений...');
  call('check_updates_github').then(function(r) {
    var c = document.getElementById('updates-list');
    c.innerHTML = '';
    if (!r || !r.ok || !r.hasUpdate) {
      c.innerHTML = '<div class="hint">У вас последняя версия или не удалось проверить.</div>';
      return;
    }
    var row = document.createElement('div');
    row.className = 'list-row';
    var info = document.createElement('div');
    info.className = 'info';
    var title = document.createElement('div');
    title.className = 'title';
    title.textContent = 'Доступна версия ' + r.version;
    var sub = document.createElement('div');
    sub.className = 'sub';
    sub.textContent = 'Источник: GitHub / ' + r.assetName;
    info.appendChild(title);
    info.appendChild(sub);
    var btnInstall = document.createElement('button');
    btnInstall.className = 'success';
    btnInstall.textContent = 'Скачать и установить';
    btnInstall.addEventListener('click', function() { applyUpdate(btnInstall, r.url); });
    row.appendChild(info);
    row.appendChild(btnInstall);
    c.appendChild(row);
  }).finally(function() { if (btn) setBusy(btn, false); });
}

function checkBeta(btn) {
  if (btn) setBusy(btn, true, 'Поиск beta...');
  call('check_updates_beta').then(function(r) {
    var c = document.getElementById('updates-list');
    c.innerHTML = '';
    if (!r || !r.ok || !r.hasUpdate) {
      c.innerHTML = '<div class="hint">Бета не найдена или URL не настроен.</div>';
      return;
    }
    var row = document.createElement('div');
    row.className = 'list-row';
    var info = document.createElement('div');
    info.className = 'info';
    var title = document.createElement('div');
    title.className = 'title';
    title.textContent = 'Beta ' + r.version;
    var sub = document.createElement('div');
    sub.className = 'sub';
    sub.textContent = r.url;
    info.appendChild(title);
    info.appendChild(sub);
    var btnInstall = document.createElement('button');
    btnInstall.className = 'danger';
    btnInstall.textContent = 'Установить Beta';
    btnInstall.addEventListener('click', function() { applyUpdate(btnInstall, r.url); });
    row.appendChild(info);
    row.appendChild(btnInstall);
    c.appendChild(row);
  }).finally(function() { if (btn) setBusy(btn, false); });
}

function applyUpdate(btn, url) {
  if (!confirm('Скачать и обновить программу?\nПрограмма перезапустится.')) return;
  setBusy(btn, true, 'Загрузка...');
  call('download_update', url).then(function(r) {
    if (r && r.ok) {
      toast('Обновление запущено. Программа закроется.', 'success');
    } else {
      toast('Ошибка: ' + (r && r.message ? r.message : 'unknown'), 'error');
    }
  }).finally(function() { setBusy(btn, false); });
}

function loadAiConfig() {
  call('ai_get_config').then(function(r) {
    if (!r) return;
    var ep = document.getElementById('ai-endpoint');
    var md = document.getElementById('ai-model');
    var ky = document.getElementById('ai-key');
    var lc = document.getElementById('ai-local-chk');
    if (ep) ep.value = r.endpoint || '';
    if (md) md.value = r.model || '';
    if (ky) ky.placeholder = r.hasKey ? '(сохранён)' : 'sk-...';
    if (lc) lc.checked = !!r.useLocal;
  });
  call('beta_get_url').then(function(r) {
    var bu = document.getElementById('beta-url-input');
    if (bu && r) bu.value = r.url || '';
  });
}

function saveAiConfig(btn) {
  var ep = document.getElementById('ai-endpoint').value.trim();
  var key = document.getElementById('ai-key').value.trim();
  var model = document.getElementById('ai-model').value.trim();
  var useLocal = document.getElementById('ai-local-chk').checked;
  if (!ep) { toast('Введите endpoint URL', 'error'); return; }
  if (!useLocal && !key) { toast('Введите API Key', 'error'); return; }
  if (!model) { toast('Введите модель', 'error'); return; }
  setBusy(btn, true, 'Сохранение...');
  call('ai_set_config', ep, key, model, useLocal ? '1' : '0').then(function(r) {
    toast(r && r.ok ? 'API сохранён' : 'Ошибка сохранения', r && r.ok ? 'success' : 'error');
    document.getElementById('ai-key').value = '';
    document.getElementById('ai-key').placeholder = '(сохранён)';
  }).finally(function() { setBusy(btn, false); });
}

function saveBetaUrl(btn) {
  var url = document.getElementById('beta-url-input').value.trim();
  call('beta_set_url', url).then(function(r) {
    toast(r && r.ok ? 'URL сохранён' : 'Ошибка', r && r.ok ? 'success' : 'error');
  });
}

function sendAiMessage(btn) {
  var input = document.getElementById('ai-user-input');
  var msg = input.value.trim();
  if (!msg) return;
  input.value = '';
  var chat = document.getElementById('ai-chat-messages');
  chat.innerHTML += '<div style="margin-bottom:8px;color:var(--accent2)"><b>Вы:</b> ' + escapeHtml(msg) + '</div>';
  chat.scrollTop = chat.scrollHeight;
  setBusy(btn, true, 'AI думает...');
  call('ai_chat', msg).then(function(r) {
    if (!r) { chat.innerHTML += '<div style="margin-bottom:8px;color:var(--error)"><b>AI:</b> пустой ответ</div>'; return; }
    var reply = r.reply || r.message || '';
    if (!reply && r.ok === false) reply = r.message || 'Ошибка';
    chat.innerHTML += '<div style="margin-bottom:8px;color:var(--text)"><b>AI:</b> ' + escapeHtml(reply) + '</div>';
    if (r.executed && r.executed.length) {
      chat.innerHTML += '<div style="margin-bottom:8px;color:var(--muted);font-size:13px"><b>Выполнено:</b><br>' + r.executed.map(function(x) { return '• ' + escapeHtml(x); }).join('<br>') + '</div>';
    }
    chat.scrollTop = chat.scrollHeight;
    pollStats();
  }).catch(function(err) {
    chat.innerHTML += '<div style="margin-bottom:8px;color:var(--error)"><b>AI:</b> Ошибка запроса: ' + escapeHtml(String(err)) + '</div>';
    chat.scrollTop = chat.scrollHeight;
  }).finally(function() { setBusy(btn, false); });
}

function loadDevCodeStatus() {
  call('dev_code_status').then(function(r) {
    var c = document.getElementById('dev-code-status');
    if (!c) return;
    if (r && r.betaAccess) {
      c.innerHTML = '<div style="color:var(--success);font-weight:700;">✅ Бета-доступ активирован. Проверьте вкладку «Обновления» для установки бета-версий.</div>';
      var betaBtn = document.getElementById('btn-check-beta');
      if (betaBtn) betaBtn.style.display = 'inline-flex';
    } else {
      c.innerHTML = '<div style="color:var(--muted);">Бета-доступ не активирован.</div>';
      var betaBtn = document.getElementById('btn-check-beta');
      if (betaBtn) betaBtn.style.display = 'none';
    }
  }).catch(function(){});
}

function submitDevCode(btn) {
  var input = document.getElementById('dev-code-input');
  var code = input.value.trim();
  if (!code) { toast('Введите код разработчика', 'error'); return; }
  setBusy(btn, true, 'Проверка...');
  call('dev_code_validate', code).then(function(r) {
    if (r && r.ok) {
      toast('Бета-доступ активирован!', 'success');
      input.value = '';
      loadDevCodeStatus();
    } else {
      toast('Неверный код разработчика', 'error');
    }
  }).finally(function() { setBusy(btn, false); });
}

function loadHistory() {
  call('get_history').then(function(r) {
    var c = document.getElementById('history-list');
    if (!c) return;
    c.innerHTML = '';
    if (!r || !r.items || !r.items.length) {
      c.innerHTML = '<div class="hint">История пуста.</div>';
      return;
    }
    r.items.forEach(function(h) {
      var row = document.createElement('div');
      row.className = 'list-row';
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = h.action;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = h.time + ' | ' + h.details;
      info.appendChild(title);
      info.appendChild(sub);
      row.appendChild(info);
      c.appendChild(row);
    });
  });
  call('rollback_list').then(function(r) {
    var c = document.getElementById('rollback-list');
    if (!c) return;
    c.innerHTML = '';
    if (!r || !r.items || !r.items.length) {
      c.innerHTML = '<div class="hint">Нет сохранённых изменений для отката.</div>';
      return;
    }
    r.items.forEach(function(h) {
      var row = document.createElement('div');
      row.className = 'list-row';
      var info = document.createElement('div');
      info.className = 'info';
      var title = document.createElement('div');
      title.className = 'title';
      title.textContent = h.description;
      var sub = document.createElement('div');
      sub.className = 'sub';
      sub.textContent = '[' + h.type + '] ' + h.target + ' • ' + h.oldValue + ' → ' + h.newValue;
      info.appendChild(title);
      info.appendChild(sub);
      var rb = document.createElement('button');
      rb.className = 'secondary';
      rb.textContent = '↶ Откатить';
      rb.addEventListener('click', function() {
        call('rollback_apply', h.id).then(function(res) {
          toast(res && res.ok ? 'Откат выполнен' : 'Ошибка отката', res && res.ok ? 'success' : 'error');
          loadHistory();
        });
      });
      row.appendChild(info);
      row.appendChild(rb);
      c.appendChild(row);
    });
  });
}

function rollbackLast(btn) {
  setBusy(btn, true, '...');
  call('rollback_last').then(function(r) {
    toast(r && r.ok ? 'Последнее действие откачено' : 'Ошибка отката', r && r.ok ? 'success' : 'error');
    loadHistory();
  }).finally(function() { setBusy(btn, false); });
}

function applyProfile(btn, name) {
  if (!confirm('Применить профиль ' + name + '?')) return;
  setBusy(btn, true, 'Применение...');
  call('apply_profile', name).then(function(r) {
    toast(r && r.ok ? 'Профиль применён' : 'Ошибка', r && r.ok ? 'success' : 'error');
  }).finally(function() { setBusy(btn, false); });
}

document.getElementById('btn-check-updates').addEventListener('click', function() { loadUpdates(this); });
document.getElementById('btn-check-beta').addEventListener('click', function() { checkBeta(this); });
document.getElementById('lang-select').addEventListener('change', function(e) { setLang(e.target.value); });
document.getElementById('drive-select').addEventListener('change', function(e) { setDrive(e.target.value); });
document.getElementById('theme-select').addEventListener('change', function(e) { setTheme(e.target.value); });
document.getElementById('dry-run-chk').addEventListener('change', function(e) {
  call('set_dry_run', e.target.checked ? '1' : '0');
  toast('Режим только чтения: ' + (e.target.checked ? 'ВКЛ' : 'ВЫКЛ'), 'success');
});

document.getElementById('btn-qa-temp').addEventListener('click', function() { quickAction(this, 'cleanup_temp'); });
document.getElementById('btn-qa-dns').addEventListener('click', function() { quickAction(this, 'flush_dns'); });
document.getElementById('btn-qa-ram').addEventListener('click', function() { quickAction(this, 'defrag_memory'); });
document.getElementById('btn-qa-standby').addEventListener('click', function() { quickAction(this, 'clear_standby'); });
document.getElementById('btn-qa-maintenance').addEventListener('click', function() { quickAction(this, 'one_click_maintenance'); });

document.getElementById('btn-smart-scan').addEventListener('click', function() { loadSmartScan(this); });
document.getElementById('btn-smart-apply').addEventListener('click', function() { applySmart(this); });

document.getElementById('btn-autostart-refresh').addEventListener('click', function() { loadAutostart(this); });
document.getElementById('btn-autostart-impact').addEventListener('click', function() { analyzeBootImpact(this); });

document.getElementById('btn-freeze-refresh').addEventListener('click', function() { loadFreezeList(); });
document.getElementById('btn-freeze-all').addEventListener('click', function() { freezeSelected(); });
document.getElementById('btn-unfreeze-all').addEventListener('click', function() { unfreezeAll(); });

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
document.getElementById('btn-bench-capture').addEventListener('click', function() { benchCapture(this); });
document.getElementById('btn-bench-compare').addEventListener('click', function() { benchCompare(this); });

document.getElementById('btn-scan-drive').addEventListener('click', function() { scanDriveAsync(this); });
document.getElementById('btn-load-programs-detailed').addEventListener('click', function() { loadProgramsDetailed(this); });
document.getElementById('btn-bulk-uninstall').addEventListener('click', function() { uninstallSelected(this); });
document.getElementById('btn-scan-leftovers').addEventListener('click', function() { scanLeftovers(this); });
document.getElementById('btn-load-security').addEventListener('click', function() { loadSecurityStatus(this); });
document.getElementById('btn-lite-debloat').addEventListener('click', function() { runLiteDebloat(this); });
document.getElementById('btn-uwp-refresh').addEventListener('click', function() { loadUwpList(); });
document.getElementById('btn-uwp-apply').addEventListener('click', function() { applyUwpRemoval(this); });
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
document.getElementById('btn-load-ctx-menu').addEventListener('click', function() { quickAction(this, 'ctx_menu_classic'); });
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
document.getElementById('btn-sched-daily').addEventListener('click', function() { call('sched_create', 'daily').then(function(r){ toast(r&&r.ok?'Задание создано':'Ошибка', r&&r.ok?'success':'error'); }); });
document.getElementById('btn-sched-weekly').addEventListener('click', function() { call('sched_create', 'weekly').then(function(r){ toast(r&&r.ok?'Задание создано':'Ошибка', r&&r.ok?'success':'error'); }); });
document.getElementById('btn-sched-onidle').addEventListener('click', function() { call('sched_create', 'onidle').then(function(r){ toast(r&&r.ok?'Задание создано':'Ошибка', r&&r.ok?'success':'error'); }); });
document.getElementById('btn-sched-delete').addEventListener('click', function() { call('sched_delete').then(function(r){ toast(r&&r.ok?'Задание удалено':'Ошибка', r&&r.ok?'success':'error'); }); });
document.getElementById('btn-ping-host').addEventListener('click', function() { pingHost(this); });
document.getElementById('btn-reset-network-2').addEventListener('click', function() { resetNetwork(this); });
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
document.getElementById('btn-ai-save-config').addEventListener('click', function() { saveAiConfig(this); });
document.getElementById('btn-ai-load-config').addEventListener('click', function() { loadAiConfig(); });
document.getElementById('btn-ai-send').addEventListener('click', function() { sendAiMessage(this); });
document.getElementById('ai-user-input').addEventListener('keypress', function(e) {
  if (e.key === 'Enter') sendAiMessage(document.getElementById('btn-ai-send'));
});
document.getElementById('btn-dev-code-submit').addEventListener('click', function() { submitDevCode(this); });
document.getElementById('dev-code-input').addEventListener('keypress', function(e) {
  if (e.key === 'Enter') submitDevCode(document.getElementById('btn-dev-code-submit'));
});
document.getElementById('btn-save-beta-url').addEventListener('click', function() { saveBetaUrl(this); });
document.getElementById('btn-profile-gaming').addEventListener('click', function() { applyProfile(this, 'gaming'); });
document.getElementById('btn-profile-work').addEventListener('click', function() { applyProfile(this, 'work'); });
document.getElementById('btn-profile-laptop').addEventListener('click', function() { applyProfile(this, 'laptop'); });
document.getElementById('btn-profile-safe').addEventListener('click', function() { applyProfile(this, 'safe'); });
document.getElementById('btn-rollback-last').addEventListener('click', function() { rollbackLast(this); });
document.getElementById('btn-rollback-refresh').addEventListener('click', function() { loadHistory(); });

document.getElementById('btn-sde-install').addEventListener('click', function() {
  setBusy(this, true, 'Установка SDE...');
  call('install_sde').then(function(r) {
    toast(r && r.ok ? 'SDE установлен' : 'Ошибка установки', r && r.ok ? 'success' : 'error');
    loadSdeStatus();
  }).finally(function() { setBusy(this, false); });
});
document.getElementById('btn-sde-remove').addEventListener('click', function() {
  setBusy(this, true, 'Удаление SDE...');
  call('remove_sde').then(function(r) {
    toast(r && r.ok ? 'SDE удалён' : 'Ошибка удаления', r && r.ok ? 'success' : 'error');
    loadSdeStatus();
  }).finally(function() { setBusy(this, false); });
});
document.getElementById('btn-sde-launch').addEventListener('click', function() {
  var path = document.getElementById('sde-target-path').value.trim();
  if (!path) { toast('Укажите путь к приложению', 'error'); return; }
  call('launch_sde', path).then(function(r) {
    toast(r && r.ok ? 'Приложение запущено через SDE' : 'Ошибка запуска', r && r.ok ? 'success' : 'error');
  });
});

document.getElementById('btn-scan-drivers').addEventListener('click', function() { scanDrivers(); });
document.getElementById('btn-update-all-drivers').addEventListener('click', function() {
  call('update_all_drivers').then(function(r) {
    toast(r && r.ok ? 'Обновление запущено' : 'Ошибка', r && r.ok ? 'success' : 'error');
    scanDrivers();
  });
});

function loadSdeStatus() {
  call('sde_status').then(function(r) {
    var c = document.getElementById('sde-status');
    if (!c) return;
    if (r && r.installed) {
      c.innerHTML = '<div class="success">✅ SDE установлен. Путь: ' + escapeHtml(r.path) + '</div>';
    } else {
      c.innerHTML = '<div class="hint">SDE не установлен. Нажмите «Установить SDE».</div>';
    }
  });
}

function boot() {
  if (!initBridge()) {
    if (++bootTries < 100) { setTimeout(boot, 100); return; }
    return;
  }
  setLang(store.get('optpro-lang', 'ru'));
  var savedTheme = store.get('optpro-theme', 'midnight');
  var sel = document.getElementById('theme-select');
  if (sel) sel.value = savedTheme;
  setTheme(savedTheme);
  loadDrives();
  loadSettingsFromServer();
  loadAiConfig();
  loadDevCodeStatus();
  setInterval(pollStats, 2000);
  pollStats();
  loadCpuFeatures();
  setTimeout(initChart, 200);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
else boot();
</script></body></html>)HTML";

struct ProcessInfo {
    std::wstring name;
    DWORD pid;
};

static std::set<std::wstring> g_frozenProcessNames;
static std::mutex g_freezeMutex;

typedef NTSTATUS(NTAPI* pfnNtSuspendProcess)(HANDLE ProcessHandle);
typedef NTSTATUS(NTAPI* pfnNtResumeProcess)(HANDLE ProcessHandle);

static std::vector<std::wstring> GetFrozenProcesses() {
    std::lock_guard<std::mutex> lk(g_freezeMutex);
    return std::vector<std::wstring>(g_frozenProcessNames.begin(), g_frozenProcessNames.end());
}

static std::vector<ProcessInfo> ListProcesses() {
    std::vector<ProcessInfo> res;
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return res;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (pe.th32ProcessID <= 4 || IsCriticalProcess(pe.szExeFile)) continue;
            res.push_back({ pe.szExeFile, pe.th32ProcessID });
        } while (Process32NextW(snap.get(), &pe));
    }
    return res;
}

static int FreezeProcesses(const std::vector<DWORD>& pids) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return 0;
    auto NtSuspendProcess = (pfnNtSuspendProcess)GetProcAddress(ntdll, "NtSuspendProcess");
    if (!NtSuspendProcess) return 0;
    int count = 0;
    EnablePrivilege(SE_DEBUG_NAME);
    for (DWORD pid : pids) {
        UniqueHandle h(OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_INFORMATION, FALSE, pid));
        if (h) {
            if (NT_SUCCESS(NtSuspendProcess(h.get()))) {
                wchar_t name[MAX_PATH] = {};
                DWORD sz = MAX_PATH;
                if (QueryFullProcessImageNameW(h.get(), 0, name, &sz)) {
                    std::wstring exe = fs::path(name).filename().wstring();
                    std::lock_guard<std::mutex> lk(g_freezeMutex);
                    g_frozenProcessNames.insert(exe);
                }
                count++;
            }
        }
    }
    return count;
}

static void UnfreezeAll() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    auto NtResumeProcess = (pfnNtResumeProcess)GetProcAddress(ntdll, "NtResumeProcess");
    if (!NtResumeProcess) return;
    std::set<std::wstring> toResume;
    {
        std::lock_guard<std::mutex> lk(g_freezeMutex);
        toResume = g_frozenProcessNames;
        g_frozenProcessNames.clear();
    }
    UniqueHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap) return;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap.get(), &pe)) {
        do {
            if (toResume.count(pe.szExeFile)) {
                UniqueHandle h(OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pe.th32ProcessID));
                if (h) NtResumeProcess(h.get());
            }
        } while (Process32NextW(snap.get(), &pe));
    }
}

struct DriverInfo {
    std::wstring name;
    std::wstring version;
    std::wstring deviceId;
};

static std::vector<DriverInfo> ScanDrivers() {
    std::vector<DriverInfo> result;
    CoInitScope com;
    WmiConnection wmi;
    if (wmi.Open(L"ROOT\\CIMV2")) {
        ComPtr<IEnumWbemClassObject> en;
        HRESULT hr = wmi.get()->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT DeviceName, DriverVersion, DeviceID FROM Win32_PnPSignedDriver WHERE DriverVersion IS NOT NULL AND DeviceName IS NOT NULL"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
        if (SUCCEEDED(hr) && en) {
            while (true) {
                ComPtr<IWbemClassObject> obj;
                ULONG ret = 0;
                hr = en->Next(1000, 1, &obj, &ret);
                if (FAILED(hr) || ret == 0 || !obj) break;
                VARIANT vName, vVer, vId;
                VariantInit(&vName); VariantInit(&vVer); VariantInit(&vId);
                DriverInfo d;
                if (SUCCEEDED(obj->Get(L"DeviceName", 0, &vName, nullptr, nullptr)) && vName.vt == VT_BSTR) d.name = vName.bstrVal;
                if (SUCCEEDED(obj->Get(L"DriverVersion", 0, &vVer, nullptr, nullptr)) && vVer.vt == VT_BSTR) d.version = vVer.bstrVal;
                if (SUCCEEDED(obj->Get(L"DeviceID", 0, &vId, nullptr, nullptr)) && vId.vt == VT_BSTR) d.deviceId = vId.bstrVal;
                if (!d.name.empty()) result.push_back(d);
                VariantClear(&vName); VariantClear(&vVer); VariantClear(&vId);
            }
        }
    }
    return result;
}

static bool UpdateDriver(const std::wstring& deviceId) {
    if (deviceId.empty()) return false;
    std::wstring cmd = L"pnputil /scan-devices";
    RunCmd(cmd, L"", false);
    return true;
}

static bool UpdateAllDrivers() {
    std::wstring cmd = L"powershell -NoProfile -Command \"Get-PnpDevice -Class * | Where-Object { $_.Status -eq 'OK' } | Update-PnpDevice -Confirm:$false -ErrorAction SilentlyContinue\"";
    RunCmd(cmd, L"Updating all drivers", false, 600);
    return true;
}

static bool InstallSde() {
    std::error_code ec;
    fs::create_directories(g_paths.SdeDir, ec);
    std::wstring url = L"https://downloadmirror.intel.com/813591/sde-external-9.33.0-2024-01-07-win.tar.xz";
    std::wstring archivePath = g_paths.SdeDir + L"\\sde.tar.xz";
    std::wstring err;
    Print(L"[SDE] Скачивание...");
    if (HttpDownloadFile(url, archivePath, err).empty()) {
        PrintErr(L"[SDE] Ошибка скачивания: " + err);
        return false;
    }
    Print(L"[SDE] Распаковка...");
    std::wstring cmd = L"tar -xf \"" + archivePath + L"\" -C \"" + g_paths.SdeDir + L"\"";
    CmdResult r = RunCmdImpl(cmd, 300);
    if (r.exitCode != 0) {
        PrintErr(L"[SDE] Ошибка распаковки");
        return false;
    }
    return true;
}

static bool RemoveSde() {
    std::error_code ec;
    fs::remove_all(g_paths.SdeDir, ec);
    fs::create_directories(g_paths.SdeDir, ec);
    return !ec;
}

static bool LaunchThroughSde(const std::wstring& targetPath) {
    std::wstring sdeExe = g_paths.SdeDir + L"\\sde.exe";
    if (!fs::exists(sdeExe)) {
        std::error_code ec;
        for (auto& p : fs::recursive_directory_iterator(g_paths.SdeDir, ec)) {
            if (!ec && p.is_regular_file() && p.path().filename() == L"sde.exe") {
                sdeExe = p.path().wstring();
                break;
            }
        }
    }
    if (!fs::exists(sdeExe)) return false;
    std::wstring cmd = L"\"" + sdeExe + L"\" -- \"" + targetPath + L"\"";
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

class App : public std::enable_shared_from_this<App> {
public:
    HWND hwnd = nullptr;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    std::atomic<bool> shuttingDown{ false };
    std::atomic<int> activeMessages{ 0 };

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

        std::wstring indexPath = g_paths.UiDir + L"\\index.html";
        std::string htmlUtf8 = WideToUtf8(HTML_UI);
        std::ofstream f(indexPath.c_str(), std::ios::binary);
        if (f) {
            f.write(htmlUtf8.data(), htmlUtf8.size());
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

                                ComPtr<ICoreWebView2Settings> settings;
                                if (SUCCEEDED(webview->get_Settings(&settings))) {
                                    settings->put_AreDevToolsEnabled(FALSE);
                                    settings->put_AreDefaultContextMenusEnabled(FALSE);
                                    settings->put_AreHostObjectsAllowed(FALSE);
                                    settings->put_IsWebMessageEnabled(TRUE);
                                }

                                ComPtr<ICoreWebView2_3> webview3;
                                if (SUCCEEDED(webview.As(&webview3))) {
                                    webview3->SetVirtualHostNameToFolderMapping(L"optpro.local", g_paths.UiDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                }

                                webview->add_NavigationStarting(
                                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                            LPWSTR uri = nullptr;
                                            args->get_Uri(&uri);
                                            if (uri) {
                                                std::wstring url(uri);
                                                CoTaskMemFree(uri);
                                                if (url.find(L"https://optpro.local") != 0) {
                                                    args->put_Cancel(TRUE);
                                                }
                                            }
                                            return S_OK;
                                        }).Get(), nullptr);

                                webview->add_NewWindowRequested(
                                    Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                            args->put_Handled(TRUE);
                                            return S_OK;
                                        }).Get(), nullptr);

                                ComPtr<ICoreWebView2_4> webview4;
                                if (SUCCEEDED(webview.As(&webview4))) {
                                    webview4->add_DownloadStarting(
                                        Callback<ICoreWebView2DownloadStartingEventHandler>(
                                            [](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                                                args->put_Cancel(TRUE);
                                                return S_OK;
                                            }).Get(), nullptr);
                                }

                                webview->add_ProcessFailed(
                                    Callback<ICoreWebView2ProcessFailedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                                            COREWEBVIEW2_PROCESS_FAILED_KIND kind;
                                            args->get_ProcessFailedKind(&kind);
                                            if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED) {
                                                MessageBoxW(hwnd, L"WebView2 process crashed. Restarting...", L"Warning", MB_ICONWARNING);
                                            }
                                            return S_OK;
                                        }).Get(), nullptr);

                                RECT rc;
                                GetClientRect(hwnd, &rc);
                                controller->put_Bounds(rc);
                                webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR msg = nullptr;
                                            HRESULT mh = args->TryGetWebMessageAsString(&msg);
                                            if (SUCCEEDED(mh) && msg) {
                                                std::wstring s(msg);
                                                CoTaskMemFree(msg);
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
                        (void)error; (void)isSuccessful;
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

    void Resize(int w, int h) {
        if (w > 0 && h > 0 && controller) {
            RECT rc = { 0, 0, w, h };
            controller->put_Bounds(rc);
        }
    }
    void Send(const std::wstring& msg) {
        if (webview && !shuttingDown) webview->PostWebMessageAsString(msg.c_str());
    }
    void SendLog(const std::wstring& s) { Send(L"L\x1F" + s); }
    void SendProgress(int p, const std::wstring& msg) { Send(L"P\x1F" + std::to_wstring(p) + L"\x1F" + msg); }
    void SendResp(int id, const std::wstring& json) { Send(L"R\x1F" + std::to_wstring(id) + L"\x1F" + json); }

    void Shutdown() {
        shuttingDown = true;
        { std::lock_guard<std::mutex> lk(g_sinkMutex); g_uiLogSink = nullptr; g_uiProgressSink = nullptr; }
        if (controller) {
            controller->Close();
            controller = nullptr;
        }
        webview = nullptr;
    }

    void HandleWinCtrl(const std::wstring& act) const {
        if (!hwnd) return;
        if (act == L"min") ShowWindow(hwnd, SW_MINIMIZE);
        else if (act == L"max") {
            if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
            else ShowWindow(hwnd, SW_MAXIMIZE);
        }
        else if (act == L"close") PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    void HandleDrag() const {
        if (hwnd) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    }
    void HandleResize(const std::wstring& dir) const {
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
        g_activeAsyncTasks++;
        std::thread([self, id, fn]() {
            try {
                std::wstring result = fn();
                self->PostUI([self, id, result]() { if (!self->shuttingDown) self->SendResp(id, result); });
            }
            catch (const std::exception& e) {
                std::wstring err = ErrCode(L"ERR_EXCEPTION", Utf8ToWide(e.what()));
                self->PostUI([self, id, err]() { if (!self->shuttingDown) self->SendResp(id, err); });
            }
            catch (...) {
                self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, ErrCode(L"ERR_UNKNOWN", L"Unknown exception")); });
            }
            g_activeAsyncTasks--;
            g_asyncCv.notify_all();
            }).detach();
    }

    void OnMessage(const std::wstring& raw) {
        int id = -1;
        try {
            if (raw.size() > MAX_IPC_MESSAGE_SIZE) {
                SendResp(-1, ErrCode(L"ERR_MESSAGE_TOO_LARGE", L"IPC message exceeds maximum size"));
                return;
            }
            if (raw.size() >= 3 && raw[0] == L'W' && raw[1] == L'\x1F') {
                std::wstring payload = raw.substr(2);
                if (payload.size() > 256) return;
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
            for (wchar_t c : raw) {
                if (c == L'\x1F') { parts.push_back(cur); cur.clear(); }
                else cur += c;
            }
            parts.push_back(cur);
            if (parts.size() < 4) return;
            if (parts.size() > MAX_ARGS_COUNT + 8) {
                SendResp(-1, ErrCode(L"ERR_TOO_MANY_ARGS", L"Too many arguments"));
                return;
            }
            for (const auto& p : parts) {
                if (p.size() > MAX_ARG_LENGTH) {
                    SendResp(-1, ErrCode(L"ERR_ARG_TOO_LARGE", L"Argument too large"));
                    return;
                }
            }
            id = SafeStoi(parts[1], -1);
            if (id < 0) return;
            const std::wstring& m = parts[2];
            auto arg = [&](size_t i) -> std::wstring { return i < parts.size() ? parts[i] : L""; };
            size_t nargs = parts.size() >= 4 ? (size_t)SafeStoi(parts[3], 0) : 0;

            auto validateArgs = [&](size_t expected, const wchar_t* method) -> bool {
                if (nargs < expected) {
                    SendResp(id, ErrCode(L"ERR_BAD_ARGS", std::wstring(method) + L": missing args"));
                    return false;
                }
                return true;
                };

            if (m == L"get_status") {
                MEMORYSTATUSEX ms = { sizeof(ms) };
                GlobalMemoryStatusEx(&ms);
                std::wstring sys;
                { std::lock_guard<std::mutex> lk(g_pcInfoMutex); sys = g_pcInfoJson; }
                std::wstring disks;
                { std::lock_guard<std::mutex> lk(g_diskHealthMutex); disks = g_diskHealthJson; }
                std::wstring j = L"{\"version\":\"" + std::wstring(APP_VERSION) + L"\",\"cpu\":" + FloatToJsonStr(g_cpuUsage.load()) + L",\"gpu\":" + FloatToJsonStr(g_gpuUsage.load()) +
                    L",\"ram_free\":" + std::to_wstring(ms.ullAvailPhys / (1024 * 1024)) + L",\"procs\":" + std::to_wstring(g_procCount.load()) +
                    L",\"temp\":" + FloatToJsonStr(g_cpuTemp.load()) + L",\"gpuTemp\":" + FloatToJsonStr(g_gpuTemp.load()) +
                    L",\"gpuFan\":" + FloatToJsonStr(g_gpuFan.load()) +
                    L",\"vramUsed\":" + std::to_wstring(g_vramUsedMB.load()) + L",\"vramTotal\":" + std::to_wstring(g_vramTotalMB.load()) +
                    L",\"throttle\":" + std::wstring(g_throttling.load() ? L"true" : L"false") +
                    L",\"ping\":" + std::to_wstring(g_pingMs.load()) +
                    L",\"battery\":" + FloatToJsonStr(g_batteryPercent.load()) +
                    L",\"ac\":" + std::wstring(g_isAcConnected.load() ? L"true" : L"false") +
                    L",\"dpc\":" + FloatToJsonStr(g_dpcIsrTime.load()) +
                    L",\"appCpu\":" + FloatToJsonStr(g_appCpuUsage.load()) +
                    L",\"appGpu\":" + FloatToJsonStr(g_appGpuUsage.load()) + L",\"appRam\":" + std::to_wstring(g_appRamMB.load()) +
                    L",\"net_down\":" + FloatToJsonStr(g_netDownMbps.load()) + L",\"net_up\":" + FloatToJsonStr(g_netUpMbps.load()) +
                    L",\"isBeta\":" + std::wstring(g_isBetaBuild.load() ? L"true" : L"false") +
                    L",\"betaAccess\":" + std::wstring(g_betaAccessEnabled.load() ? L"true" : L"false") +
                    L",\"dryRun\":" + std::wstring(g_dryRunMode.load() ? L"true" : L"false") +
                    L",\"sys\":" + sys + L",\"disks\":" + disks + L"}";
                SendResp(id, j);
                return;
            }
            if (m == L"set_dry_run") {
                if (!validateArgs(1, L"set_dry_run")) return;
                g_dryRunMode = (arg(4) == L"1");
                SendResp(id, OkJson());
                return;
            }
            if (m == L"dev_code_validate") {
                if (!validateArgs(1, L"dev_code_validate")) return;
                std::wstring code = arg(4);
                bool ok = ValidateDeveloperCode(code);
                if (ok) {
                    g_betaAccessEnabled = true;
                    SaveDevAccess(true);
                    WriteAuditLog(L"DevCodeUnlock", L"Beta access granted via developer code");
                }
                SendResp(id, ok ? L"{\"ok\":true,\"betaAccess\":true}" : L"{\"ok\":false,\"betaAccess\":false}");
                return;
            }
            if (m == L"dev_code_status") {
                SendResp(id, L"{\"betaAccess\":" + std::wstring(g_betaAccessEnabled.load() ? L"true" : L"false") + L"}");
                return;
            }
            if (m == L"beta_get_url") {
                SendResp(id, L"{\"url\":" + JsonStr(g_betaUpdateUrl) + L"}");
                return;
            }
            if (m == L"beta_set_url") {
                if (!validateArgs(1, L"beta_set_url")) return;
                g_betaUpdateUrl = arg(4);
                SendResp(id, OkJson());
                return;
            }
            if (m == L"check_updates_github") {
                return AsyncData(id, []() -> std::wstring {
                    std::wstring err;
                    std::wstring json = HttpGet(L"https://api.github.com/repos/ligqw/ZeroBloat/releases/latest", 15, err);
                    if (json.empty() || !err.empty()) {
                        return L"{\"ok\":false,\"hasUpdate\":false,\"err\":" + JsonStr(err) + L"}";
                    }
                    auto pos = json.find(L"\"tag_name\":\"");
                    if (pos == std::wstring::npos) return L"{\"ok\":false,\"hasUpdate\":false}";
                    pos += 12;
                    auto end = json.find(L"\"", pos);
                    if (end == std::wstring::npos) return L"{\"ok\":false,\"hasUpdate\":false}";
                    std::wstring version = json.substr(pos, end - pos);
                    if (version == APP_VERSION) {
                        return L"{\"ok\":true,\"hasUpdate\":false}";
                    }
                    auto urlPos = json.find(L"\"browser_download_url\":\"");
                    if (urlPos == std::wstring::npos) return L"{\"ok\":true,\"hasUpdate\":false}";
                    urlPos += 24;
                    auto urlEnd = json.find(L"\"", urlPos);
                    if (urlEnd == std::wstring::npos) return L"{\"ok\":true,\"hasUpdate\":false}";
                    std::wstring downloadUrl = json.substr(urlPos, urlEnd - urlPos);
                    if (downloadUrl.find(L".exe") == std::wstring::npos) {
                        return L"{\"ok\":true,\"hasUpdate\":false,\"version\":" + JsonStr(version) + L",\"err\":\"No .exe asset\"}";
                    }
                    return L"{\"ok\":true,\"hasUpdate\":true,\"version\":" + JsonStr(version) + L",\"url\":" + JsonStr(downloadUrl) + L",\"assetName\":\"ZeroBloat.exe\"}";
                    });
            }
            if (m == L"check_updates_beta") {
                if (g_betaUpdateUrl.empty()) {
                    SendResp(id, L"{\"ok\":false,\"hasUpdate\":false,\"err\":\"Beta URL not set\"}");
                    return;
                }
                return AsyncData(id, []() -> std::wstring {
                    std::wstring err;
                    std::wstring json = HttpGet(g_betaUpdateUrl, 10, err);
                    if (json.empty() || !err.empty()) {
                        return L"{\"ok\":false,\"hasUpdate\":false,\"err\":" + JsonStr(err) + L"}";
                    }
                    auto pos = json.find(L"\"version\":\"");
                    if (pos == std::wstring::npos) return L"{\"ok\":false,\"hasUpdate\":false}";
                    pos += 11;
                    auto end = json.find(L"\"", pos);
                    if (end == std::wstring::npos) return L"{\"ok\":false,\"hasUpdate\":false}";
                    std::wstring version = json.substr(pos, end - pos);
                    auto urlPos = json.find(L"\"url\":\"");
                    if (urlPos == std::wstring::npos) return L"{\"ok\":true,\"hasUpdate\":false}";
                    urlPos += 7;
                    auto urlEnd = json.find(L"\"", urlPos);
                    if (urlEnd == std::wstring::npos) return L"{\"ok\":true,\"hasUpdate\":false}";
                    std::wstring downloadUrl = json.substr(urlPos, urlEnd - urlPos);
                    return L"{\"ok\":true,\"hasUpdate\":true,\"version\":" + JsonStr(version) + L",\"url\":" + JsonStr(downloadUrl) + L"}";
                    });
            }
            if (m == L"download_update") {
                if (!validateArgs(1, L"download_update")) return;
                std::wstring url = arg(4);
                return AsyncData(id, [url]() -> std::wstring {
                    std::wstring tmpPath = g_paths.AppDir + L"\\update_new.exe";
                    std::wstring err;
                    if (HttpDownloadFile(url, tmpPath, err).empty()) {
                        return ErrCode(L"ERR_DOWNLOAD", err);
                    }
                    if (!VerifySignature(tmpPath)) {
                        DeleteFileW(tmpPath.c_str());
                        return ErrCode(L"ERR_BAD_SIGNATURE", L"Signature verification failed");
                    }
                    std::wstring batPath = g_paths.AppDir + L"\\update.bat";
                    std::wstring exePath = g_paths.AppDir + L"\\ZeroBloat.exe";
                    std::wstring cmd = L"@echo off\r\n"
                        L"timeout /t 2 /nobreak >nul\r\n"
                        L"copy /y \"" + tmpPath + L"\" \"" + exePath + L"\"\r\n"
                        L"start \"\" \"" + exePath + L"\"\r\n"
                        L"del \"%~f0\"\r\n";
                    std::ofstream f(batPath);
                    if (f) {
                        std::string u = WideToUtf8(cmd);
                        f.write(u.data(), u.size());
                        f.close();
                        ShellExecuteW(nullptr, L"open", batPath.c_str(), nullptr, nullptr, SW_HIDE);
                        PostMessageW(GetActiveWindow(), WM_CLOSE, 0, 0);
                        return OkJson();
                    }
                    return ErrCode(L"ERR_SCRIPT", L"Failed to create update script");
                    });
            }
            if (m == L"rollback_list") {
                SendResp(id, L"{\"items\":" + BuildRollbackListJson() + L"}");
                return;
            }
            if (m == L"rollback_apply") {
                if (!validateArgs(1, L"rollback_apply")) return;
                std::wstring rid = arg(4);
                bool ok = false;
                std::wstring err = L"Rollback entry not found";
                {
                    std::lock_guard<std::mutex> lk(g_rollbackMutex);
                    for (size_t i = 0; i < g_rollbackStack.size(); ++i) {
                        if (g_rollbackStack[i].id == rid) {
                            RollbackEntry e = g_rollbackStack[i];
                            g_rollbackStack.erase(g_rollbackStack.begin() + (std::ptrdiff_t)i);
                            ok = ApplyRollbackEntry(e, err);
                            SaveRollbackStack();
                            break;
                        }
                    }
                }
                SendResp(id, ok ? OkJson() : ErrCode(L"ERR_ROLLBACK", err));
                return;
            }
            if (m == L"rollback_last") {
                bool ok = false;
                std::wstring err = L"No rollback entries";
                {
                    std::lock_guard<std::mutex> lk(g_rollbackMutex);
                    if (!g_rollbackStack.empty()) {
                        RollbackEntry e = g_rollbackStack.back();
                        g_rollbackStack.pop_back();
                        ok = ApplyRollbackEntry(e, err);
                        SaveRollbackStack();
                    }
                }
                SendResp(id, ok ? OkJson() : ErrCode(L"ERR_ROLLBACK", err));
                return;
            }
            if (m == L"get_history") {
                std::ifstream f(g_paths.HistoryFile.c_str(), std::ios::binary);
                if (!f) { SendResp(id, L"{\"items\":[]}"); return; }
                std::stringstream ss; ss << f.rdbuf();
                SendResp(id, L"{\"items\":" + Utf8ToWide(ss.str()) + L"}");
                return;
            }
            if (m == L"apply_profile") {
                if (!validateArgs(1, L"apply_profile")) return;
                std::wstring profile = arg(4);
                if (profile != L"gaming" && profile != L"work" && profile != L"laptop" && profile != L"safe") {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Unknown profile"));
                    return;
                }
                WriteAuditLog(L"ApplyProfile", profile);
                if (profile == L"gaming") {
                    GameBooster(true);
                    DisableNetworkThrottling();
                    SetWin32PrioritySeparation(0x26);
                }
                else if (profile == L"work") {
                    GameBooster(false);
                    DisableNetworkThrottling();
                }
                else if (profile == L"laptop") {
                    RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Power Balanced", false);
                }
                else if (profile == L"safe") {
                    CleanCacheEnhanced();
                    PurgeStandbyEnhanced();
                }
                SendResp(id, OkJson());
                return;
            }
            if (m == L"bench_capture") {
                return AsyncData(id, []() -> std::wstring {
                    BenchmarkData b = CaptureBenchmark();
                    g_benchHistory.push_back(b);
                    if (g_benchHistory.size() > 50) g_benchHistory.erase(g_benchHistory.begin());
                    std::wstring j = L"{\"ok\":true,\"items\":[";
                    for (size_t i = 0; i < g_benchHistory.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"ts\":" + std::to_wstring(g_benchHistory[i].timestamp)
                            + L",\"freeRamMB\":" + std::to_wstring(g_benchHistory[i].freeRamMB)
                            + L",\"processCount\":" + std::to_wstring(g_benchHistory[i].processCount)
                            + L",\"serviceCount\":" + std::to_wstring(g_benchHistory[i].serviceCount)
                            + L",\"diskLatencyMs\":" + FloatToJsonStr((float)g_benchHistory[i].diskLatencyMs)
                            + L",\"cpuScore\":" + FloatToJsonStr((float)g_benchHistory[i].cpuScore) + L"}";
                    }
                    j += L"]}";
                    return j;
                    });
            }
            if (m == L"bench_get_history") {
                std::wstring j = L"{\"items\":[";
                for (size_t i = 0; i < g_benchHistory.size(); ++i) {
                    if (i) j += L",";
                    j += L"{\"ts\":" + std::to_wstring(g_benchHistory[i].timestamp)
                        + L",\"freeRamMB\":" + std::to_wstring(g_benchHistory[i].freeRamMB)
                        + L",\"processCount\":" + std::to_wstring(g_benchHistory[i].processCount)
                        + L",\"serviceCount\":" + std::to_wstring(g_benchHistory[i].serviceCount)
                        + L",\"diskLatencyMs\":" + FloatToJsonStr((float)g_benchHistory[i].diskLatencyMs)
                        + L",\"cpuScore\":" + FloatToJsonStr((float)g_benchHistory[i].cpuScore) + L"}";
                }
                j += L"]}";
                SendResp(id, j);
                return;
            }
            if (m == L"uwp_list") {
                std::wstring j = L"[";
                auto list = GetUwpAppList();
                for (size_t i = 0; i < list.size(); ++i) {
                    if (i) j += L",";
                    j += L"{\"packageName\":" + JsonStr(list[i].packageName)
                        + L",\"displayName\":" + JsonStr(list[i].displayName)
                        + L",\"category\":" + JsonStr(list[i].category)
                        + L",\"description\":" + JsonStr(list[i].description) + L"}";
                }
                j += L"]";
                SendResp(id, L"{\"items\":" + j + L"}");
                return;
            }
            if (m == L"uwp_remove") {
                std::vector<std::wstring> pkgs;
                for (size_t i = 4; i < parts.size(); ++i) {
                    if (parts[i].size() > 128) continue;
                    bool ok = true;
                    for (wchar_t c : parts[i]) if (!(iswalnum(c) || c == L'.' || c == L'-' || c == L'_')) { ok = false; break; }
                    if (ok) pkgs.push_back(parts[i]);
                }
                return AsyncData(id, [pkgs]() -> std::wstring {
                    int removed = 0;
                    for (const auto& pkg : pkgs) {
                        std::vector<std::wstring> args = { L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-Command",
                            L"Get-AppxPackage -AllUsers -Name '" + pkg + L"' | Remove-AppxPackage -AllUsers -ErrorAction SilentlyContinue" };
                        DWORD code = 0;
                        if (RunProcessSafe(L"powershell.exe", args, code, 300)) removed++;
                    }
                    return L"{\"ok\":true,\"count\":" + std::to_wstring(removed) + L"}";
                    });
            }
            if (m == L"sched_create") {
                if (!validateArgs(1, L"sched_create")) return;
                std::wstring schedule = arg(4);
                return AsyncData(id, [schedule]() -> std::wstring {
                    bool ok = CreateAutoCleanupTask(schedule);
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to create task");
                    });
            }
            if (m == L"sched_delete") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = DeleteAutoCleanupTask();
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to delete task");
                    });
            }
            if (m == L"scan_leftovers") {
                if (!validateArgs(1, L"scan_leftovers")) return;
                std::wstring name = arg(4);
                return AsyncData(id, [name]() -> std::wstring {
                    auto items = ScanLeftovers(name);
                    std::wstring j = L"[";
                    for (size_t i = 0; i < items.size(); ++i) {
                        if (i) j += L",";
                        j += JsonStr(items[i]);
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"get_cpu_features") {
                auto f = GetCpuFeatures();
                std::wstring j = L"{\"sse41\":" + std::wstring(f.sse41 ? L"true" : L"false") + L",\"sse42\":" + std::wstring(f.sse42 ? L"true" : L"false") +
                    L",\"popcnt\":" + std::wstring(f.popcnt ? L"true" : L"false") + L",\"avx\":" + std::wstring(f.avx ? L"true" : L"false") +
                    L",\"fma3\":" + std::wstring(f.fma3 ? L"true" : L"false") + L",\"avx2\":" + std::wstring(f.avx2 ? L"true" : L"false") +
                    L",\"bmi1\":" + std::wstring(f.bmi1 ? L"true" : L"false") + L",\"bmi2\":" + std::wstring(f.bmi2 ? L"true" : L"false") +
                    L",\"avx512\":" + std::wstring(f.avx512 ? L"true" : L"false") + L"}";
                SendResp(id, j);
                return;
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
                SendResp(id, j);
                return;
            }
            if (m == L"set_drive") {
                if (!validateArgs(1, L"set_drive")) return;
                std::wstring letterStr = arg(4);
                if (letterStr.empty() || letterStr.size() > 1) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid drive letter"));
                    return;
                }
                wchar_t letter = letterStr[0];
                if (letter >= L'a' && letter <= L'z') letter = static_cast<wchar_t>(letter - L'a' + L'A');
                if (letter < L'A' || letter > L'Z') {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid drive letter"));
                    return;
                }
                std::wstring root = std::wstring(1, letter) + L":\\";
                if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED) {
                    SendResp(id, ErrCode(L"ERR_NOT_FIXED_DRIVE", L"Not a fixed drive"));
                    return;
                }
                g_activeDrive = letter;
                bool isSys = (letter == g_systemDriveLetter);
                SendResp(id, L"{\"ok\":true,\"letter\":\"" + std::wstring(1, letter) + L"\",\"isSystem\":" + std::wstring(isSys ? L"true" : L"false") + L"}");
                return;
            }
            if (m == L"get_settings") {
                UserSettings s;
                { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
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
                    + L"}}";
                SendResp(id, j);
                return;
            }
            if (m == L"set_settings") {
                if (!validateArgs(1, L"set_settings")) return;
                std::wstring payload = arg(4);
                auto parsed = JsonParser::ParseObject(payload);
                UserSettings s;
                { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
                if (parsed.count(L"cleanTemp")) s.cleanTemp = (parsed[L"cleanTemp"] == L"true");
                if (parsed.count(L"cleanPrefetch")) s.cleanPrefetch = (parsed[L"cleanPrefetch"] == L"true");
                if (parsed.count(L"cleanRecycle")) s.cleanRecycle = (parsed[L"cleanRecycle"] == L"true");
                if (parsed.count(L"cleanThumbs")) s.cleanThumbs = (parsed[L"cleanThumbs"] == L"true");
                if (parsed.count(L"cleanShader")) s.cleanShader = (parsed[L"cleanShader"] == L"true");
                if (parsed.count(L"cleanDns")) s.cleanDns = (parsed[L"cleanDns"] == L"true");
                if (parsed.count(L"cleanDO")) s.cleanDO = (parsed[L"cleanDO"] == L"true");
                if (parsed.count(L"cleanLaunchers")) s.cleanLaunchers = (parsed[L"cleanLaunchers"] == L"true");
                if (parsed.count(L"cleanBrowsers")) s.cleanBrowsers = (parsed[L"cleanBrowsers"] == L"true");
                if (parsed.count(L"cleanChats")) s.cleanChats = (parsed[L"cleanChats"] == L"true");
                if (parsed.count(L"cleanDumps")) s.cleanDumps = (parsed[L"cleanDumps"] == L"true");
                if (parsed.count(L"ramClearStandby")) s.ramClearStandby = (parsed[L"ramClearStandby"] == L"true");
                if (parsed.count(L"ramFlushModified")) s.ramFlushModified = (parsed[L"ramFlushModified"] == L"true");
                if (parsed.count(L"ramClearWorkingSets")) s.ramClearWorkingSets = (parsed[L"ramClearWorkingSets"] == L"true");
                if (parsed.count(L"ramClearFileCache")) s.ramClearFileCache = (parsed[L"ramClearFileCache"] == L"true");
                if (parsed.count(L"autoBoost")) s.autoBoost = (parsed[L"autoBoost"] == L"true");
                if (parsed.count(L"ramAutoCleanEnabled")) s.ramAutoCleanEnabled = (parsed[L"ramAutoCleanEnabled"] == L"true");
                if (parsed.count(L"ramAutoCleanOnIdle")) s.ramAutoCleanOnIdle = (parsed[L"ramAutoCleanOnIdle"] == L"true");
                if (parsed.count(L"overlayDarkTheme")) s.overlayDarkTheme = (parsed[L"overlayDarkTheme"] == L"true");
                if (parsed.count(L"hotkeysEnabled")) s.hotkeysEnabled = (parsed[L"hotkeysEnabled"] == L"true");
                if (parsed.count(L"ramAutoCleanThresholdPct")) {
                    try { s.ramAutoCleanThresholdPct = std::stoi(parsed[L"ramAutoCleanThresholdPct"]); }
                    catch (const std::exception&) {}
                }
                if (parsed.count(L"ramAutoCleanIntervalMin")) {
                    try { s.ramAutoCleanIntervalMin = std::stoi(parsed[L"ramAutoCleanIntervalMin"]); }
                    catch (const std::exception&) {}
                }
                auto readBool50 = [&](const std::wstring& k, bool& out) { if (parsed.count(k)) out = (parsed[k] == L"true"); };
                readBool50(L"s50_telemetry", s.s50_telemetry);
                readBool50(L"s50_cortana", s.s50_cortana);
                readBool50(L"s50_adid", s.s50_adid);
                readBool50(L"s50_actfeed", s.s50_actfeed);
                readBool50(L"s50_smartscreen", s.s50_smartscreen);
                readBool50(L"s50_uac", s.s50_uac);
                readBool50(L"s50_defender", s.s50_defender);
                readBool50(L"s50_vbs", s.s50_vbs);
                readBool50(L"s50_etw", s.s50_etw);
                readBool50(L"s50_winupd", s.s50_winupd);
                readBool50(L"s50_gamemode", s.s50_gamemode);
                readBool50(L"s50_gamedvr", s.s50_gamedvr);
                readBool50(L"s50_prio", s.s50_prio);
                readBool50(L"s50_netthrottle", s.s50_netthrottle);
                readBool50(L"s50_nagle", s.s50_nagle);
                readBool50(L"s50_dynamictick", s.s50_dynamictick);
                readBool50(L"s50_hags", s.s50_hags);
                readBool50(L"s50_mouseaccel", s.s50_mouseaccel);
                readBool50(L"s50_power", s.s50_power);
                readBool50(L"s50_zero", s.s50_zero);
                readBool50(L"s50_minecraft", s.s50_minecraft);
                readBool50(L"s50_print", s.s50_print);
                readBool50(L"s50_icons", s.s50_icons);
                readBool50(L"s50_pools", s.s50_pools);
                readBool50(L"s50_paging", s.s50_paging);
                readBool50(L"s50_memcomp", s.s50_memcomp);
                readBool50(L"s50_fastdns", s.s50_fastdns);
                readBool50(L"s50_rss", s.s50_rss);
                readBool50(L"s50_resetnet", s.s50_resetnet);
                readBool50(L"s50_ping", s.s50_ping);
                readBool50(L"s50_services", s.s50_services);
                readBool50(L"s50_ctxmenu", s.s50_ctxmenu);
                readBool50(L"s50_usb", s.s50_usb);
                readBool50(L"s50_pcie", s.s50_pcie);
                readBool50(L"s50_nvapi", s.s50_nvapi);
                if (s.ramAutoCleanThresholdPct < 50) s.ramAutoCleanThresholdPct = 50;
                if (s.ramAutoCleanThresholdPct > 99) s.ramAutoCleanThresholdPct = 99;
                if (s.ramAutoCleanIntervalMin < 5) s.ramAutoCleanIntervalMin = 5;
                if (s.ramAutoCleanIntervalMin > 120) s.ramAutoCleanIntervalMin = 120;
                { std::lock_guard<std::mutex> lk(g_settingsMutex); g_userSettings = s; }
                g_overlay.darkTheme = s.overlayDarkTheme;
                if (s.hotkeysEnabled) {
                    if (!RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 'O')) PrintErr(L"Failed to register Ctrl+Alt+O");
                    if (!RegisterHotKey(hwnd, 2, MOD_CONTROL | MOD_ALT, 'M')) PrintErr(L"Failed to register Ctrl+Alt+M");
                    if (!RegisterHotKey(hwnd, 3, MOD_CONTROL | MOD_ALT, 'C')) PrintErr(L"Failed to register Ctrl+Alt+C");
                }
                else {
                    UnregisterHotKey(hwnd, 1);
                    UnregisterHotKey(hwnd, 2);
                    UnregisterHotKey(hwnd, 3);
                }
                std::thread([]() { BackupSettings(); }).detach();
                SendResp(id, OkJson());
                return;
            }
            if (m == L"apply_s50") {
                UserSettings s;
                { std::lock_guard<std::mutex> lk(g_settingsMutex); s = g_userSettings; }
                bool dangerous = s.s50_uac || s.s50_smartscreen || s.s50_defender || s.s50_vbs || s.s50_services || s.s50_winupd;
                if (dangerous) {
                    std::wstring details = L"Будут применены потенциально опасные твики (UAC/SmartScreen/Defender/VBS/SysMain/WSearch).";
                    if (!RequireDangerousConfirm(L"Apply 50+ Settings", details)) {
                        SendResp(id, ErrCode(L"ERR_CANCELLED", L"Cancelled by user"));
                        return;
                    }
                    CreateRestorePoint(L"ZeroBloat - Before 50+ Settings");
                }
                WriteAuditLog(L"ApplyS50Settings", L"User applied 50+ settings");
                ApplyS50Settings();
                SendResp(id, OkJson());
                return;
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
                if (ids.empty()) {
                    SendResp(id, ErrCode(L"ERR_EMPTY", L"No items selected"));
                    return;
                }
                return AsyncData(id, [ids]() -> std::wstring {
                    uint64_t totalFreed = 0;
                    int okCount = 0;
                    for (auto& sid : ids) {
                        uint64_t freed = 0;
                        if (SmartApplyItem(sid, freed)) {
                            okCount++;
                            totalFreed += freed;
                        }
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
                SendResp(id, L"{\"items\":" + j + L"]}");
                return;
            }
            if (m == L"toggle_autostart") {
                if (!validateArgs(3, L"toggle_autostart")) return;
                std::wstring name = arg(4), loc = arg(5), act = arg(6);
                if (name.empty() || loc.empty() || act.empty()) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Missing arguments"));
                    return;
                }
                bool ok = ToggleAutostart(name, loc, act);
                SendResp(id, ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to toggle autostart"));
                return;
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
                    SendResp(id, ErrCode(L"ERR_NOT_SYSTEM_DRIVE", L"Only for system drive"));
                    return;
                }
                if (!RequireDangerousConfirm(L"Lite Windows Debloat", L"Удаление UWP + отключение служб + телеметрия. Необратимо.")) {
                    SendResp(id, ErrCode(L"ERR_CANCELLED", L"Cancelled"));
                    return;
                }
                auto self = shared_from_this();
                std::thread([self, id]() {
                    try {
                        CreateRestorePoint(L"ZeroBloat - Lite Debloat");
                        PerformLiteDebloat();
                        self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, OkJson()); });
                    }
                    catch (const std::exception& e) {
                        std::wstring err = ErrCode(L"ERR_EXCEPTION", Utf8ToWide(e.what()));
                        self->PostUI([self, id, err]() { if (!self->shuttingDown) self->SendResp(id, err); });
                    }
                    }).detach();
                return;
            }
            if (m == L"compress_game") {
                if (!validateArgs(1, L"compress_game")) return;
                std::wstring path = arg(4);
                return AsyncData(id, [path]() -> std::wstring {
                    bool ok = CompressGameFolder(path);
                    return ok ? OkJson() : ErrCode(L"ERR_COMPRESS_FAILED", L"Compression failed or invalid path");
                    });
            }
            if (m == L"install_app") {
                if (!validateArgs(1, L"install_app")) return;
                std::wstring pkgId = arg(4);
                return AsyncData(id, [pkgId]() -> std::wstring {
                    bool ok = InstallAppWinget(pkgId);
                    return ok ? OkJson() : ErrCode(L"ERR_INSTALL_FAILED", L"Installation failed");
                    });
            }
            if (m == L"uninstall_program") {
                if (!validateArgs(1, L"uninstall_program")) return;
                std::wstring uninst = arg(4);
                return AsyncData(id, [uninst]() -> std::wstring {
                    if (uninst.empty()) return ErrCode(L"ERR_EMPTY_CMD", L"Empty uninstall command");
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
                            CloseHandle(pi.hProcess);
                            CloseHandle(pi.hThread);
                            ok = true;
                        }
                    }
                    return ok ? OkJson() : ErrCode(L"ERR_UNINSTALL_FAILED", L"Uninstall failed");
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
                if (!validateArgs(1, L"bulk_uninstall")) return;
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
                            WriteAuditLog(L"BulkUninstall", line);
                            RunCmd(line, L"Uninstalling", false, 600);
                            count++;
                        }
                    }
                    return L"{\"ok\":true,\"count\":" + std::to_wstring(count) + L"}";
                    });
            }
            if (m == L"open_url") {
                if (!validateArgs(1, L"open_url")) return;
                std::wstring u = arg(4);
                ShellExecuteW(nullptr, L"open", u.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                SendResp(id, OkJson());
                return;
            }
            if (m == L"get_context_menu") {
                std::wstring j = ListContextMenuJson();
                SendResp(id, L"{\"items\":" + j + L"}");
                return;
            }
            if (m == L"toggle_context_menu") {
                if (!validateArgs(2, L"toggle_context_menu")) return;
                std::wstring clsid = arg(4), dis = arg(5);
                if (clsid.empty()) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Empty CLSID"));
                    return;
                }
                bool ok = ToggleContextMenu(clsid, dis == L"1");
                SendResp(id, ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to toggle"));
                return;
            }
            if (m == L"set_overlay_setting") {
                if (!validateArgs(2, L"set_overlay_setting")) return;
                std::wstring key = arg(4);
                std::wstring val = arg(5);
                if (key.empty()) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Missing key"));
                    return;
                }
                bool valBool = val == L"1";
                if (key == L"cpu") g_overlay.showCpu = valBool;
                else if (key == L"gpu") g_overlay.showGpu = valBool;
                else if (key == L"ram") g_overlay.showRam = valBool;
                else if (key == L"temp") g_overlay.showTemp = valBool;
                else if (key == L"vram") g_overlay.showVram = valBool;
                else if (key == L"fan") g_overlay.showFan = valBool;
                else if (key == L"ping") g_overlay.showPing = valBool;
                else if (key == L"dark") { g_overlay.darkTheme = valBool; SaveOverlaySettings(); }
                SendResp(id, OkJson());
                return;
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
                if (!validateArgs(1, L"game_preset")) return;
                std::wstring preset = arg(4);
                if (preset != L"competitive" && preset != L"balanced" && preset != L"quality") {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Unknown preset"));
                    return;
                }
                auto self = shared_from_this();
                std::thread([self, id, preset]() {
                    try {
                        if (preset == L"competitive") {
                            SetWin32PrioritySeparation(0x26);
                            SetGameMode(true);
                            RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"High perf", false);
                            DisableNetworkThrottling();
                            g_overlay.showCpu = true;
                            g_overlay.showGpu = true;
                        }
                        else if (preset == L"balanced") {
                            SetWin32PrioritySeparation(0x2);
                            SetGameMode(true);
                            RunCmd(std::wstring(L"powercfg /setactive ") + BALANCED_GUID, L"Balanced", false);
                        }
                        else if (preset == L"quality") {
                            RunCmd(std::wstring(L"powercfg /setactive ") + HIGH_PERF_GUID, L"High perf", false);
                        }
                        self->PostUI([self, id]() { if (!self->shuttingDown) self->SendResp(id, OkJson()); });
                    }
                    catch (const std::exception& e) {
                        std::wstring err = ErrCode(L"ERR_EXCEPTION", Utf8ToWide(e.what()));
                        self->PostUI([self, id, err]() { if (!self->shuttingDown) self->SendResp(id, err); });
                    }
                    }).detach();
                return;
            }
            if (m == L"read_hosts") {
                if (g_activeDrive.load() != g_systemDriveLetter) {
                    SendResp(id, ErrCode(L"ERR_NOT_SYSTEM_DRIVE", L"Only for system drive"));
                    return;
                }
                std::wstring path = g_paths.SystemRoot + L"\\System32\\drivers\\etc\\hosts";
                if (!IsSafePath(path, g_paths.SystemRoot, true)) {
                    SendResp(id, ErrCode(L"ERR_UNSAFE_PATH", L"Unsafe path"));
                    return;
                }
                std::ifstream f(path.c_str(), std::ios::binary);
                if (!f) {
                    SendResp(id, ErrCode(L"ERR_FILE_OPEN", L"Cannot open hosts file"));
                    return;
                }
                std::stringstream ss;
                ss << f.rdbuf();
                SendResp(id, L"{\"ok\":true,\"content\":" + JsonStr(Utf8ToWide(ss.str())) + L"}");
                return;
            }
            if (m == L"write_hosts") {
                if (!validateArgs(1, L"write_hosts")) return;
                if (g_activeDrive.load() != g_systemDriveLetter) {
                    SendResp(id, ErrCode(L"ERR_NOT_SYSTEM_DRIVE", L"Only for system drive"));
                    return;
                }
                std::wstring content = arg(4);
                std::wstring path = g_paths.SystemRoot + L"\\System32\\drivers\\etc\\hosts";
                if (!IsSafePath(path, g_paths.SystemRoot, true)) {
                    SendResp(id, ErrCode(L"ERR_UNSAFE_PATH", L"Unsafe path"));
                    return;
                }
                if (content.size() > 1024 * 1024) {
                    SendResp(id, ErrCode(L"ERR_CONTENT_TOO_LARGE", L"Hosts content too large"));
                    return;
                }
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
                SendResp(id, ok ? OkJson() : ErrCode(L"ERR_WRITE_FAILED", L"Failed to write hosts file"));
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
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"control_service") {
                if (!validateArgs(2, L"control_service")) return;
                std::wstring name = arg(4), action = arg(5);
                if (name.empty() || action.empty()) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Missing arguments"));
                    return;
                }
                if (action == L"disable") {
                    if (!RequireDangerousConfirm(L"Disable Service " + name, L"Служба " + name + L" будет отключена.")) {
                        SendResp(id, ErrCode(L"ERR_CANCELLED", L"Cancelled"));
                        return;
                    }
                    CreateRestorePoint(L"ZeroBloat - Disable " + name);
                }
                return AsyncData(id, [name, action]() -> std::wstring {
                    std::wstring err;
                    bool ok = ControlService2(name, action, err);
                    if (ok) WriteAuditLog(L"ControlService", name + L" -> " + action);
                    return ok ? OkJson() : ErrCode(L"ERR_SERVICE", err);
                    });
            }
            if (m == L"find_large_files") {
                std::wstring path = arg(4);
                if (path.empty()) path = GetEnvW(L"USERPROFILE");
                if (!IsSafePath(path, L"", true)) {
                    SendResp(id, ErrCode(L"ERR_UNSAFE_PATH", L"Unsafe path"));
                    return;
                }
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
                                    catch (const std::exception&) {}
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
                std::wstring path = arg(4);
                if (path.empty()) path = GetEnvW(L"USERPROFILE") + L"\\Downloads";
                if (!IsSafePath(path, L"", true)) {
                    SendResp(id, ErrCode(L"ERR_UNSAFE_PATH", L"Unsafe path"));
                    return;
                }
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
                                catch (const std::exception&) {}
                            }
                        }
                    }
                    catch (const std::exception&) {}
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
                                catch (const std::exception&) {}
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
                if (!validateArgs(1, L"shred_file")) return;
                std::wstring path = arg(4);
                return AsyncData(id, [path]() -> std::wstring {
                    bool ok = ShredFileW(path);
                    return ok ? OkJson() : ErrCode(L"ERR_SHRED_FAILED", L"Shred failed or unsafe path");
                    });
            }
            if (m == L"stress_test") {
                return AsyncData(id, []() -> std::wstring {
                    float maxTemp = -1.0f;
                    float sumCpu = 0;
                    int n = 0;
                    std::atomic<bool> stop{ false };
                    std::vector<std::thread> workers;
                    unsigned nThreads = std::thread::hardware_concurrency();
                    if (nThreads == 0) nThreads = 4;
                    for (unsigned i = 0; i < nThreads; ++i) {
                        workers.emplace_back([&stop]() {
                            volatile double x = 1.0;
                            while (!stop) {
                                x = std::sqrt(x) * 3.14159 + 1.0;
                                if (x > 1e15) x = 1.0;
                            }
                            });
                    }
                    CpuSampler s;
                    for (int i = 0; i < 30; ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        float c = s.Sample();
                        sumCpu += c;
                        n++;
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
                    double ops = dur > 0 ? (10000000.0 / (double)dur) * 1000.0 : 0;
                    return L"{\"ok\":true,\"score\":" + FloatToJsonStr((float)ops) + L",\"duration\":" + std::to_wstring(dur) + L"}";
                    });
            }
            if (m == L"create_restore_point") {
                if (!validateArgs(1, L"create_restore_point")) return;
                std::wstring name = arg(4);
                return AsyncData(id, [name]() -> std::wstring {
                    bool ok = CreateRestorePoint(name.empty() ? L"ZeroBloat" : name);
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to create restore point");
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
                if (!validateArgs(1, L"restore_to_point")) return;
                uint32_t seq = (uint32_t)SafeStoi(arg(4), 0);
                return AsyncData(id, [seq]() -> std::wstring {
                    bool ok = RestoreToPoint(seq);
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Restore failed");
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
                if (!validateArgs(1, L"pause_updates")) return;
                int days = SafeStoi(arg(4), 7);
                if (days < 1 || days > 35) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid days"));
                    return;
                }
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
                    CoInitScope com;
                    auto v = GetTaskSchedulerTasks();
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
                SendResp(id, GetPageFileInfo());
                return;
            }
            if (m == L"set_pagefile") {
                if (!validateArgs(2, L"set_pagefile")) return;
                uint32_t initial = (uint32_t)SafeStoi(arg(4), 4096);
                uint32_t mx = (uint32_t)SafeStoi(arg(5), 8192);
                if (initial < 128 || mx < initial || mx > 1024 * 1024) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid pagefile size"));
                    return;
                }
                return AsyncData(id, [initial, mx]() -> std::wstring {
                    bool ok = SetPageFileSize(initial, mx);
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to set pagefile");
                    });
            }
            if (m == L"ping_host") {
                std::wstring host = arg(4);
                if (host.empty() || host.size() > 256) host = L"8.8.8.8";
                bool okHost = true;
                for (wchar_t c : host) if (!(iswalnum(c) || c == L'.' || c == L'-' || c == L':')) { okHost = false; break; }
                if (!okHost) { SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid host")); return; }
                return AsyncData(id, [host]() -> std::wstring {
                    std::vector<std::wstring> args = { L"-n", L"1", L"-w", L"2000", host };
                    DWORD code = 0;
                    TempFile tf;
                    std::wstring cmdOut;
                    if (RunProcessSafe(L"ping.exe", args, code, 8)) {
                        int ms = -1;
                        if (code == 0) ms = 1;
                        return L"{\"ok\":true,\"ms\":" + std::to_wstring(ms) + L"}";
                    }
                    return L"{\"ok\":true,\"ms\":-1}";
                    });
            }
            if (m == L"reset_network") {
                return AsyncData(id, []() -> std::wstring {
                    ResetNetwork();
                    return OkJson();
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
                if (!validateArgs(1, L"set_hags")) return;
                bool enable = arg(4) == L"1";
                return AsyncData(id, [enable]() -> std::wstring {
                    bool ok = ToggleHAGS(enable);
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to toggle HAGS");
                    });
            }
            if (m == L"set_mouse_accel") {
                if (!validateArgs(1, L"set_mouse_accel")) return;
                bool enable = arg(4) == L"1";
                return AsyncData(id, [enable]() -> std::wstring {
                    ToggleMouseAcceleration(enable);
                    return OkJson();
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
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to backup settings");
                    });
            }
            if (m == L"restore_settings") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = RestoreSettingsFromBackup();
                    return ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Failed to restore settings");
                    });
            }
            if (m == L"get_security_status") {
                SendResp(id, GetSecurityStatus());
                return;
            }
            if (m == L"scan_drive_fast") {
                if (!validateArgs(1, L"scan_drive_fast")) return;
                std::wstring driveStr = arg(4);
                wchar_t drive = driveStr.empty() ? L'C' : driveStr[0];
                if (drive < L'A' || drive > L'Z') {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid drive"));
                    return;
                }
                return AsyncData(id, [drive]() -> std::wstring {
                    std::wstring root = std::wstring(1, drive) + L":\\";
                    std::error_code ec{};
                    std::vector<std::pair<std::wstring, uint64_t>> folders;
                    if (fs::exists(root, ec) && !ec) {
                        for (auto& p : fs::directory_iterator(root, ec)) {
                            if (p.is_directory(ec) && !ec) {
                                std::wstring folderName = p.path().filename().wstring();
                                uint64_t size = GetFolderSizeBytes(p.path().wstring());
                                folders.push_back({ folderName, size });
                            }
                        }
                    }
                    std::sort(folders.begin(), folders.end(), [](auto& a, auto& b) { return a.second > b.second; });
                    std::wstring j = L"[";
                    for (size_t i = 0; i < folders.size() && i < 50; ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(folders[i].first) + L",\"size\":" + std::to_wstring(folders[i].second) + L"}";
                    }
                    j += L"]";
                    return L"{\"items\":" + j + L"}";
                    });
            }
            if (m == L"ai_get_config") {
                AiConfig c;
                { std::lock_guard<std::mutex> lk(g_aiMutex); c = g_aiConfig; }
                std::wstring j = L"{\"ok\":true,\"endpoint\":" + JsonStr(c.endpoint) +
                    L",\"model\":" + JsonStr(c.model) +
                    L",\"hasKey\":" + std::wstring(c.apiKey.empty() ? L"false" : L"true") +
                    L",\"useLocal\":" + std::wstring(c.useLocal ? L"true" : L"false") + L"}";
                SendResp(id, j);
                return;
            }
            if (m == L"ai_set_config") {
                if (!validateArgs(4, L"ai_set_config")) return;
                std::wstring ep = arg(4);
                std::wstring key = arg(5);
                std::wstring model = arg(6);
                bool useLocal = arg(7) == L"1";
                if (ep.empty() || model.empty()) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Empty endpoint or model"));
                    return;
                }
                {
                    std::lock_guard<std::mutex> lk(g_aiMutex);
                    g_aiConfig.endpoint = ep;
                    g_aiConfig.model = model;
                    if (!key.empty() && key != L"(сохранён)") g_aiConfig.apiKey = key;
                    g_aiConfig.enabled = true;
                    g_aiConfig.useLocal = useLocal;
                }
                SaveAiConfig();
                SendResp(id, OkJson());
                return;
            }
            if (m == L"ai_chat") {
                if (!validateArgs(1, L"ai_chat")) return;
                std::wstring userMessage = arg(4);
                if (userMessage.empty()) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Empty message"));
                    return;
                }
                return AsyncData(id, [userMessage]() -> std::wstring {
                    std::wstring err;
                    std::wstring result = AiChat(userMessage, err);
                    if (result.empty()) return ErrCode(L"ERR_AI_FAILED", err.empty() ? L"AI request failed" : err);
                    return result;
                    });
            }
            if (m == L"quick_action") {
                if (!validateArgs(1, L"quick_action")) return;
                std::wstring act = arg(4);
                if (act.empty() || act.size() > 64) {
                    SendResp(id, ErrCode(L"ERR_INVALID_ARG", L"Invalid action"));
                    return;
                }
                static const std::set<std::wstring> kDangerous = {
                    L"disable_defender", L"deep_services", L"adv_clean_winsxs", L"disable_vbs",
                    L"disable_etw", L"remove_spyware", L"disable_updates", L"disable_paging",
                    L"disable_mem_comp", L"game_mode_off", L"nvidia_max_perf", L"clear_kernel_pools"
                };
                if (kDangerous.count(act)) {
                    if (!RequireDangerousConfirm(L"Quick action: " + act, L"Опасная операция.")) {
                        SendResp(id, ErrCode(L"ERR_CANCELLED", L"Cancelled"));
                        return;
                    }
                    CreateRestorePoint(L"ZeroBloat - " + act);
                }
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
                                    PrintErr(L"[-] RAM < 32GB. Отключение файла подкачки заблокировано.");
                                }
                                else {
                                    ok = RunCmd(L"wmic computersystem set AutomaticManagedPagefile=False", L"Disable Paging");
                                    if (ok) ok = RunCmd(L"wmic pagefileset where name=\"C:\\\\pagefile.sys\" delete", L"Delete pagefile");
                                }
                            }
                        }
                        else if (act == L"disable_mem_comp") { ok = RunCmd(L"powershell -NoProfile -Command \"Disable-MMAgent -MemoryCompression\"", L"Disable Mem Comp"); }
                        else if (act == L"toggle_cache_timer") { g_cacheTimerEnabled = !g_cacheTimerEnabled.load(); ok = true; }
                        else if (act == L"check_dpc") {
                            float d = g_dpcIsrTime.load();
                            if (d > 5.0f) PrintWarn(L"[-] Высокие задержки DPC/ISR: " + Fmt(L"%.2f", d) + L"%");
                            else Print(L"[+] DPC/ISR в норме.");
                            ok = true;
                        }
                        else if (act == L"net_rss_on") { ApplyNetworkTweaks(true); ok = true; }
                        else if (act == L"net_rss_off") { ApplyNetworkTweaks(false); ok = true; }
                        else if (act == L"disable_vbs") { DisableVBS(); ok = true; }
                        else if (act == L"disable_etw") { DisableETW(); ok = true; }
                        else if (act == L"remove_spyware") { PerformLiteDebloat(); ok = true; }
                        else if (act == L"nvidia_max_perf") { ApplyNvidiaPState(0); ok = true; }
                        else if (act == L"clear_kernel_pools") { ClearKernelPools(); ok = true; }
                        else if (act == L"run_ddu") {
                            std::wstring res = RunDDU();
                            if (res.find(L"true") != std::wstring::npos) ok = true;
                        }
                    }
                    catch (const std::exception& e) {
                        std::wstring err = ErrCode(L"ERR_EXCEPTION", Utf8ToWide(e.what()));
                        self->PostUI([self, id, err]() { if (!self->shuttingDown) self->SendResp(id, err); });
                        return;
                    }
                    self->PostUI([self, id, ok]() { if (!self->shuttingDown) self->SendResp(id, ok ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Operation failed")); });
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
                        }
                    }
                    catch (const std::exception&) { allOk = false; }
                    self->PostUI([self, id, allOk]() { if (!self->shuttingDown) self->SendResp(id, allOk ? OkJson() : ErrCode(L"ERR_OP_FAILED", L"Cleanup failed")); });
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
            if (m == L"get_freeze_list") {
                return AsyncData(id, []() -> std::wstring {
                    std::vector<std::wstring> frozen = GetFrozenProcesses();
                    auto v = ListProcesses();
                    std::wstring j = L"[";
                    bool first = true;
                    for (const auto& p : v) {
                        bool isFrozen = std::find(frozen.begin(), frozen.end(), p.name) != frozen.end();
                        if (!first) j += L",";
                        first = false;
                        j += L"{\"name\":" + JsonStr(p.name) + L",\"pid\":" + std::to_wstring(p.pid) + L",\"frozen\":" + (isFrozen ? L"true" : L"false") + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"freeze_processes") {
                std::vector<DWORD> pids;
                for (size_t i = 4; i < parts.size(); ++i) pids.push_back((DWORD)SafeStoi(parts[i], 0));
                return AsyncData(id, [pids]() -> std::wstring {
                    int count = FreezeProcesses(pids);
                    return L"{\"ok\":true,\"count\":" + std::to_wstring(count) + L"}";
                    });
            }
            if (m == L"unfreeze_all") {
                return AsyncData(id, []() -> std::wstring {
                    UnfreezeAll();
                    return OkJson();
                    });
            }
            if (m == L"install_sde") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = InstallSde();
                    return ok ? OkJson() : ErrCode(L"ERR_INSTALL_FAILED", L"Failed to install SDE");
                    });
            }
            if (m == L"remove_sde") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = RemoveSde();
                    return ok ? OkJson() : ErrCode(L"ERR_REMOVE_FAILED", L"Failed to remove SDE");
                    });
            }
            if (m == L"launch_sde") {
                if (!validateArgs(1, L"launch_sde")) return;
                std::wstring path = arg(4);
                return AsyncData(id, [path]() -> std::wstring {
                    bool ok = LaunchThroughSde(path);
                    return ok ? OkJson() : ErrCode(L"ERR_LAUNCH_FAILED", L"Failed to launch through SDE");
                    });
            }
            if (m == L"sde_status") {
                std::wstring sdePath = g_paths.SdeDir + L"\\sde.exe";
                bool installed = fs::exists(sdePath);
                SendResp(id, L"{\"ok\":true,\"installed\":" + std::wstring(installed ? L"true" : L"false") + L",\"path\":" + JsonStr(sdePath) + L"}");
                return;
            }
            if (m == L"scan_drivers") {
                return AsyncData(id, []() -> std::wstring {
                    auto v = ScanDrivers();
                    std::wstring j = L"[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) j += L",";
                        j += L"{\"name\":" + JsonStr(v[i].name) + L",\"version\":" + JsonStr(v[i].version)
                            + L",\"deviceId\":" + JsonStr(v[i].deviceId) + L"}";
                    }
                    return L"{\"items\":" + j + L"]}";
                    });
            }
            if (m == L"update_driver") {
                if (!validateArgs(1, L"update_driver")) return;
                std::wstring deviceId = arg(4);
                return AsyncData(id, [deviceId]() -> std::wstring {
                    bool ok = UpdateDriver(deviceId);
                    return ok ? OkJson() : ErrCode(L"ERR_UPDATE_FAILED", L"Failed to update driver");
                    });
            }
            if (m == L"update_all_drivers") {
                return AsyncData(id, []() -> std::wstring {
                    bool ok = UpdateAllDrivers();
                    return ok ? OkJson() : ErrCode(L"ERR_UPDATE_FAILED", L"Failed to update drivers");
                    });
            }
            SendResp(id, ErrCode(L"ERR_UNKNOWN_METHOD", L"Unknown method"));
        }
        catch (const std::exception& e) {
            WriteLogFile(LogLevel::ERR, L"[OnMessage] " + Utf8ToWide(e.what()));
            if (id >= 0) SendResp(id, ErrCode(L"ERR_EXCEPTION", Utf8ToWide(e.what())));
        }
        catch (...) {
            WriteLogFile(LogLevel::ERR, L"[OnMessage] unknown exception");
            if (id >= 0) SendResp(id, ErrCode(L"ERR_UNKNOWN", L"Unknown exception"));
        }
    }
};

static bool ApplyRollbackEntry(const RollbackEntry& e, std::wstring& errOut) {
    errOut.clear();
    if (e.type == L"registry") {
        std::wstring path = e.target;
        std::wstring valueName;
        auto slash = path.rfind(L'\\');
        if (slash == std::wstring::npos) { errOut = L"Bad registry path"; return false; }
        valueName = path.substr(slash + 1);
        std::wstring keyPath = path.substr(0, slash);
        HKEY root = HKEY_CURRENT_USER;
        std::wstring sub = keyPath;
        if (sub.rfind(L"HKLM\\", 0) == 0) { root = HKEY_LOCAL_MACHINE; sub = sub.substr(5); }
        else if (sub.rfind(L"HKCU\\", 0) == 0) { root = HKEY_CURRENT_USER; sub = sub.substr(5); }
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(root, sub.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
            errOut = L"RegCreateKeyEx failed";
            return false;
        }
        DWORD val = 0;
        try { val = (DWORD)std::stoul(e.oldValue); }
        catch (const std::exception&) { val = 0; }
        LONG r = RegSetValueExW(hKey, valueName.c_str(), 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        if (r != ERROR_SUCCESS) { errOut = L"RegSetValueEx failed"; return false; }
        return true;
    }
    if (e.type == L"service") {
        std::wstring svcName = e.target;
        DWORD startType = SERVICE_AUTO_START;
        if (e.oldValue == L"disabled") startType = SERVICE_DISABLED;
        else if (e.oldValue == L"manual") startType = SERVICE_DEMAND_START;
        else if (e.oldValue == L"auto") startType = SERVICE_AUTO_START;
        ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
        if (!scm) { errOut = L"OpenSCManager failed"; return false; }
        ScHandle svc(OpenServiceW(scm.get(), svcName.c_str(), SERVICE_CHANGE_CONFIG));
        if (!svc) { errOut = L"OpenService failed"; return false; }
        if (!ChangeServiceConfigW(svc.get(), SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
            errOut = L"ChangeServiceConfig failed";
            return false;
        }
        return true;
    }
    if (e.type == L"autostart") {
        return false;
    }
    errOut = L"Unknown rollback type";
    return false;
}

static std::wstring BuildRollbackListJson() {
    std::lock_guard<std::mutex> lk(g_rollbackMutex);
    std::wstring j = L"[";
    for (size_t i = 0; i < g_rollbackStack.size(); ++i) {
        if (i) j += L",";
        const auto& e = g_rollbackStack[i];
        j += L"{\"id\":" + JsonStr(e.id)
            + L",\"type\":" + JsonStr(e.type)
            + L",\"target\":" + JsonStr(e.target)
            + L",\"oldValue\":" + JsonStr(e.oldValue)
            + L",\"newValue\":" + JsonStr(e.newValue)
            + L",\"description\":" + JsonStr(e.description)
            + L",\"timestamp\":" + std::to_wstring(e.timestamp) + L"}";
    }
    j += L"]";
    return j;
}

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
        mmi->ptMinTrackSize.x = 900;
        mmi->ptMinTrackSize.y = 600;
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
                    DWORD pid = 0;
                    GetWindowThreadProcessId(fg, &pid);
                    if (pid > 0) {
                        UniqueHandle hProc(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
                        if (hProc) {
                            wchar_t path[MAX_PATH];
                            DWORD sz = MAX_PATH;
                            if (QueryFullProcessImageNameW(hProc.get(), 0, path, &sz)) {
                                std::wstring exe = path;
                                std::transform(exe.begin(), exe.end(), exe.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });
                                if (exe.find(L"game") != std::wstring::npos) inGame = true;
                            }
                        }
                    }
                }
            }
            g_overlay.UpdateVisibility(inGame);
            return 0;
        }
        break;
    case WM_HOTKEY:
        if (wp == 1) { g_overlay.Toggle(); }
        else if (wp == 2) { PurgeStandbyEnhanced(); }
        else if (wp == 3) { CleanTemp(); FlushDns(); }
        return 0;
    case WM_APP_MSG: {
        if (wp == 2) {
            std::wstring* res = reinterpret_cast<std::wstring*>(lp);
            if (res && g_app) g_app->Send(L"R\x1F" + std::to_wstring(-1) + L"\x1F{\"ok\":true,\"items\":" + *res + L"}");
            delete res;
            return 0;
        }
        return 0;
    }
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(hWnd, IsIconic(hWnd) ? SW_RESTORE : SW_SHOW);
            SetForegroundWindow(hWnd);
        }
        else if (lp == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"Открыть ZeroBloat");
            AppendMenuW(hMenu, MF_STRING, 2, L"Свернуть в трей");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 3, L"Выход");
            AppendMenuW(hMenu, MF_STRING, 4, L"Перезапустить от админа");
            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);
            if (cmd == 1) { ShowWindow(hWnd, SW_RESTORE); SetForegroundWindow(hWnd); }
            else if (cmd == 2) ShowWindow(hWnd, SW_HIDE);
            else if (cmd == 3) PostMessageW(hWnd, WM_CLOSE, 0, 0);
            else if (cmd == 4) {
                wchar_t exe[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exe, MAX_PATH);
                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                sei.lpVerb = L"runas";
                sei.lpFile = exe;
                sei.nShow = SW_SHOWNORMAL;
                if (ShellExecuteExW(&sei)) PostMessageW(hWnd, WM_CLOSE, 0, 0);
            }
        }
        return 0;
    case WM_APP_RUNUI: {
        auto* fn = reinterpret_cast<std::function<void()>*>(lp);
        if (fn) {
            try { (*fn)(); }
            catch (const std::exception& e) { WriteLogFile(LogLevel::ERR, L"[UI] " + Utf8ToWide(e.what())); }
            delete fn;
        }
        return 0;
    }
    case WM_CLOSE:
        std::thread([]() { BackupSettings(); }).detach();
        if (g_app) { g_app->Shutdown(); }
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

static bool IsAdmin() {
    BOOL a = FALSE;
    PSID id = nullptr;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &id)) {
        CheckTokenMembership(nullptr, id, &a);
        FreeSid(id);
    }
    return a != FALSE;
}

static bool RelaunchAsAdmin(LPWSTR lpCmdLine) {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.nShow = SW_SHOWNORMAL;
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
            MessageBoxW(nullptr, L"Запуск от имени администратора отменен.", L"ZeroBloat", MB_ICONERROR);
        }
        return false;
    }
    return true;
}

static void BsodMonitorThread() {
    while (!g_stopMonitor) {
        std::wstring minidumpDir = L"C:\\Windows\\Minidump";
        std::error_code ec;
        if (fs::exists(minidumpDir, ec)) {
            for (auto& p : fs::directory_iterator(minidumpDir, ec)) {
                if (!ec && p.is_regular_file()) {
                    auto ftime = fs::last_write_time(p.path(), ec);
                    if (!ec) {
                        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                        auto now = std::chrono::system_clock::now();
                        auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - sctp).count();
                        if (diff >= 0 && diff < 5) {
                            static std::set<std::wstring> notified;
                            std::wstring fname = p.path().filename().wstring();
                            if (notified.find(fname) == notified.end()) {
                                notified.insert(fname);
                                PrintWarn(L"Обнаружен новый дамп BSOD: " + fname);
                            }
                        }
                    }
                }
            }
        }
        for (int i = 0; i < 600 && !g_stopMonitor; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!IsWindowsVersionSupported()) {
        MessageBoxW(nullptr, L"ZeroBloat requires Windows 10 version 1809 (build 17763) or newer.", L"Unsupported OS", MB_ICONERROR);
        return 1;
    }

    SingleInstanceMutex mutex(L"Global\\ZeroBloat_SingleInstance");
    if (mutex.IsAlreadyRunning()) {
        MessageBoxW(nullptr, L"ZeroBloat already running.", L"Info", MB_ICONINFORMATION);
        return 0;
    }

    g_hIconBig = LoadAppIcon(32);
    g_hIconSmall = LoadAppIcon(16);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool skipAdmin = false;
    bool startMinimized = false;
    bool autoClean = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--no-admin") == 0) skipAdmin = true;
        else if (wcscmp(argv[i], L"--silent") == 0) startMinimized = true;
        else if (wcscmp(argv[i], L"--portable") == 0) g_portableMode = true;
        else if (wcscmp(argv[i], L"--auto-clean") == 0) autoClean = true;
        else if (wcscmp(argv[i], L"--help") == 0) {
            MessageBoxW(nullptr, L"ZeroBloat CLI:\n--no-admin : без прав админа\n--silent : свернуто в трей\n--portable : portable-режим\n--auto-clean : только авто-очистка\n--help : это сообщение", L"ZeroBloat Help", MB_OK | MB_ICONINFORMATION);
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

    InitPaths();
    LoadAiConfig();
    LoadDevAccess();
    CheckBetaBuild();
    LoadRollbackStack();
    SetUnhandledExceptionFilter(AppUnhandledExceptionFilter);

    if (autoClean) {
        CleanCacheEnhanced();
        PurgeStandbyEnhanced();
        return 0;
    }

    EnablePrivilege(SE_TAKE_OWNERSHIP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);
    EnablePrivilege(SE_DEBUG_NAME);
    EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME);

    CoInitScope com(COINIT_APARTMENTTHREADED);
    if (!com.IsInitialized()) return 1;
    (void)CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

    if (!ShowBootstrapper(hInst)) return 0;

    g_overlay.Create(hInst);
    LoadOverlaySettings();
    g_overlay.SetPosition(g_overlay.posX.load(), g_overlay.posY.load());
    g_overlay.SetAlpha(g_overlay.alpha.load());

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"ZeroBloatWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = g_hIconBig;
    wc.hIconSm = g_hIconSmall;
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(WS_EX_APPWINDOW, L"ZeroBloatWnd", L"ZeroBloat",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return 1;

    SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIconBig);
    SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSmall);

    ShowWindow(hWnd, startMinimized ? SW_HIDE : SW_SHOW);

    InitTray(hWnd);

    if (startMinimized) {
        g_nid.uFlags |= NIF_INFO;
        wcsncpy_s(g_nid.szInfoTitle, L"ZeroBloat", _TRUNCATE);
        wcsncpy_s(g_nid.szInfo, L"Работает в фоновом режиме", _TRUNCATE);
        g_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    }

    if (!g_portableMode.load()) {
        if (!RegisterHotKey(hWnd, 1, MOD_CONTROL | MOD_ALT, 'O')) PrintErr(L"Failed to register Ctrl+Alt+O");
        if (!RegisterHotKey(hWnd, 2, MOD_CONTROL | MOD_ALT, 'M')) PrintErr(L"Failed to register Ctrl+Alt+M");
        if (!RegisterHotKey(hWnd, 3, MOD_CONTROL | MOD_ALT, 'C')) PrintErr(L"Failed to register Ctrl+Alt+C");
    }

    g_stopMonitor = false;
    g_app = std::make_shared<App>();
    if (!g_app->Initialize(hWnd)) {
        MessageBoxW(hWnd, L"Failed to initialize WebView2 interface.", L"Error", MB_ICONERROR);
        DestroyWindow(hWnd);
        return 1;
    }

    g_monThread = std::thread(MonitorThread);
    g_pingThread = std::thread(PingWorker);
    g_bsodMonitorThread = std::thread(BsodMonitorThread);

    SetTimer(hWnd, 1, 1000, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    KillTimer(hWnd, 1);
    UnregisterHotKey(hWnd, 1);
    UnregisterHotKey(hWnd, 2);
    UnregisterHotKey(hWnd, 3);
    g_stopMonitor = true;
    if (g_monThread.joinable()) g_monThread.join();
    if (g_pingThread.joinable()) g_pingThread.join();
    if (g_bsodMonitorThread.joinable()) g_bsodMonitorThread.join();

    {
        std::unique_lock<std::mutex> lock(g_asyncMutex);
        g_asyncCv.wait_for(lock, std::chrono::seconds(3), [] { return g_activeAsyncTasks.load() == 0; });
    }

    if (g_app) {
        g_app->Shutdown();
        g_app.reset();
    }
    g_overlay.Cleanup();
    CleanupTray();
    if (g_hIconBig) DestroyIcon(g_hIconBig);
    if (g_hIconSmall) DestroyIcon(g_hIconSmall);
    return 0;
}