#ifndef PARTICLE_H
#define PARTICLE_H

#include <glm/glm.hpp>

enum class ParticleType
{
    RAIN,
    SNOW
};

struct Particle
{
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;

    float life;
    float size;
};

#endif // PARTICLE_H
