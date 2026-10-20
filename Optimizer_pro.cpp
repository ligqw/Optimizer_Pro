// optimizer_pro.cpp — C++ порт Optimizer Pro
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wincrypt.h>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")

using namespace Microsoft::WRL;
namespace fs = std::filesystem;

static bool HAS_WEBVIEW_RUNTIME_CHECK();

// ============================================================
// КОНСТАНТЫ
// ============================================================
static const wchar_t* ULTIMATE_GUID = L"e9a42b02-d5df-448d-aa00-03f14749eb61";
static const wchar_t* HIGH_PERF_GUID = L"8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c";
static const DWORD NOCONS = 0x08000000;
static const size_t LOG_MAX_SIZE = 5ull * 1024 * 1024;

static const std::set<std::wstring> HEAVY_APPS = {
    L"chrome.exe", L"firefox.exe", L"msedge.exe", L"opera.exe", L"brave.exe", L"vivaldi.exe",
    L"discord.exe", L"telegram.exe", L"whatsapp.exe", L"skype.exe", L"slack.exe",
    L"obs64.exe", L"obs32.exe", L"spotify.exe", L"thunderbird.exe", L"steam.exe", L"epicgameslauncher.exe"
};

// ============================================================
// ПУТИ
// ============================================================
struct Paths {
    std::wstring SystemRoot, BackupDir, BackupFile, BoostStateFile, LogFile;
    std::wstring SystemDrive, Sys32;
    std::wstring PS_EXE;
};
static Paths g_paths;
static std::wstring g_origTemp;
static std::mutex g_logMutex;

static std::wstring GetEnvW(const wchar_t* name) {
    DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (!n) return L"";
    std::wstring s(n, L'\0');
    GetEnvironmentVariableW(name, &s[0], n);
    s.resize(n - 1);
    return s;
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static void InitPaths() {
    g_paths.SystemRoot = GetEnvW(L"SystemRoot");
    if (g_paths.SystemRoot.empty()) g_paths.SystemRoot = L"C:\\Windows";
    std::wstring appdata = GetEnvW(L"APPDATA");
    if (appdata.empty()) {
        wchar_t buf[MAX_PATH]; SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf);
        appdata = buf;
    }
    g_paths.BackupDir = appdata + L"\\OptimizerPro";
    fs::create_directories(g_paths.BackupDir);
    g_paths.BackupFile = g_paths.BackupDir + L"\\backup.ini";
    g_paths.BoostStateFile = g_paths.BackupDir + L"\\game_boost_state.ini";
    g_paths.LogFile = g_paths.BackupDir + L"\\optimization_log.txt";

    g_paths.SystemDrive = GetEnvW(L"SystemDrive");
    if (g_paths.SystemDrive.empty()) g_paths.SystemDrive = L"C:";
    g_paths.SystemDrive += L"\\";

    BOOL wow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &wow64);
    g_paths.Sys32 = g_paths.SystemRoot + (wow64 ? L"\\Sysnative" : L"\\System32");
    g_paths.PS_EXE = g_paths.Sys32 + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (!fs::exists(g_paths.PS_EXE)) g_paths.PS_EXE = L"powershell.exe";
}

// ============================================================
// ЛОГИРОВАНИЕ + вывод (аналог _UiTee)
// ============================================================
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
        std::string s = WideToUtf8(text) + "\r\n";
        f.write(s.data(), s.size());
    }
    catch (...) {}
}

static void Print(const std::wstring& text) {
    WriteLogFile(text);
    if (g_uiLogSink) g_uiLogSink(text);
}

// ============================================================
// ПРОЦЕССЫ И КОМАНДЫ (subprocess → CreateProcessW)
// ============================================================
struct CmdResult {
    DWORD exitCode = (DWORD)-1;
    std::string out, err;
    bool timedOut = false;
};

static std::atomic<HANDLE> g_currentProc = nullptr;
static std::mutex g_procMutex;

