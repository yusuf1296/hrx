#pragma once

#include <Platform/Macros/IsPlatform.h>

#if IS_WIN64()

#include <Platform/Windows/WindowsDynamicLibrary.h>
#include <winhttp.h>

namespace Telemetry {

inline constexpr auto kVersion = "1.5.0";
inline constexpr auto kSupabaseHost = L"xpswzknwjxqonvvxdrao.supabase.co";
inline constexpr auto kCheckinPath = L"/rest/v1/checkins";
inline constexpr auto kAnonKey = L"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inhwc3d6a253anhxb252dnhkcmFvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTI2Nzc5MTAsImV4cCI6MjEwODI1MzkxMH0.gokceJvo6_keOm_NT9Zm22ME2Av_K6zKtwqLLu8KTCk";
inline constexpr auto kUserAgent = L"HRX-Telemetry";
inline constexpr unsigned long kHeartbeatMs = 5u * 60u * 1000u;
inline constexpr long kMaxFails = 5;

namespace detail {

constexpr DWORD kSendCompleteBit = 0x00400000u;  // WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
constexpr DWORD kLegacyCompleteBit = 0x00800000u; // eski SDK'lardaki WINHTTP_CALLBACK_STATUS_REQUEST_COMPLETE

struct Api {
    HMODULE kernel32{};
    HMODULE winhttpModule{};
    HMODULE advapiModule{};
    HINTERNET session{};
    HINTERNET connectHandle{};

    decltype(&WinHttpOpen) open{};
    decltype(&WinHttpConnect) connect{};
    decltype(&WinHttpOpenRequest) openRequest{};
    decltype(&WinHttpAddRequestHeaders) addRequestHeaders{};
    decltype(&WinHttpSendRequest) sendRequest{};
    decltype(&WinHttpReceiveResponse) receiveResponse{};
    decltype(&WinHttpReadData) readData{};
    decltype(&WinHttpCloseHandle) closeHandle{};
    decltype(&WinHttpSetStatusCallback) setStatusCallback{};
    decltype(&WinHttpSetTimeouts) setTimeouts{};

    decltype(&LoadLibraryA) loadLibraryA{};
    decltype(&GetEnvironmentVariableA) getEnvironmentVariableA{};
    decltype(&GetFileAttributesA) getFileAttributesA{};
    decltype(&GetComputerNameA) getComputerNameA{};
    decltype(&GetCurrentProcessId) getCurrentProcessId{};
    decltype(&GetTickCount64) getTickCount64{};
    decltype(&GetSystemTime) getSystemTime{};
    decltype(&MultiByteToWideChar) multiByteToWideChar{};
    decltype(&WideCharToMultiByte) wideCharToMultiByte{};
    decltype(&GetLastError) getLastError{};

