#pragma once

#include <cstdint>
#include <cstring>

#include "RttiCompleteObjectLocator.h"
#include "ToRvaConverter.h"
#include <Utils/MemorySection.h>

struct RttiTypeDescriptor;

class RttiCompleteObjectLocatorFinder {
public:
    RttiCompleteObjectLocatorFinder(MemorySection rdataSection, ToRvaConverter toRvaConverter) noexcept
        : rdataSection{rdataSection}
        , toRvaConverter{toRvaConverter}
    {
    }
    
    [[nodiscard]] const RttiCompleteObjectLocator* findCompleteObjectLocator(const RttiTypeDescriptor* typeDescriptor) const noexcept
    {
        const auto typeDescriptorRva{toRvaConverter.toRva(reinterpret_cast<std::uintptr_t>(typeDescriptor))};
        HybridPatternFinder typeDescriptorCrossReferenceFinder{rdataSection.raw(), BytePattern::ofObject(typeDescriptorRva)};

        auto typeDescriptorReference{typeDescriptorCrossReferenceFinder.findNextOccurrence()};
        while (typeDescriptorReference && !isCompleteObjectLocator(reinterpret_cast<std::uintptr_t>(typeDescriptorReference)))
            typeDescriptorReference = typeDescriptorCrossReferenceFinder.findNextOccurrence();

        if (typeDescriptorReference)
            return reinterpret_cast<const RttiCompleteObjectLocator*>(typeDescriptorReference - RttiCompleteObjectLocator::kOffsetOfTypeDescriptorRva);

        return nullptr;
    }

private:
    [[nodiscard]] bool isCompleteObjectLocator(std::uintptr_t typeDescriptorReference) const noexcept
    {
        const auto completeObjectLocatorBase = typeDescriptorReference - RttiCompleteObjectLocator::kOffsetOfTypeDescriptorRva;
        if (!rdataSection.contains(completeObjectLocatorBase, RttiCompleteObjectLocator::kSizeOf))
            return false;
        return selfRvaMatches(completeObjectLocatorBase) && isCompleteObjectLocatorOfCompleteClass(completeObjectLocatorBase);
    }

    [[nodiscard]] bool selfRvaMatches(std::uintptr_t completeObjectLocatorBase) const noexcept
    {
        std::uint32_t selfRva;
        std::memcpy(&selfRva, reinterpret_cast<void*>(completeObjectLocatorBase + RttiCompleteObjectLocator::kOffsetOfSelfRva), sizeof(selfRva));
        return selfRva == toRvaConverter.toRva(completeObjectLocatorBase);
    }

    [[nodiscard]] bool isCompleteObjectLocatorOfCompleteClass(std::uintptr_t completeObjectLocatorBase) const noexcept
    {
        std::uint32_t offsetInCompleteClass;
        std::memcpy(&offsetInCompleteClass, reinterpret_cast<void*>(completeObjectLocatorBase + RttiCompleteObjectLocator::kOffsetOfOffsetInCompleteClass), sizeof(offsetInCompleteClass));
        return offsetInCompleteClass == 0;
    }

    MemorySection rdataSection;
    ToRvaConverter toRvaConverter;
};
