 // ak - Secure secret management CLI (C++ implementation)
 //
 // This tool provides a vault-based key/value store with optional GPG encryption.
 // It supports setting, getting, listing, and removing secrets,
 // profile management (save, load, unload, export, import),
 // and utilities such as copy to clipboard, search, run, guard,
 // testing service connectivity, and shell integration.
 //
 // Storage: GPG‑encrypted or plain text vault at ~/.config/ak/keys.env(.gpg)
 // Configuration directory: ~/.config/ak

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <chrono>
#include <iomanip>
#include <array>
#include <random>
#include <mutex>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#ifdef __unix__
#include <termios.h>
#include <pwd.h>
#endif

using namespace std;
namespace fs = std::filesystem;
extern char **environ;

// -------- Colors --------
namespace Colors {
    static const string RESET = "\033[0m";
    static const string BOLD = "\033[1m";
    static const string DIM = "\033[2m";
    
    // Basic colors
    static const string BLACK = "\033[30m";
    static const string RED = "\033[31m";
    static const string GREEN = "\033[32m";
    static const string YELLOW = "\033[33m";
    static const string BLUE = "\033[34m";
    static const string MAGENTA = "\033[35m";
    static const string CYAN = "\033[36m";
    static const string WHITE = "\033[37m";
    
    // Bright colors
    static const string BRIGHT_BLACK = "\033[90m";
    static const string BRIGHT_RED = "\033[91m";
    static const string BRIGHT_GREEN = "\033[92m";
    static const string BRIGHT_YELLOW = "\033[93m";
    static const string BRIGHT_BLUE = "\033[94m";
    static const string BRIGHT_MAGENTA = "\033[95m";
    static const string BRIGHT_CYAN = "\033[96m";
    static const string BRIGHT_WHITE = "\033[97m";
    
    // Background colors
    static const string BG_BLACK = "\033[40m";
    static const string BG_RED = "\033[41m";
    static const string BG_GREEN = "\033[42m";
    static const string BG_YELLOW = "\033[43m";
    static const string BG_BLUE = "\033[44m";
    static const string BG_MAGENTA = "\033[45m";
    static const string BG_CYAN = "\033[46m";
    static const string BG_WHITE = "\033[47m";
}

static bool isColorSupported() {
#ifdef __unix__
    const char* term = getenv("TERM");
    if (!term) return false;
    string termStr(term);
    return termStr != "dumb" && isatty(STDOUT_FILENO);
#else
    return false;
#endif
}

static string colorize(const string& text, const string& color) {
    if (!isColorSupported()) return text;
    return color + text + Colors::RESET;
}

static const int MASK_PREFIX = 8;
static const int MASK_SUFFIX = 4;

// -------- Config --------
struct Config
{
    string configDir;   // $XDG_CONFIG_HOME/ak or $HOME/.config/ak
    string vaultPath;   // keys.env.gpg or keys.env
    string profilesDir; // profiles directory
    bool gpgAvailable = false;
    bool json = false;
    bool forcePlain = false; // AK_DISABLE_GPG
    string presetPassphrase; // AK_PASSPHRASE
    string auditLogPath;
    string instanceId;
    string persistDir;
};

// -------- Utils --------
static bool commandExists(const string &cmd)
{
    string which = "command -v '" + cmd + "' >/dev/null 2>&1";
    return system(which.c_str()) == 0;
}
static string getenvs(const char *k, const string &defv = "")
{
    const char *v = getenv(k);
    return v ? string(v) : defv;
}
static string trim(const string &s)
{
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b]))
        ++b;
    size_t e = s.size();
    while (e > b && isspace((unsigned char)s[e - 1]))
        --e;
    return s.substr(b, e - b);
}
static string toLower(string s)
{
    for (char &c : s)
        c = tolower((unsigned char)c);
    return s;
}
static bool icontains(const string &hay, const string &needle)
{
    auto H = toLower(hay), N = toLower(needle);
    return H.find(N) != string::npos;
}
static string maskValue(const string &v)
{
    if (v.empty())
        return "(empty)";
    if ((int)v.size() <= MASK_PREFIX + MASK_SUFFIX)
        return string(v.size(), '*');
    return v.substr(0, MASK_PREFIX) + "***" + v.substr(v.size() - MASK_SUFFIX);
}
static void error(const Config &cfg, const string &msg, int code = 1)
{
    if (cfg.json)
        cerr << "{\"ok\":false,\"error\":\"" << msg << "\"}\n";
    else
        cerr << "❌ " << msg << "\n";
    exit(code);
}
static void ok(const Config &cfg, const string &msg)
{
    if (!cfg.json)
        cerr << "✅ " << msg << "\n";
}
static void warn(const Config &cfg, const string &msg)
{
    if (!cfg.json)
        cerr << "⚠️  " << msg << "\n";
}

#ifdef __unix__
static void secureChmod(const fs::path &p, mode_t mode) { ::chmod(p.c_str(), mode); }
#else
static void secureChmod(const fs::path &, int) {}
#endif
static void ensureSecureDir(const fs::path &p)
{
    if (!fs::exists(p))
        fs::create_directories(p);
#ifdef __unix__
    secureChmod(p, 0700);
#endif
}
static void ensureSecureFile(const fs::path &p)
{
    if (!fs::exists(p))
    {
        ofstream(p.string(), ios::app).close();
    }
#ifdef __unix__
    secureChmod(p, 0600);
#endif
}

// -------- Base64 --------
static string base64Encode(const string &in)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    unsigned char a3[3];
    for (size_t pos = 0; pos < in.size();)
    {
        int len = 0;
        for (; len < 3 && pos < in.size(); ++len)
            a3[len] = (unsigned char)in[pos++];
        if (len < 3)
            for (int j = len; j < 3; ++j)
                a3[j] = 0;
        unsigned int triple = (a3[0] << 16) | (a3[1] << 8) | a3[2];
        out.push_back(tbl[(triple >> 18) & 0x3F]);
        out.push_back(tbl[(triple >> 12) & 0x3F]);
        out.push_back(len >= 2 ? tbl[(triple >> 6) & 0x3F] : '=');
        out.push_back(len >= 3 ? tbl[triple & 0x3F] : '=');
    }
    return out;
}
static string base64Decode(const string &in)
{
    auto val = [&](char c) -> int
    {
        if ('A' <= c && c <= 'Z')
            return c - 'A';
        if ('a' <= c && c <= 'z')
            return c - 'a' + 26;
        if ('0' <= c && c <= '9')
            return c - '0' + 52;
        if (c == '+')
            return 62;
        if (c == '/')
            return 63;
        return -1;
    };
    string out;
    out.reserve((in.size() / 4) * 3);
    int accum = 0, bits = 0;
    for (char c : in)
    {
        if (c == '=')
            break;
        int v = val(c);
        if (v < 0)
            continue;
        accum = (accum << 6) | v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(char((accum >> bits) & 0xFF));
        }
    }
    return out;
}

// -------- Audit --------
class SHA256
{
public:
    SHA256() { reset(); }
    void update(const string &s) { update(reinterpret_cast<const uint8_t *>(s.data()), s.size()); }
    void update(const uint8_t *d, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            data[datalen] = d[i];
            datalen++;
            if (datalen == 64)
            {
                transform();
                bitlen += 512;
                datalen = 0;
            }
        }
    }
    string final()
    {
        uint32_t i = datalen;
        if (datalen < 56)
        {
            data[i++] = 0x80;
            while (i < 56)
                data[i++] = 0x00;
        }
        else
        {
            data[i++] = 0x80;
            while (i < 64)
                data[i++] = 0x00;
            transform();
            memset(data, 0, 56);
        }
        bitlen += datalen * 8ULL;
        for (int j = 0; j < 8; ++j)
            data[63 - j] = (bitlen >> (8 * j)) & 0xFF;
        transform();
        ostringstream oss;
        oss << hex << setfill('0');
        for (int j = 0; j < 8; ++j)
            oss << setw(8) << state[j];
        reset();
        return oss.str();
    }

private:
    uint8_t data[64];
    uint32_t datalen = 0;
    unsigned long long bitlen = 0;
    uint32_t state[8];
    static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t s0(uint32_t x) { return ror(x, 7) ^ ror(x, 18) ^ (x >> 3); }
    static uint32_t s1(uint32_t x) { return ror(x, 17) ^ ror(x, 19) ^ (x >> 10); }
    void transform()
    {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t m[64];
        for (int i = 0, j = 0; i < 16; ++i, j += 4)
            m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | data[j + 3];
        for (int i = 16; i < 64; ++i)
            m[i] = s1(m[i - 2]) + m[i - 7] + s0(m[i - 15]) + m[i - 16];
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i)
        {
            uint32_t t1 = h + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ch(e, f, g) + K[i] + m[i];
            uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    void reset()
    {
        datalen = 0;
        bitlen = 0;
        state[0] = 0x6a09e667;
        state[1] = 0xbb67ae85;
        state[2] = 0x3c6ef372;
        state[3] = 0xa54ff53a;
        state[4] = 0x510e527f;
        state[5] = 0x9b05688c;
        state[6] = 0x1f83d9ab;
        state[7] = 0x5be0cd19;
    }
};
static string hashKeyName(const string &name)
{
    SHA256 h;
    h.update(name);
    return h.final().substr(0, 16);
}

