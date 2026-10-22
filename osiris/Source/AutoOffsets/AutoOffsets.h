#pragma once

#include <Platform/Macros/IsPlatform.h>

#if IS_WIN64()

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include <CS2/Constants/DllNames.h>
#include <MemoryPatterns/AllMemoryPatternSearchResults.h>
#include <MemoryPatterns/PatternTypes/BaseModelEntityPatternTypes.h>
#include <MemoryPatterns/PatternTypes/C4PatternTypes.h>
#include <MemoryPatterns/PatternTypes/ClientPatternTypes.h>
#include <MemoryPatterns/PatternTypes/EntityPatternTypes.h>
#include <MemoryPatterns/PatternTypes/EntitySystemPatternTypes.h>
#include <MemoryPatterns/PatternTypes/GameRulesPatternTypes.h>
#include <MemoryPatterns/PatternTypes/PlantedC4PatternTypes.h>
#include <MemoryPatterns/PatternTypes/PlayerControllerPatternTypes.h>
#include <MemoryPatterns/PatternTypes/PlayerPawnPatternTypes.h>
#include <MemoryPatterns/PatternTypes/PlayerResourcePatternTypes.h>
#include <MemoryPatterns/PatternTypes/WeaponPatternTypes.h>
#include <MemoryPatterns/PatternTypes/WeaponServicesPatternTypes.h>
#include <MemoryPatterns/PatternTypes/WeaponVDataPatternTypes.h>
#include <MemorySearch/BytePattern.h>
#include <MemorySearch/HybridPatternFinder.h>
#include <MemorySearch/PatternStringWildcard.h>
#include <Platform/DynamicLibrary.h>
#include <Platform/Windows/FileSystem/WindowsFileSystem.h>
#include <Platform/Windows/Syscalls/WindowsSyscalls.h>
#include <Utils/HexChars.h>
#include <Utils/MemorySection.h>
#include <Utils/Wcslen.h>

#include <Windows.h>

namespace auto_offsets
{

inline constexpr std::size_t kReportBufferSize{8192};
inline constexpr std::wstring_view kReportFileName{L"\\offsets_auto.json"};

class JsonWriter {
public:
    explicit JsonWriter(std::span<char> buffer) noexcept
        : buffer{buffer}
    {
    }

    void put(std::string_view text) noexcept
    {
        for (const auto c : text)
            putChar(c);
    }

    void putChar(char c) noexcept
    {
        if (used < buffer.size())
            buffer[used++] = c;
        else
            overflowed = true;
    }

    void putInt(std::int64_t value) noexcept
    {
        if (value < 0) {
            putChar('-');
            putUInt(static_cast<std::uint64_t>(-(value + 1)) + 1);
        } else {
            putUInt(static_cast<std::uint64_t>(value));
        }
    }

    void putUInt(std::uint64_t value) noexcept
    {
        char digits[20];
        std::size_t count{0};
        do {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);

        while (count != 0)
            putChar(digits[--count]);
    }

    void putHex(std::uint64_t value) noexcept
    {
        put("0x");
        char digits[16];
        std::size_t count{0};
        do {
            digits[count++] = "0123456789ABCDEF"[value & 0xF];
            value >>= 4;
        } while (value != 0);

        while (count != 0)
            putChar(digits[--count]);
    }

    void putBool(bool value) noexcept
    {
        put(value ? "true" : "false");
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return used;
    }

