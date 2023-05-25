#include "AimbotFunctions.h"
#include "Animations.h"
#include "Resolver.h"

#include "../Logger.h"

#include "../SDK/GameEvent.h"

#include <deque>
#include "../SDK/Entity.h"
#include "Resolver.h"


std::deque<Resolver::SnapShot> snapshots;

void Resolver::reset() noexcept
{
    snapshots.clear();
}

// Structure to hold the relevant player data
struct PlayerData
{
    float visualAngle;
    float serverAngle;
    float trueHeadYaw;

};

class Vector2D
{
public:
    float x;
    float y;

    Vector2D(float _x = 0.0f, float _y = 0.0f)
        : x(_x), y(_y)
    {
    }

    float dot(const Vector2D& other) const
    {
        return x * other.x + y * other.y;
    }

    float magnitude() const
    {
        return std::sqrt(x * x + y * y);
    }

    Vector2D operator*(float scalar) const
    {
        return Vector2D(x * scalar, y * scalar);
    }

    float toAngle() const
    {
        return std::atan2(y, x);
    }

    static Vector2D fromAngle(float angle)
    {
        return Vector2D(std::cos(angle), std::sin(angle));
    }
};

float calculateTrueAngle(float visualAngle, float serverAngle)
{
    // Define your algorithm parameters here
    const float angleThreshold = 10.0f;  // Maximum allowable difference between visual and server angles
    const float maxExtrapolationDistance = 1000.0f;  // Maximum distance for extrapolation

    // Calculate the difference between the visual and server angles
    float angleDifference = std::abs(visualAngle - serverAngle);

    // Check if the angle difference is within the threshold
    if (angleDifference <= angleThreshold)
    {
        // Visual and server angles are close enough, consider server angle as the true angle
        return serverAngle;
    }
    else
    {
        // Visual and server angles are significantly different, apply complex algorithm
        // to estimate the true angle based on the available information

        // Calculate the direction vector from visual angle
        Vector2D visualDirection = Vector2D::fromAngle(visualAngle);

        // Calculate the direction vector from server angle
        Vector2D serverDirection = Vector2D::fromAngle(serverAngle);

        // Calculate the dot product between visual and server directions
        float dotProduct = visualDirection.dot(serverDirection);

        // Calculate the magnitude of the visual direction vector
        float visualMagnitude = visualDirection.magnitude();

        // Calculate the magnitude of the server direction vector
        float serverMagnitude = serverDirection.magnitude();

        // Calculate the angle between visual and server directions using the dot product
        float angleBetween = std::acos(dotProduct / (visualMagnitude * serverMagnitude));

        // Calculate the distance to extrapolate based on the angle difference
        float extrapolationDistance = (angleDifference / angleThreshold) * maxExtrapolationDistance;

        // Extrapolate the visual direction vector by the extrapolation distance
        Vector2D extrapolatedDirection = visualDirection * extrapolationDistance;

        // Calculate the final true angle by adding the extrapolated direction to the server angle
        float trueAngle = serverAngle + extrapolatedDirection.toAngle();

        // Return the calculated true angle
        return trueAngle;
    }
}

// Function to update the player's angles with the calculated true angle and true head yaw
void updatePlayerAngles(PlayerData& player, float trueHeadYaw)
{
    // Get the visual and server angles for the player
    float visualAngle = player.visualAngle;
    float serverAngle = player.serverAngle;

    // Calculate the true angle based on the visual and server angles
    float trueAngle = calculateTrueAngle(visualAngle, serverAngle);

    // Update the player's angles with the calculated true angle
    player.serverAngle = trueAngle;

    // Update the player's head yaw with the calculated true head yaw
    player.trueHeadYaw = trueHeadYaw;
}