static string isoTimeUTC()
{
    using namespace chrono;
    auto t = system_clock::to_time_t(system_clock::now());
    tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

static mutex auditMutex;
static void auditLog(const Config &cfg, const string &action, const vector<string> &keys)
{
    if (cfg.auditLogPath.empty())
        return;
    fs::create_directories(fs::path(cfg.auditLogPath).parent_path());
    ensureSecureFile(cfg.auditLogPath);
    lock_guard<mutex> lock(auditMutex);
    ofstream out(cfg.auditLogPath, ios::app);
    out << isoTimeUTC() << " action=" << action << " instance=" << cfg.instanceId << " count=" << keys.size();
    if (!keys.empty())
    {
        out << " keys=";
        bool first = true;
        for (auto &k : keys)
        {
            if (!first)
                out << ',';
            first = false;
            out << hashKeyName(k);
        }
    }
    out << "\n";
}

// -------- Secret prompt --------
static string promptSecret(const string &prompt)
{
    string value;
#ifdef __unix__
    cerr << prompt;
    cerr.flush();
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    getline(cin, value);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    cerr << "\n";
#else
    cerr << prompt;
    getline(cin, value);
#endif
    return value;
}

// -------- Run command & capture --------
static string runCmdCapture(const string &cmd, int *exitCode = nullptr)
{
    array<char, 4096> buf{};
    string out;
    FILE *p = popen(cmd.c_str(), "r");
    if (!p)
    {
        if (exitCode)
            *exitCode = -1;
        return {};
    }
    while (true)
    {
        size_t n = fread(buf.data(), 1, buf.size(), p);
        if (n > 0)
            out.append(buf.data(), n);
        if (n < buf.size())
            break;
    }
    int rc = pclose(p);
    if (exitCode)
        *exitCode = rc;
    return out;
}

// -------- Vault --------
struct KeyStore
{
    unordered_map<string, string> kv;
};

static KeyStore loadVault(const Config &cfg)
{
    KeyStore ks;
    if (!fs::exists(cfg.vaultPath))
        return ks;
    string data;
    if (cfg.gpgAvailable && !cfg.forcePlain && cfg.vaultPath.size() > 4 && cfg.vaultPath.substr(cfg.vaultPath.size() - 4) == ".gpg")
    {
        int rc = 0;
        if (!cfg.presetPassphrase.empty())
        {
            auto pfile = fs::path(cfg.configDir) / ".pass.tmp";
            {
                ofstream pf(pfile, ios::trunc);
                pf << cfg.presetPassphrase;
                pf.close();
            }
#ifdef __unix__
            ::chmod(pfile.c_str(), 0600);
#endif
            string cmd = "gpg --batch --yes --quiet --pinentry-mode loopback --passphrase-file '" + pfile.string() + "' --decrypt '" + cfg.vaultPath + "' 2>/dev/null";
            data = runCmdCapture(cmd, &rc);
            fs::remove(pfile);
        }
        else
        {
            data = runCmdCapture("gpg --quiet --decrypt '" + cfg.vaultPath + "' 2>/dev/null", &rc);
        }
        if (rc != 0)
        {
            warn(cfg, "Failed to decrypt vault");
            return ks;
        }
    }
    else
    {
        ifstream in(cfg.vaultPath);
        ostringstream ss;
        ss << in.rdbuf();
        data = ss.str();
    }
    istringstream lines(data);
    string line;
    while (getline(lines, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        auto eq = line.find('=');
        if (eq == string::npos)
            continue;
        string k = line.substr(0, eq), enc = line.substr(eq + 1);
        ks.kv[k] = base64Decode(enc);
    }
    return ks;
}

static void saveVault(const Config &cfg, const KeyStore &ks)
{
    fs::create_directories(fs::path(cfg.vaultPath).parent_path());
    auto tmp = fs::path(cfg.vaultPath).parent_path() / ".tmp.ak.vault";
    {
        ofstream out(tmp);
        for (auto &p : ks.kv)
            out << p.first << "=" << base64Encode(p.second) << "\n";
    }
    if (cfg.gpgAvailable && !cfg.forcePlain && cfg.vaultPath.size() > 4 && cfg.vaultPath.substr(cfg.vaultPath.size() - 4) == ".gpg")
    {
        string cmd;
        string passFile;
        if (!cfg.presetPassphrase.empty())
        {
            passFile = (fs::path(cfg.configDir) / ".pass.tmp").string();
            {
                ofstream pf(passFile, ios::trunc);
                pf << cfg.presetPassphrase;
                pf.close();
            }
#ifdef __unix__
            ::chmod(passFile.c_str(), 0600);
#endif
            cmd = "gpg --batch --yes -o '" + cfg.vaultPath + "' --pinentry-mode loopback --passphrase-file '" + passFile + "' --symmetric --cipher-algo AES256 '" + tmp.string() + "'";
        }
        else
        {
            cmd = "gpg --yes -o '" + cfg.vaultPath + "' --symmetric --cipher-algo AES256 '" + tmp.string() + "'";
        }
        int rc = system(cmd.c_str());
        if (!passFile.empty())
            fs::remove(passFile);
        if (rc != 0)
            error(cfg, "Failed to encrypt vault with gpg");
        fs::remove(tmp);
    }
    else
    {
        fs::rename(tmp, cfg.vaultPath);
    }
}

// -------- Profiles --------
static fs::path profilePath(const Config &cfg, const string &name) { return fs::path(cfg.profilesDir) / (name + ".profile"); }

static vector<string> listProfiles(const Config &cfg)
{
    vector<string> names;
    if (!fs::exists(cfg.profilesDir))
        return names;
    for (auto &e : fs::directory_iterator(cfg.profilesDir))
    {
        if (!e.is_regular_file())
            continue;
        auto fn = e.path().filename().string();
        if (fn.size() > 8 && fn.substr(fn.size() - 8) == ".profile")
            names.push_back(fn.substr(0, fn.size() - 8));
    }
    sort(names.begin(), names.end());
    return names;
}
static vector<string> readProfile(const Config &cfg, const string &name)
{
    vector<string> keys;
    auto path = profilePath(cfg, name);
    if (!fs::exists(path))
        return keys;
    ifstream in(path);
    string line;
    while (getline(in, line))
    {
        line = trim(line);
        if (!line.empty())
            keys.push_back(line);
    }
    return keys;
}
static void writeProfile(const Config &cfg, const string &name, const vector<string> &keys)
{
    fs::create_directories(cfg.profilesDir);
    ofstream out(profilePath(cfg, name));
    unordered_set<string> seen;
    vector<string> sorted = keys;
    sort(sorted.begin(), sorted.end());
    for (auto &k : sorted)
        if (seen.insert(k).second)
            out << k << "\n";
}

// -------- CLI help --------
static void showLogo() {
    if (!isColorSupported()) {
        cout << "AK - Secret Management CLI\n\n";
        return;
    }
    
    cout << colorize(R"(
        ████████╗   ██╗  ██╗
        ██╔═══██║   ██║ ██╔╝
        ██║██╗██║   █████╔╝
        ██║██║██║   ██╔═██╗
        ╚█████╔██║   ██║  ██╗
         ╚════╝╚═╝   ╚═╝  ╚═╝
)", Colors::BRIGHT_CYAN) << "\n";

    cout << colorize("    🔐 ", "") << colorize("Secure Secret Management", Colors::BRIGHT_WHITE + Colors::BOLD) << "\n";
    cout << colorize("    ⚡ ", "") << colorize("Fast • Secure • Developer-Friendly", Colors::BRIGHT_GREEN) << "\n\n";
}

static void showTips() {
    cout << colorize("Tips for getting started:", Colors::BRIGHT_YELLOW + Colors::BOLD) << "\n";
    cout << colorize("1. ", Colors::BRIGHT_WHITE) << colorize("Set your first secret: ", Colors::WHITE)
         << colorize("ak set API_KEY", Colors::BRIGHT_CYAN) << "\n";
    cout << colorize("2. ", Colors::BRIGHT_WHITE) << colorize("Create profiles to organize secrets: ", Colors::WHITE)
         << colorize("ak save prod API_KEY DB_URL", Colors::BRIGHT_CYAN) << "\n";
    cout << colorize("3. ", Colors::BRIGHT_WHITE) << colorize("Load secrets into your shell: ", Colors::WHITE)
         << colorize("ak load prod", Colors::BRIGHT_CYAN) << "\n";
    cout << colorize("4. ", Colors::BRIGHT_WHITE) << colorize("Import from .env files: ", Colors::WHITE)
         << colorize("ak import -p dev -f env -i .env --keys", Colors::BRIGHT_CYAN) << "\n";
    cout << colorize("5. ", Colors::BRIGHT_WHITE) << colorize("Run ", Colors::WHITE)
         << colorize("ak help", Colors::BRIGHT_MAGENTA) << colorize(" for detailed documentation", Colors::WHITE) << "\n\n";
}

static void cmd_help()
{
    showLogo();
    
    cout << colorize("USAGE:", Colors::BRIGHT_WHITE + Colors::BOLD) << "\n";
    cout << "  " << colorize("ak", Colors::BRIGHT_CYAN) << colorize(" <command> [options] [arguments]", Colors::WHITE) << "\n\n";

    cout << colorize("SECRET MANAGEMENT:", Colors::BRIGHT_GREEN + Colors::BOLD) << "\n";
    cout << "  " << colorize("ak set <NAME>", Colors::BRIGHT_CYAN) << "                   Set a secret (prompts for value)\n";
    cout << "  " << colorize("ak get <NAME> [--full]", Colors::BRIGHT_CYAN) << "          Get a secret value (--full shows unmasked)\n";
    cout << "  " << colorize("ak ls [--json]", Colors::BRIGHT_CYAN) << "                  List all secret names (--json for JSON output)\n";
    cout << "  " << colorize("ak rm <NAME>", Colors::BRIGHT_CYAN) << "                    Remove a secret\n";
    cout << "  " << colorize("ak rm --profile <NAME>", Colors::BRIGHT_CYAN) << "          Remove a profile\n";
    cout << "  " << colorize("ak search <PATTERN>", Colors::BRIGHT_CYAN) << "             Search for secrets by name pattern (case-insensitive)\n";
    cout << "  " << colorize("ak cp <NAME>", Colors::BRIGHT_CYAN) << "                    Copy secret value to clipboard\n";
    cout << "  " << colorize("ak purge [--no-backup]", Colors::BRIGHT_CYAN) << "          Remove all secrets and profiles (creates backup by default)\n\n";

    cout << colorize("PROFILE MANAGEMENT:", Colors::BRIGHT_BLUE + Colors::BOLD) << "\n";
    cout << "  " << colorize("ak save <profile> [NAMES...]", Colors::BRIGHT_CYAN) << "    Save secrets to a profile (all secrets if no names given)\n";
    cout << "  " << colorize("ak load <profile> [--persist]", Colors::BRIGHT_CYAN) << "   Load profile as environment variables\n";
    cout << "                                  " << colorize("--persist: remember profile for current directory", Colors::DIM) << "\n";
    cout << "  " << colorize("ak unload [<profile>] [--persist]", Colors::BRIGHT_CYAN) << " Unload profile environment variables\n";
    cout << "  " << colorize("ak profiles", Colors::BRIGHT_CYAN) << "                     List all available profiles\n";
    cout << "  " << colorize("ak env --profile|-p <name>", Colors::BRIGHT_CYAN) << "      Show profile as export statements\n\n";

    cout << colorize("EXPORT/IMPORT:", Colors::BRIGHT_MAGENTA + Colors::BOLD) << "\n";
    cout << "  " << colorize("ak export --profile|-p <p> --format|-f <fmt> --output|-o <file>", Colors::BRIGHT_CYAN) << "\n";
    cout << "                                  Export profile to file\n";
    cout << "  " << colorize("ak import --profile|-p <p> --format|-f <fmt> --file|-i <file> [--keys]", Colors::BRIGHT_CYAN) << "\n";
    cout << "                                  Import secrets from file to profile\n";
    cout << "                                  " << colorize("--keys: only import known service provider keys", Colors::BRIGHT_YELLOW) << "\n";
    cout << "                                  \n";
    cout << "  " << colorize("Supported formats:", Colors::WHITE) << " env, dotenv, json, yaml\n\n";

    cout << colorize("UTILITIES:", Colors::BRIGHT_YELLOW + Colors::BOLD) << "\n";
    cout << "  " << colorize("ak run --profile|-p <p> -- <cmd>", Colors::BRIGHT_CYAN) << "  Run command with profile environment loaded\n";
    cout << "  " << colorize("ak test <service>|--all [options]", Colors::BRIGHT_CYAN) << "   Test service connectivity using stored credentials\n";
    cout << "  " << colorize("ak guard enable|disable", Colors::BRIGHT_CYAN) << "         Enable/disable shell guard for secret protection\n";
    cout << "  " << colorize("ak doctor", Colors::BRIGHT_CYAN) << "                       Check system configuration and dependencies\n";
    cout << "  " << colorize("ak audit [N]", Colors::BRIGHT_CYAN) << "                    Show audit log (last N entries, default: 10)\n\n";
    
    cout << colorize("SYSTEM:", Colors::BRIGHT_RED + Colors::BOLD) << "\n";
    cout << "  " << colorize("ak help", Colors::BRIGHT_CYAN) << "                         Show this help message\n";
    cout << "  " << colorize("ak backend", Colors::BRIGHT_CYAN) << "                      Show backend information (GPG status, vault location)\n";
    cout << "  " << colorize("ak install-shell", Colors::BRIGHT_CYAN) << "                Install shell integration for auto-loading\n";
    cout << "  " << colorize("ak uninstall", Colors::BRIGHT_CYAN) << "                    Remove shell integration\n";
    cout << "  " << colorize("ak completion <shell>", Colors::BRIGHT_CYAN) << "           Generate completion script for bash, zsh, or fish\n\n";

    showTips();
    
    cout << colorize("For detailed documentation, visit: ", Colors::WHITE)
         << colorize("https://github.com/apertacodex/ak", Colors::BRIGHT_BLUE + Colors::BOLD) << "\n";
}

static void showWelcome() {
    showLogo();
    showTips();
    cout << colorize("Ready to manage your secrets securely! 🚀", Colors::BRIGHT_GREEN + Colors::BOLD) << "\n\n";
}

// -------- Instance ID --------
static string loadOrCreateInstanceId(const Config &cfg)
{
    auto path = fs::path(cfg.configDir) / "instance.id";
    if (fs::exists(path))
    {
        ifstream in(path);
        string id;
        getline(in, id);
        return id;
    }
    string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, (int)chars.size() - 1);
    string id;
    id.reserve(24);
    for (int i = 0; i < 24; ++i)
        id.push_back(chars[dis(gen)]);
    fs::create_directories(fs::path(cfg.configDir));
    ofstream out(path);
    out << id;
    ensureSecureFile(path);
    return id;
}

// -------- Clipboard --------
static bool copyClipboard(const string &text)
{
    if (commandExists("pbcopy"))
    {
        string c = "printf %s '" + text + "' | pbcopy";
        return system(c.c_str()) == 0;
    }
    if (commandExists("wl-copy"))
    {
        string c = "printf %s '" + text + "' | wl-copy";
        return system(c.c_str()) == 0;
    }
    if (commandExists("xclip"))
    {
        string c = "printf %s '" + text + "' | xclip -selection clipboard";
        return system(c.c_str()) == 0;
    }
    return false;
}

// -------- Export printer --------
static void printExportsForProfile(const Config &cfg, const string &name)
{
    auto keys = readProfile(cfg, name);
    KeyStore ks = loadVault(cfg);
    for (auto &k : keys)
    {
        auto it = ks.kv.find(k);
        if (it == ks.kv.end())
            continue;
        string v = it->second, esc;
        esc.reserve(v.size() * 2);
        for (char c : v)
        {
            if (c == '\\' || c == '"')
                esc.push_back('\\');
            if (c == '\n')
            {
                esc += "\\n";
                continue;
            }
            esc.push_back(c);
        }
        cout << "export " << k << "=\"" << esc << "\"\n";
    }
}
[[maybe_unused]] static void printUnsetsForProfile(const Config &cfg, const string &name)
{
    for (auto &k : readProfile(cfg, name))
        cout << "unset " << k << "\n";
}

// -------- Import parsing helpers --------
static vector<pair<string, string>> parse_env_file(istream &in)
{
    vector<pair<string, string>> kvs;
    string line;
    while (getline(in, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        
        // Skip shell constructs that aren't env vars
        if (line.find("alias ") == 0 ||
            line.find("[[") != string::npos ||
            line.find("$(") != string::npos ||
            line.find("function ") == 0 ||
            line.find("if ") == 0 ||
            line.find("case ") == 0 ||
            line.find("for ") == 0 ||
            line.find("while ") == 0)
            continue;
            
        if (line.rfind("export ", 0) == 0)
            line = line.substr(7);
        auto eq = line.find('=');
        if (eq == string::npos)
            continue;
        string k = trim(line.substr(0, eq)), v = line.substr(eq + 1);
        
        // Validate key name - must be valid variable name
        if (k.empty() || (!isalpha(k[0]) && k[0] != '_'))
            continue;
        for (char c : k) {
            if (!isalnum(c) && c != '_')
                goto skip_line;
        }
        
        if (!v.empty() && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        kvs.push_back({k, v});
        skip_line:;
    }
    return kvs;
}
static vector<pair<string, string>> parse_json_min(const string &txt)
{
    vector<pair<string, string>> kvs;
    size_t i = 0;
    while (true)
    {
        i = txt.find('"', i);
        if (i == string::npos)
            break;
        size_t j = txt.find('"', i + 1);
        if (j == string::npos)
            break;
        string k = txt.substr(i + 1, j - i - 1);
        size_t c = txt.find(':', j);
        if (c == string::npos)
            break;
        size_t v1 = txt.find('"', c);
        if (v1 == string::npos)
        {
            i = j + 1;
            continue;
        }
        size_t v2 = txt.find('"', v1 + 1);
        if (v2 == string::npos)
            break;
        string v = txt.substr(v1 + 1, v2 - v1 - 1);
        kvs.push_back({k, v});
        i = v2 + 1;
    }
    return kvs;
}

// -------- Service tests --------
static const map<string, string> SERVICE_KEYS = {
    {"anthropic", "ANTHROPIC_API_KEY"},
    {"azure_openai", "AZURE_OPENAI_API_KEY"},
    {"brave", "BRAVE_API_KEY"},
    {"cohere", "COHERE_API_KEY"},
    {"deepseek", "DEEPSEEK_API_KEY"},
    {"exa", "EXA_API_KEY"},
    {"fireworks", "FIREWORKS_API_KEY"},
    {"gemini", "GEMINI_API_KEY"}, // accepts GOOGLE_* too via env fallback
    {"groq", "GROQ_API_KEY"},
    {"huggingface", "HUGGINGFACE_TOKEN"},
    {"mistral", "MISTRAL_API_KEY"},
    {"openai", "OPENAI_API_KEY"},
    {"openrouter", "OPENROUTER_API_KEY"},
    {"perplexity", "PERPLEXITY_API_KEY"},
    {"sambanova", "SAMBANOVA_API_KEY"},
    {"tavily", "TAVILY_API_KEY"},
    {"together", "TOGETHER_API_KEY"},
    {"xai", "XAI_API_KEY"},
    // Cloud providers
    {"aws", "AWS_ACCESS_KEY_ID"},
    {"gcp", "GOOGLE_APPLICATION_CREDENTIALS"},
    {"azure", "AZURE_CLIENT_ID"},
    {"github", "GITHUB_TOKEN"},
    {"docker", "DOCKER_AUTH_TOKEN"},
    // Database providers
    {"mongodb", "MONGODB_URI"},
    {"postgres", "DATABASE_URL"},
    {"redis", "REDIS_URL"},
    // Other common services
    {"stripe", "STRIPE_SECRET_KEY"},
    {"sendgrid", "SENDGRID_API_KEY"},
    {"twilio", "TWILIO_AUTH_TOKEN"},
    {"slack", "SLACK_API_TOKEN"},
    {"discord", "DISCORD_TOKEN"},
    {"vercel", "VERCEL_TOKEN"},
    {"netlify", "NETLIFY_AUTH_TOKEN"},
};

// Get all known service provider keys
static unordered_set<string> getKnownServiceKeys() {
    unordered_set<string> keys;
    for (const auto& service : SERVICE_KEYS) {
        keys.insert(service.second);
    }
    // Add common variations
    keys.insert("AWS_SECRET_ACCESS_KEY");
    keys.insert("AWS_SESSION_TOKEN");
    keys.insert("GOOGLE_CLOUD_PROJECT");
    keys.insert("AZURE_CLIENT_SECRET");
    keys.insert("AZURE_TENANT_ID");
    keys.insert("GITHUB_CLIENT_ID");
    keys.insert("GITHUB_CLIENT_SECRET");
    keys.insert("DOCKER_USERNAME");
    keys.insert("DOCKER_PASSWORD");
    keys.insert("STRIPE_PUBLISHABLE_KEY");
    keys.insert("SENDGRID_FROM_EMAIL");
    keys.insert("TWILIO_ACCOUNT_SID");
    keys.insert("SLACK_WEBHOOK_URL");
    keys.insert("DISCORD_CLIENT_ID");
    keys.insert("DISCORD_CLIENT_SECRET");
    return keys;
}

static bool curl_ok(const string &args)
{
    string cmd = "curl -sS -f -L --connect-timeout 5 --max-time 12 " + args + " >/dev/null";
    return system(cmd.c_str()) == 0;
}
static bool test_one(const Config &cfg, const string &svc)
{
    auto it = SERVICE_KEYS.find(svc);
    if (it == SERVICE_KEYS.end())
        return false;
    string keyname = it->second;
    // get key from vault or env
    KeyStore ks = loadVault(cfg);
    string k = ks.kv.count(keyname) ? ks.kv[keyname] : (getenv(keyname.c_str()) ? string(getenv(keyname.c_str())) : "");
    if (svc == "gemini" && k.empty())
    {
        k = getenvs("GOOGLE_GENAI_API_KEY", getenvs("GOOGLE_API_KEY", getenvs("GEMINI_API_KEY")));
        if (k.empty() && ks.kv.count("GEMINI_API_KEY"))
            k = ks.kv["GEMINI_API_KEY"];
    }
    if (k.empty())
        return false;

    if (svc == "anthropic")
        return curl_ok("-X POST https://api.anthropic.com/v1/messages -H \"x-api-key: " + k + "\" -H \"anthropic-version: 2023-06-01\" -H \"content-type: application/json\" -d '{\"model\":\"claude-3-haiku-20240307\",\"max_tokens\":4,\"messages\":[{\"role\":\"user\",\"content\":\"ping\"}]}'");
    if (svc == "azure_openai")
    {
        string ep = getenvs("AZURE_OPENAI_ENDPOINT");
        if (ep.empty())
            return false;
        return curl_ok("-H \"api-key: " + k + "\" \"" + ep + "/openai/models?api-version=2024-10-21\"");
    }
    if (svc == "brave")
        return curl_ok("-H \"X-Subscription-Token: " + k + "\" \"https://api.search.brave.com/res/v1/web/search?q=ping\"");
    if (svc == "cohere")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.cohere.com/v1/models");
    if (svc == "deepseek")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.deepseek.com/v1/models");
    if (svc == "exa")
        return curl_ok("-X POST https://api.exa.ai/search -H \"x-api-key: " + k + "\" -H \"content-type: application/json\" -d '{\"query\":\"ping\",\"numResults\":1}'");
    if (svc == "fireworks")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.fireworks.ai/inference/v1/models");
    if (svc == "gemini")
        return curl_ok("\"https://generativelanguage.googleapis.com/v1beta/models?key=" + k + "\"");
    if (svc == "groq")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.groq.com/openai/v1/models");
    if (svc == "huggingface")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://huggingface.co/api/whoami-v2");
    if (svc == "mistral")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.mistral.ai/v1/models");
    if (svc == "openai")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.openai.com/v1/models");
    if (svc == "openrouter")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://openrouter.ai/api/v1/models");
    if (svc == "perplexity")
        return curl_ok("-X POST https://api.perplexity.ai/chat/completions -H \"Authorization: Bearer " + k + "\" -H \"Content-Type: application/json\" -d '{\"model\":\"sonar\",\"messages\":[{\"role\":\"user\",\"content\":\"hello\"}],\"max_tokens\":4}'");
    if (svc == "sambanova")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.sambanova.ai/v1/models");
    if (svc == "tavily")
        return curl_ok("-X POST https://api.tavily.com/search -H \"Content-Type: application/json\" -d '{\"api_key\":\"" + k + "\",\"query\":\"ping\"}'");
    if (svc == "together")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.together.ai/v1/models");
    if (svc == "xai")
        return curl_ok("-H \"Authorization: Bearer " + k + "\" https://api.x.ai/v1/models");
    return false;
}

// -------- Guard --------
static void guard_enable(const Config &)
{
    int ec = 0;
    string hook = runCmdCapture("git rev-parse --git-path hooks/pre-commit", &ec);
    if (ec != 0 || hook.empty())
        error(*(new Config), "Not a git repo.");
    if (!hook.empty() && hook.back() == '\n')
        hook.pop_back();
    fs::create_directories(fs::path(hook).parent_path());
    const char *script =
        R"(#!/usr/bin/env bash
set -euo pipefail
files=$(git diff --cached --name-only --diff-filter=ACM)
[ -z "$files" ] && exit 0
if command -v gitleaks >/dev/null 2>&1; then
  gitleaks protect --staged
else
  if grep -EIHn --exclude-dir=.git -- $'AKIA|ASIA|ghp_[A-Za-z0-9]{36}|xox[baprs]-|-----BEGIN (PRIVATE|OPENSSH PRIVATE) KEY-----|api_key|_API_KEY|_TOKEN' $files; then
    echo "ak guard: possible secrets detected in staged files above."
    echo "Commit aborted. Override with: git commit -n"
    exit 1
  fi
fi
exit 0
)";
    ofstream out(hook, ios::trunc);
    out << script;
    out.close();
    chmod(hook.c_str(), 0755);
    cerr << "✅ Installed pre-commit secret guard.\n";
}
static void guard_disable()
{
    int ec = 0;
    string hook = runCmdCapture("git rev-parse --git-path hooks/pre-commit", &ec);
    if (ec != 0 || hook.empty())
    {
        cerr << "ℹ️  No guard (not a git repo).\n";
        return;
    }
    if (!hook.empty() && hook.back() == '\n')
        hook.pop_back();
    if (unlink(hook.c_str()) == 0)
        cerr << "✅ Removed pre-commit secret guard.\n";
    else
        cerr << "ℹ️  No guard installed.\n";
}

// -------- Ensure default profile --------
static void ensureDefaultProfile(const Config &cfg)
{
    auto p = profilePath(cfg, "default");
    if (!fs::exists(p))
    {
        fs::create_directories(cfg.profilesDir);
        ofstream(p).close();
    }
}

static string getCwd()
{
    char buf[4096];
    if (getcwd(buf, sizeof(buf)))
        return string(buf);
    return ".";
}

static string sha256_hex(const string &s)
{
    SHA256 h;
    h.update(s);
    return h.final();
}

// path for per-directory mapping: one file per cwd-hash
static string mappingFileForDir(const Config &cfg, const string &dir)
{
    string h = sha256_hex(dir);
    return (fs::path(cfg.persistDir) / (h + ".map")).string();
}

// read list of profiles mapped to this dir
static vector<string> readDirProfiles(const Config &cfg, const string &dir)
{
    vector<string> out;
    string mapf = mappingFileForDir(cfg, dir);
    if (!fs::exists(mapf))
        return out;
    ifstream in(mapf);
    string line;
    if (!getline(in, line))
        return out;
    // stored as: "<abs-dir>\tprofile1,profile2,..."
    auto tab = line.find('\t');
    if (tab == string::npos)
        return out;
    string stored_dir = line.substr(0, tab);
    if (stored_dir != dir)
        return out; // safety
    string csv = line.substr(tab + 1);
    string item;
    stringstream ss(csv);
    while (getline(ss, item, ','))
    {
        item = trim(item);
        if (!item.empty())
            out.push_back(item);
    }
    sort(out.begin(), out.end());
    out.erase(unique(out.begin(), out.end()), out.end());
    return out;
}

static void writeDirProfiles(const Config &cfg, const string &dir, const vector<string> &profiles)
{
    fs::create_directories(cfg.persistDir);
    ofstream out(mappingFileForDir(cfg, dir), ios::trunc);
    // one line: "<abs-dir>\tprofile1,profile2,..."
    out << dir << "\t";
    for (size_t i = 0; i < profiles.size(); ++i)
    {
        if (i)
            out << ",";
        out << profiles[i];
    }
    out << "\n";
}

// bundle file for a profile (encrypted export blob)
static string bundleFile(const Config &cfg, const string &profile)
{
    return (fs::path(cfg.persistDir) / (profile + ".bundle")).string();
}

// create/update encrypted bundle for a profile
static void writeEncryptedBundle(const Config &cfg, const string &profile, const string &exports)
{
    fs::create_directories(cfg.persistDir);
    // We use openssl aes-256-cbc -base64 -pass pass:... (same trick as the bash version)
    string pass = "ak-persist-" + getenvs("USER", "user");
    string tmp = (fs::path(cfg.persistDir) / (profile + ".tmp")).string();
    {
        ofstream o(tmp, ios::trunc);
        o << exports;
    }
    string cmd = "openssl enc -aes-256-cbc -base64 -pass pass:" + pass +
                 " -in '" + tmp + "' -out '" + bundleFile(cfg, profile) + "' 2>/dev/null";
    int rc = system(cmd.c_str());
    fs::remove(tmp);
    if (rc != 0)
        warn(cfg, "Could not encrypt persist bundle (openssl missing?)");
}

// decrypt bundle (returns empty on error)
[[maybe_unused]] static string readEncryptedBundle(const Config &cfg, const string &profile)
{
    string pass = "ak-persist-" + getenvs("USER", "user");
    string cmd = "openssl enc -aes-256-cbc -base64 -d -pass pass:" + pass +
                 " -in '" + bundleFile(cfg, profile) + "' 2>/dev/null";
    int ec = 0;
    string out = runCmdCapture(cmd, &ec);
    return (ec == 0) ? out : string();
}

// make `export` script for a profile (as one string)
static string makeExportsForProfile(const Config &cfg, const string &name)
{
    auto keys = readProfile(cfg, name);
    KeyStore ks = loadVault(cfg);
    ostringstream oss;
    for (auto &k : keys)
    {
        auto it = ks.kv.find(k);
        if (it == ks.kv.end())
            continue;
        string v = it->second, esc;
        esc.reserve(v.size() * 2);
        for (char c : v)
        {
            if (c == '\\' || c == '"')
                esc.push_back('\\');
            if (c == '\n')
            {
                esc += "\\n";
                continue;
            }
            esc.push_back(c);
        }
        oss << "export " << k << "=\"" << esc << "\"\n";
    }
    return oss.str();
}

[[maybe_unused]] static string detectShellRc()
{
    // Best effort. We’ll touch both bashrc and zshrc below, but pick a primary for logs.
    const char *sh = getenv("SHELL");
    if (sh && string(sh).find("zsh") != string::npos)
        return string(getenv("HOME")) + "/.zshrc";
    return string(getenv("HOME")) + "/.bashrc";
}

static bool fileContains(const string &path, const string &needle)
{
    if (!fs::exists(path))
        return false;
    ifstream in(path);
    string line;
    while (getline(in, line))
    {
        if (line.find(needle) != string::npos)
            return true;
    }
    return false;
}

static void appendLine(const string &path, const string &line)
{
    ofstream out(path, ios::app);
    out << line << "\n";
}
// Resolve target user (handles sudo) for correct HOME and SHELL
struct TargetUser {
    string home;
    string shellPath;
    string shellName;
};

static TargetUser resolveTargetUser() {
    TargetUser t;
    string sudoUser = getenvs("SUDO_USER");
#ifdef __unix__
    if (!sudoUser.empty()) {
        struct passwd *pw = getpwnam(sudoUser.c_str());
        if (pw) {
            if (pw->pw_dir) t.home = pw->pw_dir;
            if (pw->pw_shell) t.shellPath = pw->pw_shell;
        }
    }
    if (t.home.empty()) {
        const char* h = getenv("HOME");
        if (h) t.home = h;
        else {
            struct passwd* pw = getpwuid(getuid());
            if (pw && pw->pw_dir) t.home = pw->pw_dir;
        }
    }
    if (t.shellPath.empty()) {
        const char* sh = getenv("SHELL");
        if (sh) t.shellPath = sh;
        else {
            struct passwd* pw = getpwuid(getuid());
            if (pw && pw->pw_shell) t.shellPath = pw->pw_shell;
            else t.shellPath = "/bin/bash";
        }
    }
#else
    const char* h = getenv("HOME");
    if (h) t.home = h; else t.home = "";
    const char* sh = getenv("SHELL");
    t.shellPath = sh ? sh : "/bin/bash";
#endif
    t.shellName = fs::path(t.shellPath).filename().string();
    return t;
}

static void writeShellInitFile(const Config &cfg)
{
    fs::create_directories(cfg.configDir);
    string initPath = (fs::path(cfg.configDir) / "shell-init.sh").string();

    // ----- your provided snippet verbatim -----
    const char *snippet = R"DELIM(# --- ak shell auto-load (directory persistence) ---
# Source this in ~/.bashrc or ~/.zshrc

ak_auto_load_dir() {
  # require openssl + ak
  command -v ak >/dev/null 2>&1 || return
  command -v openssl >/dev/null 2>&1 || return

  local cfg_dir="${XDG_CONFIG_HOME:-$HOME/.config}/ak"
  local persist_dir="$cfg_dir/persist"

  [ -d "$persist_dir" ] || return

  # hash current dir the same way ak does (sha256, first 64 hex is fine)
  # portable: use sha256sum if available, else python fallback
  local hash
  if command -v sha256sum >/dev/null 2>&1; then
    hash=$(printf '%s' "$(pwd)" | sha256sum | awk '{print $1}')
  else
    hash=$(python3 - <<'PY'
import hashlib, os, sys
print(hashlib.sha256(os.getcwd().encode()).hexdigest())
PY
)
  fi

  local map_file="$persist_dir/${hash}.map"
  [ -f "$map_file" ] || return

  # format: "<abs-dir>\tprofile1,profile2,..."
  local line
  line=$(head -n1 "$map_file" 2>/dev/null) || return
  local mapped_dir profiles_csv
  mapped_dir="${line%%	*}"
  profiles_csv="${line#*	}"

  # only auto-load if exact path matches (no prefix games)
  [ "$mapped_dir" = "$(pwd)" ] || return

  # iterate profiles in CSV (bash/zsh compatible)
  if [ -n "$ZSH_VERSION" ]; then
    # zsh syntax
    IFS=',' read -A profiles <<< "$profiles_csv"
  else
    # bash syntax
    IFS=',' read -r -a profiles <<< "$profiles_csv"
  fi
  for prof in "${profiles[@]}"; do
    prof="${prof## }"; prof="${prof%% }"
    bundle="$persist_dir/${prof}.bundle"
    [ -f "$bundle" ] || continue
    # decrypt + eval
    exports=$(openssl enc -aes-256-cbc -base64 -d -pass pass:"ak-persist-$USER" -in "$bundle" 2>/dev/null)
    if [ -n "$exports" ]; then
      eval "$exports"
      export AK_KEYS_LOADED="${AK_KEYS_LOADED:+$AK_KEYS_LOADED,}$prof"
    fi
  done
}

# zsh: chpwd hook; bash: PROMPT_COMMAND
if [ -n "$ZSH_VERSION" ]; then
  autoload -Uz add-zsh-hook 2>/dev/null || true
  add-zsh-hook chpwd ak_auto_load_dir
  # run once for the initial directory
  ak_auto_load_dir
elif [ -n "$BASH_VERSION" ]; then
  case ":$PROMPT_COMMAND:" in
    *:ak_auto_load_dir:*) ;;
    *) PROMPT_COMMAND="ak_auto_load_dir${PROMPT_COMMAND:+;$PROMPT_COMMAND}" ;;
  esac
  # run once for the initial directory
  ak_auto_load_dir
fi
# --- end ak shell auto-load ---

# Shell wrapper function for ak commands that need to modify current shell
ak() {
  local cmd="${1:-help}"
  
  case "$cmd" in
    load)
      # Load profile: eval the export statements
      if [ $# -lt 2 ]; then
        command ak "$@"
        return $?
      fi
      local output
      output=$(AK_SHELL_WRAPPER_ACTIVE=1 command ak "$@" 2>&1)
      local exit_code=$?
      if [ $exit_code -eq 0 ] && [ -n "$output" ]; then
        # Only eval if ak succeeded and produced output
        eval "$output"
      else
        # Print any error messages
        [ -n "$output" ] && echo "$output" >&2
      fi
      return $exit_code
      ;;
    unload)
      # Unload profile: eval the unset statements
      if [ $# -lt 2 ]; then
        command ak "$@"
        return $?
      fi
      local output
      output=$(AK_SHELL_WRAPPER_ACTIVE=1 command ak "$@" 2>&1)
      local exit_code=$?
      if [ $exit_code -eq 0 ] && [ -n "$output" ]; then
        # Only eval if ak succeeded and produced output
        eval "$output"
        echo "✅ Unloaded profile: ${2}"
      else
        # Print any error messages
        [ -n "$output" ] && echo "$output" >&2
      fi
      return $exit_code
      ;;
    *)
      # Pass through all other commands to the real ak binary
      command ak "$@"
      ;;
  esac
}
)DELIM";

    ofstream out(initPath, ios::trunc);
    out << snippet;
