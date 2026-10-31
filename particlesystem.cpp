#include "particlesystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "scenedata.h"

namespace
{
constexpr std::size_t kMaxSplashes = 8000;

// Сколько снега добавляет одна снежинка в ячейку покрытия.
constexpr float kSnowPerFlake = 0.10f;

// Минимальная вертикальная скорость удара, при которой капля даёт брызги.
constexpr float kSplashSpeed = 3.0f;

float randomFloat(float minValue, float maxValue)
{
    const float r =
        static_cast<float>(std::rand()) /
        static_cast<float>(RAND_MAX);

    return minValue + r * (maxValue - minValue);
}
}

ParticleSystem::ParticleSystem(std::size_t maxParticles)
    : m_particles(maxParticles)
    , m_activeCount(maxParticles / 2)
    , m_type(ParticleType::SNOW)
    , m_windDirection(1.0f, 0.0f, 0.0f)
    , m_windStrength(3.0f)
    , m_windAngle(0.0f)
    , m_splashes(kMaxSplashes)
    , m_splashCursor(0)
    , m_time(0.0f)
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    for (Particle& s : m_splashes)
        s.life = 0.0f;

    init();
}

void ParticleSystem::init()
{
    for (Particle& particle : m_particles)
    {
        resetParticle(particle);

        // Разбрасываем частицы по высоте,
        // чтобы экран сразу был заполнен осадками.
        particle.position.y =
            randomFloat(3.0f, 18.0f);
    }
}

void ParticleSystem::resetParticle(Particle& particle)
{
    const float x = randomFloat(-14.0f, 14.0f);
    const float z = randomFloat(-12.0f, 12.0f);

    particle.position = glm::vec3(
        x,
        randomFloat(13.0f, 20.0f),
        z
        );

    particle.life = 1.0f;

    if (m_type == ParticleType::RAIN)
    {
        particle.velocity = glm::vec3(
            randomFloat(-0.3f, 0.3f),
            randomFloat(-12.0f, -9.0f),
            randomFloat(-0.3f, 0.3f)
            );

        particle.acceleration =
            glm::vec3(0.0f, -9.81f, 0.0f);

        particle.size = 0.07f;
    }
    else
    {
        particle.velocity = glm::vec3(
            randomFloat(-0.8f, 0.8f),
            randomFloat(-2.0f, -1.0f),
            randomFloat(-0.5f, 0.5f)
            );

        particle.acceleration =
            glm::vec3(0.0f, -2.5f, 0.0f);

        particle.size = randomFloat(0.08f, 0.16f);
    }
}

float ParticleSystem::surfaceHeight(float x, float z) const
{
    // Рельеф и объекты берутся из общего описания сцены,
    // поэтому физика всегда совпадает с тем, что нарисовано.
    return sceneHeight(x, z);
}

void ParticleSystem::spawnSplash(const glm::vec3& position)
{
    for (int k = 0; k < 2; ++k)
    {
        Particle& s = m_splashes[m_splashCursor];
        m_splashCursor = (m_splashCursor + 1) % m_splashes.size();

        const float angle = randomFloat(0.0f, 6.2831853f);
        const float speed = randomFloat(0.6f, 1.8f);

        s.position = position + glm::vec3(0.0f, 0.03f, 0.0f);

        s.velocity = glm::vec3(
            std::cos(angle) * speed,
            randomFloat(1.5f, 3.2f),
            std::sin(angle) * speed
            );

        s.acceleration = glm::vec3(0.0f);
        s.life = 1.0f;
        s.size = 0.03f;
    }
}

void ParticleSystem::updateSplashes(float deltaTime)
{
    for (Particle& s : m_splashes)
    {
        if (s.life <= 0.0f)
            continue;

        // Брызги — баллистические частицы: только гравитация.
        s.velocity.y -= 9.81f * deltaTime;
        s.position += s.velocity * deltaTime;

        // Живут около трети секунды.
        s.life -= deltaTime * 3.0f;

        if (s.position.y <= surfaceHeight(s.position.x, s.position.z))
            s.life = 0.0f;
    }
}