static CmdResult RunCmdImpl(const std::wstring& cmd, int timeoutSec, bool capture) {
    CmdResult res;
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    wchar_t outPath[MAX_PATH], errPath[MAX_PATH];
    GetTempFileNameW(tmp, L"op", 0, outPath);
    GetTempFileNameW(tmp, L"op", 0, errPath);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hOut = CreateFileW(outPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE hErr = CreateFileW(errPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = capture ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = capture ? hErr : GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    std::wstring cl = L"cmd.exe /c " + cmd;
    BOOL ok = CreateProcessW(nullptr, &cl[0], nullptr, nullptr, TRUE,
        NOCONS, nullptr, nullptr, &si, &pi);
    CloseHandle(hOut); CloseHandle(hErr);
    if (!ok) { DeleteFileW(outPath); DeleteFileW(errPath); return res; }

    {
        std::lock_guard<std::mutex> lk(g_procMutex);
        g_currentProc.store(pi.hProcess);
    }
    DWORD wr = WaitForSingleObject(pi.hProcess, timeoutSec * 1000);
    if (wr == WAIT_TIMEOUT) {
        res.timedOut = true;
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
    }
    GetExitCodeProcess(pi.hProcess, &res.exitCode);
    {
        std::lock_guard<std::mutex> lk(g_procMutex);
        g_currentProc.store(nullptr);
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    auto readFile = [](const wchar_t* p) -> std::string {
        std::ifstream f(p, std::ios::binary);
        if (!f) return "";
        std::stringstream ss; ss << f.rdbuf(); return ss.str();
        };
    res.out = readFile(outPath);
    res.err = readFile(errPath);
    DeleteFileW(outPath); DeleteFileW(errPath);
    return res;
}

// Аналог run_cmd: печатает статус, пишет в лог
static bool RunCmd(const std::wstring& cmd, const std::wstring& text,
    bool check = true, int timeoutSec = 300,
    std::initializer_list<DWORD> allowed = { 0 }) {
    if (!text.empty()) Print(L"  [+] " + text + L"...");
    CmdResult r = RunCmdImpl(cmd, timeoutSec, true);
    if (r.timedOut) {
        Print(L"      +-- [ОШИБКА] Таймаут");
        return false;
    }
    for (DWORD a : allowed) {
        if (r.exitCode == a) {
            if (!text.empty()) Print(L"      +-- [OK]");
            return true;
        }
    }
    std::string detail = r.err.empty() ? r.out : r.err;
    if (detail.empty()) detail = "Код " + std::to_string(r.exitCode);
    if (detail.size() > 80) detail = detail.substr(0, 80);
    Print(std::wstring(L"      +-- [") + (check ? L"ОШИБКА" : L"ПРЕДУПРЕЖДЕНИЕ") +
        L"] " + Utf8ToWide(detail));
    return false;
}

// ============================================================
// REGISTRY
// ============================================================
static bool RegReadDword(HKEY h, const std::wstring& path, const std::wstring& name, DWORD& out) {
    HKEY k;
    if (RegOpenKeyExW(h, path.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    DWORD sz = sizeof(DWORD), type = 0;
    LONG rc = RegQueryValueExW(k, name.c_str(), nullptr, &type, (LPBYTE)&out, &sz);
    RegCloseKey(k);
    return rc == ERROR_SUCCESS && type == REG_DWORD;
}

static bool RegWriteDword(HKEY h, const std::wstring& path, const std::wstring& name, DWORD val) {
    HKEY k;
    if (RegCreateKeyExW(h, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    LONG rc = RegSetValueExW(k, name.c_str(), 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
    RegCloseKey(k);
    return rc == ERROR_SUCCESS;
}

static bool RegDeleteValue(HKEY h, const std::wstring& path, const std::wstring& name) {
    HKEY k;
    if (RegOpenKeyExW(h, path.c_str(), 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return false;
    LONG rc = RegDeleteValueW(k, name.c_str());
    RegCloseKey(k);
    return rc == ERROR_SUCCESS;
}

// ============================================================
// БЭКАП (формат INI-подобный, чтобы не тащить JSON-парсер)
// ============================================================
struct BackupData {
    int version = 2;
    std::wstring activeScheme;
    std::vector<std::wstring> createdSchemes;
    std::vector<std::wstring> removedAppx;
    struct SvcData { std::wstring startType; bool isRunning; };
    std::map<std::wstring, SvcData> services;
    bool visualFxExists = false;
    int visualFx = 0;
    bool hibernate = false;
};

class BackupLock {
    HANDLE hMutex = nullptr;
public:
    BackupLock() {
        hMutex = CreateMutexW(nullptr, FALSE, L"Global\\OptimizerPro_BackupLock");
        if (hMutex) WaitForSingleObject(hMutex, 15000);
    }
    ~BackupLock() {
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    }
};

static bool BackupExists() {
    std::error_code ec;
    return fs::exists(g_paths.BackupFile, ec);
}

static bool ParseBackup(const std::wstring& content, BackupData& b) {
    b = BackupData();
    std::wstringstream ss(content);
    std::wstring line;
    bool gotVersion = false;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = line.substr(0, eq);
        std::wstring val = line.substr(eq + 1);
        if (key == L"version") { b.version = std::stoi(val); gotVersion = true; }
        else if (key == L"active_scheme") b.activeScheme = val;
        else if (key == L"created_scheme") b.createdSchemes.push_back(val);
        else if (key == L"removed_appx") b.removedAppx.push_back(val);
        else if (key == L"visual_fx_exists") b.visualFxExists = (val == L"1");
        else if (key == L"visual_fx") b.visualFx = std::stoi(val);
        else if (key == L"hibernate") b.hibernate = (val == L"1");
        else if (key.rfind(L"svc.", 0) == 0) {
            // svc.<name>.start_type = ... / svc.<name>.is_running = 0/1
            auto rest = key.substr(4);
            auto lastDot = rest.rfind(L'.');
            if (lastDot == std::wstring::npos) continue;
            std::wstring svcName = rest.substr(0, lastDot);
            std::wstring field = rest.substr(lastDot + 1);
            if (field == L"start_type") b.services[svcName].startType = val;
            else if (field == L"is_running") b.services[svcName].isRunning = (val == L"1");
        }
    }
    return gotVersion;
}

static bool ReadBackup(BackupData& b) {
    std::ifstream f(g_paths.BackupFile, std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    return ParseBackup(Utf8ToWide(ss.str()), b);
}

static bool IsValidBackup(const BackupData& b) {
    return b.version == 2;
}

static bool WriteBackup(const BackupData& b) {
    std::wostringstream w;
    w << L"version=2\r\n";
    if (!b.activeScheme.empty()) w << L"active_scheme=" << b.activeScheme << L"\r\n";
    for (auto& s : b.createdSchemes) w << L"created_scheme=" << s << L"\r\n";
    for (auto& a : b.removedAppx) w << L"removed_appx=" << a << L"\r\n";
    for (auto& kv : b.services) {
        w << L"svc." << kv.first << L".start_type=" << kv.second.startType << L"\r\n";
        w << L"svc." << kv.first << L".is_running=" << (kv.second.isRunning ? 1 : 0) << L"\r\n";
    }
    w << L"visual_fx_exists=" << (b.visualFxExists ? 1 : 0) << L"\r\n";
    w << L"visual_fx=" << b.visualFx << L"\r\n";
    w << L"hibernate=" << (b.hibernate ? 1 : 0) << L"\r\n";

    std::string utf8 = WideToUtf8(w.str());
    std::wstring tmp = g_paths.BackupFile + L".tmp";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(utf8.data(), utf8.size());
    f.close();
    std::error_code ec;
    fs::rename(tmp, g_paths.BackupFile, ec);
    return !ec;
}

// Чтение схем питания
static std::wstring GetActiveScheme() {
    CmdResult r = RunCmdImpl(L"powercfg /getactivescheme", 15, true);
    std::wstring out = Utf8ToWide(r.out);
    for (size_t i = 0; i + 36 <= out.size(); i++) {
        bool ok = true;
        for (size_t j = 0; j < 36; j++) {
            wchar_t c = out[i + j];
            bool dashPos = (j == 8 || j == 13 || j == 18 || j == 23);
            if (dashPos) { if (c != L'-') { ok = false; break; } }
            else if (!iswxdigit(c)) { ok = false; break; }
        }
        if (ok) return out.substr(i, 36);
    }
    return L"";
}

static std::set<std::wstring> GetAllSchemes() {
    std::set<std::wstring> s;
    CmdResult r = RunCmdImpl(L"powercfg /list", 15, true);
    std::wstring out = Utf8ToWide(r.out);
    for (size_t i = 0; i + 36 <= out.size(); i++) {
        bool ok = true;
        for (size_t j = 0; j < 36; j++) {
            wchar_t c = out[i + j];
            bool dashPos = (j == 8 || j == 13 || j == 18 || j == 23);
            if (dashPos) { if (c != L'-') { ok = false; break; } }
            else if (!iswxdigit(c)) { ok = false; break; }
        }
        if (ok) { std::wstring g = out.substr(i, 36); for (auto& ch : g) ch = towlower(ch); s.insert(g); }
    }
    return s;
}

// Чтение Start/DelayedAutostart для службы
static bool RegGetServiceStart(const std::wstring& svc, DWORD& start, DWORD& delayed) {
    std::wstring path = L"SYSTEM\\CurrentControlSet\\Services\\" + svc;
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS)
        return false;
    DWORD sz = sizeof(DWORD);
    if (RegQueryValueExW(k, L"Start", nullptr, nullptr, (LPBYTE)&start, &sz) != ERROR_SUCCESS)
        start = 3;
    sz = sizeof(DWORD);
    if (RegQueryValueExW(k, L"DelayedAutostart", nullptr, nullptr, (LPBYTE)&delayed, &sz) != ERROR_SUCCESS)
        delayed = 0;
    RegCloseKey(k);
    return true;
}

static std::wstring StartTypeToString(DWORD start, DWORD delayed) {
    if (start == 2 && delayed) return L"delayed-auto";
    switch (start) {
    case 0: return L"boot";
    case 1: return L"system";
    case 2: return L"auto";
    case 3: return L"demand";
    case 4: return L"disabled";
    }
    return L"demand";
}

static bool ServiceIsRunning(const std::wstring& svc) {
    CmdResult r = RunCmdImpl(L"sc query " + svc, 10, true);
    std::string out = r.out;
    // Ищем "RUNNING" в STATE
    auto pos = out.find("RUNNING");
    if (pos != std::string::npos) return true;
    return false;
}

static bool HibernateEnabled() {
    DWORD v;
    if (RegReadDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Power",
        L"HibernateEnabled", v))
        return v != 0;
    return fs::exists(fs::path(g_paths.SystemDrive + L"hiberfil.sys"));
}

static bool SaveBackup() {
    Print(L"  [+] Создание резервной копии...");
    try {
        BackupLock lk;
        if (BackupExists()) {
            BackupData old;
            if (ReadBackup(old) && IsValidBackup(old)) {
                Print(L"      +-- [ПРОПУЩЕНО] Бэкап уже существует.");
                return true;
            }
        }
        BackupData b;
        b.activeScheme = GetActiveScheme();
        for (auto svc : { L"DiagTrack", L"SysMain", L"WSearch" }) {
            DWORD start = 3, delayed = 0;
            if (RegGetServiceStart(svc, start, delayed)) {
                b.services[svc] = { StartTypeToString(start, delayed), ServiceIsRunning(svc) };
            }
        }
        DWORD fx;
        if (RegReadDword(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects",
            L"VisualFXSetting", fx)) {
            b.visualFxExists = true;
            b.visualFx = (int)fx;
        }
        b.hibernate = HibernateEnabled();
        bool ok = WriteBackup(b);
        Print(ok ? L"      +-- [OK]" : L"      +-- [ОШИБКА]");
        return ok;
    }
    catch (const std::exception& e) {
        Print(L"  [!] Ошибка бэкапа: " + Utf8ToWide(e.what()));
        return false;
    }
}

// ============================================================
// RAM
// ============================================================
static ULONGLONG GetFreeRamMb() {
    MEMORYSTATUSEX ms = { sizeof(ms) };
    if (!GlobalMemoryStatusEx(&ms)) return 0;
    return ms.ullAvailPhys / (1024 * 1024);
}

// ============================================================
// TEMP / CACHE
// ============================================================
static void RemoveReadonly(const fs::path& p) {
    try { fs::permissions(p, fs::perms::owner_write | fs::perms::owner_read, fs::perm_options::add); }
    catch (...) {}
}

static void ClearDirContents(const fs::path& dir, bool& ok) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    for (auto& e : fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
        try {
            if (e.is_directory()) {
                std::error_code ec2;
                fs::remove_all(e.path(), ec2);
            }
            else {
                RemoveReadonly(e.path());
                std::error_code ec2;
                fs::remove(e.path(), ec2);
            }
        }
        catch (...) { ok = false; }
    }
}

static bool CleanTempAndCache(bool cleanBrowsers, bool cleanTemp) {
    bool ok = true;
    if (cleanTemp) {
        Print(L"  [+] Очистка Temp...");
        std::vector<fs::path> paths;
        for (auto& s : { GetEnvW(L"TEMP"), g_origTemp, g_paths.SystemRoot + L"\\Temp" }) {
            if (!s.empty() && s.size() > 3) paths.push_back(s);
        }
        bool any = false;
        for (auto& p : paths) {
            std::error_code ec;
            if (fs::is_directory(p, ec)) { ClearDirContents(p, ok); any = true; }
        }
        if (!any) { Print(L"      +-- [ПРЕДУПРЕЖДЕНИЕ] Папки Temp не найдены"); ok = false; }
    }
    if (cleanBrowsers) {
        Print(L"  [+] Очистка кэша браузеров...");
        std::wstring local = GetEnvW(L"LOCALAPPDATA");
        std::wstring roaming = GetEnvW(L"APPDATA");
        std::vector<fs::path> dirs = {
            local + L"\\Google\\Chrome\\User Data\\Default\\Cache",
            local + L"\\Google\\Chrome\\User Data\\Default\\Code Cache",
            roaming + L"\\discord\\Cache",
            roaming + L"\\discord\\Code Cache",
            roaming + L"\\discord\\GPUCache",
            roaming + L"\\Telegram Desktop\\tdata\\user_data\\cache"
        };
        for (auto& d : dirs) ClearDirContents(d, ok);
    }
    return ok;
}

// ============================================================
// СЕТЬ
// ============================================================
static bool ResetNetwork() {
    Print(L"  [+] Сброс сети...");
    bool ok = true;
    if (!RunCmd(L"ipconfig /flushdns", L"Очистка DNS")) ok = false;
    if (!RunCmd(L"netsh winsock reset", L"Сброс Winsock")) ok = false;
    if (!RunCmd(L"netsh int ip reset", L"Сброс IP")) ok = false;
    return ok;
}

// ============================================================
// POWER / SERVICES / EFFECTS
// ============================================================
static bool EnableUltimate() {
    auto before = GetAllSchemes();
    CmdResult r = RunCmdImpl(L"powercfg -duplicatescheme " + std::wstring(ULTIMATE_GUID), 15, true);
    if (r.exitCode == 0) {
        auto after = GetAllSchemes();
        std::wstring newGuid;
        for (auto& g : after) if (!before.count(g)) { newGuid = g; break; }
        if (!newGuid.empty()) {
            bool recorded = false;
            try {
                BackupLock lk;
                if (BackupExists()) {
                    BackupData b;
                    if (ReadBackup(b) && IsValidBackup(b)) {
                        bool has = false;
                        for (auto& g : b.createdSchemes) if (g == newGuid) has = true;
                        if (!has) b.createdSchemes.push_back(newGuid);
                        recorded = WriteBackup(b);
                    }
                }
            }
            catch (...) {}
            if (!recorded) {
                Print(L"  [!] Не удалось записать схему в бэкап — активация отменена");
                RunCmd(L"powercfg /delete " + newGuid, L"", false);
                return false;
            }
            return RunCmd(L"powercfg /setactive " + newGuid, L"Ultimate", false);
        }
    }
    return RunCmd(L"powercfg /setactive " + std::wstring(HIGH_PERF_GUID), L"High Perf", false);
}

// ============================================================
// RESTORE BACKUP
// ============================================================
static bool RestoreBackup() {
    Print(L"  [+] Восстановление...");
    BackupData b;
    {
        BackupLock lk;
        if (!BackupExists()) { Print(L"  [!] Бэкап не найден"); return false; }
        if (!ReadBackup(b) || !IsValidBackup(b)) {
            Print(L"  [!] Бэкап повреждён или несовместим");
            return false;
        }
    }
    std::vector<std::wstring> errors;

    if (!b.activeScheme.empty()) {
        if (!RunCmd(L"powercfg /setactive " + b.activeScheme, L"Схема питания", false))
            errors.push_back(L"схема питания");
    }
    for (auto& g : b.createdSchemes) {
        if (!RunCmd(L"powercfg /delete " + g, L"", false))
            errors.push_back(L"удаление схемы " + g.substr(0, 8));
    }
    for (auto& kv : b.services) {
        if (!RunCmd(L"sc config " + kv.first + L" start= " + kv.second.startType,
            L"Служба " + kv.first, false))
            errors.push_back(L"служба " + kv.first + L" (config)");
        std::wstring action = kv.second.isRunning ? L"start" : L"stop";
        if (!RunCmd(L"sc " + action + L" " + kv.first, L"", false, 300, { 0, 1056, 1062 }))
            errors.push_back(L"служба " + kv.first + L" (state)");
    }
    if (b.visualFxExists) {
        if (!RegWriteDword(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects",
            L"VisualFXSetting", (DWORD)b.visualFx))
            errors.push_back(L"визуальные эффекты");
    }
    else {
        RegDeleteValue(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects",
            L"VisualFXSetting");
    }
    if (!RunCmd(std::wstring(L"powercfg -h ") + (b.hibernate ? L"on" : L"off"),
        L"Гибернация", false))
        errors.push_back(L"гибернация");

    if (errors.empty()) {
        Print(L"      +-- [OK]");
        try { BackupLock lk; std::error_code ec; fs::remove(g_paths.BackupFile, ec); }
        catch (...) {}
        return true;
    }
    std::wstring shortMsg;
    for (size_t i = 0; i < errors.size() && i < 3; i++) {
        if (i) shortMsg += L"; ";
        shortMsg += errors[i];
    }
    Print(L"      +-- [ОШИБКА] Не всё восстановлено: " + shortMsg);
    return false;
}

// ============================================================
// POWERSCHELL -Enc (base64 UTF-16LE)
// ============================================================
static std::string Base64EncodeUtf16LE(const std::wstring& s) {
    DWORD outLen = 0;
    const BYTE* p = reinterpret_cast<const BYTE*>(s.data());
    DWORD inLen = (DWORD)(s.size() * sizeof(wchar_t));
    if (!CryptBinaryToStringA(p, inLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen))
        return "";
    std::string out(outLen, '\0');
    CryptBinaryToStringA(p, inLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

// ============================================================
// DEBLOAT
// ============================================================
static bool IsSafeAppxName(const std::wstring& s) {
    if (s.empty()) return false;
    for (wchar_t c : s) {
        if (!(iswalnum(c) || c == L'.' || c == L'_' || c == L'-')) return false;
    }
    return true;
}

static bool DebloatSelected(const std::vector<std::wstring>& apps) {
    if (!fs::exists(g_paths.PS_EXE)) {
        Print(L"  [!] PowerShell не найден");
        return false;
    }
    if (!SaveBackup()) {
        Print(L"  [!] Не удалось создать бэкап — debloat отменён");
        return false;
    }
    Print(L"  [+] Debloat: " + std::to_wstring(apps.size()) + L" приложений...");
    std::vector<std::wstring> removed, failed;
    for (auto& a : apps) {
        if (!IsSafeAppxName(a)) {
            Print(L"  [!] Пропущено небезопасное имя: " + a);
            failed.push_back(a);
            continue;
        }
        std::wstring script =
            L"$x=Get-AppxPackage *" + a + L"* -AllUsers -EA 0; "
            L"$y=Get-AppxProvisionedPackage -Online -EA 0 | Where {$_.DisplayName -like '*" + a + L"*'}; "
            L"if($x){$x | Remove-AppxPackage -AllUsers -EA 0}; "
            L"if($y){$y | Remove-AppxProvisionedPackage -Online -EA 0 | Out-Null}";
        std::string b64 = Base64EncodeUtf16LE(script);
        std::wstring cmd = L"\"" + g_paths.PS_EXE + L"\" -EP Bypass -NoP -NonInteractive -Enc " + Utf8ToWide(b64);
        CmdResult r = RunCmdImpl(cmd, 120, false);
        if (r.exitCode == 0) removed.push_back(a);
        else failed.push_back(a);
    }
    if (!removed.empty()) {
        try {
            BackupLock lk;
            if (BackupExists()) {
                BackupData b;
                if (ReadBackup(b)) {
                    for (auto& r : removed) b.removedAppx.push_back(r);
                    WriteBackup(b);
                }
            }
        }
        catch (...) {}
    }
    if (!failed.empty()) {
        Print(L"  [!] Не удалено: " + std::to_wstring(failed.size()) + L" из " +
            std::to_wstring(apps.size()));
        return false;
    }
    return true;
}

// ============================================================
// ГЛАВНАЯ ФУНКЦИЯ ОПТИМИЗАЦИИ
// ============================================================
struct OptFlags {
    bool restore = true, clean = true, browsers = true, hibernate = true,
        visualFx = true, power = true, services = true, sfc = false;
};

static bool RunOptBackend(OptFlags f) {
    std::vector<std::wstring> errors;
    bool needsBackup = f.services || f.power || f.hibernate || f.visualFx;
    if (needsBackup && !SaveBackup()) {
        Print(L"[!] Не удалось создать бэкап — обратимые операции пропущены");
        errors.push_back(L"бэкап");
        f.services = f.power = f.hibernate = f.visualFx = false;
    }

    if (f.restore) {
        if (fs::exists(g_paths.PS_EXE)) {
            Print(L"  [+] Создание точки восстановления Windows...");
            std::wstring ps = L"Enable-ComputerRestore -Drive 'C:\\'; "
                L"Checkpoint-Computer -Description 'Optimizer' "
                L"-RestorePointType 'MODIFY_SETTINGS' -EA Stop";
            std::string b64 = Base64EncodeUtf16LE(ps);
            std::wstring cmd = L"\"" + g_paths.PS_EXE + L"\" -EP Bypass -NoP -NonInteractive -Enc " + Utf8ToWide(b64);
            if (!RunCmd(cmd, L"", true, 600)) errors.push_back(L"точка восстановления");
        }
        else {
            Print(L"  [!] PowerShell недоступен — точка восстановления не создана");
            errors.push_back(L"PowerShell");
        }
    }
    if ((f.clean || f.browsers) && !CleanTempAndCache(f.browsers, f.clean))
        errors.push_back(L"очистка кэша");
    if (f.hibernate) {
        if (!RunCmd(L"powercfg -h off", L"Гибернация"))
            errors.push_back(L"гибернация");
    }
    if (f.visualFx) {
        std::wstring path = L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VisualEffects";
        if (!RunCmd(L"reg add \"" + path + L"\" /v \"VisualFXSetting\" /t REG_DWORD /d 2 /f",
            L"Эффекты"))
            errors.push_back(L"эффекты");
    }
    if (f.power && !EnableUltimate()) errors.push_back(L"питание");
    if (f.services) {
        for (auto svc : { L"DiagTrack", L"SysMain", L"WSearch" }) {
            if (!RunCmd(L"sc config " + std::wstring(svc) + L" start= disabled", L"", false))
                errors.push_back(std::wstring(svc) + L" config");
            if (!RunCmd(L"sc stop " + std::wstring(svc), L"", false, 300, { 0, 1062 }))
                errors.push_back(std::wstring(svc) + L" stop");
        }
    }
    if (f.sfc) {
        std::wstring dism = g_paths.Sys32 + L"\\dism.exe";
        std::wstring sfc = g_paths.Sys32 + L"\\sfc.exe";
        if (!RunCmd(L"\"" + dism + L"\" /online /cleanup-image /restorehealth", L"DISM", true, 3600))
            errors.push_back(L"DISM");
        if (!RunCmd(L"\"" + sfc + L"\" /scannow", L"SFC", true, 3600))
            errors.push_back(L"SFC");
    }
    if (!errors.empty()) {
        std::wstring s;
        for (size_t i = 0; i < errors.size() && i < 4; i++) { if (i) s += L", "; s += errors[i]; }
        Print(L"[ОШИБКА] Не выполнено: " + s);
        return false;
    }
    return true;
}

// ============================================================
// GAME BOOST
// ============================================================
struct ProcInfo { DWORD pid; std::wstring name; SIZE_T memBytes; DWORD priority; };

static std::vector<ProcInfo> ListProcesses() {
    std::vector<ProcInfo> v;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return v;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            ProcInfo info = { pe.th32ProcessID, pe.szExeFile, 0, 0 };
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
            if (h) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) info.memBytes = pmc.WorkingSetSize;
                info.priority = GetPriorityClass(h);
                CloseHandle(h);
            }
            v.push_back(info);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return v;
}

static void EmptyWorkingSetPid(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pid);
    if (h) { EmptyWorkingSet(h); CloseHandle(h); }
}

static bool SetPidPriority(DWORD pid, DWORD prio) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return false;
    BOOL ok = SetPriorityClass(h, prio);
    CloseHandle(h);
    return ok != 0;
}

static bool GameBoost(const std::wstring& gameName) {
    std::wstring name = gameName;
    if (name.size() < 4 || name.substr(name.size() - 4) != L".exe") name += L".exe";
    for (auto& c : name) c = towlower(c);

    DWORD gamePid = 0, gamePrio = NORMAL_PRIORITY_CLASS;
    std::vector<std::pair<DWORD, DWORD>> saved;  // pid, priority

    auto procs = ListProcesses();
    for (auto& p : procs) {
        std::wstring n = p.name; for (auto& c : n) c = towlower(c);
        if (n == name && gamePid == 0) { gamePid = p.pid; gamePrio = p.priority; }
        else if (HEAVY_APPS.count(n)) saved.push_back({ p.pid, p.priority });
    }
    if (gamePid == 0) {
        Print(L"  [!] Процесс '" + name + L"' не найден.");
        return false;
    }
    Print(L"  [+] Оптимизация " + name + L" (PID " + std::to_wstring(gamePid) + L")...");

    // Сохраняем состояние
    {
        std::wostringstream w;
        w << L"game_pid=" << gamePid << L"\r\n";
        w << L"game_name=" << name << L"\r\n";
        w << L"game_prio=" << gamePrio << L"\r\n";
        for (auto& kv : saved) w << L"proc." << kv.first << L".nice=" << kv.second << L"\r\n";
        std::string s = WideToUtf8(w.str());
        std::ofstream f(g_paths.BoostStateFile, std::ios::binary | std::ios::trunc);
        if (!f) { Print(L"  [!] Не удалось сохранить состояние"); return false; }
        f.write(s.data(), s.size());
    }

    // EmptyWorkingSet для тяжёлых процессов (RSS > 50MB)
    for (auto& p : procs) {
        std::wstring n = p.name; for (auto& c : n) c = towlower(c);
        if (p.pid == gamePid || HEAVY_APPS.count(n)) {
            if (p.memBytes > 50 * 1024 * 1024) EmptyWorkingSetPid(p.pid);
        }
    }
    SetPidPriority(gamePid, HIGH_PRIORITY_CLASS);
    for (auto& p : procs) {
        std::wstring n = p.name; for (auto& c : n) c = towlower(c);
        if (HEAVY_APPS.count(n)) SetPidPriority(p.pid, IDLE_PRIORITY_CLASS);
    }
    Print(L"      +-- [OK] Приоритеты расставлены");
    return true;
}

static bool BoostStateValid() {
    std::error_code ec;
    return fs::exists(g_paths.BoostStateFile, ec);
}

static bool RestorePriorities() {
    if (!BoostStateValid()) { Print(L"  [!] Нет активного Game Boost"); return false; }
    std::map<std::wstring, std::wstring> kv;
    {
        std::ifstream f(g_paths.BoostStateFile, std::ios::binary);
        std::stringstream ss; ss << f.rdbuf();
        std::wstringstream ws(Utf8ToWide(ss.str()));
        std::wstring line;
        while (std::getline(ws, line)) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            auto eq = line.find(L'=');
            if (eq != std::wstring::npos) kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
    }
    if (kv.count(L"game_pid") && kv.count(L"game_prio")) {
        DWORD pid = std::stoul(kv[L"game_pid"]);
        DWORD prio = std::stoul(kv[L"game_prio"]);
        SetPidPriority(pid, prio);
    }
    for (auto& e : kv) {
        // proc.<pid>.nice = <val>
        if (e.first.rfind(L"proc.", 0) != 0) continue;
        auto rest = e.first.substr(5);
        auto dot = rest.find(L'.');
        if (dot == std::wstring::npos) continue;
        DWORD pid = std::stoul(rest.substr(0, dot));
        SetPidPriority(pid, std::stoul(e.second));
    }
    std::error_code ec; fs::remove(g_paths.BoostStateFile, ec);
    Print(L"  [+] Приоритеты восстановлены.");
    return true;
}

// ============================================================
// АВТОЗАГРУЗКА
// ============================================================
struct AutoItem { std::wstring name, path, source, type; };

static void EnumRunKey(HKEY hive, const std::wstring& sub, const std::wstring& label,
    const std::wstring& type, std::vector<AutoItem>& out) {
    HKEY k;
    if (RegOpenKeyExW(hive, sub.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    DWORD idx = 0;
    wchar_t nameBuf[512], valBuf[2048];
    DWORD nameLen, valLen, typev;
    while (true) {
        nameLen = 512; valLen = sizeof(valBuf);
        LONG rc = RegEnumValueW(k, idx++, nameBuf, &nameLen, nullptr, &typev,
            (LPBYTE)valBuf, &valLen);
        if (rc != ERROR_SUCCESS) break;
        out.push_back({ std::wstring(nameBuf, nameLen), std::wstring(valBuf, valLen / 2), label, type });
    }
    RegCloseKey(k);
}

static std::vector<AutoItem> GetAutostart() {
    std::vector<AutoItem> items;
    EnumRunKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"Пользователь (HKCU)", L"reg_hkcu", items);
    EnumRunKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"Система (HKLM)", L"reg_hklm", items);
    for (auto& [dir, label, type] : {
            std::tuple<std::wstring,std::wstring,std::wstring>{
                GetEnvW(L"APPDATA") + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup",
                L"Папка (Пользователь)", L"folder_user"},
            std::tuple<std::wstring,std::wstring,std::wstring>{
                GetEnvW(L"ProgramData") + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup",
                L"Папка (Система)", L"folder_sys"}
        }) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        for (auto& e : fs::directory_iterator(dir, ec)) {
            auto ext = e.path().extension().wstring();
            for (auto& c : ext) c = towlower(c);
            if (ext == L".lnk" || ext == L".exe") {
                items.push_back({ e.path().filename().wstring(), e.path().wstring(), label, type });
            }
        }
    }
    return items;
}

static bool DeleteAutostart(const AutoItem& it) {
    try {
        if (it.type == L"reg_hkcu" || it.type == L"reg_hklm") {
            HKEY hive = (it.type == L"reg_hkcu") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
            return RegDeleteValue(hive, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", it.name);
        }
        if (it.type == L"folder_user" || it.type == L"folder_sys") {
            std::wstring validDir = (it.type == L"folder_user")
                ? GetEnvW(L"APPDATA") + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup"
                : GetEnvW(L"ProgramData") + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
            auto parent = fs::path(it.path).parent_path();
            auto valid = fs::path(validDir);
            if (fs::weakly_canonical(parent) != fs::weakly_canonical(valid)) return false;
            RemoveReadonly(it.path);
            std::error_code ec; return fs::remove(it.path, ec);
        }
    }
    catch (...) {}
    return false;
}

// ============================================================
// JSON-помощник (только сериализация)
// ============================================================
class Json {
public:
    static std::wstring Escape(const std::wstring& s) {
        std::wstring r;
        for (wchar_t c : s) {
            switch (c) {
            case L'"':  r += L"\\\""; break;
            case L'\\': r += L"\\\\"; break;
            case L'\n': r += L"\\n";  break;
            case L'\r': r += L"\\r";  break;
            case L'\t': r += L"\\t";  break;
            default:
                if (c < 32) { wchar_t buf[8]; swprintf_s(buf, L"\\u%04x", (int)c); r += buf; }
                else r += c;
            }
        }
        return r;
    }
    static std::wstring Str(const std::wstring& s) { return L"\"" + Escape(s) + L"\""; }
    static std::wstring Bool(bool b) { return b ? L"true" : L"false"; }
    static std::wstring Int(long long n) { return std::to_wstring(n); }
};

// ============================================================
// WEBVIEW2 HOST
// ============================================================
const char* HTML_UI = R"HTML(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<style>
:root{--bg:#0e1116;--bg2:#161b22;--surface:#1c2229;--surface2:#242c35;--border:#2d3641;--text:#e6edf3;--text-dim:#8b949e;--accent:#4f8cff;--success:#3fb950;--warning:#d29922;--error:#f85149}
*{box-sizing:border-box;margin:0;padding:0}html,body{height:100%}
body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);overflow:hidden;font-size:14px}
.app{display:grid;grid-template-columns:220px 1fr;height:100vh}
.sidebar{background:var(--bg2);border-right:1px solid var(--border);padding:20px 0;display:flex;flex-direction:column}
.logo{padding:0 20px 20px;font-size:18px;font-weight:700;color:var(--accent)}
.logo::before{content:"⚡ "}
.nav-item{padding:12px 20px;cursor:pointer;color:var(--text-dim);border-left:3px solid transparent;display:flex;gap:12px}
.nav-item:hover{background:var(--surface);color:var(--text)}
.nav-item.active{background:var(--surface);color:var(--accent);border-left-color:var(--accent)}
.nav-spacer{flex:1}.nav-danger{color:var(--error)}
.main{display:flex;flex-direction:column;overflow:hidden}
.content{flex:1;overflow-y:auto;padding:30px 36px}
.tab{display:none}.tab.active{display:block}
h1{font-size:24px;margin-bottom:6px}h2{font-size:16px;margin:24px 0 12px}
.subtitle{color:var(--text-dim);margin-bottom:24px;font-size:13px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-bottom:24px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:16px}
.card-label{color:var(--text-dim);font-size:11px;text-transform:uppercase;margin-bottom:6px}
.card-value{font-size:20px;font-weight:600}
.card-value.good{color:var(--success)}.card-value.bad{color:var(--error)}.card-value.warn{color:var(--warning)}
.section{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:20px;margin-bottom:16px}
.check-item{display:flex;align-items:center;padding:10px 0;border-bottom:1px solid var(--border);cursor:pointer}
.check-item input{margin-right:12px;width:18px;height:18px;accent-color:var(--accent)}
button{background:var(--accent);color:#fff;border:none;padding:10px 20px;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;display:inline-flex;align-items:center;gap:8px}
button:hover:not(:disabled){background:#3a6ecb}
button.danger{background:var(--error)}button.danger:hover:not(:disabled){background:#c94040}
button.secondary{background:var(--surface2);color:var(--text)}
button:disabled{opacity:.5;cursor:not-allowed}
.btn-spinner{width:14px;height:14px;border:2px solid rgba(255,255,255,0.3);border-top-color:#fff;border-radius:50%;animation:spin 1s linear infinite;display:none}
button.loading .btn-spinner{display:block}
@keyframes spin{to{transform:rotate(360deg)}}
.input-row{display:flex;gap:10px;margin-bottom:12px}
input[type="text"]{flex:1;background:var(--bg2);border:1px solid var(--border);color:var(--text);padding:10px 14px;border-radius:8px;font-size:14px;outline:none}
.log{background:#0a0d12;border:1px solid var(--border);border-radius:8px;padding:12px;font-family:monospace;font-size:12.5px;overflow-y:auto;white-space:pre-wrap;height:100%}
.log-line.err{color:var(--error)}.log-line.ok{color:var(--success)}.log-line.warn{color:var(--warning)}.log-line.header{color:var(--accent);font-weight:600}
.status-bar{padding:10px 36px;background:var(--bg2);border-top:1px solid var(--border);font-size:12px;color:var(--text-dim);display:flex;gap:12px}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--success)}
.status-dot.busy{background:var(--warning);animation:pulse 1s infinite}
@keyframes pulse{50%{opacity:.3}}
.autostart-row{display:flex;align-items:center;padding:12px 16px;background:var(--surface);border:1px solid var(--border);border-radius:8px;margin-bottom:8px}
.autostart-row .info{flex:1;margin:0 12px;overflow:hidden}
.autostart-row .name{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.autostart-row .path{font-size:11px;color:var(--text-dim);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.autostart-row .badge{font-size:10px;background:var(--surface2);padding:2px 6px;border-radius:4px;color:var(--text-dim)}
.hint{background:rgba(79,140,255,.1);border-left:3px solid var(--accent);padding:12px 16px;border-radius:6px;font-size:13px;color:var(--text-dim);margin:12px 0}
.hint b{color:var(--text)}
.toast-container{position:fixed;bottom:24px;right:24px;z-index:9999;display:flex;flex-direction:column;gap:10px}
.toast{background:var(--surface);border-left:4px solid var(--accent);padding:12px 20px;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.5);transition:opacity .3s}
.toast.success{border-left-color:var(--success)}.toast.error{border-left-color:var(--error)}
</style>
</head>
<body>
<div class="app">
<div class="sidebar">
<div class="logo">Optimizer Pro</div>
<div class="nav-item active" data-tab="home"><span>📊</span> Главная</div>
<div class="nav-item" data-tab="optimize"><span>🚀</span> Оптимизация</div>
<div class="nav-item" data-tab="boost"><span>🎮</span> Game Boost</div>
<div class="nav-item" data-tab="debloat"><span>🧹</span> Debloat</div>
<div class="nav-item" data-tab="autostart"><span>⚡</span> Автозагрузка</div>
<div class="nav-item" data-tab="network"><span>🌐</span> Сеть</div>
<div class="nav-item" data-tab="log"><span>📝</span> Лог</div>
<div class="nav-spacer"></div>
<div class="nav-item nav-danger" data-tab="restore"><span>↩</span> Откатить всё</div>
</div>
<div class="main">
<div class="content">
<div class="tab active" id="tab-home">
<h1>Панель управления</h1>
<p class="subtitle">Обзор состояния системы</p>
<div class="cards" id="status-cards"></div>
<div class="section">
<h2>Быстрый старт</h2>
<div class="hint"><b>Полная оптимизация</b> выполнит все безопасные действия: точку восстановления, очистку Temp и кэшей, отключение гибернации, настройку питания и проверку SFC/DISM.</div>
<button onclick="fullOptimize(this)"><span class="btn-spinner"></span> 🚀 Запустить полную оптимизацию</button>
</div>
</div>
<div class="tab" id="tab-optimize">
<h1>Оптимизация</h1>
<p class="subtitle">Выберите нужные действия</p>
<div class="section">
<div class="check-item" onclick="toggle('opt-restore')"><input type="checkbox" id="opt-restore" checked onclick="event.stopPropagation()"><label>Создать точку восстановления Windows</label></div>
<div class="check-item" onclick="toggle('opt-clean')"><input type="checkbox" id="opt-clean" checked onclick="event.stopPropagation()"><label>Очистка Temp</label></div>
<div class="check-item" onclick="toggle('opt-browsers')"><input type="checkbox" id="opt-browsers" checked onclick="event.stopPropagation()"><label>Очистка кэша браузеров и мессенджеров</label></div>
<div class="check-item" onclick="toggle('opt-hibernate')"><input type="checkbox" id="opt-hibernate" checked onclick="event.stopPropagation()"><label>Отключить гибернацию</label></div>
<div class="check-item" onclick="toggle('opt-visualfx')"><input type="checkbox" id="opt-visualfx" checked onclick="event.stopPropagation()"><label>Отключить визуальные эффекты Windows</label></div>
<div class="check-item" onclick="toggle('opt-power')"><input type="checkbox" id="opt-power" checked onclick="event.stopPropagation()"><label>Максимальная производительность (Ultimate)</label></div>
<div class="check-item" onclick="toggle('opt-services')"><input type="checkbox" id="opt-services" checked onclick="event.stopPropagation()"><label>Отключить службы телеметрии</label></div>
<div class="check-item" onclick="toggle('opt-sfc')"><input type="checkbox" id="opt-sfc" onclick="event.stopPropagation()"><label>Проверка целостности (SFC / DISM)</label></div>
</div>
<button onclick="runSelective(this)"><span class="btn-spinner"></span> ▶ Запустить выбранное</button>
</div>
<div class="tab" id="tab-boost">
<h1>Game Boost</h1>
<p class="subtitle">Максимальный FPS и умный приоритет</p>
<div id="boost-idle">
<div class="section">
<h2>Введите имя процесса игры</h2>
<div class="input-row">
<input type="text" id="boost-name" placeholder="cs2.exe, valorant.exe, ...">
<button onclick="startBoost(this)"><span class="btn-spinner"></span> 🎮 Активировать</button>
</div>
<div class="hint">Приоритет игры будет поднят до <b>HIGH</b>. Браузеры и Discord будут опущены до <b>IDLE</b>.</div>
</div>
<div class="section">
<h2>Топ процессов по памяти</h2>
<div id="proc-list" style="max-height:300px;overflow-y:auto;border:1px solid var(--border);border-radius:8px"></div>
<div style="margin-top:12px"><button class="secondary" onclick="refreshProcs()">🔄 Обновить</button></div>
</div>
</div>
<div id="boost-active" style="display:none"></div>
</div>
<div class="tab" id="tab-debloat">
<h1>Debloat (Выборочно)</h1>
<p class="subtitle">Удаление встроенных приложений</p>
<div class="hint">Приложения из Xbox <b>не удалены</b> по умолчанию, чтобы не сломать Game Pass.</div>
<div class="section" id="debloat-list"></div>
<button class="danger" onclick="runDebloat(this)"><span class="btn-spinner"></span> 🗑 Удалить выбранные</button>
</div>
<div class="tab" id="tab-autostart">
<h1>Автозагрузка</h1>
<p class="subtitle">Полный список (HKCU, HKLM, Папки Startup)</p>
<div id="autostart-list"></div>
</div>
<div class="tab" id="tab-network">
<h1>Сброс сети</h1>
<p class="subtitle">Решение проблем с пингом</p>
<div class="section">
<div class="hint">Сбрасывает кэш DNS, Winsock и IP-стек. <b>Перезагрузите ПК после сброса.</b></div>
<button onclick="resetNetwork(this)"><span class="btn-spinner"></span> 🌐 Выполнить сброс сети</button>
</div>
</div>
<div class="tab" id="tab-log" style="height:calc(100vh - 130px);display:none;flex-direction:column">
<h1>Лог</h1>
<div class="log" id="log-container" style="flex:1"></div>
<div style="margin-top:16px;display:flex;gap:10px">
<button class="secondary" onclick="document.getElementById('log-container').innerHTML=''">Очистить</button>
<button class="secondary" onclick="openLog()">Открыть файл</button>
</div>
</div>
<div class="tab" id="tab-restore">
<h1>Откатить изменения</h1>
<p class="subtitle">Возврат к состоянию до оптимизации</p>
<div class="hint" id="restore-info">Проверка...</div>
<button class="danger" onclick="doRestore(this)"><span class="btn-spinner"></span> ↩ Восстановить</button>
</div>
</div>
<div class="status-bar">
<div class="status-dot" id="status-dot"></div>
<span id="status-text">Готово</span>
</div>
</div>
</div>
<div class="toast-container" id="toasts"></div>
<script>
/* ---- Мост C++ ---- */
(function(){
    let msgId = 0;
    const pending = {};
    window.chrome.webview.addEventListener('message', (e) => {
        const raw = e.data;
        const parts = raw.split('\x1F');
        const t = parts[0];
        if (t === 'L') { window.__log && window.__log(parts.slice(1).join('\x1F')); }
        else if (t === 'D') { try { window.__done && window.__done(JSON.parse(parts[1])); } catch(_){} }
        else if (t === 'R') {
            const id = parseInt(parts[1]);
            const p = pending[id]; if (!p) return;
            delete pending[id];
            try { p.resolve(JSON.parse(parts[2])); } catch(e) { p.reject(e); }
        }
    });
    window.pywebview = {
        api: new Proxy({}, { get: (_, method) => (...args) => new Promise((resolve, reject) => {
            const id = ++msgId;
            pending[id] = { resolve, reject };
            const flat = [];
            args.forEach(a => {
                if (Array.isArray(a)) a.forEach(x => flat.push(typeof x === 'boolean' ? (x ? "1" : "0") : String(x)));
                else if (typeof a === 'boolean') flat.push(a ? "1" : "0");
                else flat.push(String(a));
            });
            const msg = `C\x1F${id}\x1F${method}\x1F${flat.length}\x1F${flat.join('\x1F')}`;
            window.chrome.webview.postMessage(msg);
        })})
    };
})();

const py = () => window.pywebview.api;
const DEBLOAT_APPS = ["BingWeather","GetHelp","Getstarted","MicrosoftOfficeHub","SkypeApp","XboxApp","ZuneMusic","MicrosoftSolitaireCollection","Clipchamp","Teams"];
function esc(s){const d=document.createElement('div');d.textContent=String(s);return d.innerHTML;}

document.querySelectorAll('.nav-item').forEach(el => {
    el.addEventListener('click', () => {
        document.querySelectorAll('.nav-item').forEach(e => e.classList.remove('active'));
        el.classList.add('active');
        switchTab(el.dataset.tab);
    });
});
function switchTab(name){
    document.querySelectorAll('.tab').forEach(t => { t.classList.remove('active'); t.style.display=''; });
    const t = document.getElementById('tab-'+name);
    if (t) { t.classList.add('active'); if (name==='log') t.style.display='flex'; }
    if (name==='autostart') loadAutostart();
    if (name==='boost') refreshProcs();
    if (name==='restore') loadRestoreInfo();
    if (name==='home') loadStatus();
}
function toggle(id){const c=document.getElementById(id);if(c)c.checked=!c.checked;}
function toast(msg,type=''){const c=document.getElementById('toasts');const t=document.createElement('div');t.className='toast '+type;t.textContent=msg;c.appendChild(t);setTimeout(()=>{t.style.opacity='0';setTimeout(()=>t.remove(),300);},3000);}
window.__log = (text) => {
    const c = document.getElementById('log-container'); if (!c) return;
    const line = document.createElement('div'); line.className = 'log-line';
    if (text.includes('[ОШИБКА]')) line.classList.add('err');
    else if (text.includes('[OK]')) line.classList.add('ok');
    else if (text.includes('[ПРЕДУПРЕЖДЕНИЕ]') || text.includes('[!]')) line.classList.add('warn');
    else if (text.startsWith('  [+]')) line.classList.add('header');
    line.textContent = text; c.appendChild(line); c.scrollTop = c.scrollHeight;
};
function setBusy(btn,busy,text){
    document.getElementById('status-dot').className='status-dot '+(busy?'busy':'');
    document.getElementById('status-text').textContent=text||(busy?'Выполняется...':'Готово');
    if (btn) { btn.classList.toggle('loading', busy); btn.disabled = busy; }
}
window.__done = (result) => {
    document.querySelectorAll('button').forEach(b => { b.disabled = false; b.classList.remove('loading'); });
    document.getElementById('status-dot').className = 'status-dot';
    document.getElementById('status-text').textContent = 'Готово';
    if (result && result.ok === false) toast(result.error || 'Ошибка', 'error');
    else toast('Операция завершена', 'success');
    if (document.getElementById('tab-home').classList.contains('active')) loadStatus();
    if (document.getElementById('tab-boost').classList.contains('active')) refreshProcs();
    if (document.getElementById('tab-restore').classList.contains('active')) loadRestoreInfo();
};
async function callAsync(btn,statusText,method,...args){
    setBusy(btn,true,statusText);
    try { const r = await py()[method](...args);
        if (r && r.started === false) { setBusy(btn,false); toast(r.error||'Уже выполняется','error'); }
    } catch(e){ setBusy(btn,false); toast('Сбой API: '+e,'error'); }
}
async function loadStatus(){
    try {
        const s = await py().get_status();
        document.getElementById('status-cards').innerHTML = `
            <div class="card"><div class="card-label">ОЗУ свободно</div><div class="card-value good">${Number(s.ram_free)||0} МБ</div></div>
            <div class="card"><div class="card-label">Права админа</div><div class="card-value ${s.admin?'good':'bad'}">${s.admin?'ДА':'НЕТ'}</div></div>
            <div class="card"><div class="card-label">Бэкап</div><div class="card-value ${s.has_backup?'good':'warn'}">${s.has_backup?'Есть':'Нет'}</div></div>
            <div class="card"><div class="card-label">Game Boost</div><div class="card-value ${s.has_boost?'warn':'good'}">${s.has_boost?'Активен':'Свободен'}</div></div>`;
    } catch(e){ toast('Ошибка обновления статуса','error'); }
}
async function fullOptimize(btn){
    if (!confirm('Полная оптимизация включает SFC и DISM — может занять 1–2 часа. Продолжить?')) return;
    window.__log('=== ПОЛНАЯ ОПТИМИЗАЦИЯ ===');
    await callAsync(btn, 'Полная оптимизация...', 'run_full_optimization');
}
async function runSelective(btn){
    const flags = [
        document.getElementById('opt-restore').checked,
        document.getElementById('opt-clean').checked,
        document.getElementById('opt-browsers').checked,
        document.getElementById('opt-hibernate').checked,
        document.getElementById('opt-visualfx').checked,
        document.getElementById('opt-power').checked,
        document.getElementById('opt-services').checked,
        document.getElementById('opt-sfc').checked
    ];
    await callAsync(btn, 'Выполнение...', 'run_selective', flags);
}
async function refreshProcs(){
    try {
        const data = await py().get_processes();
        const list = document.getElementById('proc-list');
        list.innerHTML = '<div style="padding:8px 12px;color:var(--text-dim);font-size:11px;background:var(--bg2);border-bottom:1px solid var(--border)">Процесс — ОЗУ</div>';
        for (const p of data.procs) {
            const row = document.createElement('div');
            row.style = "padding:8px 12px;border-bottom:1px solid var(--border);cursor:pointer;display:flex;justify-content:space-between;";
            row.onmouseover = () => row.style.background = 'var(--surface2)';
            row.onmouseout = () => row.style.background = 'transparent';
            row.innerHTML = `<span>${esc(p.name)} <span style="color:var(--text-dim);font-size:11px">(PID ${Number(p.pid)||0})</span></span><span style="color:var(--text-dim);font-size:12px">${Number(p.mem_mb)||0} МБ</span>`;
            row.onclick = () => { document.getElementById('boost-name').value = p.name; };
            list.appendChild(row);
        }
        const info = await py().get_boost_info();
        document.getElementById('boost-idle').style.display = info.active ? 'none' : 'block';
        const activeDiv = document.getElementById('boost-active');
        activeDiv.style.display = info.active ? 'block' : 'none';
        if (info.active) activeDiv.innerHTML = `<div class="section"><h2>Game Boost активен</h2><div class="hint">Игра: <b>${esc(info.game_name)}</b></div><button class="danger" onclick="restorePriors(this)"><span class="btn-spinner"></span> ⏹ Остановить</button></div>`;
    } catch(e){ toast('Ошибка загрузки процессов','error'); }
}
async function startBoost(btn){
    const name = document.getElementById('boost-name').value.trim();
    if (!name) { toast('Введите имя','error'); return; }
    await callAsync(btn, 'Game Boost...', 'start_game_boost', name);
}
async function restorePriors(btn){ await callAsync(btn, 'Восстановление...', 'restore_priorities'); }
function initDebloat(){
    const c = document.getElementById('debloat-list'); c.innerHTML = '';
    DEBLOAT_APPS.forEach(app => {
        const isXbox = app.includes('Xbox');
        const div = document.createElement('div');
        div.className = 'check-item';
        div.onclick = () => { const ch = document.getElementById('deb-'+app); if (ch) ch.checked = !ch.checked; };
        div.innerHTML = `<input type="checkbox" id="deb-${esc(app)}" ${isXbox?'':'checked'} onclick="event.stopPropagation()"><label>${esc(app)} ${isXbox?'<span style="color:var(--warning);font-size:11px">(Game Pass)</span>':''}</label>`;
        c.appendChild(div);
    });
}
async function runDebloat(btn){
    const selected = DEBLOAT_APPS.filter(a => document.getElementById('deb-'+a)?.checked);
    if (!selected.length) { toast('Ничего не выбрано','error'); return; }
    if (!confirm(`Удалить ${selected.length} приложений?`)) return;
    await callAsync(btn, 'Debloat...', 'run_debloat', selected);
}
async function loadAutostart(){
    const list = document.getElementById('autostart-list');
    list.innerHTML = '<div class="hint">Загрузка...</div>';
    try {
        const data = await py().get_autostart();
        if (!data.items.length) { list.innerHTML = '<div class="hint">Пусто.</div>'; return; }
        list.innerHTML = '';
        for (const item of data.items) {
            const row = document.createElement('div');
            row.className = 'autostart-row';
            const badge = document.createElement('span');
            badge.className = 'badge'; badge.textContent = item.source;
            const info = document.createElement('div'); info.className = 'info';
            const nameEl = document.createElement('div'); nameEl.className = 'name'; nameEl.textContent = item.name;
            const pathEl = document.createElement('div'); pathEl.className = 'path'; pathEl.textContent = item.path;
            info.appendChild(nameEl); info.appendChild(pathEl);
            const btn = document.createElement('button'); btn.className = 'danger';
            btn.innerHTML = '<span class="btn-spinner"></span> Удалить';
            btn.onclick = () => delAutostart(btn, item);
            row.appendChild(badge); row.appendChild(info); row.appendChild(btn);
            list.appendChild(row);
        }
    } catch(e){ list.innerHTML = '<div class="hint" style="color:var(--error)">Ошибка загрузки</div>'; }
}
async function delAutostart(btn, item){
    if (!confirm('Удалить: ' + item.name + '?')) return;
    setBusy(btn, true);
    try { const r = await py().delete_autostart(item.name, item.path, item.source, item.type);
        if (r.ok) { await loadAutostart(); toast('Удалено','success'); } else toast('Не удалось удалить','error');
    } catch(e){ toast('Сбой API','error'); } finally { setBusy(btn, false); }
}
async function resetNetwork(btn){
    if (!confirm('Сброс сети требует перезагрузки ПК. Продолжить?')) return;
    await callAsync(btn, 'Сброс...', 'run_network_reset');
}
async function openLog(){
    try { const r = await py().open_log_file(); if (!r.ok) toast(r.error||'Не удалось открыть лог','error'); }
    catch(e){ toast('Сбой API','error'); }
}
async function loadRestoreInfo(){
    try {
        const s = await py().get_status();
        const info = document.getElementById('restore-info');
        info.innerHTML = s.has_backup ? '<b>Бэкап найден.</b> Можно откатить настройки.' : '<b>Бэкап не найден.</b>';
    } catch(e){ document.getElementById('restore-info').textContent = 'Ошибка проверки'; }
}
async function doRestore(btn){
    if (!confirm('Восстановить настройки?')) return;
    window.__log('=== ОТКАТ ===');
    await callAsync(btn, 'Восстановление...', 'run_restore_backup');
}
window.addEventListener('pywebviewready', () => { loadStatus(); initDebloat(); window.__log('Optimizer Pro готов.'); });
// WebView2 не генерирует pywebviewready — вызываем принудительно
setTimeout(() => { loadStatus(); initDebloat(); window.__log('Optimizer Pro готов.'); }, 100);
</script>
</body>
</html>)HTML";

// ------------------------- WebView2 host -------------------------
class App {
public:
    HWND hwnd = nullptr;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;

    std::atomic<bool> busy{ false };
    std::mutex busyLock;

    bool Initialize(HWND hWnd) {
        hwnd = hWnd;
        // Устанавливаем глобальный log-sink
        g_uiLogSink = [this](const std::wstring& s) { SendLog(s); };

        ICoreWebView2EnvironmentOptions* options = nullptr;
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, options,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result) || !env) return result;
                    env->CreateCoreWebView2Controller(hwnd,
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this, env](HRESULT r, ICoreWebView2Controller* ctrl) -> HRESULT {
                                if (FAILED(r) || !ctrl) return r;
                                controller = ctrl;
                                controller->get_CoreWebView2(&webview);
                                // Заполняем на весь клиент
                                RECT rc; GetClientRect(hwnd, &rc);
                                controller->put_Bounds(rc);
                                // Вставляем HTML
                                webview->NavigateToString(Utf8ToWide(HTML_UI).c_str());
                                // Обработчик сообщений
                                webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR msg = nullptr;
                                            args->TryGetWebMessageAsString(&msg);
                                            if (msg) { OnMessage(std::wstring(msg)); CoTaskMemFree(msg); }
                                            return S_OK;
                                        }).Get(), nullptr);
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());

        return SUCCEEDED(hr);
    }

    void Resize(int w, int h) {
        if (controller) {
            RECT rc = { 0, 0, w, h };
            controller->put_Bounds(rc);
        }
    }

    void Send(const std::wstring& msg) {
        if (webview) webview->PostWebMessageAsString(msg.c_str());
    }

    void SendLog(const std::wstring& s) { Send(L"L\x1F" + s); }

    void SendDone(bool ok, const std::wstring& error = L"") {
        std::wstring json = L"{\"ok\":" + Json::Bool(ok);
        if (!error.empty()) json += L",\"error\":" + Json::Str(error);
        json += L"}";
        Send(L"D\x1F" + json);
    }

    void SendResp(int id, const std::wstring& json) {
        Send(L"R\x1F" + std::to_wstring(id) + L"\x1F" + json);
    }

    // -------- API dispatch --------
    void OnMessage(const std::wstring& raw) {
        // C \x1F <id> \x1F <method> \x1F <argc> \x1F <arg1> ...
        if (raw.size() < 4 || raw[0] != L'C') return;
        std::vector<std::wstring> parts = Split(raw, L'\x1F');
        if (parts.size() < 4) return;
        int id = std::stoi(parts[1]);
        std::wstring method = parts[2];
        int argc = std::stoi(parts[3]);
        std::vector<std::wstring> args(parts.begin() + 4, parts.end());
        if ((int)args.size() < argc) argc = (int)args.size();

        if (method == L"get_status")            return SendResp(id, SyncGetStatus());
        if (method == L"get_processes")         return SendResp(id, SyncGetProcesses());
        if (method == L"get_boost_info")        return SendResp(id, SyncGetBoostInfo());
        if (method == L"get_autostart")         return SendResp(id, SyncGetAutostart());
        if (method == L"delete_autostart")      return SendResp(id, SyncDeleteAutostart(args));
        if (method == L"open_log_file")         return SendResp(id, SyncOpenLog());

        if (method == L"run_full_optimization")  return Async([this] { return RunOptBackend(OptFlags{}); }, id);
        if (method == L"run_selective")          return AsyncSelective(args, id);
        if (method == L"run_debloat")            return AsyncDebloat(args, id);
        if (method == L"run_restore_backup")     return Async([this] { return RestoreBackup(); }, id);
        if (method == L"start_game_boost")       return AsyncBoost(args.empty() ? L"" : args[0], id);
        if (method == L"restore_priorities")     return Async([this] { return RestorePriorities(); }, id);
        if (method == L"run_network_reset")      return Async([this] { return ResetNetwork(); }, id);

        SendResp(id, L"{\"ok\":false,\"error\":\"Unknown method\"}");
    }

    // -------- Sync handlers --------
    std::wstring SyncGetStatus() {
        std::wstring j = L"{\"ram_free\":" + Json::Int((long long)GetFreeRamMb());
        // admin
        BOOL isAdmin = FALSE; PSID id = nullptr; SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &id)) {
            CheckTokenMembership(nullptr, id, &isAdmin);
            FreeSid(id);
        }
        j += L",\"admin\":" + Json::Bool(isAdmin != FALSE);
        j += L",\"has_backup\":" + Json::Bool(BackupExists());
        j += L",\"has_boost\":" + Json::Bool(BoostStateValid());
        j += L"}";
        return j;
    }
    std::wstring SyncGetProcesses() {
        auto procs = ListProcesses();
        std::sort(procs.begin(), procs.end(),
            [](const ProcInfo& a, const ProcInfo& b) { return a.memBytes > b.memBytes; });
        std::wstring j = L"{\"procs\":[";
        size_t n = std::min<size_t>(procs.size(), 30);
        for (size_t i = 0; i < n; i++) {
            if (i) j += L",";
            j += L"{\"name\":" + Json::Str(procs[i].name);
            j += L",\"pid\":" + Json::Int(procs[i].pid);
            j += L",\"mem_mb\":" + Json::Int((long long)(procs[i].memBytes / (1024 * 1024))) + L"}";
        }
        j += L"]}";
        return j;
    }
    std::wstring SyncGetBoostInfo() {
        if (!BoostStateValid()) return L"{\"active\":false}";
        std::ifstream f(g_paths.BoostStateFile, std::ios::binary);
        std::stringstream ss; ss << f.rdbuf();
        std::wstring content = Utf8ToWide(ss.str());
        std::wstring gameName;
        size_t p = content.find(L"game_name=");
        if (p != std::wstring::npos) {
            p += 10;
            size_t e = content.find_first_of(L"\r\n", p);
            gameName = content.substr(p, e - p);
        }
        return L"{\"active\":true,\"game_name\":" + Json::Str(gameName) + L"}";
    }
    std::wstring SyncGetAutostart() {
        auto items = GetAutostart();
        std::wstring j = L"{\"items\":[";
        for (size_t i = 0; i < items.size(); i++) {
            if (i) j += L",";
            j += L"{\"name\":" + Json::Str(items[i].name) + L",\"path\":" + Json::Str(items[i].path);
            j += L",\"source\":" + Json::Str(items[i].source) + L",\"type\":" + Json::Str(items[i].type) + L"}";
        }
        j += L"]}";
        return j;
    }
    std::wstring SyncDeleteAutostart(const std::vector<std::wstring>& args) {
        if (args.size() < 4) return L"{\"ok\":false}";
        AutoItem it{ args[0], args[1], args[2], args[3] };
        return std::wstring(L"{\"ok\":") + Json::Bool(DeleteAutostart(it)) + L"}";
    }
    std::wstring SyncOpenLog() {
        std::error_code ec;
        if (!fs::exists(g_paths.LogFile, ec) || fs::file_size(g_paths.LogFile, ec) == 0)
            return L"{\"ok\":false,\"error\":\"Лог пуст\"}";
        ShellExecuteW(nullptr, L"open", g_paths.LogFile.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return L"{\"ok\":true}";
    }

    // -------- Async --------
    template <typename Fn>
    void Async(Fn fn, int id) {
        std::lock_guard<std::mutex> lk(busyLock);
        if (busy.load()) { SendResp(id, L"{\"started\":false,\"error\":\"Уже выполняется операция\"}"); return; }
        busy = true;
        SendResp(id, L"{\"started\":true}");
        std::thread([this, fn]() {
            bool ok = false; std::wstring err;
            try { ok = fn(); }
            catch (const std::exception& e) { err = Utf8ToWide(e.what()); }
            catch (...) { err = L"Неизвестная ошибка"; }
            { std::lock_guard<std::mutex> lk2(busyLock); busy = false; }
            SendDone(ok, err);
            }).detach();
    }
    void AsyncSelective(const std::vector<std::wstring>& args, int id) {
        OptFlags f;
        if (args.size() >= 8) {
            f.restore = args[0] != L"0";
            f.clean = args[1] != L"0";
            f.browsers = args[2] != L"0";
            f.hibernate = args[3] != L"0";
            f.visualFx = args[4] != L"0";
            f.power = args[5] != L"0";
            f.services = args[6] != L"0";
            f.sfc = args[7] != L"0";
        }
        Async([this, f] { return RunOptBackend(f); }, id);
    }
    void AsyncDebloat(const std::vector<std::wstring>& apps, int id) {
        Async([this, apps] { return DebloatSelected(apps); }, id);
    }
    void AsyncBoost(const std::wstring& name, int id) {
        Async([this, name] { return GameBoost(name); }, id);
    }

    static std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
        std::vector<std::wstring> out; std::wstring cur;
        for (wchar_t c : s) {
            if (c == sep) { out.push_back(cur); cur.clear(); }
            else cur += c;
        }
        out.push_back(cur);
        return out;
    }
};