#ifdef __unix__
    ::chmod(initPath.c_str(), 0644);
#endif
}

// -------- Short Flag Support --------
static vector<string> expandShortFlags(const vector<string> &args)
{
    vector<string> expanded;
    
    for (const auto &arg : args)
    {
        if (arg.size() == 2 && arg[0] == '-' && arg[1] != '-')
        {
            // Handle short flags
            char flag = arg[1];
            switch (flag)
            {
                case 'p':
                    expanded.push_back("--profile");
                    break;
                case 'f':
                    expanded.push_back("--format");
                    break;
                case 'o':
                    expanded.push_back("--output");
                    break;
                case 'i':
                    expanded.push_back("--file");  // for import, -i means input file
                    break;
                case 'j':
                    expanded.push_back("--json");
                    break;
                case 'h':
                    expanded.push_back("--help");
                    break;
                default:
                    expanded.push_back(arg); // Keep unknown short flags as-is
                    break;
            }
        }
        else
        {
            expanded.push_back(arg);
        }
    }
    
    return expanded;
}

// Helper functions to write completion scripts to files
static void writeBashCompletionToFile(const string &path)
{
    ofstream out(path, ios::trunc);
    out << "#!/bin/bash\n"
        << "_ak_completion()\n"
        << "{\n"
        << "    local cur prev opts commands\n"
        << "    COMPREPLY=()\n"
        << "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
        << "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
        << "    \n"
        << "    commands=\"help backend set get ls rm search cp save load unload env export import migrate profiles run guard test doctor audit install-shell uninstall completion\"\n"
        << "    \n"
        << "    # Handle subcommands and options\n"
        << "    case \"${prev}\" in\n"
        << "        ak)\n"
        << "            COMPREPLY=($(compgen -W \"${commands}\" -- ${cur}))\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        get|cp|rm)\n"
        << "            # Complete with secret names\n"
        << "            if command -v ak >/dev/null 2>&1; then\n"
        << "                local secrets=$(ak ls 2>/dev/null | awk '{print $1}')\n"
        << "                COMPREPLY=($(compgen -W \"${secrets}\" -- ${cur}))\n"
        << "            fi\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        save|load|unload|env)\n"
        << "            # Complete with profile names\n"
        << "            if command -v ak >/dev/null 2>&1; then\n"
        << "                local profiles=$(ak profiles 2>/dev/null)\n"
        << "                COMPREPLY=($(compgen -W \"${profiles}\" -- ${cur}))\n"
        << "            fi\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        --profile|-p)\n"
        << "            # Complete with profile names\n"
        << "            if command -v ak >/dev/null 2>&1; then\n"
        << "                local profiles=$(ak profiles 2>/dev/null)\n"
        << "                COMPREPLY=($(compgen -W \"${profiles}\" -- ${cur}))\n"
        << "            fi\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        --format|-f)\n"
        << "            COMPREPLY=($(compgen -W \"env dotenv json yaml\" -- ${cur}))\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        guard)\n"
        << "            COMPREPLY=($(compgen -W \"enable disable\" -- ${cur}))\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        test)\n"
        << "            COMPREPLY=($(compgen -W \"aws gcp azure github docker heroku --all\" -- ${cur}))\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        completion)\n"
        << "            COMPREPLY=($(compgen -W \"bash zsh fish\" -- ${cur}))\n"
        << "            return 0\n"
        << "            ;;\n"
        << "        *)\n"
        << "            # Handle flags\n"
        << "            if [[ ${cur} == -* ]]; then\n"
        << "                case \"${COMP_WORDS[1]}\" in\n"
        << "                    get)\n"
        << "                        COMPREPLY=($(compgen -W \"--full\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    ls)\n"
        << "                        COMPREPLY=($(compgen -W \"--json -j\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    load|unload)\n"
        << "                        COMPREPLY=($(compgen -W \"--persist\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    export)\n"
        << "                        COMPREPLY=($(compgen -W \"--profile -p --format -f --output -o\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    import)\n"
        << "                        COMPREPLY=($(compgen -W \"--profile -p --format -f --file -i\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    test)\n"
        << "                        COMPREPLY=($(compgen -W \"--json -j --fail-fast --all\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    env)\n"
        << "                        COMPREPLY=($(compgen -W \"--profile -p\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    run)\n"
        << "                        COMPREPLY=($(compgen -W \"--profile -p\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                    *)\n"
        << "                        COMPREPLY=($(compgen -W \"--json -j --help -h\" -- ${cur}))\n"
        << "                        ;;\n"
        << "                esac\n"
        << "            fi\n"
        << "            ;;\n"
        << "    esac\n"
        << "}\n"
        << "\n"
        << "complete -F _ak_completion ak\n";
    out.close();
}

