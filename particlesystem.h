#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

#include <vector>
#include <cstddef>
#include <memory>

#include <glm/glm.hpp>

#include "particle.h"
#include "surfacecover.h"
#include "collision.h"

class ParticleSystem
{
public:
    explicit ParticleSystem(std::size_t maxParticles = 15000);

    void init();
    void update(float deltaTime);

    void setType(ParticleType type);
    void setWindAngle(float degrees);
    void setWindStrength(float metersPerSecond);
    void setIntensity(std::size_t count);

    ParticleType getType() const;
    float getWindAngle() const;
    float getWindStrength() const;

    const std::vector<Particle>& getParticles() const;
    std::size_t getActiveCount() const;

    // Брызги от капель дождя (частица активна, пока life > 0).
    const std::vector<Particle>& getSplashes() const;

    // Покрытие поверхностей: снег и влажность.
    const SurfaceCover& cover() const;
    void resetCover();
    void setCollisionAlgorithm(CollisionAlgorithm algorithm);
    CollisionAlgorithm collisionAlgorithm() const;
    void setObjectCount(int count);          // 0 = базовая сцена
    const BoxList& boxes() const;

private:
    void resetParticle(Particle& particle);
    void spawnSplash(const glm::vec3& position);
    void updateSplashes(float deltaTime);

    // Возвращает высоту поверхности под точкой.
    float surfaceHeight(float x, float z) const;

    std::vector<Particle> m_particles;

    std::size_t m_activeCount;

    ParticleType m_type;

    glm::vec3 m_windDirection;
    float m_windStrength;
    float m_windAngle;

    SurfaceCover m_cover;

    std::vector<Particle> m_splashes;
    std::size_t m_splashCursor;

    float m_time;
    BoxList m_boxes;
    std::unique_ptr<ICollision> m_collision;
    CollisionAlgorithm m_algorithm = CollisionAlgorithm::BruteForce;
    float m_precipHalfX = 14.0f;
    float m_precipHalfZ = 12.0f;
    float m_meanSurfaceHeight = 0.0f;
};

#endif // PARTICLESYSTEM_H
