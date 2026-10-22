// Otomatik guncelleme (auto-update) - OsirisInjector.exe
//
// Mimari:
//   1. Osiris.version (Osiris.dll yaninda, derleme post-build kopyalar) -> yerel surum
//   2. updateUrl -> uzak version dosyasi: 1. satir = surum, 2. satir = DLL URL'si (goreli olabilir)
//   3. Uzak > yerel ise %APPDATA%\OsirisCS2\update_available.txt marker'i yazilir (DLL menuye uyarar)
//   4. Kullanici onay verirse DLL indirilir, .bak yedegi alinir, yerine konur, marker silinir
//
// Kaynak domain istemez: GitHub raw / herhangi bir statik host / yerel veya AG yolu hepsi calisir.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif
#ifndef WINHTTP_OPTION_REDIRECT_POLICY
#define WINHTTP_OPTION_REDIRECT_POLICY 62
#endif
#ifndef WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
#define WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS 2
#endif

namespace updater
{

constexpr auto kIniFileName = L"OsirisInjector.ini";
constexpr auto kLocalVersionFileName = L"Osiris.version";
constexpr auto kMarkerFileName = L"update_available.txt";
constexpr std::uint64_t kMaxTextBytes = 64ull * 1024;
constexpr std::uint64_t kMaxDllBytes = 64ull * 1024 * 1024;
constexpr DWORD kTimeoutMs = 5000;

constexpr auto kIniTemplate =
    "; OsirisInjector otomatik guncelleme ayarlari\r\n"
    "[update]\r\n"
    "; Alan adi gerekmez: GitHub raw / herhangi bir HTTP(S) URL ya da yerel/AG yolu\r\n"
    "enabled=1\r\n"
    "url=\r\n";

inline std::wstring toWide(const char* text)
{
    if (!text)
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (length <= 0)
        return {};
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), length);
    wide.resize(static_cast<std::size_t>(length - 1));
    return wide;
}

inline std::wstring trimWide(std::wstring text)
{
    const auto isSpace = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
    std::size_t begin{};
    while (begin < text.length() && isSpace(text[begin]))
        ++begin;
    std::size_t end = text.length();
    while (end > begin && isSpace(text[end - 1]))
        --end;
    return text.substr(begin, end - begin);
}

inline std::string trim(std::string text)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    std::size_t begin{};
    while (begin < text.length() && isSpace(text[begin]))
        ++begin;
    std::size_t end = text.length();
    while (end > begin && isSpace(text[end - 1]))
        --end;
    return text.substr(begin, end - begin);
}

inline bool startsWithNoCase(const std::wstring& text, const wchar_t* prefix)
{
    const std::wstring pattern{prefix};
    if (text.length() < pattern.length())
        return false;
    return _wcsnicmp(text.c_str(), pattern.c_str(), pattern.length()) == 0;
}

inline std::wstring moduleDirectory()
{
    wchar_t modulePath[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
        return {};
    std::wstring directory{modulePath};
    const auto slash = directory.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        directory.resize(slash + 1);
    else
        directory.clear();
    return directory;
}

inline bool readWholeFile(const std::wstring& path, std::vector<std::uint8_t>& out, std::uint64_t capBytes)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || static_cast<std::uint64_t>(size.QuadPart) > capBytes) {
        CloseHandle(file);
        return false;
    }

    out.resize(static_cast<std::size_t>(size.QuadPart));
    std::size_t readTotal{};
    while (readTotal < out.size()) {
        DWORD read{};
        if (!ReadFile(file, out.data() + readTotal, static_cast<DWORD>(out.size() - readTotal), &read, nullptr) || read == 0)
            break;
        readTotal += read;
    }

    CloseHandle(file);
    return readTotal == out.size();
}

inline bool writeWholeFile(const std::wstring& path, const void* data, std::size_t size)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::size_t writtenTotal{};
    while (writtenTotal < size) {
        DWORD written{};
        if (!WriteFile(file, bytes + writtenTotal, static_cast<DWORD>(size - writtenTotal), &written, nullptr) || written == 0) {
            CloseHandle(file);
            DeleteFileW(path.c_str());
            return false;
        }
        writtenTotal += written;
    }

    CloseHandle(file);
    return true;
}

inline std::wstring directoryOf(const std::wstring& filePath)
{
    const auto slash = filePath.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return {};
    return filePath.substr(0, slash + 1);
}

inline bool isHttpUrl(const std::wstring& url)
{
    return startsWithNoCase(url, L"https://") || startsWithNoCase(url, L"http://");
}