static void writeZshCompletionToFile(const string &path)
{
    ofstream out(path, ios::trunc);
    out << "#compdef ak\n"
        << "\n"
        << "_ak() {\n"
        << "    local context state state_descr line\n"
        << "    local -A opt_args\n"
        << "\n"
        << "    _arguments -C \\\n"
        << "        '(--json -j)'{--json,-j}'[Output in JSON format]' \\\n"
        << "        '(--help -h)'{--help,-h}'[Show help message]' \\\n"
        << "        '1: :->commands' \\\n"
        << "        '*:: :->args' && return 0\n"
        << "\n"
        << "    case $state in\n"
        << "        commands)\n"
        << "            local commands=(\n"
        << "                'help:Show help message'\n"
        << "                'backend:Show backend information'\n"
        << "                'set:Set a secret value'\n"
        << "                'get:Get a secret value'\n"
        << "                'ls:List all secrets'\n"
        << "                'rm:Remove a secret'\n"
        << "                'search:Search for secrets'\n"
        << "                'cp:Copy secret to clipboard'\n"
        << "                'save:Save secrets to profile'\n"
        << "                'load:Load profile environment'\n"
        << "                'unload:Unload profile environment'\n"
        << "                'env:Show profile as exports'\n"
        << "                'export:Export profile to file'\n"
        << "                'import:Import secrets from file'\n"
        << "                'migrate:Migrate from old format'\n"
        << "                'profiles:List profiles'\n"
        << "                'run:Run command with profile'\n"
        << "                'guard:Enable/disable shell guard'\n"
        << "                'test:Test service connectivity'\n"
        << "                'doctor:Check configuration'\n"
        << "                'audit:Show audit log'\n"
        << "                'install-shell:Install shell integration'\n"
        << "                'uninstall:Remove shell integration'\n"
        << "                'completion:Generate completion script'\n"
        << "            )\n"
        << "            _describe 'ak commands' commands\n"
        << "            ;;\n"
        << "        args)\n"
        << "            case $line[1] in\n"
        << "                get|cp|rm)\n"
        << "                    _ak_secrets\n"
        << "                    ;;\n"
        << "                save|load|unload|env)\n"
        << "                    _ak_profiles\n"
        << "                    ;;\n"
        << "                guard)\n"
        << "                    _arguments '1:action:(enable disable)'\n"
        << "                    ;;\n"
        << "                test)\n"
        << "                    _arguments \\\n"
        << "                        '(--json -j)'{--json,-j}'[JSON output]' \\\n"
        << "                        '--fail-fast[Stop on first failure]' \\\n"
        << "                        '--all[Test all services]' \\\n"
        << "                        '1:service:(aws gcp azure github docker heroku)'\n"
        << "                    ;;\n"
        << "                export)\n"
        << "                    _arguments \\\n"
        << "                        '(--profile -p)'{--profile,-p}'[Profile name]:profile:_ak_profiles' \\\n"
        << "                        '(--format -f)'{--format,-f}'[Export format]:format:(env dotenv json yaml)' \\\n"
        << "                        '(--output -o)'{--output,-o}'[Output file]:file:_files'\n"
        << "                    ;;\n"
        << "                import)\n"
        << "                    _arguments \\\n"
        << "                        '(--profile -p)'{--profile,-p}'[Profile name]:profile:_ak_profiles' \\\n"
        << "                        '(--format -f)'{--format,-f}'[Import format]:format:(env dotenv json yaml)' \\\n"
        << "                        '(--file -i)'{--file,-i}'[Input file]:file:_files'\n"
        << "                    ;;\n"
        << "                completion)\n"
        << "                    _arguments '1:shell:(bash zsh fish)'\n"
        << "                    ;;\n"
        << "            esac\n"
        << "            ;;\n"
        << "    esac\n"
        << "}\n"
        << "\n"
        << "_ak_secrets() {\n"
        << "    local secrets\n"
        << "    secrets=($(ak ls 2>/dev/null | awk '{print $1}'))\n"
        << "    _describe 'secrets' secrets\n"
        << "}\n"
        << "\n"
        << "_ak_profiles() {\n"
        << "    local profiles\n"
        << "    profiles=($(ak profiles 2>/dev/null))\n"
        << "    _describe 'profiles' profiles\n"
        << "}\n"
        << "\n"
        << "_ak \"$@\"\n";
    out.close();
}

