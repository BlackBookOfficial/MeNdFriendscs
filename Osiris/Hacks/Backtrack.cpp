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

// Define the minimum and maximum values for the extrapolatedTickcount
const float extrapolatedTickcountMin = 0.0f;
const float extrapolatedTickcountMax = 100.0f;
const float extrapolatedTickcountPrecision = 0.01f;

// Constants
constexpr float MAX_SPEED = 250.0f;  // Maximum player speed in units per second
constexpr float TICK_RATE = 64.0f;  // Server tick rate in ticks per second

// Calculate the ideal backtrack.extrapolatedTickcount
float calculateExtrapolatedTickcount(float speed)
{
    // Convert travel time to ticks by multiplying with server tick rate
    float tickcount = speed * memory->globalVars->currenttime;

    // Round the tickcount to the nearest integer value
    int roundedTickcount = static_cast<int>(tickcount + 0.5f);

    // Clamp the rounded tickcount to a minimum of 1 tick
    int clampedTickcount = max(roundedTickcount, 1);

    // Return the clamped tickcount as the ideal backtrack.extrapolatedTickcount
    return static_cast<float>(clampedTickcount);
}

/*// Function to calculate the ideal backtrack extrapolated tick count
float calculateExtrapolatedTickCount(const std::vector<float>& targetTickRates, float maxTickCount)
{
    // Sort the target tick rates in ascending order
    std::vector<float> sortedTickRates = targetTickRates;
    std::sort(sortedTickRates.begin(), sortedTickRates.end());

    // Initialize the ideal tick count to the maximum tick count
    float idealTickCount = maxTickCount;

    // Iterate through the sorted tick rates
    for (size_t i = 0; i < sortedTickRates.size() - 1; i++)
    {
        // Calculate the tick count difference between adjacent tick rates
        float tickRateDifference = sortedTickRates[i + 1] - sortedTickRates[i];

        // Calculate the maximum tick count between the two tick rates
        float maxTickCountBetweenRates = std::ceil(1.0f / tickRateDifference);

        // Update the ideal tick count if the maximum tick count is smaller
        if (maxTickCountBetweenRates < idealTickCount)
            idealTickCount = maxTickCountBetweenRates;
    }

    return idealTickCount;
}*/
/*float calculateExtrapolatedTickcount(const Config& config, const Vector& localPlayerEyePosition, const std::vector<BacktrackRecord>& records) {
    float bestFov = std::numeric_limits<float>::max();
    float bestExtrapolatedTickcount = 0.0f;

    for (const auto& record : records) {
        if (!Backtrack::valid(record.simulationTime))
            continue;

        const Vector& position = record.position;

        const Vector direction = localPlayerEyePosition - position;
        const float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        const Vector normalizedDirection = { direction.x / distance, direction.y / distance, direction.z / distance };

        const Vector extrapolatedHeadPosition = position + normalizedDirection * config.backtrackExtrapolatedTickcount;

        // Calculate the desired metrics (e.g., FOV) based on extrapolatedHeadPosition and other parameters
        // ...

        // Example: Calculate FOV
        const Vector angle = calculateRelativeAngle(localPlayerEyePosition, extrapolatedHeadPosition, localPlayer->getAbsAngle());
        const float fov = std::hypotf(angle.x, angle.y);

        if (fov < bestFov) {
            bestFov = fov;
            bestExtrapolatedTickcount = config.backtrackExtrapolatedTickcount;
        }
    }

    return bestExtrapolatedTickcount;
}*/

float Backtrack::getLerp() noexcept
{
    // Get the interpolation ratio.
    float ratio = std::clamp(cvars.interpRatio->getFloat(), cvars.minInterpRatio->getFloat(), cvars.maxInterpRatio->getFloat());

    // Calculate the interpolation factor.
    float interpolationFactor = (std::max)(cvars.interp->getFloat(), ratio / ((cvars.maxUpdateRate) ? cvars.maxUpdateRate->getFloat() : cvars.updateRate->getFloat()));

    // Return the interpolation factor.
    return interpolationFactor;
}

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

   // float targetTickRates = memory->globalVars->currenttime; // Sample target tick rates
    //float maxTickCount = 16.0f; // Maximum tick count

    // Calculate the ideal extrapolated tick count
   // const float extrapolatedTickCount = calculateExtrapolatedTickCount(targetTickRates.data(), targetTickRates.data() + targetTickRates.size());

    const auto aimPunch = activeWeapon->requiresRecoilControl() ? localPlayer->getAimPunch() : Vector{ };

    // Iterate through all valid players
    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        auto entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive() || !entity->isOtherEnemy(localPlayer.get()))
            continue;

        const auto player = Animations::getPlayer(i);
        if (!player.gotMatrix)
            continue;

        if (player.backtrackRecords.empty() || (!config->backtrack.ignoreSmoke && memory->lineGoesThroughSmoke(localPlayer->getEyePosition(), entity->getAbsOrigin(), 1)))
            continue;

        const float interpolationAmount = getLerp();

        // Find the best backtrack tick for the current player
        int bestPlayerTick = 0;
        float bestPlayerFov = 255.f;

        for (const auto& record : player.backtrackRecords) {
            if (!Backtrack::valid(record.simulationTime))
                continue;

            const Vector& position = record.positions.front();

            const int tickCorrection = customTickCorrection(record);

            const Vector extrapolatedHeadPosition = position + (localPlayerEyePosition - position).normalized()* calculateExtrapolatedTickcount(entity->velocity().length2D());//config->backtrack.extrapolatedTickcount;

            const Vector angle = AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, extrapolatedHeadPosition, entity->getAbsAngle());
            const float fov = std::hypotf(angle.x, angle.y);

            if (fov < bestPlayerFov) {
                bestPlayerFov = fov;
                bestPlayerTick = timeToTicks(record.simulationTime + interpolationAmount) + tickCorrection;
            }
        }

        // Update the best backtrack tick if a better one is found
        if (bestPlayerFov < bestFov) {
            bestFov = bestPlayerFov;
            bestTargetIndex = i;
            bestBacktrackTick = bestPlayerTick;
        }
    }

    const auto player = Animations::getPlayer(bestTargetIndex);
    if (!player.gotMatrix)
        return;

    if (bestTargetIndex && bestBacktrackTick) {
        //Logger::addLog((" [Backtrack] Targeting Tick " + std::to_string(bestBacktrackTick)).c_str());

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