inline std::wstring stripFileScheme(std::wstring path)
{
    if (startsWithNoCase(path, L"file://")) {
        path.erase(0, 7);
        // file:///C:/... -> /C:/... -> C:/...
        if (path.length() > 2 && path[0] == L'/' && ((path[1] >= L'a' && path[1] <= L'z') || (path[1] >= L'A' && path[1] <= L'Z')) && path[2] == L':')
            path.erase(0, 1);
    }
    return path;
}

inline bool httpGet(const std::wstring& url, std::vector<std::uint8_t>& out, std::uint64_t capBytes)
{
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.length()), 0, &components))
        return false;
    if (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS)
        return false;

    const std::wstring host{components.lpszHostName, components.dwHostNameLength};
    std::wstring path{components.lpszUrlPath, components.dwUrlPathLength};
    if (components.dwExtraInfoLength)
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    if (path.empty())
        path = L"/";

    bool success = false;
    if (const HINTERNET session = WinHttpOpen(L"OsirisInjector/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)) {
        WinHttpSetTimeouts(session, static_cast<int>(kTimeoutMs), static_cast<int>(kTimeoutMs), static_cast<int>(kTimeoutMs), static_cast<int>(kTimeoutMs));
        if (const HINTERNET connection = WinHttpConnect(session, host.c_str(), components.nPort, 0)) {
            const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
            if (const HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags)) {
                DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
                WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

                if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                    && WinHttpReceiveResponse(request, nullptr)) {
                    DWORD statusCode{};
                    DWORD statusSize = sizeof(statusCode);
                    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)
                        && statusCode >= 200 && statusCode < 300) {
                        success = true;
                        for (;;) {
                            DWORD available{};
                            if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
                                break;
                            if (out.size() + static_cast<std::uint64_t>(available) > capBytes) {
                                success = false;
                                break;
                            }
                            const auto oldSize = out.size();
                            out.resize(oldSize + available);
                            DWORD read{};
                            if (!WinHttpReadData(request, out.data() + oldSize, available, &read)) {
                                success = false;
                                break;
                            }
                            out.resize(oldSize + read);
                            if (read == 0)
                                break;
                        }
                    }
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connection);
        }
        WinHttpCloseHandle(session);
    }
    return success;
}

inline bool fetchBytes(const std::wstring& url, std::vector<std::uint8_t>& out, std::uint64_t capBytes)
{
    if (isHttpUrl(url))
        return httpGet(url, out, capBytes);
    return readWholeFile(stripFileScheme(url), out, capBytes);
}

inline bool isValidPeX64(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS64))
        return false;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return false;

    const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
    if (ntOffset + sizeof(IMAGE_NT_HEADERS64) > bytes.size())
        return false;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data() + ntOffset);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        return false;
    if (!nt->OptionalHeader.SizeOfImage || nt->OptionalHeader.SizeOfImage > (256ull << 20))
        return false;
    return true;
}

struct RemoteInfo {
    std::wstring version;
    std::wstring dllUrl;
};

inline bool isVersionCharacter(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '+';
}

inline bool parseRemoteInfo(const std::vector<std::uint8_t>& bytes, RemoteInfo& out)
{
    std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    std::vector<std::string> lines;
    std::size_t lineStart{};
    for (std::size_t i = 0; i <= text.length(); ++i) {
        if (i == text.length() || text[i] == '\n') {
            lines.push_back(trim(text.substr(lineStart, i - lineStart)));
            lineStart = i + 1;
        }
    }

    std::size_t lineIndex{};
    while (lineIndex < lines.size() && lines[lineIndex].empty())
        ++lineIndex;
    if (lineIndex >= lines.size())
        return false;

    const std::string version = lines[lineIndex++];
    if (version.empty() || version.length() > 63)
        return false;
    for (const char c : version) {
        if (!isVersionCharacter(c))
            return false;
    }
    out.version = toWide(version.c_str());

    while (lineIndex < lines.size() && lines[lineIndex].empty())
        ++lineIndex;
    if (lineIndex < lines.size() && !lines[lineIndex].empty())
        out.dllUrl = toWide(lines[lineIndex].c_str());

    return !out.version.empty();
}

inline int parseVersionPart(const std::wstring& version, std::size_t& index)
{
    while (index < version.length() && (version[index] == L'v' || version[index] == L'V'))
        ++index;

    int value{};
    bool anyDigit{};
    while (index < version.length() && value < 1000000) {
        const wchar_t c = version[index];
        if (c >= L'0' && c <= L'9') {
            value = value * 10 + (c - L'0');
            anyDigit = true;
            ++index;
        } else {
            break;
        }
    }
    if (index < version.length() && version[index] == L'.')
        ++index;
    return anyDigit ? value : 0;
}