static void writeFishCompletionToFile(const string &path)
{
    ofstream out(path, ios::trunc);
    out << "# Fish completion for ak\n"
        << "\n"
        << "# Commands\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'help backend set get ls rm search cp save load unload env export import migrate profiles run guard test doctor audit install-shell uninstall completion'\n"
        << "\n"
        << "# Command descriptions\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'help' -d 'Show help message'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'backend' -d 'Show backend information'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'set' -d 'Set a secret value'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'get' -d 'Get a secret value'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'ls' -d 'List all secrets'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'rm' -d 'Remove a secret'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'search' -d 'Search for secrets'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'cp' -d 'Copy secret to clipboard'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'save' -d 'Save secrets to profile'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'load' -d 'Load profile environment'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'unload' -d 'Unload profile environment'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'env' -d 'Show profile as exports'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'export' -d 'Export profile to file'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'import' -d 'Import secrets from file'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'migrate' -d 'Migrate from old format'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'profiles' -d 'List profiles'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'run' -d 'Run command with profile'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'guard' -d 'Enable/disable shell guard'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'test' -d 'Test service connectivity'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'doctor' -d 'Check configuration'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'audit' -d 'Show audit log'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'install-shell' -d 'Install shell integration'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'uninstall' -d 'Remove shell integration'\n"
        << "complete -c ak -n '__fish_use_subcommand' -xa 'completion' -d 'Generate completion script'\n"
        << "\n"
        << "# Global options\n"
        << "complete -c ak -l json -s j -d 'Output in JSON format'\n"
        << "complete -c ak -l help -s h -d 'Show help message'\n"
        << "\n"
        << "# Secret name completions for get, cp, rm\n"
        << "complete -c ak -n '__fish_seen_subcommand_from get cp rm' -xa '(ak ls 2>/dev/null | awk \"{print \\$1}\")'\n"
        << "\n"
        << "# Profile name completions for save, load, env, run (unload profiles are optional)\n"
        << "complete -c ak -n '__fish_seen_subcommand_from save load env run' -xa '(ak profiles 2>/dev/null)'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from unload' -xa '(ak profiles 2>/dev/null)' -d 'Profile to unload (optional - unloads all if none specified)'\n"
        << "\n"
        << "# Options for specific commands\n"
        << "complete -c ak -n '__fish_seen_subcommand_from get' -l full -d 'Show full value unmasked'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from ls' -l json -s j -d 'Output in JSON format'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from load unload' -l persist -d 'Persist for current directory'\n"
        << "\n"
        << "# Profile options with short flags\n"
        << "complete -c ak -n '__fish_seen_subcommand_from export import env run' -l profile -s p -d 'Profile name' -xa '(ak profiles 2>/dev/null)'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from export import' -l format -s f -d 'File format' -xa 'env dotenv json yaml'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from export' -l output -s o -d 'Output file' -F\n"
        << "complete -c ak -n '__fish_seen_subcommand_from import' -l file -s i -d 'Input file' -F\n"
        << "\n"
        << "complete -c ak -n '__fish_seen_subcommand_from guard' -xa 'enable disable'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from test' -xa 'aws gcp azure github docker heroku'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from test' -l json -s j -d 'JSON output'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from test' -l fail-fast -d 'Stop on first failure'\n"
        << "complete -c ak -n '__fish_seen_subcommand_from test' -l all -d 'Test all services'\n"
        << "\n"
        << "complete -c ak -n '__fish_seen_subcommand_from completion' -xa 'bash zsh fish'\n";
    out.close();
}

static void ensureSourcedInRc(const Config &cfg)
{
    // Install into the invoking user's shell rc (handles sudo correctly)
    TargetUser t = resolveTargetUser();

    string initPath = (fs::path(cfg.configDir) / "shell-init.sh").string();
    string sourceLine = "source \"" + initPath + "\"";

    string configFile;
    string completionFile;
    
    // Install completion scripts for the appropriate shell
    if (t.shellName == "zsh") {
        configFile = t.home + "/.zshrc";
        // Install zsh completion
        string zshCompDir = t.home + "/.config/zsh/completions";
        fs::create_directories(zshCompDir);
        completionFile = zshCompDir + "/_ak";
        
        writeZshCompletionToFile(completionFile);
        
        // Add fpath line to zshrc if not present
        string fpathLine = "fpath=(~/.config/zsh/completions $fpath)";
        if (!fileContains(configFile, fpathLine)) {
            appendLine(configFile, fpathLine);
        }
        string autoloadLine = "autoload -U compinit && compinit";
        if (!fileContains(configFile, autoloadLine)) {
            appendLine(configFile, autoloadLine);
        }
        
    } else if (t.shellName == "bash") {
        configFile = t.home + "/.bashrc";
        // Install bash completion
        completionFile = t.home + "/.config/ak/ak-completion.bash";
        
        writeBashCompletionToFile(completionFile);
        
        // Source completion in bashrc
        string compSourceLine = "source \"" + completionFile + "\"";
        if (!fileContains(configFile, compSourceLine)) {
            appendLine(configFile, compSourceLine);
        }
        
    } else if (t.shellName == "fish") {
        string fishConfigDir = t.home + "/.config/fish";
        fs::create_directories(fishConfigDir);
        configFile = fishConfigDir + "/config.fish";
        
        // Install fish completion
        string fishCompDir = fishConfigDir + "/completions";
        fs::create_directories(fishCompDir);
        completionFile = fishCompDir + "/ak.fish";
        
        writeFishCompletionToFile(completionFile);
        
    } else {
        // Fallback to .profile for unknown shells
        configFile = t.home + "/.profile";
        // Install basic bash completion as fallback
        completionFile = t.home + "/.config/ak/ak-completion.bash";
        
        writeBashCompletionToFile(completionFile);
        
        string compSourceLine = "source \"" + completionFile + "\"";
        if (!fileContains(configFile, compSourceLine)) {
            appendLine(configFile, compSourceLine);
        }
    }

    // Create config file if it doesn't exist
    if (!fs::exists(configFile)) {
        ofstream(configFile).close();
    }

    // Add source line if not already present
    if (!fileContains(configFile, sourceLine)) {
        appendLine(configFile, ""); // spacer
        appendLine(configFile, "# Added by ak installer");
        appendLine(configFile, sourceLine);
        cerr << "✅ Added ak shell integration to " << configFile << "\n";
    } else {
        cerr << "✅ Shell integration already configured in " << configFile << "\n";
    }
    
    // Report completion installation
    if (!completionFile.empty()) {
        cerr << "✅ Installed " << t.shellName << " completion to " << completionFile << "\n";
    }
}

