#include "../Config.h"

#include "AimbotFunctions.h"
#include "Animations.h"
#include "Backtrack.h"
#include "Tickbase.h"
#include "../Logger.h"

#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/FrameStage.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"

static std::deque<Backtrack::incomingSequence> sequences;

struct Cvars {
    ConVar* updateRate;
    ConVar* maxUpdateRate;
    ConVar* interp;
    ConVar* interpRatio;
    ConVar* minInterpRatio;
    ConVar* maxInterpRatio;
    ConVar* maxUnlag;
};

static Cvars cvars;

float Backtrack::getLerp() noexcept
{
    // Get the interpolation ratio.
    float ratio = std::clamp(cvars.interpRatio->getFloat(), cvars.minInterpRatio->getFloat(), cvars.maxInterpRatio->getFloat());

    // Calculate the interpolation factor.
    float interpolationFactor = (std::max)(cvars.interp->getFloat(), ratio / ((cvars.maxUpdateRate) ? cvars.maxUpdateRate->getFloat() : cvars.updateRate->getFloat()));

    // Return the interpolation factor.
    return interpolationFactor;
}

/*void Backtrack::run(UserCmd* cmd) noexcept
{


    if (!config->backtrack.enabled)
        return;

    if (!(cmd->buttons & UserCmd::IN_ATTACK))
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!config->backtrack.ignoreFlash && localPlayer->isFlashed())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return;

    auto localPlayerEyePosition = localPlayer->getEyePosition();

    auto bestFov{ 255.f };
    int bestTargetIndex{ };
    int bestRecord{ };

    const auto aimPunch = activeWeapon->requiresRecoilControl() ? localPlayer->getAimPunch() : Vector{ };

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
            || !entity->isOtherEnemy(localPlayer.get()))
            continue;

        const auto player = Animations::getPlayer(i);
        if (!player.gotMatrix)
            continue;

        if (player.backtrackRecords.empty() || (!config->backtrack.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayer->getEyePosition(), entity->getAbsOrigin(), 1)))
            continue;

        for (int j = static_cast<int>(player.backtrackRecords.size() - 1U); j >= 0; j--)
        {
            const Vector localPlayerEyePosition = localPlayer->getEyePosition();
            const Vector localPlayerAbsAngle = localPlayer->getAbsAngle();


            if (Backtrack::valid(player.backtrackRecords.at(j).simulationTime))
            {

                //chat gpt lol
                const auto& prevRecord = player.backtrackRecords[j - 1];
                const auto& curRecord = player.backtrackRecords[j];
                const Vector& prevPosition = prevRecord.positions.front();
                const Vector& curPosition = curRecord.positions.front();

                // Interpolate between previous and current tick position
                const float interpolationFactor = getLerp();
                const Vector interpolatedPosition = prevPosition + (curPosition - prevPosition) * interpolationFactor;

                // Extrapolate the real head position using local player's absolute angle
                const Vector extrapolatedHeadPosition = interpolatedPosition + (localPlayerEyePosition - interpolatedPosition).normalized() * 5;

                // Calculate the angle between local player's eye position and the extrapolated head position
                const Vector angle = AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, extrapolatedHeadPosition, localPlayerAbsAngle);
                const float fov = std::hypotf(angle.x, angle.y);

                if (fov < bestFov)
                {
                    bestFov = fov;
                    bestRecord = j;
                    bestTargetIndex = i;
                }

            }
        }
    }

    const auto player = Animations::getPlayer(bestTargetIndex);
    if (!player.gotMatrix)
        return;

    if (bestRecord) {
        const auto& record = player.backtrackRecords[bestRecord];
        cmd->tickCount = timeToTicks(record.simulationTime + getLerp());
    }
}
*/
/*void Backtrack::run(UserCmd* cmd) noexcept
{
    if (!config->backtrack.enabled)
        return;

    if (!(cmd->buttons & UserCmd::IN_ATTACK))
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!config->backtrack.ignoreFlash && localPlayer->isFlashed())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return;

    auto localPlayerEyePosition = localPlayer->getEyePosition();

    auto bestFov = 255.f;
    int bestTargetIndex = 0;
    int bestRecordIndex = 0;
    int bestTickCorrection = 0;
    int bestBacktrackTick = 0; // Added variable to store the targeted backtrack tick

    const auto aimPunch = activeWeapon->requiresRecoilControl() ? localPlayer->getAimPunch() : Vector{ };

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || !entity->isOtherEnemy(localPlayer.get()))
            continue;

        const auto player = Animations::getPlayer(i);
        if (!player.gotMatrix)
            continue;

        if (player.backtrackRecords.empty() || (!config->backtrack.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayer->getEyePosition(), entity->getAbsOrigin(), 1)))
            continue;

        for (size_t j = 0; j < player.backtrackRecords.size(); j++) {
            const auto& record = player.backtrackRecords[j];

            if (!Backtrack::valid(record.simulationTime))
                continue;

            const Vector& position = record.positions.front();

            // Apply custom tick correction
            const int tickCorrection = customTickCorrection(record);

            // Calculate the extrapolated head position with tick correction
            const Vector extrapolatedHeadPosition = position + (localPlayerEyePosition - position).normalized() * 5;

            // Calculate the angle between local player's eye position and the extrapolated head position
            const Vector angle = AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, extrapolatedHeadPosition, localPlayer->getAbsAngle());
            const float fov = std::hypotf(angle.x, angle.y);

            if (fov < bestFov) {
                bestFov = fov;
                bestTargetIndex = i;
                bestRecordIndex = j;
                bestTickCorrection = tickCorrection;
                bestBacktrackTick = timeToTicks(record.simulationTime + getLerp()) + bestTickCorrection; // Calculate the targeted backtrack tick
            }
        }
    }

    const auto player = Animations::getPlayer(bestTargetIndex);
    if (!player.gotMatrix)
        return;

    if (bestRecordIndex) {
        const auto& record = player.backtrackRecords[bestRecordIndex];
        const int correctedTickCount = timeToTicks(record.simulationTime + getLerp()) + bestTickCorrection;
        cmd->tickCount = correctedTickCount;
    }
    if (!config->resolver.enable) {
        // Output the targeted backtrack tick
        if (bestTargetIndex && bestBacktrackTick)
            Logger::addLog((" [Backtrack] Targeting Tick " + std::to_string(bestBacktrackTick)).c_str());
    }
}
*/