// ------------------------- Win32 host window -------------------------
static App* g_app = nullptr;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (g_app) g_app->Resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

static bool IsAdmin() {
    BOOL isAdmin = FALSE; PSID id = nullptr; SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &id)) {
        CheckTokenMembership(nullptr, id, &isAdmin);
        FreeSid(id);
    }
    return isAdmin != FALSE;
}

static bool RelaunchAsAdmin() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

static void ConsoleMode();

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool consoleMode = false, elevated = false;
    for (int i = 0; i < argc; i++) {
        if (!wcscmp(argv[i], L"--console")) consoleMode = true;
        else if (!wcscmp(argv[i], L"--elevated")) elevated = true;
        else if (!wcscmp(argv[i], L"--orig-temp") && i + 1 < argc) {
            g_origTemp = argv[i + 1];
            i++;
        }
    }
    LocalFree(argv);

    if (!IsAdmin() && !elevated) {
        if (RelaunchAsAdmin()) return 0;
        MessageBoxW(nullptr, L"Требуются права администратора.\n\nЗапустите программу от имени администратора.",
            L"Optimizer Pro", MB_ICONERROR);
        return 1;
    }

    InitPaths();

    if (consoleMode) {
        ConsoleMode();
        return 0;
    }

    // GUI
    if (HAS_WEBVIEW_RUNTIME_CHECK()) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"OptProWnd";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassExW(&wc);

        HWND hWnd = CreateWindowExW(0, L"OptProWnd", L"Optimizer Pro",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750,
            nullptr, nullptr, hInst, nullptr);
        if (!hWnd) return 1;
        ShowWindow(hWnd, SW_SHOW);

        App app;
        g_app = &app;
        if (!app.Initialize(hWnd)) {
            MessageBoxW(nullptr, L"Не удалось инициализировать WebView2", L"Optimizer Pro", MB_ICONERROR);
            return 1;
        }
        MSG m;
        while (GetMessageW(&m, nullptr, 0, 0)) {
            TranslateMessage(&m); DispatchMessageW(&m);
        }
        g_app = nullptr;
        return 0;
    }

    MessageBoxW(nullptr,
        L"Не установлен WebView2 Runtime.\n\n"
        L"Установите Microsoft Edge WebView2 Runtime:\n"
        L"https://developer.microsoft.com/microsoft-edge/webview2/\n\n"
        L"Или запустите с флагом --console из cmd.exe.",
        L"Optimizer Pro", MB_ICONERROR);
    return 1;
}