    decltype(&RegOpenKeyExW) regOpenKeyExW{};
    decltype(&RegQueryValueExW) regQueryValueExW{};
    decltype(&RegCloseKey) regCloseKey{};
};

inline Api& api() noexcept
{
    static Api a{};
    return a;
}

inline volatile long g_state = 0;
inline volatile long g_fails = 0;
inline volatile long g_phase = 0;
inline HINTERNET g_request = nullptr;
inline unsigned long long g_lastSend = 0;
inline bool g_metaReady = false;
inline bool g_headersReady = false;

inline char g_session[48]{};
inline char g_pc[80]{};
inline char g_user[160]{};
inline char g_steam[80]{};
inline char g_body[1024]{};
inline unsigned g_bodyLen = 0;
inline wchar_t g_headers[768]{};
inline char g_readBuffer[64]{};

inline unsigned len(const char* s) noexcept
{
    unsigned n = 0;
    if (s)
        while (s[n])
            ++n;
    return n;
}

inline bool strEq(const char* a, const char* b) noexcept
{
    if (!a || !b)
        return false;
    while (*a && *b) {
        if (*a != *b)
            return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

inline bool endsWithIgnoreCase(const char* s, const char* suffix) noexcept
{
    const auto sl = len(s);
    const auto xl = len(suffix);
    if (sl < xl)
        return false;
    for (unsigned i = 0; i < xl; ++i) {
        char a = s[sl - xl + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z')
            a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = static_cast<char>(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}

inline GenericPointer proc(HMODULE module, const char* name, int depth = 0) noexcept;

inline GenericPointer resolveForwarder(const char* forwarder, int depth) noexcept
{
    if (!forwarder || depth > 3)
        return {};

    const char* split = forwarder;
    for (const char* p = forwarder; *p; ++p)
        if (*p == '.')
            split = p;

    if (split == forwarder)
        return {};

    char moduleName[128]{};
    unsigned m = 0;
    for (const char* p = forwarder; p != split && m + 1 < sizeof(moduleName); ++p)
        moduleName[m++] = *p;
    moduleName[m] = '\0';

    const char* functionName = split + 1;
    if (!functionName[0] || functionName[0] == '#')
        return {};
    if (!endsWithIgnoreCase(moduleName, ".dll")) {
        static constexpr auto dll = ".dll";
        for (unsigned i = 0; dll[i] && m + 1 < sizeof(moduleName); ++i)
            moduleName[m++] = dll[i];
        moduleName[m] = '\0';
    }

    HMODULE target = WindowsDynamicLibrary{ moduleName }.getHandle();
    const auto& a = api();
    if (!target && a.loadLibraryA)
        target = a.loadLibraryA(moduleName);
    if (!target)
        return {};
    return proc(target, functionName, depth + 1);
}

inline GenericPointer proc(HMODULE module, const char* name, int depth) noexcept
{
    if (!module || !name)
        return {};

    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return {};
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return {};
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress || !dir.Size)
        return {};

    const auto* exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
    const auto* names = reinterpret_cast<const DWORD*>(base + exportDir->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(base + exportDir->AddressOfNameOrdinals);
    const auto* functions = reinterpret_cast<const DWORD*>(base + exportDir->AddressOfFunctions);

    for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
        const auto* exportName = reinterpret_cast<const char*>(base + names[i]);
        if (!strEq(exportName, name))
            continue;
        const DWORD rva = functions[ordinals[i]];
        if (rva >= dir.VirtualAddress && rva - dir.VirtualAddress < dir.Size)
            return resolveForwarder(reinterpret_cast<const char*>(base + rva), depth);
        return GenericPointer{ base + rva };
    }
    return {};
}

inline void wcopy(wchar_t* dst, unsigned cap, const wchar_t* src) noexcept
{
    unsigned i = 0;
    if (src)
        while (src[i] && i + 1 < cap) {
            dst[i] = src[i];
            ++i;
        }
    if (cap)
        dst[i < cap ? i : cap - 1] = L'\0';
}

struct Buf {
    char* data;
    unsigned cap;
    unsigned len = 0;

    void ch(char c) noexcept
    {
        if (len + 1 < cap)
            data[len++] = c;
    }

    void str(const char* s) noexcept
    {
        if (!s)
            return;
        while (*s)
            ch(*s++);
    }

    void esc(const char* s) noexcept
    {
        if (!s)
            return;
        for (; *s; ++s) {
            const auto c = static_cast<unsigned char>(*s);
            if (c == '"' || c == '\\') {
                ch('\\');
                ch(static_cast<char>(c));
            } else if (c >= 0x20) {
                ch(static_cast<char>(c));
            }
        }
    }

    void u64(unsigned long long v) noexcept
    {
        if (v == 0) {
            ch('0');
            return;
        }
        char tmp[24];
        unsigned n = 0;
        while (v) {
            tmp[n++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
        while (n)
            ch(tmp[--n]);
    }

    void hex64(unsigned long long v) noexcept
    {
        static constexpr auto digits = "0123456789abcdef";
        char tmp[16];
        for (int i = 15; i >= 0; --i) {
            tmp[i] = digits[v & 0xF];
            v >>= 4;
        }
        for (int i = 0; i < 16; ++i)
            ch(tmp[i]);
    }

    void pad2(unsigned v) noexcept
    {
        ch(static_cast<char>('0' + (v / 10) % 10));
        ch(static_cast<char>('0' + v % 10));
    }

    void pad4(unsigned v) noexcept
    {
        ch(static_cast<char>('0' + (v / 1000) % 10));
        ch(static_cast<char>('0' + (v / 100) % 10));
        ch(static_cast<char>('0' + (v / 10) % 10));
        ch(static_cast<char>('0' + v % 10));
    }

    void utcTimestamp() noexcept
    {
        SYSTEMTIME st{};
        api().getSystemTime(&st);
        pad4(st.wYear);
        ch('-');
        pad2(st.wMonth);
        ch('-');
        pad2(st.wDay);
        ch('T');
        pad2(st.wHour);
        ch(':');
        pad2(st.wMinute);
        ch(':');
        pad2(st.wSecond);
        ch('Z');
    }

    void terminate() noexcept
    {
        data[len < cap ? len : cap - 1] = '\0';
    }
};

inline bool acpToUtf8(const char* src, char* dst, unsigned cap) noexcept
{
    if (!src || !src[0] || cap == 0)
        return false;
    wchar_t wide[256]{};
    const auto& a = api();
    const int wideLen = a.multiByteToWideChar(0 /*CP_ACP*/, 0, src, -1, wide, 256);
    if (wideLen <= 0)
        return false;
    const int utf8Len = a.wideCharToMultiByte(65001 /*CP_UTF8*/, 0, wide, -1, dst, static_cast<int>(cap), nullptr, nullptr);
    return utf8Len > 0;
}

inline bool wideToUtf8(const wchar_t* src, char* dst, unsigned cap) noexcept
{
    if (!src || !src[0] || cap == 0)
        return false;
    const auto& a = api();
    const int utf8Len = a.wideCharToMultiByte(65001 /*CP_UTF8*/, 0, src, -1, dst, static_cast<int>(cap), nullptr, nullptr);
    return utf8Len > 0;
}

inline bool keyLooksValid() noexcept
{
    const auto c0 = kAnonKey[0];
    const auto c1 = kAnonKey[1];
    const auto c2 = kAnonKey[2];
    return (c0 == L'e' || c0 == L'E') && (c1 == L'y' || c1 == L'Y') && (c2 == L'j' || c2 == L'J');
}

inline bool killSwitchOn() noexcept
{
    const auto& a = api();
    if (!a.getEnvironmentVariableA || !a.getFileAttributesA)
        return false;
    char appData[340]{};
    if (a.getEnvironmentVariableA("APPDATA", appData, sizeof(appData)) == 0)
        return false;
    unsigned i = 0;
    while (appData[i])
        ++i;
    static constexpr auto suffix = "\\HRX\\telemetry.off";
    unsigned s = 0;
    while (suffix[s]) {
        if (i + 1 < sizeof(appData))
            appData[i++] = suffix[s];
        ++s;
    }
    appData[i] = '\0';
    const auto attributes = a.getFileAttributesA(appData);
    return attributes != 0xFFFFFFFF /*INVALID_FILE_ATTRIBUTES*/ && (attributes & 0x10 /*FILE_ATTRIBUTE_DIRECTORY*/) == 0;
}

inline bool resolve() noexcept
{
    auto& a = api();
    if (a.open)
        return true;

    const WindowsDynamicLibrary kernel32{ "KERNEL32.dll" };
    if (!kernel32)
        return false;
    a.kernel32 = kernel32.getHandle();

    a.loadLibraryA = proc(a.kernel32, "LoadLibraryA").as<decltype(&LoadLibraryA)>();
    a.getEnvironmentVariableA = proc(a.kernel32, "GetEnvironmentVariableA").as<decltype(&GetEnvironmentVariableA)>();
    a.getFileAttributesA = proc(a.kernel32, "GetFileAttributesA").as<decltype(&GetFileAttributesA)>();
    a.getComputerNameA = proc(a.kernel32, "GetComputerNameA").as<decltype(&GetComputerNameA)>();
    a.getCurrentProcessId = proc(a.kernel32, "GetCurrentProcessId").as<decltype(&GetCurrentProcessId)>();
    a.getTickCount64 = proc(a.kernel32, "GetTickCount64").as<decltype(&GetTickCount64)>();
    a.getSystemTime = proc(a.kernel32, "GetSystemTime").as<decltype(&GetSystemTime)>();
    a.multiByteToWideChar = proc(a.kernel32, "MultiByteToWideChar").as<decltype(&MultiByteToWideChar)>();
    a.wideCharToMultiByte = proc(a.kernel32, "WideCharToMultiByte").as<decltype(&WideCharToMultiByte)>();
    a.getLastError = proc(a.kernel32, "GetLastError").as<decltype(&GetLastError)>();
    if (!a.loadLibraryA || !a.getEnvironmentVariableA || !a.getFileAttributesA || !a.getComputerNameA || !a.getCurrentProcessId
        || !a.getTickCount64 || !a.getSystemTime || !a.multiByteToWideChar || !a.wideCharToMultiByte)
        return false;

    a.loadLibraryA("winhttp.dll");
    const WindowsDynamicLibrary winhttp{ "winhttp.dll" };
    if (!winhttp)
        return false;
    a.winhttpModule = winhttp.getHandle();
    a.open = proc(a.winhttpModule, "WinHttpOpen").as<decltype(&WinHttpOpen)>();
    a.connect = proc(a.winhttpModule, "WinHttpConnect").as<decltype(&WinHttpConnect)>();
    a.openRequest = proc(a.winhttpModule, "WinHttpOpenRequest").as<decltype(&WinHttpOpenRequest)>();
    a.addRequestHeaders = proc(a.winhttpModule, "WinHttpAddRequestHeaders").as<decltype(&WinHttpAddRequestHeaders)>();
    a.sendRequest = proc(a.winhttpModule, "WinHttpSendRequest").as<decltype(&WinHttpSendRequest)>();
    a.receiveResponse = proc(a.winhttpModule, "WinHttpReceiveResponse").as<decltype(&WinHttpReceiveResponse)>();
    a.readData = proc(a.winhttpModule, "WinHttpReadData").as<decltype(&WinHttpReadData)>();
    a.closeHandle = proc(a.winhttpModule, "WinHttpCloseHandle").as<decltype(&WinHttpCloseHandle)>();
    a.setStatusCallback = proc(a.winhttpModule, "WinHttpSetStatusCallback").as<decltype(&WinHttpSetStatusCallback)>();
    a.setTimeouts = proc(a.winhttpModule, "WinHttpSetTimeouts").as<decltype(&WinHttpSetTimeouts)>();
    if (!a.open || !a.connect || !a.openRequest || !a.addRequestHeaders || !a.sendRequest || !a.receiveResponse || !a.readData
        || !a.closeHandle || !a.setStatusCallback || !a.setTimeouts)
        return false;

    a.loadLibraryA("ADVAPI32.dll");
    const WindowsDynamicLibrary advapi32{ "ADVAPI32.dll" };
    if (advapi32) {
        a.advapiModule = advapi32.getHandle();
        a.regOpenKeyExW = proc(a.advapiModule, "RegOpenKeyExW").as<decltype(&RegOpenKeyExW)>();
        a.regQueryValueExW = proc(a.advapiModule, "RegQueryValueExW").as<decltype(&RegQueryValueExW)>();
        a.regCloseKey = proc(a.advapiModule, "RegCloseKey").as<decltype(&RegCloseKey)>();
    }
    return true;
}

inline void collectMeta() noexcept
{
    if (g_metaReady)
        return;
    g_metaReady = true;
    const auto& a = api();

    char rawPc[80]{};
    DWORD rawPcSize = static_cast<DWORD>(sizeof(rawPc));
    if (a.getComputerNameA(rawPc, &rawPcSize))
        acpToUtf8(rawPc, g_pc, sizeof(g_pc));

    char rawUser[160]{};
    if (a.getEnvironmentVariableA("USERNAME", rawUser, sizeof(rawUser)) > 0)
        acpToUtf8(rawUser, g_user, sizeof(g_user));

    if (a.regOpenKeyExW && a.regQueryValueExW && a.regCloseKey) {
        HKEY key{};
        if (a.regOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, 0x0001 /*KEY_QUERY_VALUE*/, &key) == 0) {
            wchar_t steamUser[64]{};
            DWORD type = 0;
            DWORD size = sizeof(steamUser);
            if (a.regQueryValueExW(key, L"AutoLoginUser", nullptr, &type, reinterpret_cast<BYTE*>(steamUser), &size) == 0
                && (type == 1 /*REG_SZ*/ || type == 2 /*REG_EXPAND_SZ*/))
                wideToUtf8(steamUser, g_steam, sizeof(g_steam));
            a.regCloseKey(key);
        }
    }

    Buf b{ g_session, sizeof(g_session) };
    b.str("s");
    b.hex64(a.getTickCount64());
    b.str("p");
    b.u64(a.getCurrentProcessId());
    b.terminate();
}

inline void buildHeaders() noexcept
{
    if (g_headersReady)
        return;
    g_headersReady = true;

    unsigned i = 0;
    const auto appendW = [&](const wchar_t* s) {
        unsigned n = 0;
        while (s[n] && i + 1 < 768) {
            g_headers[i++] = s[n];
            ++n;
        }
    };
    appendW(L"Content-Type: application/json\r\napikey: ");
    appendW(kAnonKey);
    appendW(L"\r\nAuthorization: Bearer ");
    appendW(kAnonKey);
    appendW(L"\r\nPrefer: return=minimal\r\n");
    g_headers[i] = L'\0';
}

inline void buildBody() noexcept
{
    const auto& a = api();
    if (!a.getSystemTime)
        return;
    Buf b{ g_body, sizeof(g_body) };
    b.str("{\"session\":\"");
    b.esc(g_session);
    b.str("\",\"pc\":\"");
    b.esc(g_pc);
    b.str("\",\"winuser\":\"");
    b.esc(g_user);
    b.str("\",\"steam\":\"");
    b.esc(g_steam);
    b.str("\",\"version\":\"");
    b.esc(kVersion);
    b.str("\",\"in_game\":true,\"ts\":\"");
    b.utcTimestamp();
    b.str("\"}");
    b.terminate();
    g_bodyLen = b.len;
}

inline void failCurrentRequest() noexcept
{
    const auto& a = api();
    if (g_request && a.closeHandle) {
        auto request = g_request;
        g_request = nullptr;
        a.closeHandle(request);
    }
    g_phase = 0;
    g_state = 0;
    const long fails = ++g_fails;
    if (fails >= kMaxFails)
        g_state = -1;
}

inline void completeCurrentRequest() noexcept
{
    const auto& a = api();
    if (g_request && a.closeHandle) {
        auto request = g_request;
        g_request = nullptr;
        a.closeHandle(request);
    }
    g_phase = 0;
    g_fails = 0;
    g_state = 0;
}

void CALLBACK statusCallback(HINTERNET, DWORD_PTR, DWORD status, LPVOID, DWORD) noexcept
{
    const auto& a = api();
    if (g_state == -1 || !g_request || !a.closeHandle)
        return;

    if (status & WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        failCurrentRequest();
        return;
    }
    if (status & (kSendCompleteBit | kLegacyCompleteBit)) {
        if (g_phase == 0) {
            g_phase = 1;
            const BOOL ok = a.receiveResponse(g_request, nullptr);
            if (!ok && a.getLastError && a.getLastError() != 997 /*ERROR_IO_PENDING*/)
                failCurrentRequest();
            return;
        }
        failCurrentRequest();
        return;
    }
    if ((status & WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) && g_phase == 1) {
        g_phase = 2;
        const BOOL ok = a.readData(g_request, g_readBuffer, static_cast<DWORD>(sizeof(g_readBuffer)), 0);
        if (!ok && a.getLastError && a.getLastError() != 997 /*ERROR_IO_PENDING*/)
            failCurrentRequest();
        return;
    }
    if ((status & WINHTTP_CALLBACK_STATUS_READ_COMPLETE) && g_phase == 2) {
        completeCurrentRequest();
        return;
    }
}

inline bool beginSend() noexcept
{
    if (g_state == -1)
        return false;
    return InterlockedCompareExchange(&g_state, 1, 0) == 0;
}

inline void fireLocked() noexcept
{
    auto& a = api();
    if (!a.open || !a.connectHandle) {
        g_state = -1;
        return;
    }
    HINTERNET request = a.openRequest(a.connectHandle, L"POST", kCheckinPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        failCurrentRequest();
        g_lastSend = a.getTickCount64();
        return;
    }
    g_request = request;
    g_phase = 0;
    if (!a.addRequestHeaders(request, g_headers, static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD)) {
        failCurrentRequest();
        g_lastSend = a.getTickCount64();
        return;
    }
    const auto bodyLength = static_cast<DWORD>(g_bodyLen);
    const BOOL sent = a.sendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0L, g_body, bodyLength, bodyLength, 0);
    if (!sent) {
        const bool pending = a.getLastError && a.getLastError() == 997 /*ERROR_IO_PENDING*/;
        if (!pending) {
            failCurrentRequest();
            g_lastSend = a.getTickCount64();
            return;
        }
    }
    g_lastSend = a.getTickCount64();
}

inline bool openSession() noexcept
{
    auto& a = api();
    if (a.session)
        return true;

#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    constexpr auto accessType = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
#else
    constexpr auto accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
#endif
    a.session = a.open(kUserAgent, accessType, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!a.session)
        return false;
    a.setTimeouts(a.session, 5000, 5000, 5000, 5000);
    a.setStatusCallback(a.session, &statusCallback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);
    a.connectHandle = a.connect(a.session, kSupabaseHost, 443, 0);
    return a.connectHandle != nullptr;
}

}

inline void initOnce() noexcept
{
    static bool done = false;
    if (done)
        return;
    done = true;

    if (!detail::keyLooksValid() || !detail::resolve() || !detail::openSession()) {
        detail::g_state = -1;
        return;
    }
    detail::collectMeta();
    detail::buildHeaders();
    if (detail::killSwitchOn()) {
        detail::g_state = -1;
        return;
    }
    if (!detail::beginSend())
        return;
    detail::buildBody();
    detail::fireLocked();
}

inline void heartbeat() noexcept
{
    if (detail::g_state != 0)
        return;
    const auto& a = detail::api();
    if (!a.getTickCount64)
        return;
    if (a.getTickCount64() - detail::g_lastSend < kHeartbeatMs)
        return;
    if (detail::killSwitchOn()) {
        detail::g_state = -1;
        return;
    }
    if (!detail::beginSend())
        return;
    detail::buildBody();
    detail::fireLocked();
}

inline void shutdown() noexcept
{
    detail::g_state = -1;
    const auto& a = detail::api();
    if (a.setStatusCallback && a.session)
        a.setStatusCallback(a.session, nullptr, 0, 0);
}

}

#else

namespace Telemetry {
inline void initOnce() noexcept {}
inline void heartbeat() noexcept {}
inline void shutdown() noexcept {}
}

#endif