    [[nodiscard]] bool isOverflowed() const noexcept
    {
        return overflowed;
    }

private:
    std::span<char> buffer;
    std::size_t used{0};
    bool overflowed{false};
};

struct ConvertedPattern {
    char bytes[128];
    std::size_t size{0};
};

inline bool convertPattern(std::string_view text, ConvertedPattern& out) noexcept
{
    std::size_t readPosition{0};
    std::size_t writePosition{0};

    while (readPosition < text.size()) {
        const auto c = text[readPosition];
        if (c == ' ') {
            ++readPosition;
            continue;
        }

        if (writePosition + 1 >= sizeof(out.bytes))
            return false;

        if (c == kPatternStringWildcard) {
            out.bytes[writePosition++] = kPatternStringWildcard;
            ++readPosition;
            continue;
        }

        if (readPosition + 1 >= text.size())
            return false;

        const auto high = hexDigitToDecimal(text[readPosition]);
        const auto low = hexDigitToDecimal(text[readPosition + 1]);
        if (high > 0xF || low > 0xF)
            return false;

        out.bytes[writePosition++] = static_cast<char>((high << 4) | low);
        readPosition += 2;
    }

    out.size = writePosition;
    return writePosition != 0;
}

struct ScanResult {
    std::uintptr_t address{};
    int hits{0};
};

struct ScanOutcome {
    ScanResult scan;
    bool insideModule{false};
    bool healed{false};
};

inline ScanResult scanRipRelative(std::span<const std::byte> code, std::string_view patternText, std::uint8_t displacementOffset, std::uint8_t instructionLength) noexcept
{
    ScanResult result;

    ConvertedPattern converted;
    if (!convertPattern(patternText, converted))
        return result;

    const BytePattern pattern{std::string_view{converted.bytes, converted.size}, kPatternStringWildcard};
    HybridPatternFinder finder{code, pattern};

    if (auto found = finder.findNextOccurrence()) {
        std::int32_t displacement{};
        std::memcpy(&displacement, found + displacementOffset, sizeof(displacement));
        result.address = reinterpret_cast<std::uintptr_t>(found) + instructionLength + displacement;
        result.hits = 1;

        while (finder.findNextOccurrence())
            ++result.hits;
    }

    return result;
}

struct ScanTarget {
    const char* name;
    std::string_view pattern;
    std::uint8_t displacementOffset;
    std::uint8_t instructionLength;
};

inline constexpr ScanTarget kScanTargets[]{
    {"dwEntityList", "48 8B 0D ? ? ? ? EB 1C 48 8B 0D ? ? ? ? 48 85 C9 74", 3, 7},
    {"dwLocalPlayerController", "48 39 1D ? ? ? ? 75 04 B0 01", 3, 7},
    {"dwViewMatrix", "48 8D 0D ? ? ? ? 48 C1 E0 06", 3, 7},
    {"dwGlobalVars", "48 8B 05 ? ? ? ? 0F 57 C0 8B 48", 3, 7},
    {"dwGameRules", "F6 ? ? 0F 85 ? ? ? ? ? 8B ? ? ? ? ? ? 85 ? 0F", 12, 16},
    {"dwPlantedC4", "8B 0E 48 8B 1D ? ? ? ?", 5, 9},
};

inline constexpr auto kScanTargetCount{sizeof(kScanTargets) / sizeof(kScanTargets[0])};

struct ResolvedAddresses {
    std::uintptr_t values[kScanTargetCount]{};
};

inline ResolvedAddresses collectResolvedAddresses(const AllMemoryPatternSearchResults& results) noexcept
{
    ResolvedAddresses addresses;
    addresses.values[0] = reinterpret_cast<std::uintptr_t>(results.get<EntitySystemPointer>());
    addresses.values[1] = reinterpret_cast<std::uintptr_t>(results.get<LocalPlayerControllerPointer>());
    addresses.values[2] = reinterpret_cast<std::uintptr_t>(results.get<WorldToProjectionMatrixPointer>());
    addresses.values[3] = reinterpret_cast<std::uintptr_t>(results.get<GlobalVarsPointer>());
    addresses.values[4] = reinterpret_cast<std::uintptr_t>(results.get<GameRulesPointer>());
    addresses.values[5] = reinterpret_cast<std::uintptr_t>(results.get<PlantedC4sPointer>());
    return addresses;
}

template <typename PatternType>
void writeResolvedEntry(JsonWriter& writer, const AllMemoryPatternSearchResults& results, std::uintptr_t moduleBase, const char* name, bool& first) noexcept
{
    const auto value = results.get<PatternType>();

    if (!first)
        writer.put(", ");
    first = false;

    writer.putChar('"');
    writer.put(name);
    writer.put("\": ");

    if constexpr (requires { value.rawOffset(); }) {
        writer.putInt(static_cast<std::int64_t>(value.rawOffset()));
    } else {
        const auto address = reinterpret_cast<std::uintptr_t>(value);
        writer.putInt(address >= moduleBase ? static_cast<std::int64_t>(address - moduleBase) : 0);
    }
}

inline void writeResolvedSection(JsonWriter& writer, const AllMemoryPatternSearchResults& results, std::uintptr_t moduleBase) noexcept
{
    writer.put("\"resolved_by_patterns\": {");
    bool first{true};

    writeResolvedEntry<EntitySystemPointer>(writer, results, moduleBase, "dwEntityList", first);
    writeResolvedEntry<LocalPlayerControllerPointer>(writer, results, moduleBase, "dwLocalPlayerController", first);
    writeResolvedEntry<GlobalVarsPointer>(writer, results, moduleBase, "dwGlobalVars", first);
    writeResolvedEntry<GameRulesPointer>(writer, results, moduleBase, "dwGameRules", first);
    writeResolvedEntry<PlantedC4sPointer>(writer, results, moduleBase, "dwPlantedC4", first);
    writeResolvedEntry<WorldToProjectionMatrixPointer>(writer, results, moduleBase, "dwViewMatrix", first);
    writeResolvedEntry<ViewRenderPointer>(writer, results, moduleBase, "dwViewRender", first);
    writeResolvedEntry<PointerToPlayerResource>(writer, results, moduleBase, "dwPlayerResource", first);
    writeResolvedEntry<PointerToClientMode>(writer, results, moduleBase, "dwClientMode", first);

    writeResolvedEntry<EntityListOffset>(writer, results, moduleBase, "m_EntityList", first);
    writeResolvedEntry<OffsetToGameSceneNode>(writer, results, moduleBase, "m_pGameSceneNode", first);
    writeResolvedEntry<OffsetToHealth>(writer, results, moduleBase, "m_iHealth", first);
    writeResolvedEntry<OffsetToLifeState>(writer, results, moduleBase, "m_lifeState", first);
    writeResolvedEntry<OffsetToTeamNumber>(writer, results, moduleBase, "m_iTeamNum", first);
    writeResolvedEntry<OffsetToVData>(writer, results, moduleBase, "m_pSubclassVData", first);
    writeResolvedEntry<OffsetToRenderComponent>(writer, results, moduleBase, "m_pRenderComponent", first);
    writeResolvedEntry<OffsetToOwnerEntity>(writer, results, moduleBase, "m_hOwnerEntity", first);
    writeResolvedEntry<OffsetToGlowProperty>(writer, results, moduleBase, "m_Glow", first);
    writeResolvedEntry<OffsetToGameSceneNodeOwner>(writer, results, moduleBase, "m_pOwner", first);

    writeResolvedEntry<OffsetToWeaponServices>(writer, results, moduleBase, "m_pWeaponServices", first);
    writeResolvedEntry<OffsetToPlayerController>(writer, results, moduleBase, "m_hController", first);
    writeResolvedEntry<OffsetToPlayerPawnImmunity>(writer, results, moduleBase, "m_bGunGameImmunity", first);
    writeResolvedEntry<OffsetToIsDefusing>(writer, results, moduleBase, "m_bIsDefusing", first);
    writeResolvedEntry<OffsetToIsPickingUpHostage>(writer, results, moduleBase, "m_bIsGrabbingHostage", first);
    writeResolvedEntry<OffsetToHostageServices>(writer, results, moduleBase, "m_pHostageServices", first);
    writeResolvedEntry<OffsetToFlashBangEndTime>(writer, results, moduleBase, "m_flFlashBangTime", first);
    writeResolvedEntry<OffsetToIsScoped>(writer, results, moduleBase, "m_bIsScoped", first);
    writeResolvedEntry<OffsetToPlayerPawnSceneObjectUpdaterHandle>(writer, results, moduleBase, "m_sceneObjectUpdaterHandle", first);

    writeResolvedEntry<OffsetToBasePawnHandle>(writer, results, moduleBase, "m_hPawn", first);
    writeResolvedEntry<OffsetToPlayerColor>(writer, results, moduleBase, "m_iCompTeammateColor", first);

    writeResolvedEntry<OffsetToActiveWeapon>(writer, results, moduleBase, "m_hActiveWeapon", first);
    writeResolvedEntry<OffsetToWeapons>(writer, results, moduleBase, "m_hMyWeapons", first);
    writeResolvedEntry<OffsetToClipAmmo>(writer, results, moduleBase, "m_iClip1", first);
    writeResolvedEntry<OffsetToWeaponMode>(writer, results, moduleBase, "m_weaponMode", first);
    writeResolvedEntry<OffsetToWeaponSpread>(writer, results, moduleBase, "m_flSpread", first);

    writeResolvedEntry<OffsetToIsBeingPlanted>(writer, results, moduleBase, "m_bStartedArming", first);
    writeResolvedEntry<OffsetToArmingEndTime>(writer, results, moduleBase, "m_fArmedTime", first);
    writeResolvedEntry<BombSiteOffset>(writer, results, moduleBase, "m_nBombSite", first);
    writeResolvedEntry<BombTickingOffset>(writer, results, moduleBase, "m_bBombTicking", first);
    writeResolvedEntry<BombBlowTimeOffset>(writer, results, moduleBase, "m_flC4Blow", first);
    writeResolvedEntry<BombDefuserOffset>(writer, results, moduleBase, "m_hBombDefuser", first);
    writeResolvedEntry<BombDefuseEndTimeOffset>(writer, results, moduleBase, "m_flDefuseCountDown", first);

    writeResolvedEntry<RoundStartTimeOffset>(writer, results, moduleBase, "m_fRoundStartTime", first);
    writeResolvedEntry<OffsetToRoundRestartTime>(writer, results, moduleBase, "m_flRestartRoundTime", first);
    writeResolvedEntry<OffsetToRoundLength>(writer, results, moduleBase, "m_iRoundTime", first);
    writeResolvedEntry<OffsetToRoundWinStatus>(writer, results, moduleBase, "m_iRoundWinStatus", first);

    writer.putChar('}');
}

inline void writeScanSection(JsonWriter& writer, const ScanOutcome* outcomes, const ResolvedAddresses& resolved, std::uintptr_t moduleBase) noexcept
{
    writer.put("\"scanned\": {");

    for (std::size_t i = 0; i < kScanTargetCount; ++i) {
        const auto& target = kScanTargets[i];
        const auto& outcome = outcomes[i];

        if (i != 0)
            writer.put(", ");

        writer.putChar('"');
        writer.put(target.name);
        writer.put("\": {");

        writer.put("\"address\": ");
        if (outcome.scan.address != 0) {
            writer.putChar('"');
            writer.putHex(outcome.scan.address);
            writer.putChar('"');
        } else {
            writer.put("null");
        }

        writer.put(", \"offset\": ");
        writer.putInt(outcome.scan.address >= moduleBase ? static_cast<std::int64_t>(outcome.scan.address - moduleBase) : 0);

        writer.put(", \"hits\": ");
        writer.putInt(outcome.scan.hits);

        writer.put(", \"valid\": ");
        writer.putBool(outcome.insideModule);

        writer.put(", \"matches_resolved\": ");
        writer.putBool(outcome.scan.address != 0 && outcome.scan.address == resolved.values[i]);

        writer.put(", \"healed\": ");
        writer.putBool(outcome.healed);

        writer.putChar('}');
    }

    writer.putChar('}');
}

template <typename PatternType>
bool healIfMissing(AllMemoryPatternSearchResults& results, const ScanOutcome& outcome) noexcept
{
    if (!outcome.insideModule || outcome.scan.address == 0 || outcome.scan.hits != 1)
        return false;

    if (reinterpret_cast<std::uintptr_t>(results.get<PatternType>()) != 0)
        return false;

    results.clientPatternSearchResults.set<PatternType>(reinterpret_cast<UnpackStrongTypeAliasT<PatternType>>(outcome.scan.address));
    return true;
}

inline bool writeReportFile(const wchar_t* directoryPath, const char* data, std::size_t size) noexcept
{
    if (!directoryPath)
        return false;

    constexpr auto kMaxPathLength{320};
    const auto directoryLength = utils::wcslen(directoryPath);
    if (directoryLength + kReportFileName.size() + 1 > kMaxPathLength)
        return false;

    wchar_t path[kMaxPathLength];
    std::memcpy(path, directoryPath, directoryLength * sizeof(wchar_t));
    std::memcpy(path + directoryLength, kReportFileName.data(), kReportFileName.size() * sizeof(wchar_t));
    path[directoryLength + kReportFileName.size()] = L'\0';

    WindowsFileSystem::createDirectory(directoryPath);

    const auto handle = WindowsFileSystem::createFileForOverwrite(path);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    const auto written = WindowsFileSystem::writeFile(handle, 0, const_cast<char*>(data), size);
    WindowsSyscalls::NtClose(handle);
    return written == size;
}

inline bool writeReport(AllMemoryPatternSearchResults& results, const wchar_t* directoryPath) noexcept
{
    const DynamicLibrary clientDLL{cs2::CLIENT_DLL};
    if (!clientDLL)
        return false;

    static constinit char buffer[kReportBufferSize];
    JsonWriter writer{std::span{buffer}};

    const auto moduleBase = reinterpret_cast<std::uintptr_t>(clientDLL.getHandle());
    const auto clientCode = clientDLL.getCodeSection().raw();
    const auto clientData = clientDLL.getDataSection();

    ScanOutcome outcomes[kScanTargetCount];
    for (std::size_t i = 0; i < kScanTargetCount; ++i) {
        const auto& target = kScanTargets[i];
        outcomes[i].scan = scanRipRelative(clientCode, target.pattern, target.displacementOffset, target.instructionLength);
        outcomes[i].insideModule = outcomes[i].scan.address != 0 && clientData.contains(outcomes[i].scan.address, 8);
    }

    outcomes[0].healed = healIfMissing<EntitySystemPointer>(results, outcomes[0]);
    outcomes[1].healed = healIfMissing<LocalPlayerControllerPointer>(results, outcomes[1]);
    outcomes[2].healed = healIfMissing<WorldToProjectionMatrixPointer>(results, outcomes[2]);
    outcomes[3].healed = healIfMissing<GlobalVarsPointer>(results, outcomes[3]);
    outcomes[4].healed = healIfMissing<GameRulesPointer>(results, outcomes[4]);
    outcomes[5].healed = healIfMissing<PlantedC4sPointer>(results, outcomes[5]);

    const auto resolved = collectResolvedAddresses(results);

    writer.put("{\n  \"generator\": \"Osiris auto-offset scanner\",\n  \"mechanism\": \"runtime signature scan + resolved pattern results\",\n  \"module\": \"client.dll\",\n  \"module_base\": \"");
    writer.putHex(moduleBase);
    writer.put("\",\n  ");

    writeResolvedSection(writer, results, moduleBase);
    writer.put(",\n  ");

    writeScanSection(writer, outcomes, resolved, moduleBase);
    writer.put("\n}\n");

    if (writer.isOverflowed())
        return false;

    return writeReportFile(directoryPath, buffer, writer.size());
}

}

template <typename HookContext>
class AutoOffsets {
public:
    explicit AutoOffsets(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void writeReport() noexcept
    {
        auto_offsets::writeReport(hookContext.mutablePatternSearchResults(), hookContext.osirisDirectoryPath().get());
    }

private:
    HookContext& hookContext;
};

#else

template <typename HookContext>
class AutoOffsets {
public:
    explicit AutoOffsets(HookContext&) noexcept
    {
    }

    void writeReport() noexcept
    {
    }
};

#endif
