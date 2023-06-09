#pragma once

#include "Animations.h"
#include "../SDK/GameEvent.h"
#include "../SDK/Entity.h"

namespace Resolver
{
    struct SnapShot
    {
        Animations::Players player;
        const Model* model{ nullptr };
        Vector eyePosition{};
        Vector lbytest{};
        Vector bulletImpact{};
        bool gotImpact{ false };
        float time{ -1 };
        int playerIndex{ -1 };
        int backtrackRecord{ -1 };
    };

    void reset() noexcept;

    void processMissedShots() noexcept;
    void saveRecord(int playerIndex, float playerSimulationTime) noexcept;
    void getEvent(GameEvent* event) noexcept;

    void runPreUpdate(Animations::Players player, Entity* entity) noexcept;
    void runPostUpdate(Animations::Players player, Entity* entity) noexcept;

    void updateEventListeners(bool forceRemove = false) noexcept;
}