// Osiris manual-map injector
// Kullanim:
//   OsirisInjector.exe [dll-yolu] [--pid N | --process ad.exe] [--wait] [--keep-headers]
// Varsayilan: cs2.exe bulunur, Osiris.dll manual map edilir.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#pragma comment(lib, "advapi32.lib")

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "update.h"

namespace
{

constexpr auto kDefaultProcess = L"cs2.exe";
constexpr auto kDefaultDll = L"Osiris.dll";

void pauseIfDoubleClicked()
{
    DWORD processes[4]{};
    if (GetConsoleProcessList(processes, 4) <= 1) {
        std::printf("\nKapatmak icin Enter'a bas...");
        (void)std::getchar();
    }
}

bool enableDebugPrivilege()
{
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool enabled = false;
    if (LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &privileges.Privileges[0].Luid)) {
        SetLastError(ERROR_SUCCESS);
        enabled = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), nullptr, nullptr) != FALSE
            && GetLastError() == ERROR_SUCCESS;
    }

    CloseHandle(token);
    return enabled;
}

struct RemoteModule {
    std::wstring name;
    std::uintptr_t base{};
    std::size_t size{};
};

std::vector<RemoteModule> enumerateModules(DWORD pid)
{
    std::vector<RemoteModule> modules;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return modules;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            modules.push_back(RemoteModule{entry.szModule, reinterpret_cast<std::uintptr_t>(entry.modBaseAddr), entry.modBaseSize});
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return modules;
}

std::uintptr_t findModuleBase(const std::vector<RemoteModule>& modules, const char* name)
{
    if (!name || !*name)
        return 0;

    wchar_t wideName[260]{};
    if (MultiByteToWideChar(CP_ACP, 0, name, -1, wideName, 260) <= 0)
        return 0;

    for (const auto& module : modules) {
        if (_wcsicmp(module.name.c_str(), wideName) == 0)
            return module.base;
    }
    return 0;
}