void ParticleSystem::update(float deltaTime)
{
    deltaTime = std::min(deltaTime, 0.033f);

    m_time += deltaTime;

    // Порывы ветра: сила плавно «дышит» около базового значения.
    const float gust =
        1.0f +
        0.35f * std::sin(m_time * 0.7f) +
        0.20f * std::sin(m_time * 1.9f + 1.0f);

    // Скорость ветра с учётом порыва, м/с.
    const glm::vec3 wind =
        m_windDirection * (m_windStrength * gust);

    const bool rain =
        (m_type == ParticleType::RAIN);

    // Модель обтекания: частица стремится к скорости окружающего
    // воздуха (ветер + падение с предельной скоростью):
    //     a = k * (v_target - v)
    // Эта форма одновременно описывает гравитацию и сопротивление
    // среды: без ветра частица падает строго вниз с предельной скоростью.
    const float k =
        rain ? 1.5f : 4.0f;            // 1/с: как быстро частица «подстраивается»

    // Доля скорости ветра, которую набирает частица
    // (эмпирический коэффициент увлечения).
    const float windResponse =
        rain ? 1.2f : 0.35f;

    for (std::size_t i = 0; i < m_activeCount; ++i)
    {
        Particle& p = m_particles[i];

        // Предельная скорость падения: у снежинок зависит от размера.
        const float terminal =
            rain ? 9.0f : (0.8f + p.size * 6.0f);

        glm::vec3 target =
            wind * windResponse +
            glm::vec3(0.0f, -terminal, 0.0f);

        if (!rain)
        {
            // Естественное покачивание снежинок.
            target.x += std::sin(p.position.y * 1.7f) * 0.3f;
            target.z += std::cos(p.position.y * 1.3f) * 0.2f;
        }

        p.acceleration =
            (target - p.velocity) * k;

        p.velocity +=
            p.acceleration * deltaTime;

        p.position +=
            p.velocity * deltaTime;

        p.life -= deltaTime * 0.03f;

        // Коллизия с поверхностью.
        const float ground =
            surfaceHeight(
                p.position.x,
                p.position.z
                );

        if (p.position.y <= ground)
        {
            const glm::vec3 hit(
                p.position.x,
                ground,
                p.position.z
                );

            if (rain)
            {
                // Поверхность намокает, у сильного удара — брызги.
                m_cover.addRain(hit.x, hit.z);

                if (-p.velocity.y > kSplashSpeed)
                    spawnSplash(hit);
            }
            else
            {
                // Снежинка остаётся на поверхности и утолщает слой.
                m_cover.addSnow(hit.x, hit.z, kSnowPerFlake);
            }

            resetParticle(p);
            continue;
        }

        // Если частица улетела за пределы сцены.
        if (p.position.y < -2.0f ||
            std::abs(p.position.x) > 16.0f ||
            std::abs(p.position.z) > 14.0f ||
            p.life <= 0.0f)
        {
            resetParticle(p);
        }
    }

    updateSplashes(deltaTime);

    m_cover.update(deltaTime, !rain);
}

void ParticleSystem::setType(ParticleType type)
{
    if (m_type == type)
        return;

    m_type = type;
    init();
}

void ParticleSystem::setWindAngle(float degrees)
{
    m_windAngle = degrees;

    const float radians =
        glm::radians(degrees);

    m_windDirection =
        glm::vec3(
            std::cos(radians),
            0.0f,
            std::sin(radians)
            );
}

void ParticleSystem::setWindStrength(float metersPerSecond)
{
    m_windStrength =
        std::max(0.0f, metersPerSecond);
}

float ParticleSystem::getWindStrength() const
{
    return m_windStrength;
}

void ParticleSystem::setIntensity(std::size_t count)
{
    m_activeCount =
        std::min(count, m_particles.size());
}

ParticleType ParticleSystem::getType() const
{
    return m_type;
}

float ParticleSystem::getWindAngle() const
{
    return m_windAngle;
}

const std::vector<Particle>&
ParticleSystem::getParticles() const
{
    return m_particles;
}

std::size_t ParticleSystem::getActiveCount() const
{
    return m_activeCount;
}

const std::vector<Particle>&
ParticleSystem::getSplashes() const
{
    return m_splashes;
}

const SurfaceCover& ParticleSystem::cover() const
{
    return m_cover;
}

void ParticleSystem::resetCover()
{
    m_cover.clear();
}