// Проверка наличия WebView2 Runtime по ключу реестра
static bool HAS_WEBVIEW_RUNTIME_CHECK() {
    HKEY k;
    const wchar_t* keys[] = {
        L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
        L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
    };
    for (auto kp : keys) {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kp, 0, KEY_READ, &k) == ERROR_SUCCESS) {
            RegCloseKey(k); return true;
        }
    }
    return false;
}

// ============================================================
// КОНСОЛЬНЫЙ РЕЖИМ
// ============================================================
static void ConsoleMode() {
    // Показываем консоль
    HWND hC = GetConsoleWindow();
    if (hC) ShowWindow(hC, SW_SHOW);

    while (true) {
        system("cls");
        wprintf(L"==================================================\n");
        wprintf(L"  OPTIMIZER PRO - КОНСОЛЬНЫЙ РЕЖИМ (C++)\n");
        wprintf(L"==================================================\n");
        wprintf(L"\n  1. Полная оптимизация\n");
        wprintf(L"  2. Очистка Temp и кэша\n");
        wprintf(L"  3. Сброс сети\n");
        wprintf(L"  4. Активировать Game Boost\n");
        wprintf(L"  5. Остановить Game Boost\n");
        wprintf(L"  6. Откат системных изменений\n");
        wprintf(L"  7. Выход\n");
        wprintf(L"\n  Выбор: ");

        std::wstring line;
        wchar_t buf[256];
        if (!fgetws(buf, 256, stdin)) break;
        line = buf;
        while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r')) line.pop_back();
        if (line == L"1") {
            bool ok = RunOptBackend(OptFlags{});
            wprintf(L"  %s\n", ok ? L"Готово." : L"Ошибка.");
        }
        else if (line == L"2") {
            wprintf(L"  %s\n", CleanTempAndCache(true, true) ? L"Готово." : L"Завершено с ошибками.");
        }
        else if (line == L"3") {
            wprintf(L"  %s\n", ResetNetwork() ? L"Готово (перезагрузите ПК)." : L"Ошибка.");
        }
        else if (line == L"4") {
            wprintf(L"  Имя процесса игры: ");
            if (!fgetws(buf, 256, stdin)) break;
            std::wstring n = buf;
            while (!n.empty() && (n.back() == L'\n' || n.back() == L'\r')) n.pop_back();
            wprintf(L"  %s\n", GameBoost(n) ? L"Game Boost активирован." : L"Ошибка.");
        }
        else if (line == L"5") {
            wprintf(L"  %s\n", RestorePriorities() ? L"Восстановлено." : L"Ошибка.");
        }
        else if (line == L"6") {
            wprintf(L"  %s\n", RestoreBackup() ? L"Готово." : L"Ошибка.");
        }
        else if (line == L"7") break;
        else wprintf(L"  Неверный ввод.\n");
        wprintf(L"\n  Enter...");
        fgetws(buf, 256, stdin);
    }
}