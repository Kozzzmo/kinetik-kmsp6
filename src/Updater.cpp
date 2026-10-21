#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
extern char** environ;
#endif
#include "Updater.hpp"
#include <vector>
#include <ctime>
#include <cstdio>
#include <cstring>

// Kinetik — update check, system side (keep identical in all repositories). See Updater.hpp.
namespace kn {
namespace upd {

// ---- small system helpers
// On Windows every path here is UTF-8 and goes through the wide (UTF-16) API: the ANSI getenv / fopen
// mangled user folders with accents ("C:\\Users\\Hélène"), so the update check always failed there.
#ifdef _WIN32
std::wstring widen(const std::string& s)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w((size_t)(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
static std::string narrow(const std::wstring& w)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)(n > 0 ? n : 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}
static std::string envUtf8(const wchar_t* name)
{
    const wchar_t* v = _wgetenv(name);
    return v ? narrow(v) : std::string();
}
#endif

std::string dataDir()
{
#ifdef _WIN32
    const std::string base = envUtf8(L"APPDATA");
    if (base.empty()) return std::string();
    const std::string d = base + "\\Kinetik";
    CreateDirectoryW(widen(d).c_str(), nullptr);
    return d + "\\";
#else
    std::string d = lic::licenseDir(true);       // <user data>/Kinetik/Licenses/
    if (d.size() > 10) d = d.substr(0, d.size() - 9);  // -> <user data>/Kinetik/
    return d;
#endif
}
std::string tempDir()
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH + 2] = {};
    const DWORD n = GetTempPathW(MAX_PATH + 1, buf);     // ends with a backslash
    if (n > 0 && n <= MAX_PATH + 1) return narrow(buf);
    return "C:\\Windows\\Temp\\";
#else
    const char* t = std::getenv("TMPDIR");
    std::string s = t ? t : "/tmp";
    if (s.empty() || s.back() != '/') s += '/';
    return s;
#endif
}
// downloads go to a folder only this user can write (<user data>/Kinetik/Updates), not the shared temp folder:
// nothing else can swap the installer between its check and its opening
static std::string downloadDir()
{
    const std::string base = dataDir();
    if (base.empty()) return tempDir();
    const std::string d = base + "Updates";
#ifdef _WIN32
    CreateDirectoryW(widen(d).c_str(), nullptr);
    return d + "\\";
#else
    mkdir(d.c_str(), 0700);
    return d + "/";
#endif
}
static void removeFile(const std::string& p)
{
#ifdef _WIN32
    _wremove(widen(p).c_str());
#else
    std::remove(p.c_str());
#endif
}

static bool exists(const std::string& p)
{
#ifdef _WIN32
    return GetFileAttributesW(widen(p).c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    return access(p.c_str(), F_OK) == 0;
#endif
}

std::string strayCopy(const char* bin)
{
#if defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home) return "";
    const std::string base = std::string(home) + "/Library/Audio/Plug-Ins/";
    const std::string c[3] = {base + "VST3/" + bin + ".vst3", base + "Components/" + bin + ".component", base + "CLAP/" + bin + ".clap"};
#elif defined(_WIN32)
    const char* la = std::getenv("LOCALAPPDATA");
    if (!la) return "";
    const std::string c[3] = {std::string(la) + "\\Programs\\Common\\VST3\\" + bin + ".vst3",
                              std::string(la) + "\\Programs\\Common\\CLAP\\" + bin + ".clap", ""};
#else
    (void)bin;
    const std::string c[3];
#endif
    for (const auto& p : c) if (!p.empty() && exists(p)) return p;
    return "";
}

// download url to path with curl (follows redirects, https only, fails on HTTP errors). true on success.
bool download(const std::string& url, const std::string& path, int timeoutSec)
{
    if (url.rfind("https://", 0) != 0) return false;
    const std::string to = std::to_string(timeoutSec);
#ifdef _WIN32
    const std::string sr = envUtf8(L"SystemRoot");
    const std::wstring curl = widen((sr.empty() ? std::string("C:\\Windows") : sr) + "\\System32\\curl.exe");
    std::wstring cmd = L"\"" + curl + L"\" -sSfL --proto =https --max-time " + widen(to) + L" -o \"" + widen(path) + L"\" \"" + widen(url) + L"\"";
    STARTUPINFOW si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(curl.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    if (WaitForSingleObject(pi.hProcess, (DWORD)(timeoutSec + 10) * 1000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return code == 0;
#else
    const char* curl = "/usr/bin/curl";
    std::vector<std::string> a = {curl, "-sSfL", "--proto", "=https", "--max-time", to, "-o", path, url};
    std::vector<char*> argv;
    for (auto& s : a) argv.push_back(&s[0]);
    argv.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawn(&pid, curl, nullptr, nullptr, argv.data(), environ) != 0) return false;
    int st = 0;
    if (waitpid(pid, &st, 0) < 0) return false;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
#endif
}

bool readAll(const std::string& path, std::string& out, size_t maxBytes)
{
#ifdef _WIN32
    FILE* fp = _wfopen(widen(path).c_str(), L"rb");
#else
    FILE* fp = std::fopen(path.c_str(), "rb");
#endif
    if (!fp) return false;
    out.clear();
    char buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) { out.append(buf, n); if (out.size() > maxBytes) break; }
    std::fclose(fp);
    return out.size() <= maxBytes;
}

// opens the downloaded installer with the system (macOS Installer, Windows UAC prompt + setup)
bool openInstaller(const std::string& path)
{
#ifdef _WIN32
    return (INT_PTR)ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
#elif defined(__APPLE__)
    std::vector<std::string> a = {"/usr/bin/open", path};
    std::vector<char*> argv; for (auto& s : a) argv.push_back(&s[0]); argv.push_back(nullptr);
    pid_t pid = 0;
    return posix_spawn(&pid, "/usr/bin/open", nullptr, nullptr, argv.data(), environ) == 0;
#else
    (void)path; return false;
#endif
}
bool openFolder(const std::string& path)
{
    if (path.empty()) return false;
#ifdef _WIN32
    return (INT_PTR)ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
#else
  #ifdef __APPLE__
    const char* opener = "/usr/bin/open";
  #else
    const char* opener = "/usr/bin/xdg-open";
  #endif
    std::vector<std::string> a = {opener, path};
    std::vector<char*> argv; for (auto& s : a) argv.push_back(&s[0]); argv.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawn(&pid, opener, nullptr, nullptr, argv.data(), environ) != 0) return false;
    std::thread([pid] { int st = 0; waitpid(pid, &st, 0); }).detach();   // reap it (no zombie)
    return true;
#endif
}
bool openPage(const std::string& url)
{
    if (url.rfind("https://", 0) != 0) return false;
#ifdef _WIN32
    return (INT_PTR)ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
#else
  #ifdef __APPLE__
    const char* opener = "/usr/bin/open";
  #else
    const char* opener = "/usr/bin/xdg-open";
  #endif
    std::vector<std::string> a = {opener, url};
    std::vector<char*> argv; for (auto& s : a) argv.push_back(&s[0]); argv.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawn(&pid, opener, nullptr, nullptr, argv.data(), environ) != 0) return false;
    std::thread([pid] { int st = 0; waitpid(pid, &st, 0); }).detach();   // reap it (no zombie)
    return true;
#endif
}

// ---- settings: <user data>/Kinetik/updates.txt ("auto 1", "last <time>", "skip-<product> <version>")
struct Settings {
    bool autoCheck = true;
    long long last = 0;
    std::string skip;
};
Settings loadSettings(const char* product)
{
    Settings s;
    std::string t;
    if (!readAll(dataDir() + "updates.txt", t, 65536)) return s;
    size_t pos = 0;
    const std::string skipKey = std::string("skip-") + product + " ", lastKey = std::string("last-") + product + " ";
    while (pos < t.size()) {
        size_t e = t.find('\n', pos); if (e == std::string::npos) e = t.size();
        const std::string l = lic::trim(t.substr(pos, e - pos)); pos = e + 1;
        if (l.rfind("auto ", 0) == 0) s.autoCheck = l.substr(5) != "0";
        else if (l.rfind(lastKey, 0) == 0) s.last = std::atoll(l.c_str() + lastKey.size());
        else if (l.rfind(skipKey, 0) == 0) s.skip = l.substr(skipKey.size());
    }
    return s;
}
void saveSettings(const char* product, const Settings& s)
{
    // keep the other products' lines
    std::string t, out;
    readAll(dataDir() + "updates.txt", t, 65536);
    const std::string skipKey = std::string("skip-") + product + " ", lastKey = std::string("last-") + product + " ";
    size_t pos = 0;
    while (pos < t.size()) {
        size_t e = t.find('\n', pos); if (e == std::string::npos) e = t.size();
        const std::string l = lic::trim(t.substr(pos, e - pos)); pos = e + 1;
        if (l.empty() || l.rfind("auto ", 0) == 0 || l.rfind(skipKey, 0) == 0 || l.rfind(lastKey, 0) == 0) continue;
        out += l + "\n";
    }
    out += std::string("auto ") + (s.autoCheck ? "1" : "0") + "\n";
    out += lastKey + std::to_string(s.last) + "\n";
    if (!s.skip.empty()) out += skipKey + s.skip + "\n";
#ifdef _WIN32
    FILE* fp = _wfopen(widen(dataDir() + "updates.txt").c_str(), L"wb");
#else
    FILE* fp = std::fopen((dataDir() + "updates.txt").c_str(), "wb");
#endif
    if (!fp) return;
    std::fwrite(out.data(), 1, out.size(), fp);
    std::fclose(fp);
}

// ---- checker
Checker::~Checker() { if (worker.joinable()) worker.join(); }

void Checker::startWork(void (*fn)(Checker*, bool), bool arg)
{
    if (worker.joinable()) worker.join();
    busy.store(true);
    worker = std::thread([this, fn, arg] { fn(this, arg); busy.store(false); });
}

void Checker::check(bool force)
{
    if (busy.load()) return;
    const Settings s = loadSettings(prod);
    if (!force && (!s.autoCheck || (long long)std::time(nullptr) - s.last < 20 * 3600)) return;
    startWork(&Checker::doCheck, force);
}

void Checker::doCheck(Checker* c, bool force)
{
    c->state.store(stChecking);
    Settings st = loadSettings(c->prod);
    st.last = (long long)std::time(nullptr);
    saveSettings(c->prod, st);
    const std::string tmp = downloadDir() + "kinetik-" + c->prod + "-update.txt";
    std::string text;
    if (!download(c->url, tmp, 20) || !readAll(tmp, text, 64 * 1024)) {
        c->setMsg("Could not reach the update server."); c->state.store(force ? stFailed : stIdle); return;
    }
    removeFile(tmp);
    const Manifest m = parse(text, c->prod);
    if (!m.valid) { c->setMsg("The update information is not valid."); c->state.store(force ? stFailed : stIdle); return; }
    { std::lock_guard<std::mutex> lk(c->mx); c->man = m; }
    const bool skipped = !force && st.skip == m.version;
    c->state.store(newer(m.version, c->current) && !skipped ? stAvailable : stUpToDate);
}

void Checker::install()
{
    if (busy.load()) return;
    const Manifest m = manifest();
    if (!m.valid) return;
    if (m.url.empty()) { openPage(m.page); return; }   // no installer for this system: the download page
    startWork(&Checker::doInstall, false);
}

void Checker::doInstall(Checker* c, bool)
{
    const Manifest m = c->manifest();
    c->state.store(stDownloading);
    const size_t slash = m.url.find_last_of('/');
    const std::string path = downloadDir() + (slash == std::string::npos ? std::string("kinetik-setup") : m.url.substr(slash + 1));
    std::string data;
    if (!download(m.url, path, 600) || !readAll(path, data, 512u * 1024 * 1024)) { c->setMsg("The download failed."); c->state.store(stFailed); return; }
    if (sha512Hex(data) != m.sha512) { removeFile(path); c->setMsg("The download is damaged or not genuine: it was deleted."); c->state.store(stFailed); return; }
    data.clear();
    if (!openInstaller(path)) { c->setMsg("Could not open the installer."); c->state.store(stFailed); return; }
    c->state.store(stOpened);
}

void Checker::skipThisVersion()
{
    Settings s = loadSettings(prod); s.skip = manifest().version; saveSettings(prod, s);
    state.store(stUpToDate);
}
bool Checker::autoCheck() const { return loadSettings(prod).autoCheck; }
void Checker::setAutoCheck(bool on) { Settings s = loadSettings(prod); s.autoCheck = on; saveSettings(prod, s); }

} // namespace upd
} // namespace kn