inline int compareVersions(const std::wstring& left, const std::wstring& right)
{
    std::size_t leftIndex{};
    std::size_t rightIndex{};
    for (int part = 0; part < 4; ++part) {
        const int leftValue = parseVersionPart(left, leftIndex);
        const int rightValue = parseVersionPart(right, rightIndex);
        if (leftValue != rightValue)
            return leftValue < rightValue ? -1 : 1;
    }
    return 0;
}

inline std::wstring resolveUrl(const std::wstring& baseUrl, const std::wstring& reference)
{
    if (reference.empty())
        return {};

    if (reference.find(L"://") != std::wstring::npos)
        return reference;
    if (reference.length() >= 2 && reference[0] == L'\\' && reference[1] == L'\\')
        return reference;
    if (reference.length() >= 3 && reference[1] == L':' && (reference[2] == L'\\' || reference[2] == L'/'))
        return reference;

    const auto isHttpBase = isHttpUrl(baseUrl);
    const auto slash = isHttpBase ? baseUrl.find_last_of(L'/') : baseUrl.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
        return reference;
    return baseUrl.substr(0, slash + 1) + reference;
}

inline std::wstring readLocalVersion(const std::wstring& dllPath)
{
    const auto versionFilePath = directoryOf(dllPath) + kLocalVersionFileName;
    std::vector<std::uint8_t> bytes;
    if (!readWholeFile(versionFilePath, bytes, kMaxTextBytes) || bytes.empty())
        return {};

    std::string line;
    for (const auto byte : bytes) {
        const char c = static_cast<char>(byte);
        if (c == '\n' || c == '\r')
            break;
        line.push_back(c);
    }
    line = trim(line);
    if (line.empty() || line.length() > 63)
        return {};
    for (const char c : line) {
        if (!isVersionCharacter(c))
            return {};
    }
    return toWide(line.c_str());
}

struct Options {
    bool enabled = true;
    std::wstring url;
};

inline bool isDisabledValue(const std::string& value)
{
    const auto lowered = trim(value);
    return lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off";
}

inline void applyIniLine(const std::string& line, Options& options)
{
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#' || trimmed[0] == '[')
        return;

    const auto equals = trimmed.find('=');
    if (equals == std::string::npos)
        return;

    std::string key = trimmed.substr(0, equals);
    key = trim(key);
    for (auto& character : key)
        character = static_cast<char>((character >= 'A' && character <= 'Z') ? character - 'A' + 'a' : character);

    const std::string value = trimmed.substr(equals + 1);
    if (key == "url")
        options.url = trimWide(toWide(value.c_str()));
    else if (key == "enabled")
        options.enabled = !isDisabledValue(value);
}

inline Options loadOptions()
{
    Options options;
    const auto iniPath = moduleDirectory() + kIniFileName;

    std::vector<std::uint8_t> bytes;
    if (!readWholeFile(iniPath, bytes, kMaxTextBytes)) {
        writeWholeFile(iniPath, kIniTemplate, std::strlen(kIniTemplate));
        return options;
    }

    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::size_t lineStart{};
    for (std::size_t i = 0; i <= text.length(); ++i) {
        if (i == text.length() || text[i] == '\n') {
            applyIniLine(text.substr(lineStart, i - lineStart), options);
            lineStart = i + 1;
        }
    }
    return options;
}

inline std::wstring appDataOsirisDirectory()
{
    wchar_t appDataPath[MAX_PATH]{};
    if (!ExpandEnvironmentStringsW(L"%APPDATA%", appDataPath, MAX_PATH))
        return {};
    std::wstring directory{appDataPath};
    if (directory.empty())
        return {};
    if (directory.back() != L'\\')
        directory.push_back(L'\\');
    directory += L"OsirisCS2";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory;
}

inline void writeMarker(const std::wstring& directory, const RemoteInfo& remote)
{
    if (directory.empty())
        return;
    std::string content;
    for (const wchar_t character : remote.version)
        content.push_back(static_cast<char>(character));
    content += "\n";
    if (!remote.dllUrl.empty()) {
        for (const wchar_t character : remote.dllUrl)
            content.push_back(static_cast<char>(character));
        content += "\n";
    }
    writeWholeFile(directory + L"\\" + kMarkerFileName, content.data(), content.length());
}

inline void deleteMarker(const std::wstring& directory)
{
    if (directory.empty())
        return;
    DeleteFileW((directory + L"\\" + kMarkerFileName).c_str());
}