// -------- Completion Functions --------
static void generateBashCompletion()
{
    cout << "#!/bin/bash\n"
            "_ak_completion()\n"
            "{\n"
            "    local cur prev opts commands\n"
            "    COMPREPLY=()\n"
            "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
            "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
            "    \n"
            "    commands=\"help backend set get ls rm search cp save load unload env export import migrate profiles run guard test doctor audit install-shell uninstall completion\"\n"
            "    \n"
            "    # Handle subcommands and options\n"
            "    case \"${prev}\" in\n"
            "        ak)\n"
            "            COMPREPLY=($(compgen -W \"${commands}\" -- ${cur}))\n"
            "            return 0\n"
            "            ;;\n"
            "        get|cp|rm)\n"
            "            # Complete with secret names\n"
            "            if command -v ak >/dev/null 2>&1; then\n"
            "                local secrets=$(ak ls 2>/dev/null | awk '{print $1}')\n"
            "                COMPREPLY=($(compgen -W \"${secrets}\" -- ${cur}))\n"
            "            fi\n"
            "            return 0\n"
            "            ;;\n"
            "        save|load|unload|env)\n"
            "            # Complete with profile names\n"
            "            if command -v ak >/dev/null 2>&1; then\n"
            "                local profiles=$(ak profiles 2>/dev/null)\n"
            "                COMPREPLY=($(compgen -W \"${profiles}\" -- ${cur}))\n"
            "            fi\n"
            "            return 0\n"
            "            ;;\n"
            "        --profile)\n"
            "            # Complete with profile names\n"
            "            if command -v ak >/dev/null 2>&1; then\n"
            "                local profiles=$(ak profiles 2>/dev/null)\n"
            "                COMPREPLY=($(compgen -W \"${profiles}\" -- ${cur}))\n"
            "            fi\n"
            "            return 0\n"
            "            ;;\n"
            "        --format)\n"
            "            COMPREPLY=($(compgen -W \"env dotenv json yaml\" -- ${cur}))\n"
            "            return 0\n"
            "            ;;\n"
            "        guard)\n"
            "            COMPREPLY=($(compgen -W \"enable disable\" -- ${cur}))\n"
            "            return 0\n"
            "            ;;\n"
            "        test)\n"
            "            COMPREPLY=($(compgen -W \"aws gcp azure github docker heroku --all\" -- ${cur}))\n"
            "            return 0\n"
            "            ;;\n"
            "        completion)\n"
            "            COMPREPLY=($(compgen -W \"bash zsh fish\" -- ${cur}))\n"
            "            return 0\n"
            "            ;;\n"
            "        *)\n"
            "            # Handle flags\n"
            "            if [[ ${cur} == -* ]]; then\n"
            "                case \"${COMP_WORDS[1]}\" in\n"
            "                    get)\n"
            "                        COMPREPLY=($(compgen -W \"--full\" -- ${cur}))\n"
            "                        ;;\n"
            "                    ls)\n"
            "                        COMPREPLY=($(compgen -W \"--json\" -- ${cur}))\n"
            "                        ;;\n"
            "                    load|unload)\n"
            "                        COMPREPLY=($(compgen -W \"--persist\" -- ${cur}))\n"
            "                        ;;\n"
            "                    export)\n"
            "                        COMPREPLY=($(compgen -W \"--profile --format --output\" -- ${cur}))\n"
            "                        ;;\n"
            "                    import)\n"
            "                        COMPREPLY=($(compgen -W \"--profile --format --file\" -- ${cur}))\n"
            "                        ;;\n"
            "                    test)\n"
            "                        COMPREPLY=($(compgen -W \"--json --fail-fast --all\" -- ${cur}))\n"
            "                        ;;\n"
            "                    *)\n"
            "                        COMPREPLY=($(compgen -W \"--json --help\" -- ${cur}))\n"
            "                        ;;\n"
            "                esac\n"
            "            fi\n"
            "            ;;\n"
            "    esac\n"
            "}\n"
            "\n"
            "complete -F _ak_completion ak\n";
}

static void generateZshCompletion()
{
    cout << "#compdef ak\n"
            "\n"
            "_ak() {\n"
            "    local context state state_descr line\n"
            "    local -A opt_args\n"
            "\n"
            "    _arguments -C \\\n"
            "        '--json[Output in JSON format]' \\\n"
            "        '--help[Show help message]' \\\n"
            "        '1: :->commands' \\\n"
            "        '*:: :->args' && return 0\n"
            "\n"
            "    case $state in\n"
            "        commands)\n"
            "            local commands=(\n"
            "                'help:Show help message'\n"
            "                'backend:Show backend information'\n"
            "                'set:Set a secret value'\n"
            "                'get:Get a secret value'\n"
            "                'ls:List all secrets'\n"
            "                'rm:Remove a secret'\n"
            "                'search:Search for secrets'\n"
            "                'cp:Copy secret to clipboard'\n"
            "                'save:Save secrets to profile'\n"
            "                'load:Load profile environment'\n"
            "                'unload:Unload profile environment'\n"
            "                'env:Show profile as exports'\n"
            "                'export:Export profile to file'\n"
            "                'import:Import secrets from file'\n"
            "                'migrate:Migrate from old format'\n"
            "                'profiles:List profiles'\n"
            "                'run:Run command with profile'\n"
            "                'guard:Enable/disable shell guard'\n"
            "                'test:Test service connectivity'\n"
            "                'doctor:Check configuration'\n"
            "                'audit:Show audit log'\n"
            "                'install-shell:Install shell integration'\n"
            "                'uninstall:Remove shell integration'\n"
            "                'completion:Generate completion script'\n"
            "            )\n"
            "            _describe 'ak commands' commands\n"
            "            ;;\n"
            "        args)\n"
            "            case $line[1] in\n"
            "                get|cp|rm)\n"
            "                    _ak_secrets\n"
            "                    ;;\n"
            "                save|load|unload|env)\n"
            "                    _ak_profiles\n"
            "                    ;;\n"
            "                guard)\n"
            "                    _arguments '1:action:(enable disable)'\n"
            "                    ;;\n"
            "                test)\n"
            "                    _arguments \\\n"
            "                        '--json[JSON output]' \\\n"
            "                        '--fail-fast[Stop on first failure]' \\\n"
            "                        '--all[Test all services]' \\\n"
            "                        '1:service:(aws gcp azure github docker heroku)'\n"
            "                    ;;\n"
            "                export)\n"
            "                    _arguments \\\n"
            "                        '--profile[Profile name]:profile:_ak_profiles' \\\n"
            "                        '--format[Export format]:format:(env dotenv json yaml)' \\\n"
            "                        '--output[Output file]:file:_files'\n"
            "                    ;;\n"
            "                import)\n"
            "                    _arguments \\\n"
            "                        '--profile[Profile name]:profile:_ak_profiles' \\\n"
            "                        '--format[Import format]:format:(env dotenv json yaml)' \\\n"
            "                        '--file[Input file]:file:_files'\n"
            "                    ;;\n"
            "                completion)\n"
            "                    _arguments '1:shell:(bash zsh fish)'\n"
            "                    ;;\n"
            "            esac\n"
            "            ;;\n"
            "    esac\n"
            "}\n"
            "\n"
            "_ak_secrets() {\n"
            "    local secrets\n"
            "    secrets=($(ak ls 2>/dev/null | awk '{print $1}'))\n"
            "    _describe 'secrets' secrets\n"
            "}\n"
            "\n"
            "_ak_profiles() {\n"
            "    local profiles\n"
            "    profiles=($(ak profiles 2>/dev/null))\n"
            "    _describe 'profiles' profiles\n"
            "}\n"
            "\n"
            "_ak \"$@\"\n";
}

static void generateFishCompletion()
{
    cout << "# Fish completion for ak\n"
            "\n"
            "# Commands\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'help backend set get ls rm search cp save load unload env export import migrate profiles run guard test doctor audit install-shell uninstall completion'\n"
            "\n"
            "# Command descriptions\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'help' -d 'Show help message'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'backend' -d 'Show backend information'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'set' -d 'Set a secret value'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'get' -d 'Get a secret value'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'ls' -d 'List all secrets'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'rm' -d 'Remove a secret'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'search' -d 'Search for secrets'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'cp' -d 'Copy secret to clipboard'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'save' -d 'Save secrets to profile'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'load' -d 'Load profile environment'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'unload' -d 'Unload profile environment'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'env' -d 'Show profile as exports'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'export' -d 'Export profile to file'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'import' -d 'Import secrets from file'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'migrate' -d 'Migrate from old format'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'profiles' -d 'List profiles'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'run' -d 'Run command with profile'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'guard' -d 'Enable/disable shell guard'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'test' -d 'Test service connectivity'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'doctor' -d 'Check configuration'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'audit' -d 'Show audit log'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'install-shell' -d 'Install shell integration'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'uninstall' -d 'Remove shell integration'\n"
            "complete -c ak -n '__fish_use_subcommand' -xa 'completion' -d 'Generate completion script'\n"
            "\n"
            "# Global options\n"
            "complete -c ak -l json -d 'Output in JSON format'\n"
            "complete -c ak -l help -d 'Show help message'\n"
            "\n"
            "# Secret name completions for get, cp, rm\n"
            "complete -c ak -n '__fish_seen_subcommand_from get cp rm' -xa '(ak ls 2>/dev/null | awk \"{print \\$1}\")'\n"
            "\n"
            "# Profile name completions for save, load, env (unload profiles are optional)\n"
            "complete -c ak -n '__fish_seen_subcommand_from save load env' -xa '(ak profiles 2>/dev/null)'\n"
            "complete -c ak -n '__fish_seen_subcommand_from unload' -xa '(ak profiles 2>/dev/null)' -d 'Profile to unload (optional - unloads all if none specified)'\n"
            "\n"
            "# Options for specific commands\n"
            "complete -c ak -n '__fish_seen_subcommand_from get' -l full -d 'Show full value unmasked'\n"
            "complete -c ak -n '__fish_seen_subcommand_from ls' -l json -d 'Output in JSON format'\n"
            "complete -c ak -n '__fish_seen_subcommand_from load unload' -l persist -d 'Persist for current directory'\n"
            "\n"
            "complete -c ak -n '__fish_seen_subcommand_from export import' -l profile -d 'Profile name' -xa '(ak profiles 2>/dev/null)'\n"
            "complete -c ak -n '__fish_seen_subcommand_from export import' -l format -d 'File format' -xa 'env dotenv json yaml'\n"
            "complete -c ak -n '__fish_seen_subcommand_from export' -l output -d 'Output file' -F\n"
            "complete -c ak -n '__fish_seen_subcommand_from import' -l file -d 'Input file' -F\n"
            "\n"
            "complete -c ak -n '__fish_seen_subcommand_from guard' -xa 'enable disable'\n"
            "complete -c ak -n '__fish_seen_subcommand_from test' -xa 'aws gcp azure github docker heroku'\n"
            "complete -c ak -n '__fish_seen_subcommand_from test' -l json -d 'JSON output'\n"
            "complete -c ak -n '__fish_seen_subcommand_from test' -l fail-fast -d 'Stop on first failure'\n"
            "complete -c ak -n '__fish_seen_subcommand_from test' -l all -d 'Test all services'\n"
            "\n"
            "complete -c ak -n '__fish_seen_subcommand_from completion' -xa 'bash zsh fish'\n";
}