// Complex interpolation and extrapolation algorithm for determining the true angle of the body and head
float complexInterpolationExtrapolation(float footYaw)
{
    // Define your interpolation and extrapolation parameters here
    const float minFootYaw = -360.0f;  // Minimum possible footYaw value
    const float maxFootYaw = 360.0f;   // Maximum possible footYaw value
    const float minBodyYaw = -360.0f;  // Minimum possible true bodyYaw value
    const float maxBodyYaw = 360.0f;   // Maximum possible true bodyYaw value
    const float minHeadYaw = -360.0f;  // Minimum possible true headYaw value
    const float maxHeadYaw = 360.0f;   // Maximum possible true headYaw value

    // Apply complex mathematical functions for interpolation and extrapolation
    float t = (footYaw - minFootYaw) / (maxFootYaw - minFootYaw);
    t = std::pow(t, 3);  // Cubic interpolation
    t = std::sin(t * M_PI);  // Sine function interpolation

    // You can introduce more complex mathematical functions here based on your requirements
    // For example, you can use exponential functions, logarithmic functions, or custom functions

    // Perform interpolation and extrapolation based on modified t value
    const float trueBodyYaw = minBodyYaw + t * (maxBodyYaw - minBodyYaw);
    const float trueHeadYaw = minHeadYaw + t * (maxHeadYaw - minHeadYaw);

    // Return the calculated true headYaw value
    return trueHeadYaw;
}

void Resolver::saveRecord(int playerIndex, float playerSimulationTime) noexcept
{
    const auto entity = interfaces->entityList->getEntity(playerIndex);
    const auto player = Animations::getPlayer(playerIndex);

    if (!player.gotMatrix || !entity)
        return;

    SnapShot snapshot;
    snapshot.player = player;
    snapshot.playerIndex = playerIndex;
    snapshot.eyePosition = localPlayer->getEyePosition();
    snapshot.model = entity->getModel();

    snapshot.lbytest = entity->getAbsAngle();

    if (player.simulationTime == playerSimulationTime)
    {
        snapshots.push_back(snapshot);
        return;
    }

    for (int i = 0; i < static_cast<int>(player.backtrackRecords.size()); i++)
    {
        if (player.backtrackRecords.at(i).simulationTime == playerSimulationTime)
        {
            snapshot.backtrackRecord = i;
            snapshots.push_back(snapshot);
            return;
        }
    }
}

void Resolver::getEvent(GameEvent* event) noexcept
{
    if (!event || !localPlayer || interfaces->engine->isHLTV())
        return;

    switch (fnv::hashRuntime(event->getName())) {
    case fnv::hash("round_start"):
    {
        //Reset all
        auto players = Animations::setPlayers();
        if (players->empty())
            break;

        for (int i = 0; i < static_cast<int>(players->size()); i++)
        {
            players->at(i).misses = 0;
        }
        snapshots.clear();
        break;
    }
    case fnv::hash("player_death"):
    {
        //Reset player
        const auto playerId = event->getInt("userid");
        if (playerId == localPlayer->getUserId())
            break;

        const auto index = interfaces->engine->getPlayerForUserID(playerId);
        Animations::setPlayer(index)->misses = 0;
        break;
    }
    case fnv::hash("player_hurt"):
    {
        if (snapshots.empty())
            break;

        if (event->getInt("attacker") != localPlayer->getUserId())
            break;

        const auto hitgroup = event->getInt("hitgroup");
        if (hitgroup < HitGroup::Head || hitgroup > HitGroup::RightLeg)
            break;

        snapshots.pop_front(); //Hit somebody so dont calculate
        break;
    }
    case fnv::hash("bullet_impact"):
    {
        if (snapshots.empty())
            break;

        if (event->getInt("userid") != localPlayer->getUserId())
            break;

        auto& snapshot = snapshots.front();

        if (!snapshot.gotImpact)
        {
            snapshot.time = memory->globalVars->serverTime();
            snapshot.bulletImpact = Vector{ event->getFloat("x"), event->getFloat("y"), event->getFloat("z") };
            snapshot.gotImpact = true;
        }
        break;
    }
    default:
        break;
    }
    if (!config->resolver.enable)
        snapshots.clear();
}

