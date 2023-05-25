#include "AntiAim.h"
#include "EnginePrediction.h"
#include "Fakelag.h"
#include "Tickbase.h"

#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/Localplayer.h"
#include "../SDK/Vector.h"

void Fakelag::run(bool& sendPacket) noexcept
{
    static bool FlipLag = false;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    const auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    if (AntiAim::getDidShoot()) {
        sendPacket = true;
        return;
    }

    auto chokedPackets = config->legitAntiAim.enabled || config->fakeAngle.enabled ? 2 : 0;
    if (config->fakelag.enabled)
    {
        const float speed = EnginePrediction::getVelocity().length2D() >= 15.0f ? EnginePrediction::getVelocity().length2D() : 0.0f;
        float velocity = EnginePrediction::getVelocity().length2D() * memory->globalVars->intervalPerTick;
        static int iTicksToChoke = std::clamp(static_cast<int>(std::ceil(64.f / velocity)), 1, config->fakelag.limit);
        switch (config->fakelag.mode) {
        case 0: //Static
            chokedPackets = config->fakelag.limit;
            break;
        case 1: //Adaptive
            //chokedPackets = std::clamp(static_cast<int>(std::ceilf(64 / (speed * memory->globalVars->intervalPerTick))), 1, config->fakelag.limit);
            chokedPackets = iTicksToChoke;
            break;
        case 2: // Random
            srand(static_cast<unsigned int>(time(NULL)));
            chokedPackets = rand() % config->fakelag.limit + 1;
            break;
        case 3: // autsim
            if (EnginePrediction::getVelocity().length2D() < 50)
                chokedPackets = config->fakelag.limit;
            else
                chokedPackets = std::clamp(static_cast<int>(std::ceilf((rand() % 128 + 32) / (speed * (memory->globalVars->intervalPerTick))))^2, 1, config->fakelag.limit);
            break;
        case 4: // tickbased random
            if (EnginePrediction::getFlags() & 1)
                chokedPackets = std::clamp(static_cast<int>(std::ceilf(128 / memory->globalVars->intervalPerTick)), 1, config->fakelag.limit);
            else
                chokedPackets = std::clamp(static_cast<int>(std::ceilf(64.f / ((rand() % 260 + 1) * memory->globalVars->intervalPerTick))), 1, config->fakelag.limit);
            break;
        case 5: // flip, long to quick
            chokedPackets = FlipLag ? config->fakelag.secondchoke : config->fakelag.limit;
            FlipLag = !FlipLag;
            break;
        case 6: // Dynamic
            // Calculate the variation in ticks based on the jitter percentage
            const float averageLatency = interfaces->engine->getNetworkChannel()->getLatency(0);
            const int averageLatencyTicks = static_cast<int>(averageLatency * memory->globalVars->intervalPerTick);
            const int jitterTicks = static_cast<int>(config->fakelag.jitter * averageLatencyTicks);

            // Calculate the current tickrate
            const float tickRate = 1.0f / memory->globalVars->intervalPerTick;

            // Calculate the number of ticks per second
            const int ticksPerSecond = static_cast<int>(tickRate);

            // Calculate the number of choked packets based on the average latency
            chokedPackets = std::clamp(averageLatencyTicks, 1, config->fakelag.limit);

            // Add random jitter to the number of choked packets
            chokedPackets += rand() % (2 * jitterTicks + 1) - jitterTicks;

            // Clamp the number of choked packets to a valid range
            chokedPackets = std::clamp(chokedPackets, 0, config->fakelag.limit);
            break;
        }
    }

    chokedPackets = std::clamp(chokedPackets, 0, maxUserCmdProcessTicks - Tickbase::getTargetTickShift());

    sendPacket = netChannel->chokedPackets >= chokedPackets;
}