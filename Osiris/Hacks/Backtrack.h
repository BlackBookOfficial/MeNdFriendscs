#pragma once

#include <array>
#include <deque>
#include "Animations.h"

#include "../ConfigStructs.h"

#include "../SDK/NetworkChannel.h"
#include "../SDK/matrix3x4.h"
#include "../SDK/ModelInfo.h"
#include "../SDK/Vector.h"

enum class FrameStage;
struct UserCmd;

namespace Backtrack
{
    void run(UserCmd*) noexcept;

    void addLatencyToNetwork(NetworkChannel*, float) noexcept;
    void updateIncomingSequences() noexcept;

    float getLerp() noexcept;
    int customTickCorrection(const Animations::Players::Record& record) noexcept;

    struct incomingSequence {
        int inreliablestate;
        int sequencenr;
        float servertime;
    };

    bool valid(float simtime) noexcept;
    void init() noexcept;
    float getMaxUnlag() noexcept;
}