DWORD findProcess(const wchar_t* processName, bool wait)
{
    const auto deadline = GetTickCount64() + (wait ? 60000ull : 0ull);
    for (;;) {
        DWORD foundPid{};
        std::size_t bestClientSize{};

        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (Process32FirstW(snapshot, &entry)) {
                do {
                    if (_wcsicmp(entry.szExeFile, processName) != 0)
                        continue;

                    std::size_t clientSize{};
                    const HANDLE moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, entry.th32ProcessID);
                    if (moduleSnapshot != INVALID_HANDLE_VALUE) {
                        MODULEENTRY32W module{};
                        module.dwSize = sizeof(module);
                        if (Module32FirstW(moduleSnapshot, &module)) {
                            do {
                                if (_wcsicmp(module.szModule, L"client.dll") == 0) {
                                    clientSize = module.modBaseSize;
                                    break;
                                }
                            } while (Module32NextW(moduleSnapshot, &module));
                        }
                        CloseHandle(moduleSnapshot);
                    }

                    if (!foundPid || clientSize > bestClientSize) {
                        foundPid = entry.th32ProcessID;
                        bestClientSize = clientSize;
                    }
                } while (Process32NextW(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }

        if (foundPid)
            return foundPid;

        if (GetTickCount64() >= deadline)
            return 0;

        std::printf("bekleniyor: %ls (60 sn)\n", processName);
        Sleep(1000);
    }
}

bool readFileBytes(const wchar_t* path, std::vector<std::uint8_t>& out)
{
    const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > (64ll << 20)) {
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

std::vector<std::uint8_t> makeCallStub(std::uintptr_t function, std::uint64_t arg1, std::uint32_t arg2, std::uint64_t arg3)
{
    std::vector<std::uint8_t> stub;
    stub.reserve(40);

    const auto emitImm64 = [&stub](std::uint64_t value) {
        for (int i = 0; i < 8; ++i)
            stub.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    };

    stub.push_back(0x48);
    stub.push_back(0x83);
    stub.push_back(0xEC);
    stub.push_back(0x28);

    stub.push_back(0x48);
    stub.push_back(0xB9);
    emitImm64(arg1);

    stub.push_back(0xBA);
    for (int i = 0; i < 4; ++i)
        stub.push_back(static_cast<std::uint8_t>(arg2 >> (i * 8)));

    stub.push_back(0x49);
    stub.push_back(0xB8);
    emitImm64(arg3);

    stub.push_back(0x48);
    stub.push_back(0xB8);
    emitImm64(function);

    stub.push_back(0xFF);
    stub.push_back(0xD0);

    stub.push_back(0x48);
    stub.push_back(0x83);
    stub.push_back(0xC4);
    stub.push_back(0x28);

    stub.push_back(0xC3);
    return stub;
}

bool remoteCall(HANDLE process, std::uintptr_t function, std::uint64_t arg1, std::uint32_t arg2, std::uint64_t arg3, DWORD* result = nullptr)
{
    const auto stub = makeCallStub(function, arg1, arg2, arg3);
    void* remoteStub = VirtualAllocEx(process, nullptr, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteStub)
        return false;

    bool ok = false;
    if (WriteProcessMemory(process, remoteStub, stub.data(), stub.size(), nullptr)) {
        const HANDLE thread = CreateRemoteThread(process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteStub), nullptr, 0, nullptr);
        if (thread) {
            if (WaitForSingleObject(thread, 20000) == WAIT_OBJECT_0) {
                DWORD exitCode{};
                if (GetExitCodeThread(thread, &exitCode)) {
                    if (result)
                        *result = exitCode;
                    ok = true;
                }
            }
            CloseHandle(thread);
        }
    }

    VirtualFreeEx(process, remoteStub, 0, MEM_RELEASE);
    return ok;
}

std::uintptr_t loadRemoteLibrary(HANDLE process, DWORD pid, const char* dllName, const std::vector<RemoteModule>& modules)
{
    void* remoteName = VirtualAllocEx(process, nullptr, std::strlen(dllName) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteName)
        return 0;

    std::uintptr_t result{};
    if (WriteProcessMemory(process, remoteName, dllName, std::strlen(dllName) + 1, nullptr)) {
        const std::uintptr_t localKernel32 = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"kernel32.dll"));
        const std::uintptr_t remoteKernel32 = findModuleBase(modules, "kernel32.dll");
        if (localKernel32 && remoteKernel32) {
            const auto loadLibraryA = reinterpret_cast<std::uintptr_t>(GetProcAddress(reinterpret_cast<HMODULE>(localKernel32), "LoadLibraryA"));
            if (loadLibraryA) {
                const auto remoteLoadLibrary = remoteKernel32 + (loadLibraryA - localKernel32);
                remoteCall(process, remoteLoadLibrary, reinterpret_cast<std::uint64_t>(remoteName), 0, 0);
                result = findModuleBase(enumerateModules(pid), dllName);
            }
        }
    }

    VirtualFreeEx(process, remoteName, 0, MEM_RELEASE);

    if (!result) {
        const HMODULE localAlias = LoadLibraryA(dllName);
        if (localAlias) {
            wchar_t localPath[MAX_PATH]{};
            if (GetModuleFileNameW(localAlias, localPath, MAX_PATH)) {
                const wchar_t* baseName = wcsrchr(localPath, L'\\');
                baseName = baseName ? baseName + 1 : localPath;
                char narrowName[MAX_PATH]{};
                if (WideCharToMultiByte(CP_ACP, 0, baseName, -1, narrowName, MAX_PATH, nullptr, nullptr) > 0)
                    result = findModuleBase(enumerateModules(pid), narrowName);
            }
        }
    }

    return result;
}

DWORD protectionFor(std::uint32_t characteristics)
{
    const bool executable = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    const bool readable = (characteristics & IMAGE_SCN_MEM_READ) != 0;
    const bool writable = (characteristics & IMAGE_SCN_MEM_WRITE) != 0;

    if (executable)
        return writable ? PAGE_EXECUTE_READWRITE : (readable ? PAGE_EXECUTE_READ : PAGE_EXECUTE);
    if (writable)
        return PAGE_READWRITE;
    return PAGE_READONLY;
}