// -------- main --------
int main(int argc, char **argv)
{
    Config cfg;
    string base = getenvs("XDG_CONFIG_HOME", getenvs("HOME") + "/.config");
    cfg.configDir = base + "/ak";
    cfg.profilesDir = cfg.configDir + "/profiles";
    cfg.gpgAvailable = commandExists("gpg");
    if (getenv("AK_DISABLE_GPG"))
        cfg.forcePlain = true;
    if (getenv("AK_PASSPHRASE"))
        cfg.presetPassphrase = getenvs("AK_PASSPHRASE");
    if (cfg.forcePlain)
        cfg.gpgAvailable = false;
    cfg.vaultPath = cfg.configDir + ((cfg.gpgAvailable && !cfg.forcePlain) ? "/keys.env.gpg" : "/keys.env");
    cfg.auditLogPath = cfg.configDir + "/audit.log";
    ensureSecureDir(cfg.configDir);
    cfg.instanceId = loadOrCreateInstanceId(cfg);
    cfg.persistDir = cfg.configDir + "/persist";

    // parse global flags - first expand short flags
    vector<string> rawArgs;
    rawArgs.reserve(argc - 1);
    for (int i = 1; i < argc; ++i)
    {
        rawArgs.push_back(argv[i]);
    }
    
    // Expand short flags
    vector<string> expandedArgs = expandShortFlags(rawArgs);
    
    // Now process global flags
    vector<string> args;
    args.reserve(expandedArgs.size());
    for (const auto &a : expandedArgs)
    {
        if (a == "--json")
            cfg.json = true;
        else
            args.push_back(a);
    }
    string cmd = args.empty() ? "help" : args[0];

    ensureDefaultProfile(cfg);

    if (cmd == "help" || cmd == "--help" || cmd == "-h")
    {
        cmd_help();
        return 0;
    }
    else if (cmd == "backend")
    {
        cout << ((cfg.gpgAvailable && !cfg.forcePlain) ? "gpg" : "plain") << "\n";
        return 0;
    }
    else if (cmd == "set")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak set <NAME>");
        string name = args[1], value = promptSecret("Enter value for " + name + ": ");
        if (value.empty())
            error(cfg, "Empty value");
        KeyStore ks = loadVault(cfg);
        ks.kv[name] = value;
        saveVault(cfg, ks);
        ok(cfg, "Stored " + name + ".");
        auditLog(cfg, "set", {name});
        return 0;
    }
    else if (cmd == "get")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak get <NAME> [--full]");
        string name = args[1];
        bool full = (args.size() >= 3 && args[2] == "--full");
        KeyStore ks = loadVault(cfg);
        auto it = ks.kv.find(name);
        if (it == ks.kv.end())
            error(cfg, name + " not found.");
        cout << (full ? it->second : maskValue(it->second)) << "\n";
        auditLog(cfg, "get", {name});
        return 0;
    }
    else if (cmd == "cp")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak cp <NAME>");
        string name = args[1];
        KeyStore ks = loadVault(cfg);
        auto it = ks.kv.find(name);
        if (it == ks.kv.end())
            error(cfg, name + " not found.");
        if (!copyClipboard(it->second))
            error(cfg, "No clipboard utility found (pbcopy/wl-copy/xclip).");
        ok(cfg, "Copied " + name + " to clipboard.");
        auditLog(cfg, "cp", {name});
        return 0;
    }
    else if (cmd == "ls")
    {
        KeyStore ks = loadVault(cfg);
        vector<string> names;
        names.reserve(ks.kv.size());
        for (auto &p : ks.kv)
            names.push_back(p.first);
        sort(names.begin(), names.end());
        auditLog(cfg, "ls", names);
        if (cfg.json)
        {
            cout << "[";
            bool first = true;
            for (auto &n : names)
            {
                if (!first)
                    cout << ",";
                first = false;
                cout << "{\"name\":\"" << n << "\",\"masked\":\"" << maskValue(ks.kv[n]) << "\"}";
            }
            cout << "]\n";
        }
        else
        {
            for (auto &n : names)
                cout << left << setw(34) << n << " " << maskValue(ks.kv[n]) << "\n";
        }
        return 0;
    }
    else if (cmd == "search")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak search <PATTERN>");
        string pat = args[1];
        KeyStore ks = loadVault(cfg);
        vector<string> hits;
        for (auto &p : ks.kv)
            if (icontains(p.first, pat))
                hits.push_back(p.first);
        sort(hits.begin(), hits.end());
        for (auto &h : hits)
            cout << h << "\n";
        auditLog(cfg, "search", hits);
        return 0;
    }
    else if (cmd == "rm")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak rm <NAME> (remove secret) or ak rm --profile <NAME> (remove profile)");
        
        // Check if removing a profile
        if (args.size() >= 3 && args[1] == "--profile")
        {
            string profileName = args[2];
            auto profiles = listProfiles(cfg);
            if (find(profiles.begin(), profiles.end(), profileName) == profiles.end())
                error(cfg, "Profile '" + profileName + "' not found.");
            
            // Remove profile file
            string profilePath = cfg.profilesDir + "/" + profileName + ".profile";
            if (fs::exists(profilePath))
            {
                fs::remove(profilePath);
                ok(cfg, "Removed profile '" + profileName + "'.");
                auditLog(cfg, "rm_profile", {profileName});
            }
            else
                error(cfg, "Profile file not found: " + profilePath);
            return 0;
        }
        
        // Remove secret key
        string name = args[1];
        KeyStore ks = loadVault(cfg);
        if (!ks.kv.erase(name))
            error(cfg, name + " not found.");
        saveVault(cfg, ks);
        ok(cfg, "Removed " + name + ".");
        auditLog(cfg, "rm", {name});
        return 0;
    }
    else if (cmd == "purge")
    {
        bool backup = true;
        // Check for --no-backup flag
        if (args.size() >= 2 && args[1] == "--no-backup")
            backup = false;
        
        KeyStore ks = loadVault(cfg);
        auto profiles = listProfiles(cfg);
        
        if (ks.kv.empty() && profiles.empty())
        {
            ok(cfg, "Nothing to purge - vault and profiles are already empty.");
            return 0;
        }
        
        string backupDir;
        if (backup)
        {
            // Create backup directory with timestamp
            auto now = chrono::system_clock::now();
            auto time_t = chrono::system_clock::to_time_t(now);
            stringstream ss;
            ss << put_time(gmtime(&time_t), "%Y%m%d_%H%M%S");
            backupDir = cfg.configDir + "/backups/purge_" + ss.str();
            fs::create_directories(backupDir);
            
            // Backup vault
            if (!ks.kv.empty())
            {
                string backupVault = backupDir + "/keys.env";
                ofstream out(backupVault);
                for (auto &p : ks.kv)
                    out << p.first << "=" << base64Encode(p.second) << "\n";
                out.close();
            }
            
            // Backup profiles
            if (!profiles.empty())
            {
                string backupProfilesDir = backupDir + "/profiles";
                fs::create_directories(backupProfilesDir);
                for (const auto& profile : profiles)
                {
                    string srcPath = cfg.profilesDir + "/" + profile;
                    string dstPath = backupProfilesDir + "/" + profile;
                    if (fs::exists(srcPath))
                        fs::copy_file(srcPath, dstPath);
                }
            }
            
            ok(cfg, "Created backup at: " + backupDir);
        }
        
        // Purge all secrets
        size_t secretCount = ks.kv.size();
        ks.kv.clear();
        saveVault(cfg, ks);
        
        // Purge all profiles
        size_t profileCount = profiles.size();
        for (const auto& profile : profiles)
        {
            string profilePath = cfg.profilesDir + "/" + profile + ".profile";
            if (fs::exists(profilePath))
                fs::remove(profilePath);
        }
        
        // Clear persistence directory
        if (fs::exists(cfg.persistDir))
            fs::remove_all(cfg.persistDir);
        
        string msg = "Purged " + to_string(secretCount) + " secrets and " + to_string(profileCount) + " profiles";
        if (backup)
            msg += " (backup created)";
        ok(cfg, msg + ".");
        
        auditLog(cfg, "purge", {});
        return 0;
    }
    else if (cmd == "save")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak save <profile> [NAMES...]");
        string profile = args[1];
        vector<string> names;
        if (args.size() > 2)
            names.insert(names.end(), args.begin() + 2, args.end());
        else
        {
            KeyStore ks = loadVault(cfg);
            for (auto &p : ks.kv)
                names.push_back(p.first);
        }
        writeProfile(cfg, profile, names);
        ok(cfg, "Saved profile '" + profile + "' (" + to_string(names.size()) + " keys).");
        auditLog(cfg, "save_profile", names);
        return 0;
    }
    else if (cmd == "env")
    {
        if (args.size() < 3 || args[1] != "--profile")
            error(cfg, "Usage: ak env --profile <name>");
        string profile = args[2];
        printExportsForProfile(cfg, profile);
        auditLog(cfg, "env", readProfile(cfg, profile));
        return 0;
    }
    else if (cmd == "load")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak load <profile> [--persist]");
        string profile = args[1];
        bool persist = (find(args.begin() + 2, args.end(), string("--persist")) != args.end());

        // If the shell wrapper didn't call us (no AK_SHELL_WRAPPER_ACTIVE) and stdout is a TTY,
        // warn the user that this won't affect the current shell unless eval'd.
        bool wrapperActive = (getenv("AK_SHELL_WRAPPER_ACTIVE") != nullptr);
#ifdef __unix__
        bool stdoutIsTTY = isatty(STDOUT_FILENO);
#else
        bool stdoutIsTTY = true;