void Resolver::processMissedShots() noexcept
{
    if (!config->resolver.enable)
    {
        snapshots.clear();
        return;
    }

    if (!localPlayer || !localPlayer->isAlive())
    {
        snapshots.clear();
        return;
    }

    if (snapshots.empty())
        return;

    if (snapshots.front().time == -1) //Didnt get data yet
        return;

    auto snapshot = snapshots.front();
    snapshots.pop_front(); //got the info no need for this
    if (!snapshot.player.gotMatrix)
        return;

    const auto entity = interfaces->entityList->getEntity(snapshot.playerIndex);

    if (!entity)
        return;

    const Model* model = snapshot.model;
    if (!model)
        return;

    StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
    if (!hdr)
        return;

    StudioHitboxSet* set = hdr->getHitboxSet(0);
    if (!set)
        return;
    const Vector angleVector(snapshot.player.absAngle.x, snapshot.player.absAngle.y, snapshot.player.absAngle.z);
    const auto end = snapshot.bulletImpact + angleVector * 2000.f;

    const auto angle = AimbotFunction::calculateRelativeAngle(snapshot.eyePosition, snapshot.bulletImpact, Vector{ });

    const auto matrix = snapshot.backtrackRecord == -1 ? snapshot.player.matrix.data() : snapshot.player.backtrackRecords.at(snapshot.backtrackRecord).matrix;

    bool resolverMissed = false;

    for (int hitbox = 0; hitbox < Hitboxes::Max; hitbox++)
    {
        if (AimbotFunction::hitboxIntersection(matrix, hitbox, set, snapshot.eyePosition, end))
        {
            resolverMissed = true;
            std::string missed = "Missed " + entity->getPlayerName() + " due to";
            if (snapshot.backtrackRecord > 0)
                missed += " - Bad BT [" + std::to_string(snapshot.backtrackRecord) + "]";
            else
                missed += " - Bad resolve";
            Logger::addLog(missed);
            Animations::setPlayer(snapshot.playerIndex)->misses++;
            break;
        }
    }
    if (!resolverMissed)
        Logger::addLog("Missed due to spread");
}

void Resolver::runPreUpdate(Animations::Players player, Entity* entity) noexcept
{
    if (!config->resolver.enable)
        return;

    const auto misses = player.misses;
    if (!entity || !entity->isAlive())
        return;

    if (player.chokedPackets <= 0)
        return;

    // Calculate the true head yaw based on entity-specific information
    float trueHeadYaw = complexInterpolationExtrapolation(entity->getAnimstate()->footYaw);

    // Update the player's angles with the calculated true angle and true head yaw
    PlayerData playerData;
    playerData.visualAngle = entity->getAnimstate()->eyeYaw;
    playerData.serverAngle = entity->getAnimstate()->footYaw;

    updatePlayerAngles(playerData, trueHeadYaw);

    // Set the updated true angle as the player's server angle
    entity->getAnimstate()->footYaw = playerData.serverAngle;

    // Set the updated true head yaw as the player's eye yaw
    entity->getAnimstate()->eyeYaw = playerData.trueHeadYaw;
}

void Resolver::runPostUpdate(Animations::Players player, Entity* entity) noexcept
{
    if (!config->resolver.enable)
        return;

    const auto misses = player.misses;
    if (!entity || !entity->isAlive())
        return;

    if (player.chokedPackets <= 0)
        return;
}

void Resolver::updateEventListeners(bool forceRemove) noexcept
{
    class ImpactEventListener : public GameEventListener {
    public:
        void fireGameEvent(GameEvent* event) {
            getEvent(event);
        }
    };

    static ImpactEventListener listener;
    static bool listenerRegistered = false;

    if (config->resolver.enable && !listenerRegistered) {
        interfaces->gameEventManager->addListener(&listener, "bullet_impact");
        listenerRegistered = true;
    }
    else if ((!config->resolver.enable || forceRemove) && listenerRegistered) {
        interfaces->gameEventManager->removeListener(&listener);
        listenerRegistered = false;
    }
}