bool manualMap(HANDLE process, DWORD pid, const std::vector<std::uint8_t>& file, bool eraseHeaders)
{
    if (file.size() < sizeof(IMAGE_DOS_HEADER))
        return false;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > file.size()) {
        std::printf("gecersiz PE (DOS)\n");
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        std::printf("gecersiz PE (NT)\n");
        return false;
    }
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        std::printf("DLL x64 degil\n");
        return false;
    }

    const std::size_t imageSize = nt->OptionalHeader.SizeOfImage;
    const std::size_t headersSize = nt->OptionalHeader.SizeOfHeaders;
    const std::uintptr_t preferredBase = nt->OptionalHeader.ImageBase;
    if (imageSize < headersSize || imageSize > (256ull << 20)) {
        std::printf("gecersiz SizeOfImage\n");
        return false;
    }

    std::vector<std::uint8_t> image(imageSize, 0);
    std::memcpy(image.data(), file.data(), (std::min)(headersSize, file.size()));

    std::vector<IMAGE_SECTION_HEADER> sections;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        sections.push_back(*section);
        if (!section->PointerToRawData || !section->SizeOfRawData)
            continue;

        const std::size_t virtualSize = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData;
        const std::size_t copySize = (std::min)(static_cast<std::size_t>(section->SizeOfRawData), virtualSize);
        if (static_cast<std::size_t>(section->VirtualAddress) + copySize > imageSize || static_cast<std::size_t>(section->PointerToRawData) + copySize > file.size()) {
            std::printf("bozuk section: %.8s\n", section->Name);
            return false;
        }
        std::memcpy(image.data() + section->VirtualAddress, file.data() + section->PointerToRawData, copySize);
    }

    void* remoteBaseRaw = VirtualAllocEx(process, nullptr, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBaseRaw) {
        std::printf("VirtualAllocEx basarisiz (%lu)\n", GetLastError());
        return false;
    }
    const auto remoteBase = reinterpret_cast<std::uintptr_t>(remoteBaseRaw);
    std::printf("uzak taban: 0x%llX (tercih: 0x%llX)\n", static_cast<unsigned long long>(remoteBase), static_cast<unsigned long long>(preferredBase));

    const auto delta = static_cast<std::int64_t>(remoteBase) - static_cast<std::int64_t>(preferredBase);
    if (delta != 0) {
        const auto& relocDirectory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (!relocDirectory.Size) {
            std::printf("relocation yok, tercih edilen taban zorunlu\n");
            VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
            return false;
        }

        std::size_t offset{};
        while (offset + sizeof(IMAGE_BASE_RELOCATION) <= relocDirectory.Size) {
            const auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(image.data() + relocDirectory.VirtualAddress + offset);
            if (!block->SizeOfBlock || block->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION))
                break;

            const std::size_t entryCount = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            const auto* entries = reinterpret_cast<const WORD*>(block + 1);
            for (std::size_t i = 0; i < entryCount; ++i) {
                const WORD type = entries[i] >> 12;
                const WORD position = entries[i] & 0x0FFF;
                if (type == IMAGE_REL_BASED_ABSOLUTE)
                    continue;
                if (type != IMAGE_REL_BASED_DIR64) {
                    std::printf("desteklenmeyen reloc tipi: %u\n", type);
                    VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
                    return false;
                }

                const std::size_t patchOffset = block->VirtualAddress + position;
                if (patchOffset + sizeof(std::uint64_t) > imageSize) {
                    std::printf("reloc sinir asimi\n");
                    VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
                    return false;
                }

                auto* patch = reinterpret_cast<std::uint64_t*>(image.data() + patchOffset);
                *patch = static_cast<std::uint64_t>(static_cast<std::int64_t>(*patch) + delta);
            }

            offset += block->SizeOfBlock;
        }
        std::printf("relocation uygulandi\n");
    }

    auto modules = enumerateModules(pid);
    const auto& importDirectory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirectory.Size) {
        auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image.data() + importDirectory.VirtualAddress);
        for (; descriptor->Name; ++descriptor) {
            const char* dllName = reinterpret_cast<const char*>(image.data() + descriptor->Name);
            std::uintptr_t remoteDll = findModuleBase(modules, dllName);
            if (!remoteDll) {
                std::printf("uzak modul yukleniyor: %s\n", dllName);
                remoteDll = loadRemoteLibrary(process, pid, dllName, modules);
                modules = enumerateModules(pid);
            }
            if (!remoteDll) {
                std::printf("import modulu bulunamadi: %s\n", dllName);
                VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
                return false;
            }

            HMODULE localDll = LoadLibraryA(dllName);
            if (!localDll)
                localDll = LoadLibraryExA(dllName, nullptr, DONT_RESOLVE_DLL_REFERENCES);
            if (!localDll) {
                std::printf("yerel import modulu yuklenemedi: %s\n", dllName);
                VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
                return false;
            }

            const auto originalThunkRva = descriptor->OriginalFirstThunk ? descriptor->OriginalFirstThunk : descriptor->FirstThunk;
            auto* originalThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(image.data() + originalThunkRva);
            auto* firstThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(image.data() + descriptor->FirstThunk);

            for (; originalThunk->u1.AddressOfData; ++originalThunk, ++firstThunk) {
                std::uintptr_t localFunction{};
                const char* functionName{};

                if (originalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                    localFunction = reinterpret_cast<std::uintptr_t>(GetProcAddress(localDll, MAKEINTRESOURCEA(originalThunk->u1.Ordinal & 0xFFFF)));
                } else {
                    const auto* importByName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(image.data() + originalThunk->u1.AddressOfData);
                    functionName = reinterpret_cast<const char*>(importByName->Name);
                    localFunction = reinterpret_cast<std::uintptr_t>(GetProcAddress(localDll, functionName));
                }

                if (!localFunction) {
                    std::printf("import cozulemedi: %s!%s\n", dllName, functionName ? functionName : "#ordinal");
                    FreeLibrary(localDll);
                    VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
                    return false;
                }

                firstThunk->u1.Function = remoteDll + (localFunction - reinterpret_cast<std::uintptr_t>(localDll));
            }

            FreeLibrary(localDll);
        }
        std::printf("importlar cozuldu\n");
    } else {
        std::printf("import yok (statik bagimsiz DLL)\n");
    }

    if (!WriteProcessMemory(process, remoteBaseRaw, image.data(), imageSize, nullptr)) {
        std::printf("WriteProcessMemory basarisiz (%lu)\n", GetLastError());
        VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
        return false;
    }

    for (const auto& header : sections) {
        const std::size_t size = header.Misc.VirtualSize ? header.Misc.VirtualSize : header.SizeOfRawData;
        if (!size)
            continue;

        DWORD oldProtection{};
        VirtualProtectEx(process, reinterpret_cast<void*>(remoteBase + header.VirtualAddress), size, protectionFor(header.Characteristics), &oldProtection);
    }

    if (eraseHeaders) {
        std::vector<std::uint8_t> zeros(headersSize, 0);
        DWORD oldProtection{};
        if (VirtualProtectEx(process, remoteBaseRaw, headersSize, PAGE_READWRITE, &oldProtection)) {
            WriteProcessMemory(process, remoteBaseRaw, zeros.data(), zeros.size(), nullptr);
            VirtualProtectEx(process, remoteBaseRaw, headersSize, PAGE_NOACCESS, &oldProtection);
            std::printf("PE header silindi\n");
        }
    } else {
        DWORD oldProtection{};
        VirtualProtectEx(process, remoteBaseRaw, headersSize, PAGE_READONLY, &oldProtection);
    }

    const auto& tlsDirectory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDirectory.Size) {
        const auto* tls = reinterpret_cast<const IMAGE_TLS_DIRECTORY64*>(image.data() + tlsDirectory.VirtualAddress);
        if (tls->AddressOfCallBacks) {
            const auto callbacksOffset = static_cast<std::size_t>(tls->AddressOfCallBacks - preferredBase);
            if (callbacksOffset + sizeof(std::uintptr_t) <= imageSize) {
                const auto* callbacks = reinterpret_cast<const std::uintptr_t*>(image.data() + callbacksOffset);
                for (; *callbacks; ++callbacks) {
                    if (!remoteCall(process, *callbacks, remoteBase, DLL_PROCESS_ATTACH, 0)) {
                        std::printf("TLS callback basarisiz: 0x%llX\n", static_cast<unsigned long long>(*callbacks));
                        VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
                        return false;
                    }
                }
                std::printf("TLS callback'leri calisti\n");
            }
        }
    }

    const std::uintptr_t entryPoint = remoteBase + nt->OptionalHeader.AddressOfEntryPoint;
    if (!nt->OptionalHeader.AddressOfEntryPoint) {
        std::printf("entry point yok\n");
        VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
        return false;
    }

    FlushInstructionCache(process, remoteBaseRaw, imageSize);

    DWORD dllMainResult{};
    if (!remoteCall(process, entryPoint, remoteBase, DLL_PROCESS_ATTACH, 0, &dllMainResult)) {
        std::printf("DllMain cagrisi basarisiz\n");
        VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
        return false;
    }

    if (!dllMainResult) {
        std::printf("DllMain FALSE dondurdu\n");
        VirtualFreeEx(process, remoteBaseRaw, 0, MEM_RELEASE);
        return false;
    }

    std::printf("manual map OK -> taban 0x%llX\n", static_cast<unsigned long long>(remoteBase));
    return true;
}

