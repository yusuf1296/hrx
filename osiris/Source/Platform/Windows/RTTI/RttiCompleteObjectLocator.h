#pragma once

struct RttiCompleteObjectLocator {
    static constexpr auto kOffsetOfOffsetInCompleteClass{4};
    static constexpr auto kOffsetOfTypeDescriptorRva{12};
    static constexpr auto kOffsetOfSelfRva{20};

    static constexpr auto kSizeOf{24};
};