#endif
        if (!wrapperActive && stdoutIsTTY && !cfg.json)
        {
            cerr << "⚠️  Not applied to current shell. Use: eval \"$(ak load " << profile
                 << ")\" or run 'ak load " << profile << "' (no ./) after sourcing your shell init.\n";
        }

        // Always print exports for eval in current shell
        string exports = makeExportsForProfile(cfg, profile);
        cout << exports; // user should: eval "$(ak load profile)"

        // Optional: record persistence for this directory
        if (persist)
        {
            string dir = getCwd();
            auto profiles = readDirProfiles(cfg, dir);
            if (find(profiles.begin(), profiles.end(), profile) == profiles.end())
                profiles.push_back(profile);
            sort(profiles.begin(), profiles.end());
            writeDirProfiles(cfg, dir, profiles);

            // write/update encrypted bundle for that profile
            writeEncryptedBundle(cfg, profile, exports);

            ok(cfg, "Profile '" + profile + "' loaded into current shell and persisted for this directory.");
        }

        auditLog(cfg, "load", {profile});
        return 0;
    }

    else if (cmd == "unload")
    {
        vector<string> profilesToUnload;
        bool persist = false;
        
        // Parse arguments to determine profiles and flags
        if (args.size() == 1)
        {
            // No profile specified - unload all profiles
            profilesToUnload = listProfiles(cfg);
        }
        else
        {
            // Check for --persist flag and extract profile names
            for (size_t i = 1; i < args.size(); i++)
            {
                if (args[i] == "--persist")
                    persist = true;
                else
                    profilesToUnload.push_back(args[i]);
            }
            
            // If no profiles specified after removing flags, unload all
            if (profilesToUnload.empty())
                profilesToUnload = listProfiles(cfg);
        }
        
        if (profilesToUnload.empty())
        {
            ok(cfg, "No profiles found to unload.");
            return 0;
        }

        // Warn if user is calling the binary directly in a TTY (unsets won't apply without eval)
        bool wrapperActive = (getenv("AK_SHELL_WRAPPER_ACTIVE") != nullptr);
#ifdef __unix__
        bool stdoutIsTTY = isatty(STDOUT_FILENO);
#else
        bool stdoutIsTTY = true;
#endif
        if (!wrapperActive && stdoutIsTTY && !cfg.json)
        {
            if (profilesToUnload.size() == 1)
            {
                cerr << "⚠️  Not applied to current shell. Use: eval \"$(ak unload " << profilesToUnload[0]
                     << ")\" or run 'ak unload " << profilesToUnload[0] << "' (no ./) after sourcing your shell init.\n";
            }
            else
            {
                cerr << "⚠️  Not applied to current shell. Use: eval \"$(ak unload)\" or run 'ak unload' (no ./) after sourcing your shell init.\n";
            }
        }

        // Print unset lines for all profiles (eval them to drop from current shell)
        unordered_set<string> allKeys;
        for (const string &profile : profilesToUnload)
        {
            for (auto &k : readProfile(cfg, profile))
                allKeys.insert(k);
        }
        
        for (const string &key : allKeys)
            cout << "unset " << key << "\n";

        // If asked, remove from this directory's persistence mapping
        if (persist)
        {
            string dir = getCwd();
            auto profiles = readDirProfiles(cfg, dir);
            vector<string> kept;
            kept.reserve(profiles.size());
            for (auto &p : profiles)
            {
                if (find(profilesToUnload.begin(), profilesToUnload.end(), p) == profilesToUnload.end())
                    kept.push_back(p);
            }
            writeDirProfiles(cfg, dir, kept);
            
            if (profilesToUnload.size() == 1)
                ok(cfg, "Removed '" + profilesToUnload[0] + "' persistence for this directory.");
            else
                ok(cfg, "Removed persistence for " + to_string(profilesToUnload.size()) + " profiles from this directory.");
        }

        auditLog(cfg, "unload", profilesToUnload);
        return 0;
    }

    else if (cmd == "export")
    {
        string prof = "default", fmt = "dotenv", out;
        for (size_t i = 1; i < args.size();)
        {
            if (args[i] == "--profile" && i + 1 < args.size())
            {
                prof = args[i + 1];
                i += 2;
            }
            else if (args[i] == "--format" && i + 1 < args.size())
            {
                fmt = args[i + 1];
                i += 2;
            }
            else if (args[i] == "--output" && i + 1 < args.size())
            {
                out = args[i + 1];
                i += 2;
            }
            else
                error(cfg, "Unknown or incomplete flag: " + args[i]);
        }
        if (out.empty())
            error(cfg, "Usage: ak export --profile <p> --format env|dotenv|json|yaml --output <file>");
        auto keys = readProfile(cfg, prof);
        KeyStore ks = loadVault(cfg);
        ofstream ofs(out);
        if (!ofs)
            error(cfg, "Failed to open output file");
        if (fmt == "env" || fmt == "dotenv")
        {
            for (auto &k : keys)
            {
                auto it = ks.kv.find(k);
                if (it == ks.kv.end())
                    continue;
                string v = it->second, esc;
                for (char c : v)
                {
                    if (c == '"' || c == '\\')
                        esc.push_back('\\');
                    if (c == '\n')
                    {
                        esc += "\\n";
                        continue;
                    }
                    esc.push_back(c);
                }
                ofs << k << "=\"" << esc << "\"\n";
            }
        }
        else if (fmt == "json")
        {
            ofs << "{";
            bool first = true;
            for (auto &k : keys)
            {
                auto it = ks.kv.find(k);
                if (it == ks.kv.end())
                    continue;
                if (!first)
                    ofs << ",";
                first = false;
                string v = it->second, esc;
                for (char c : v)
                {
                    if (c == '\\')
                        esc += "\\\\";
                    else if (c == '"')
                        esc += "\\\"";
                    else if (c == '\n')
                        esc += "\\n";
                    else
                        esc.push_back(c);
                }
                ofs << "\"" << k << "\":\"" << esc << "\"";
            }
            ofs << "}";
        }
        else if (fmt == "yaml")
        {
            for (auto &k : keys)
            {
                auto it = ks.kv.find(k);
                if (it == ks.kv.end())
                    continue;
                string v = it->second, esc;
                for (char c : v)
                {
                    if (c == '"' || c == '\\')
                        esc.push_back('\\');
                    if (c == '\n')
                    {
                        esc += "\\n";
                        continue;
                    }
                    esc.push_back(c);
                }
                ofs << k << ": \"" << esc << "\"\n";
            }
        }
        else
            error(cfg, "Unknown format: " + fmt);
        ok(cfg, "Exported profile '" + prof + "' -> " + fmt + ": " + out + " (" + to_string(keys.size()) + " keys)");
        auditLog(cfg, "export", keys);
        return 0;
    }
    else if (cmd == "import")
    {
        string prof = "default", fmt = "dotenv", file;
        bool keysOnly = false;
        for (size_t i = 1; i < args.size();)
        {
            if (args[i] == "--profile" && i + 1 < args.size())
            {
                prof = args[i + 1];
                i += 2;
            }
            else if (args[i] == "--format" && i + 1 < args.size())
            {
                fmt = args[i + 1];
                i += 2;
            }
            else if (args[i] == "--file" && i + 1 < args.size())
            {
                file = args[i + 1];
                i += 2;
            }
            else if (args[i] == "--keys")
            {
                keysOnly = true;
                i++;
            }
            else
                error(cfg, "Unknown or incomplete flag: " + args[i]);
        }
        if (file.empty())
            error(cfg, "Usage: ak import --profile <p> --format env|dotenv|json|yaml --file <file> [--keys]");
        ifstream in(file);
        if (!in)
            error(cfg, "File not found: " + file);
        
        KeyStore ks = loadVault(cfg);
        unordered_set<string> importedKeys;
        unordered_set<string> knownKeys;
        
        if (keysOnly)
            knownKeys = getKnownServiceKeys();
        
        if (fmt == "env" || fmt == "dotenv")
        {
            auto rows = parse_env_file(in);
            for (auto &kv : rows)
            {
                // Filter for known service keys if --keys flag is set
                if (keysOnly && knownKeys.find(kv.first) == knownKeys.end())
                    continue;
                
                ks.kv[kv.first] = kv.second;
                importedKeys.insert(kv.first);
            }
        }
        else if (fmt == "json")
        {
            ostringstream ss;
            ss << in.rdbuf();
            auto rows = parse_json_min(ss.str());
            for (auto &kv : rows)
            {
                // Filter for known service keys if --keys flag is set
                if (keysOnly && knownKeys.find(kv.first) == knownKeys.end())
                    continue;
                
                ks.kv[kv.first] = kv.second;
                importedKeys.insert(kv.first);
            }
        }
        else if (fmt == "yaml")
        {
            string line;
            while (getline(in, line))
            {
                line = trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                auto c = line.find(':');
                if (c == string::npos)
                    continue;
                string k = trim(line.substr(0, c));
                string v = trim(line.substr(c + 1));
                if (!v.empty() && v.front() == '"' && v.back() == '"')
                    v = v.substr(1, v.size() - 2);
                
                // Filter for known service keys if --keys flag is set
                if (keysOnly && knownKeys.find(k) == knownKeys.end())
                    continue;
                
                ks.kv[k] = v;
                importedKeys.insert(k);
            }
        }
        else
            error(cfg, "Unknown format: " + fmt);
        
        saveVault(cfg, ks);
        
        // merge into profile
        auto existing = readProfile(cfg, prof);
        unordered_set<string> set(existing.begin(), existing.end());
        for (auto &k : importedKeys)
            set.insert(k);
        vector<string> merged(set.begin(), set.end());
        writeProfile(cfg, prof, merged);
        
        string msg = "Imported " + to_string(importedKeys.size()) + " keys into profile '" + prof + "'";
        if (keysOnly)
            msg += " (filtered for known service provider keys)";
        ok(cfg, msg);
        
        vector<string> imp(importedKeys.begin(), importedKeys.end());
        auditLog(cfg, "import", imp);
        return 0;
    }
    else if (cmd == "migrate")
    {
        if (args.size() == 3 && args[1] == "exports")
        {
            string file = args[2];
            ifstream in(file);
            if (!in)
                error(cfg, "File not found: " + file);
            auto rows = parse_env_file(in);
            KeyStore ks = loadVault(cfg);
            vector<string> names;
            for (auto &kv : rows)
            {
                if (kv.second.empty() || kv.second == "omitted")
                    continue;
                ks.kv[kv.first] = kv.second;
                names.push_back(kv.first);
            }
            saveVault(cfg, ks);
            // add to default profile
            auto existing = readProfile(cfg, "default");
            unordered_set<string> set(existing.begin(), existing.end());
            for (auto &k : names)
                set.insert(k);
            vector<string> merged(set.begin(), set.end());
            writeProfile(cfg, "default", merged);
            ok(cfg, "Migration complete. Imported: " + to_string(names.size()));
            auditLog(cfg, "migrate_exports", names);
            return 0;
        }
        error(cfg, "Usage: ak migrate exports <file>");
    }
    else if (cmd == "profiles")
    {
        for (auto &n : listProfiles(cfg))
            cout << n << "\n";
        return 0;
    }
    else if (cmd == "run")
    {
        if (args.size() < 4 || args[1] != "--profile" || args[3] != "--")
            error(cfg, "Usage: ak run --profile <name> -- <cmd...>");
        string prof = args[2];
        vector<string> cmdv(args.begin() + 4, args.end());
        if (cmdv.empty())
            error(cfg, "Provide a command");
        // set env vars for this process before exec
        KeyStore ks = loadVault(cfg);
        for (auto &k : readProfile(cfg, prof))
        {
            auto it = ks.kv.find(k);
            if (it != ks.kv.end())
                setenv(k.c_str(), it->second.c_str(), 1);
        }
        // exec
        vector<char *> cargv;
        for (auto &s : cmdv)
            cargv.push_back(const_cast<char *>(s.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        perror("execvp");
        return 1;
    }
    else if (cmd == "guard")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak guard enable|disable");
        if (args[1] == "enable")
            guard_enable(cfg);
        else if (args[1] == "disable")
            guard_disable();
        else
            error(cfg, "Usage: ak guard enable|disable");
        return 0;
    }
    else if (cmd == "test")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak test <service>|--all [--json] [--fail-fast]");
        bool all = (args[1] == "--all" || args[1] == "all");
        bool ff = (find(args.begin(), args.end(), "--fail-fast") != args.end());
        if (all)
        {
            vector<string> svcs;
            for (auto &p : SERVICE_KEYS)
                svcs.push_back(p.first);
            sort(svcs.begin(), svcs.end());
            bool ok_all = true;
            if (cfg.json)
                cout << "[";
            bool first = true;
            for (auto &s : svcs)
            {
                bool ok1 = test_one(cfg, s);
                ok_all = ok_all && ok1;
                if (cfg.json)
                {
                    if (!first)
                        cout << ",";
                    first = false;
                    cout << "{\"service\":\"" << s << "\",\"ok\":" << (ok1 ? "true" : "false") << "}";
                }
                else
                    cerr << s << " " << (ok1 ? "OK" : "failed") << "\n";
                if (ff && !ok1)
                    break;
            }
            if (cfg.json)
                cout << "]\n";
            return ok_all ? 0 : 2;
        }
        else
        {
            string s = args[1];
            bool ok1 = test_one(cfg, s);
            if (cfg.json)
                cout << "{\"service\":\"" << s << "\",\"ok\":" << (ok1 ? "true" : "false") << "}\n";
            else
                cerr << s << " " << (ok1 ? "OK" : "failed") << "\n";
            return ok1 ? 0 : 2;
        }
    }
    else if (cmd == "doctor")
    {
        cout << "backend: " << ((cfg.gpgAvailable && !cfg.forcePlain) ? "gpg" : "plain") << "\n";
        if (cfg.gpgAvailable)
            cout << "found: gpg\n";
        for (auto t : {"pbcopy", "wl-copy", "xclip"})
            if (commandExists(t))
                cout << "clipboard: " << t << "\n";
        cout << "profiles: " << listProfiles(cfg).size() << "\n";
        cout << "vault: " << cfg.vaultPath << "\n";
        return 0;
    }
    else if (cmd == "audit")
    {
        size_t tail = 50;
        if (args.size() >= 2)
        {
            try
            {
                tail = stoul(args[1]);
            }
            catch (...)
            {
            }
        }
        ifstream in(cfg.auditLogPath);
        if (!in)
            error(cfg, "No audit log");
        vector<string> lines;
        string line;
        while (getline(in, line))
            lines.push_back(line);
        size_t start = lines.size() > tail ? lines.size() - tail : 0;
        for (size_t i = start; i < lines.size(); ++i)
            cout << lines[i] << "\n";
        return 0;
    }
    else if (cmd == "install-shell")
    {
        // Determine the target user (handles sudo) and install into their config directory
        TargetUser t = resolveTargetUser();

        // Override cfg.configDir to the target user's config dir for shell integration artifacts
        string originalCfgDir = cfg.configDir;
        cfg.configDir = t.home + "/.config/ak";
        ensureSecureDir(cfg.configDir);

        writeShellInitFile(cfg);
        ensureSourcedInRc(cfg);

        // Determine which config file to mention in the message
        string configFileMessage;
        if (t.shellName == "zsh") {
            configFileMessage = "source ~/.zshrc";
        } else if (t.shellName == "bash") {
            configFileMessage = "source ~/.bashrc";
        } else if (t.shellName == "fish") {
            configFileMessage = "restart your terminal (fish config updated)";
        } else {
            configFileMessage = "source ~/.profile";
        }

        cerr << "✅ Installed shell auto-load. Restart your terminal or run: " << configFileMessage << "\n";
        return 0;
    }
    else if (cmd == "uninstall")
    {
        // Determine the target user (handles sudo) for proper cleanup
        TargetUser t = resolveTargetUser();
        
        // Override cfg.configDir to the target user's config dir
        cfg.configDir = t.home + "/.config/ak";
        
        string dir = cfg.configDir;
        string bin = argv[0];
        string initPath = (fs::path(cfg.configDir) / "shell-init.sh").string();
        string sourceLine = "source \"" + initPath + "\"";
        
        // Remove shell completion files and configuration lines
        string configFile;
        string completionFile;
        
        if (t.shellName == "zsh") {
            configFile = t.home + "/.zshrc";
            completionFile = t.home + "/.config/zsh/completions/_ak";
            
            // Remove zsh completion file
            if (fs::exists(completionFile)) {
                fs::remove(completionFile);
                cerr << "✅ Removed zsh completion file: " << completionFile << "\n";
            }
            
        } else if (t.shellName == "bash") {
            configFile = t.home + "/.bashrc";
            completionFile = t.home + "/.config/ak/ak-completion.bash";
            // Completion file will be removed with config dir
            
        } else if (t.shellName == "fish") {
            configFile = t.home + "/.config/fish/config.fish";
            completionFile = t.home + "/.config/fish/completions/ak.fish";
            
            // Remove fish completion file
            if (fs::exists(completionFile)) {
                fs::remove(completionFile);
                cerr << "✅ Removed fish completion file: " << completionFile << "\n";
            }
            
        } else {
            // Fallback to .profile
            configFile = t.home + "/.profile";
            completionFile = t.home + "/.config/ak/ak-completion.bash";
            // Completion file will be removed with config dir
        }
        
        // Remove lines from shell config file
        if (fs::exists(configFile)) {
            vector<string> lines;
            ifstream in(configFile);
            string line;
            bool modified = false;
            
            while (getline(in, line)) {
                // Skip the source line and related ak lines
                if (line.find(sourceLine) != string::npos ||
                    line.find("# Added by ak installer") != string::npos ||
                    line.find("fpath=(~/.config/zsh/completions $fpath)") != string::npos ||
                    line.find("autoload -U compinit && compinit") != string::npos ||
                    line.find("source \"" + t.home + "/.config/ak/ak-completion.bash\"") != string::npos) {
                    modified = true;
                    continue;
                }
                lines.push_back(line);
            }
            in.close();
            
            if (modified) {
                ofstream out(configFile, ios::trunc);
                for (const auto& l : lines) {
                    out << l << "\n";
                }
                out.close();
                cerr << "✅ Removed ak integration from " << configFile << "\n";
            }
        }
        
        // Remove config directory
        string rmCmd = "rm -rf '" + dir + "'";
        int rc = system(rmCmd.c_str());
        if (rc != 0)
            cerr << "⚠️  Failed to remove " << dir << " (rc=" << rc << ")\n";
        else
            cerr << "✅ Removed " << dir << "\n";
            
        // Try to remove binary if it's an absolute path
        if (!bin.empty() && bin[0] == '/') {
            if (unlink(bin.c_str()) == 0) {
                cerr << "✅ Removed binary: " << bin << "\n";
            } else {
                cerr << "⚠️  Could not remove binary: " << bin << " (may need sudo)\n";
            }
        } else {
            cerr << "⚠️  If installed system-wide, also remove the binary manually (e.g., sudo rm /usr/local/bin/ak)\n";
        }
        
        cerr << "✅ Uninstall complete. Restart your terminal for changes to take effect.\n";
        return 0;
    }
    else if (cmd == "completion")
    {
        if (args.size() < 2)
            error(cfg, "Usage: ak completion <shell>\nSupported shells: bash, zsh, fish");
        string shell = args[1];
        if (shell == "bash")
            generateBashCompletion();
        else if (shell == "zsh")
            generateZshCompletion();
        else if (shell == "fish")
            generateFishCompletion();
        else
            error(cfg, "Unsupported shell: " + shell + "\nSupported shells: bash, zsh, fish");
        return 0;
    }
    else
    {
        error(cfg, "Unknown command '" + cmd + "' (try: ak help)");
    }

    return 0;
}