// Interpolates the position based on a given time within a set of recorded positions
Vector interpolatePosition(const const std::deque<Vector>& positions, float targetTime)
{
    if (positions.empty())
        return Vector{};

    // Find the closest recorded positions based on index
    size_t startIndex = 0;
    size_t endIndex = positions.size() - 1;
    while (startIndex < endIndex - 1)
    {
        size_t midIndex = (startIndex + endIndex) / 2;
        float midTime = static_cast<float>(midIndex) / positions.size(); // Assuming uniform distribution of positions over time
        if (targetTime < midTime)
            endIndex = midIndex;
        else
            startIndex = midIndex;
    }

    const Vector& startPosition = positions[startIndex];
    const Vector& endPosition = positions[endIndex];

    // Calculate the interpolation factor
    const float interpolationFactor = (targetTime - static_cast<float>(startIndex) / positions.size()) /
        (static_cast<float>(endIndex) / positions.size() - static_cast<float>(startIndex) / positions.size());

    // Perform linear interpolation to get the interpolated position
    const Vector interpolatedPosition = startPosition + (endPosition - startPosition) * interpolationFactor;

    return interpolatedPosition;
}

// Adjusts the aim position based on the backtracked position and aim punch (recoil)
Vector adjustAimPosition(const Vector& backtrackPosition, const Vector& aimPunch)
{
    // Apply aim punch adjustment to the backtracked position
    Vector adjustedAimPosition = backtrackPosition - aimPunch;

    // Perform additional adjustment logic if needed
    // ...

    return adjustedAimPosition;
}

void Backtrack::run(UserCmd* cmd) noexcept
{
    if (!config->backtrack.enabled)
        return;

    if (!(cmd->buttons & UserCmd::IN_ATTACK))
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!config->backtrack.ignoreFlash && localPlayer->isFlashed())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return;

    auto localPlayerEyePosition = localPlayer->getEyePosition();

    auto bestFov = 255.f;
    int bestTargetIndex = 0;
    int bestRecordIndex = 0;
    int bestTickCorrection = 0;
    int bestBacktrackTick = 0;

    const auto aimPunch = activeWeapon->requiresRecoilControl() ? localPlayer->getAimPunch() : Vector{ };

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || !entity->isOtherEnemy(localPlayer.get()))
            continue;

        const auto player = Animations::getPlayer(i);
        if (!player.gotMatrix)
            continue;

        if (player.backtrackRecords.empty() || (!config->backtrack.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayer->getEyePosition(), entity->getAbsOrigin(), 1)))
            continue;

        for (size_t j = 0; j < player.backtrackRecords.size(); j++) {
            const auto& record = player.backtrackRecords[j];

            if (!Backtrack::valid(record.simulationTime))
                continue;

            const Vector& position = record.positions.front();

            const int tickCorrection = customTickCorrection(record);

            const Vector extrapolatedHeadPosition = position + (localPlayerEyePosition - position).normalized() * 5;

            const Vector angle = AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, extrapolatedHeadPosition, localPlayer->getAbsAngle());
            const float fov = std::hypotf(angle.x, angle.y);

            if (fov < bestFov) {
                bestFov = fov;
                bestTargetIndex = i;
                bestRecordIndex = j;
                bestTickCorrection = tickCorrection;
                bestBacktrackTick = timeToTicks(record.simulationTime + getLerp()) + bestTickCorrection;
            }
        }
    }

    const auto player = Animations::getPlayer(bestTargetIndex);
    if (!player.gotMatrix)
        return;

    if (bestRecordIndex) {
        const auto& record = player.backtrackRecords[bestRecordIndex];
        const int correctedTickCount = timeToTicks(record.simulationTime + getLerp()) + bestTickCorrection;
        cmd->tickCount = correctedTickCount;

    }

        if (bestTargetIndex && bestBacktrackTick) {
            Logger::addLog((" [Backtrack] Targeting Tick " + std::to_string(bestBacktrackTick)).c_str());

            // Perform additional complex backtracking operations using the bestBacktrackTick value
            const auto& record = player.backtrackRecords[bestRecordIndex];
            const float backtrackTime = record.simulationTime + getLerp();
            const Vector backtrackPosition = interpolatePosition(record.positions, backtrackTime);
            const Vector adjustedAimPosition = adjustAimPosition(backtrackPosition, aimPunch);

            // Use the adjusted aim position for further processing or aiming
            // ...
        }
}