bool resolveDllPath(const wchar_t* argument, std::wstring& out)
{
    if (argument && *argument) {
        out = argument;
        return true;
    }

    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH)) {
        std::wstring directory{modulePath};
        const auto slash = directory.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            directory.resize(slash + 1);
        else
            directory.clear();

        const std::wstring candidates[]{
            directory + kDefaultDll,
            directory + L"..\\osiris\\x64\\Release\\Osiris.dll",
            directory + L"..\\..\\osiris\\x64\\Release\\Osiris.dll",
        };

        for (const auto& candidate : candidates) {
            if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
                out = candidate;
                return true;
            }
        }
    }

    out = kDefaultDll;
    return true;
}

}

int main(int argc, char** argv)
{
    const wchar_t* dllArgument = nullptr;
    const wchar_t* processName = kDefaultProcess;
    DWORD explicitPid{};
    bool waitForProcess = false;
    bool keepHeaders = false;
    bool noUpdate = false;
    bool checkUpdate = false;
    std::wstring updateUrlOverride;

    std::vector<std::wstring> wideArguments;
    wideArguments.reserve(argc);
    for (int i = 1; i < argc; ++i) {
        const int length = MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(length) > 0 ? length - 1 : 0, L'\0');
        if (length > 0)
            MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, wide.data(), length);
        wideArguments.push_back(std::move(wide));
    }

    for (std::size_t i = 0; i < wideArguments.size(); ++i) {
        const auto& argument = wideArguments[i];

        if (argument == L"--help" || argument == L"-h" || argument == L"/?") {
            std::printf("Kullanim: OsirisInjector.exe [dll] [--pid N] [--process ad.exe] [--wait] [--keep-headers]\n");
            std::printf("  dll             manual map edilecek DLL (varsayilan: Osiris.dll)\n");
            std::printf("  --pid N         hedef PID\n");
            std::printf("  --process ad    hedef surec adi (varsayilan: cs2.exe)\n");
            std::printf("  --wait          surec 60 sn boyunca beklenir\n");
            std::printf("  --keep-headers  PE header'larini silme\n");
            std::printf("  --update-url u  guncelleme URL'sini gecersiz kil (ini yerine)\n");
            std::printf("  --check-update  sadece guncelleme kontrolu yap, enjekte etme\n");
            std::printf("  --no-update     guncelleme kontrolunu atla\n");
            return 0;
        }

        if (argument == L"--wait") {
            waitForProcess = true;
        } else if (argument == L"--keep-headers") {
            keepHeaders = true;
        } else if (argument == L"--no-update") {
            noUpdate = true;
        } else if (argument == L"--check-update") {
            checkUpdate = true;
        } else if (argument == L"--update-url" && i + 1 < wideArguments.size()) {
            updateUrlOverride = wideArguments[i + 1];
            ++i;
        } else if (argument == L"--pid" && i + 1 < wideArguments.size()) {
            explicitPid = static_cast<DWORD>(_wtoi(wideArguments[++i].c_str()));
        } else if (argument == L"--process" && i + 1 < wideArguments.size()) {
            processName = wideArguments[i + 1].c_str();
            ++i;
        } else if (!dllArgument && !argument.empty() && argument[0] != L'-') {
            dllArgument = wideArguments[i].c_str();
        }
    }

    std::wstring dllPath;
    resolveDllPath(dllArgument, dllPath);

    std::printf("Osiris manual-map injector\n");
    std::printf("DLL: %ls\n", dllPath.c_str());

    updater::Params updateParams;
    updateParams.dllPath = dllPath;
    updateParams.urlOverride = updateUrlOverride;
    updateParams.noUpdate = noUpdate;
    updateParams.checkOnly = checkUpdate;
    updater::run(updateParams);

    if (checkUpdate) {
        pauseIfDoubleClicked();
        return 0;
    }

    std::vector<std::uint8_t> dllBytes;
    if (!readFileBytes(dllPath.c_str(), dllBytes)) {
        std::printf("DLL okunamadi: %ls\n", dllPath.c_str());
        pauseIfDoubleClicked();
        return 1;
    }

    if (!enableDebugPrivilege())
        std::printf("uyari: SeDebugPrivilege alinamadi\n");

    const DWORD pid = explicitPid ? explicitPid : findProcess(processName, waitForProcess);
    if (!pid) {
        std::printf("hedef surec bulunamadi: %ls\n", explicitPid ? L"(pid)" : processName);
        pauseIfDoubleClicked();
        return 1;
    }

    if (pid == GetCurrentProcessId()) {
        std::printf("kendine enjekte edilemez\n");
        return 1;
    }

    std::printf("hedef PID: %lu\n", static_cast<unsigned long>(pid));

    const HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!process) {
        std::printf("OpenProcess basarisiz (%lu). Yonetici olarak calistir.\n", GetLastError());
        pauseIfDoubleClicked();
        return 1;
    }

    const bool ok = manualMap(process, pid, dllBytes, !keepHeaders);
    CloseHandle(process);

    std::printf(ok ? "islem tamam.\n" : "islem basarisiz.\n");
    pauseIfDoubleClicked();
    return ok ? 0 : 1;
}