inline bool askDownload()
{
    std::printf("Indirilsin mi? [E/h]: ");
    std::fflush(stdout);
    char answer[16]{};
    if (!std::fgets(answer, sizeof(answer), stdin))
        return true;
    const auto trimmed = trim(answer);
    if (trimmed.empty())
        return true;
    const char first = trimmed[0];
    return !(first == 'h' || first == 'H' || first == 'n' || first == 'N');
}

inline bool replaceDll(const std::wstring& dllPath, const std::vector<std::uint8_t>& bytes, std::wstring& error)
{
    const auto tempPath = dllPath + L".update";
    if (!writeWholeFile(tempPath, bytes.data(), bytes.size())) {
        error = L"gecici dosya yazilamadi: " + tempPath;
        return false;
    }

    const auto backupPath = dllPath + L".bak";
    DeleteFileW(backupPath.c_str());

    if (!MoveFileExW(dllPath.c_str(), backupPath.c_str(), 0)) {
        error = L"mevcut DLL yedeklenemedi (kilitli olabilir)";
        DeleteFileW(tempPath.c_str());
        return false;
    }

    if (!MoveFileExW(tempPath.c_str(), dllPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(backupPath.c_str(), dllPath.c_str(), 0);
        error = L"yeni DLL yerine konulamadi";
        DeleteFileW(tempPath.c_str());
        return false;
    }
    return true;
}

struct Params {
    std::wstring dllPath;
    std::wstring urlOverride;
    bool noUpdate{};
    bool checkOnly{};
};

inline void run(const Params& params)
{
    if (params.noUpdate)
        return;

    Options options = loadOptions();
    if (!params.urlOverride.empty()) {
        options.url = params.urlOverride;
        options.enabled = true;
    }

    if (!options.enabled) {
        std::printf("guncelleme kontrolu devre disi (ini)\n");
        return;
    }
    if (options.url.empty()) {
        std::printf("guncelleme URL'si ayarli degil, kontrol atlandi.\n");
        std::printf("  ipucu: OsirisInjector.ini icine [update] url=... ekleyin\n");
        std::printf("  (GitHub raw yeterli, alan adi gerekmez; yerel/AG yolu da olur)\n");
        return;
    }

    const auto localVersion = readLocalVersion(params.dllPath);
    if (localVersion.empty()) {
        std::printf("Osiris.version bulunamadi (Osiris.dll yaninda), kontrol atlandi.\n");
        return;
    }

    std::vector<std::uint8_t> remoteBytes;
    if (!fetchBytes(options.url, remoteBytes, kMaxTextBytes)) {
        std::printf("guncelleme sunucusuna ulasilamadi, yerel surumle devam.\n");
        return;
    }

    RemoteInfo remote;
    if (!parseRemoteInfo(remoteBytes, remote)) {
        std::printf("guncelleme dosyasi okunemedi, yerel surumle devam.\n");
        return;
    }

    const auto markerDirectory = appDataOsirisDirectory();
    const int comparison = compareVersions(localVersion, remote.version);
    if (comparison >= 0) {
        deleteMarker(markerDirectory);
        if (comparison == 0)
            std::printf("guncelleme yok (surum %ls guncel).\n", localVersion.c_str());
        else
            std::printf("yerel surum (%ls) yayindakinden (%ls) yeni.\n", localVersion.c_str(), remote.version.c_str());
        return;
    }

    writeMarker(markerDirectory, remote);
    std::printf("GUNCELLEME BULUNDU: %ls -> %ls\n", localVersion.c_str(), remote.version.c_str());

    if (remote.dllUrl.empty()) {
        std::printf("version dosyasinda DLL URL'si yok (2. satir), indirme yapilmadi.\n");
        return;
    }

    if (!askDownload()) {
        std::printf("indirme iptal edildi, menu guncelleme uyarisi gosterilecek.\n");
        return;
    }

    const auto dllUrl = resolveUrl(options.url, remote.dllUrl);
    std::vector<std::uint8_t> dllBytes;
    if (!fetchBytes(dllUrl, dllBytes, kMaxDllBytes)) {
        std::printf("DLL indirilemedi, yerel DLL ile devam.\n");
        return;
    }

    if (!isValidPeX64(dllBytes)) {
        std::printf("indirilen dosya gecersiz PE (x64), yerel DLL ile devam.\n");
        return;
    }

    std::wstring error;
    if (!replaceDll(params.dllPath, dllBytes, error)) {
        std::printf("DLL degistirilemedi: %ls\n", error.c_str());
        return;
    }

    deleteMarker(markerDirectory);
    std::printf("guncelleme tamamlandi: %ls -> %ls\n", localVersion.c_str(), remote.version.c_str());
}

}