int Backtrack::customTickCorrection(const Animations::Players::Record& record) noexcept
{
    const float latency = interfaces->engine->getNetworkChannel()->getLatency(0);
    const float serverTime = memory->globalVars->serverTime();
    const float interpolationAmount = getLerp();

    // Calculate the difference between the current server time and the recorded simulation time
    const float timeDelta = serverTime - record.simulationTime;

    // Calculate the number of ticks the player has advanced since the record was made
    const int ticksPassed = timeToTicks(timeDelta);

    // Calculate the interpolation time in ticks
    const int interpolatedTicks = timeToTicks(interpolationAmount);

    // Retrieve the last recorded position from the positions deque
    const Vector& recordedPosition = record.positions.back();

    // Calculate the interpolated position based on the recorded position and interpolation amount
    const Vector interpolatedPosition = recordedPosition + (record.absAngle - recordedPosition) * interpolationAmount;

    // Calculate the difference between the interpolated position and the current position
    const float positionDelta = (record.absAngle - interpolatedPosition).length();

    // Adjust the tick correction based on the latency, tick delta, and position delta
    const int tickCorrection = static_cast<int>(std::round(latency / memory->globalVars->intervalPerTick)) - (ticksPassed - interpolatedTicks) - static_cast<int>(positionDelta / 64.f);

    return tickCorrection;
}



void Backtrack::addLatencyToNetwork(NetworkChannel* network, float latency) noexcept
{
    for (auto& sequence : sequences)
    {
        if (memory->globalVars->serverTime() - sequence.servertime >= latency)
        {
            network->inReliableState = sequence.inreliablestate;
            network->inSequenceNr = sequence.sequencenr;
            break;
        }
    }
}

void Backtrack::updateIncomingSequences() noexcept
{
    static int lastIncomingSequenceNumber = 0;

    if (!localPlayer)
        return;

    auto network = interfaces->engine->getNetworkChannel();
    if (!network)
        return;

    if (network->inSequenceNr != lastIncomingSequenceNumber)
    {
        lastIncomingSequenceNumber = network->inSequenceNr;

        incomingSequence sequence{ };
        sequence.inreliablestate = network->inReliableState;
        sequence.sequencenr = network->inSequenceNr;
        sequence.servertime = memory->globalVars->serverTime();
        sequences.push_front(sequence);
    }

    while (sequences.size() > 2048)
        sequences.pop_back();
}


bool Backtrack::valid(float simtime) noexcept
{
    const auto network = interfaces->engine->getNetworkChannel();
    if (!network)
        return false;

    const auto deadTime = static_cast<int>(memory->globalVars->serverTime() - cvars.maxUnlag->getFloat());
    if (simtime < deadTime)
        return false;

    const auto extraTickbaseDelta = Tickbase::canShift(Tickbase::getTargetTickShift()) ? ticksToTime(Tickbase::getTargetTickShift()) : 0.0f;
    const auto delta = std::clamp(network->getLatency(0) + network->getLatency(1) + getLerp(), 0.f, cvars.maxUnlag->getFloat()) - (memory->globalVars->serverTime() - extraTickbaseDelta - simtime);
    return std::abs(delta) <= 0.2f;
}

void Backtrack::init() noexcept
{
    cvars.updateRate = interfaces->cvar->findVar("cl_updaterate");
    cvars.maxUpdateRate = interfaces->cvar->findVar("sv_maxupdaterate");
    cvars.interp = interfaces->cvar->findVar("cl_interp");
    cvars.interpRatio = interfaces->cvar->findVar("cl_interp_ratio");
    cvars.minInterpRatio = interfaces->cvar->findVar("sv_client_min_interp_ratio");
    cvars.maxInterpRatio = interfaces->cvar->findVar("sv_client_max_interp_ratio");
    cvars.maxUnlag = interfaces->cvar->findVar("sv_maxunlag");
}

float Backtrack::getMaxUnlag() noexcept
{
    return cvars.maxUnlag->getFloat();
}
