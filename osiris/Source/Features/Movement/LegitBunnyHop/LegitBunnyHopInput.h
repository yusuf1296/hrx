#pragma once

#include <Platform/Macros/IsPlatform.h>

#if IS_WIN64()

#include <Platform/Windows/WindowsDynamicLibrary.h>

class LegitBunnyHopInput {
public:
    LegitBunnyHopInput() noexcept
    {
        const WindowsDynamicLibrary user32{"USER32.dll"};
        if (user32) {
            sendInput = user32.getFunctionAddress("SendInput").as<decltype(&SendInput)>();
            getAsyncKeyState = user32.getFunctionAddress("GetAsyncKeyState").as<decltype(&GetAsyncKeyState)>();
        }
    }

    [[nodiscard]] bool isJumpKeyHeld() const noexcept
    {
        return getAsyncKeyState && (getAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    }

    // Baskiyi tazeler: oyun her inişte yeni bir bosluk tusu basinimi gormeli.
    void repressJumpKey() const noexcept
    {
        if (!sendInput)
            return;

        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_SPACE;
        inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = VK_SPACE;
        sendInput(2, inputs, sizeof(INPUT));
    }

    void releaseJumpKey() const noexcept
    {
        if (!sendInput)
            return;

        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = VK_SPACE;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        sendInput(1, &input, sizeof(INPUT));
    }

private:
    decltype(&SendInput) sendInput{nullptr};
    decltype(&GetAsyncKeyState) getAsyncKeyState{nullptr};
};

#else

class LegitBunnyHopInput {
public:
    [[nodiscard]] bool isJumpKeyHeld() const noexcept
    {
        return false;
    }

    void repressJumpKey() const noexcept
    {
    }

    void releaseJumpKey() const noexcept
    {
    }
};

#endif
