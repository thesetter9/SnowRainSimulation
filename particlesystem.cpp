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

// ---- Модель падения частицы в воздухе (общая для update() и спавна) ----
//
// Частица стремится к скорости воздуха: a = k * (v_target - v), где
// v_target = ветер * windResponse + (0, -terminal, 0).

// Предельная скорость падения: у снежинок зависит от размера.
float terminalSpeed(bool rain, float size)
{
    return rain ? 9.0f : (0.8f + size * 6.0f);
}

// 1/с: как быстро частица «подстраивается» под воздух.
float relaxRate(bool rain)
{
    return rain ? 1.5f : 4.0f;
}

// Доля скорости ветра, которую набирает частица
// (эмпирический коэффициент увлечения).
float windResponse(bool rain)
{
    return rain ? 1.2f : 0.35f;
}

// На сколько метров ветер сносит частицу по горизонтали, пока она
// падает с высоты height. Разгон от нулевой горизонтальной скорости
// до u = ветер * windResponse с постоянной времени 1/k даёт
//     drift = u * (T - (1 - exp(-k*T)) / k),   T = height / terminal.
float windDrift(bool rain, float size, float height, float windSpeed)
{
    const float k = relaxRate(rain);
    const float fallTime = height / terminalSpeed(rain, size);
    const float u = windSpeed * windResponse(rain);

    return u * (fallTime - (1.0f - std::exp(-k * fallTime)) / k);
}

// Высота падения до средней поверхности: частицы, садящиеся на высокие
// объекты, летят короче, а значит ветер сносит их меньше.
float fallHeight(float startHeight, float meanSurface)
{
    return std::max(startHeight - meanSurface, 0.5f);
}

// Минимальная область, над которой выпадают осадки (центр в начале
// координат). Для больших сцен она расширяется (см. setObjectCount).
constexpr float kRainHalfX = 14.0f;
constexpr float kRainHalfZ = 12.0f;

// Зазор между краем крайнего объекта и краем области осадков, м.
constexpr float kRainMargin = 2.0f;

// Карта материалов под сетку покрытия для заданного набора объектов.
std::vector<unsigned char> materialMapFor(const BoxList& boxes)
{
    return buildMaterialMap(
        boxes,
        SurfaceCover::kSize,
        SurfaceCover::kHalfExtent
        );
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

    m_precipHalfX = kRainHalfX;
    m_precipHalfZ = kRainHalfZ;
    m_meanSurfaceHeight = 0.0f;

    m_boxes = defaultSceneBoxes();
    m_collision = makeCollision(m_algorithm, m_boxes);
    m_cover.setMaterialMap(materialMapFor(m_boxes));

    init();
}

void ParticleSystem::init()
{
    const bool rain = (m_type == ParticleType::RAIN);

    for (Particle& particle : m_particles)
    {
        resetParticle(particle);

        // Разбрасываем частицы по высоте,
        // чтобы экран сразу был заполнен осадками.
        // Точка приземления при этом сохраняется: меняем высоту и
        // пересчитываем смещение против ветра.
        const float oldHeight = particle.position.y;
        const float newHeight = randomFloat(3.0f, 18.0f);

        const float shift =
            windDrift(rain, particle.size,
                      fallHeight(oldHeight, m_meanSurfaceHeight), m_windStrength) -
            windDrift(rain, particle.size,
                      fallHeight(newHeight, m_meanSurfaceHeight), m_windStrength);

        particle.position.x += m_windDirection.x * shift;
        particle.position.z += m_windDirection.z * shift;
        particle.position.y = newHeight;
    }
}

void ParticleSystem::resetParticle(Particle& particle)
{
    const bool rain = (m_type == ParticleType::RAIN);

    // Куда частица должна упасть: равномерно по области осадков.
    const float landX = randomFloat(-m_precipHalfX, m_precipHalfX);
    const float landZ = randomFloat(-m_precipHalfZ, m_precipHalfZ);
    const float height = randomFloat(13.0f, 20.0f);

    particle.life = 1.0f;

    if (rain)
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

    // Пока частица падает, ветер сносит её. Чтобы осадки ложились на
    // всю область, а не только на её подветренную часть, стартуем
    // с запасом против ветра на величину ожидаемого сноса.
    const float drift =
        windDrift(rain, particle.size,
                  fallHeight(height, m_meanSurfaceHeight), m_windStrength);

    particle.position = glm::vec3(
        landX - m_windDirection.x * drift,
        height,
        landZ - m_windDirection.z * drift
        );
}

float ParticleSystem::surfaceHeight(float x, float z) const
{
    // Поиск поверхности делегируется выбранному алгоритму коллизий.
    return m_collision->heightAt(x, z);
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
    const float k = relaxRate(rain);
    const float response = windResponse(rain);

    // Границы «живой» области: сцена плюс запас на снос ветром (с учётом
    // порывов и самых лёгких частиц). За ними частица пересоздаётся.
    // Запас нужен, чтобы частицы, стартующие против ветра, не удалялись
    // сразу после появления.
    const float driftMargin =
        std::min(
            windDrift(rain, rain ? 0.07f : 0.08f, 20.0f, m_windStrength * 1.6f),
            25.0f
            );
    const float liveLimit = 22.0f + driftMargin;

    for (std::size_t i = 0; i < m_activeCount; ++i)
    {
        Particle& p = m_particles[i];

        const float terminal =
            terminalSpeed(rain, p.size);

        glm::vec3 target =
            wind * response +
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
                // Материал решает, будут ли они: по траве капли почти
                // не отскакивают, по асфальту и камню брызги заметнее.
                m_cover.addRain(hit.x, hit.z);

                if (-p.velocity.y > kSplashSpeed)
                {
                    const Material material =
                        m_cover.materialAt(hit.x, hit.z);

                    if (randomFloat(0.0f, 1.0f) < materialProps(material).splash)
                        spawnSplash(hit);
                }
            }
            else
            {
                // Снежинка остаётся на поверхности и утолщает слой
                // (сколько удержится, зависит от материала).
                m_cover.addSnow(hit.x, hit.z, kSnowPerFlake);
            }

            resetParticle(p);
            continue;
        }

        // Если частица улетела за пределы сцены.
        if (p.position.y < -2.0f ||
            std::abs(p.position.x) > liveLimit ||
            std::abs(p.position.z) > liveLimit ||
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

// ---------------------------------------------------------------------
// Коллизии и состав сцены
// ---------------------------------------------------------------------

void ParticleSystem::setCollisionAlgorithm(CollisionAlgorithm algorithm)
{
    m_algorithm = algorithm;
    m_collision = makeCollision(m_algorithm, m_boxes);
}

CollisionAlgorithm ParticleSystem::collisionAlgorithm() const
{
    return m_algorithm;
}

// count <= 0 — базовая сцена, иначе случайная сцена из count объектов.
void ParticleSystem::setObjectCount(int count)
{
    m_boxes = (count <= 0)
    ? defaultSceneBoxes()
    : makeRandomBoxes(count);

    m_collision = makeCollision(m_algorithm, m_boxes);

    // Новая карта материалов; покрытие при этом сбрасывается.
    m_cover.setMaterialMap(materialMapFor(m_boxes));

    // Область осадков должна накрывать всю сцену: иначе внешние объекты
    // большой сцены останутся сухими. Для базовой сцены область прежняя.
    float maxX = 0.0f;
    float maxZ = 0.0f;

    for (const SceneBox& b : m_boxes)
    {
        maxX = std::max({ maxX, std::abs(b.x0), std::abs(b.x1) });
        maxZ = std::max({ maxZ, std::abs(b.z0), std::abs(b.z1) });
    }

    const float limit = SurfaceCover::kHalfExtent - kRainMargin;

    m_precipHalfX = std::min(limit, std::max(kRainHalfX, maxX + kRainMargin));
    m_precipHalfZ = std::min(limit, std::max(kRainHalfZ, maxZ + kRainMargin));

    // Средняя высота поверхности над областью осадков (выборка по сетке):
    // нужна, чтобы корректно оценивать снос ветром при падении.
    const int samples = 24;
    float sum = 0.0f;

    for (int iz = 0; iz < samples; ++iz)
    {
        for (int ix = 0; ix < samples; ++ix)
        {
            const float x =
                -m_precipHalfX + (static_cast<float>(ix) + 0.5f) *
                                     2.0f * m_precipHalfX / static_cast<float>(samples);
            const float z =
                -m_precipHalfZ + (static_cast<float>(iz) + 0.5f) *
                                     2.0f * m_precipHalfZ / static_cast<float>(samples);

            sum += surfaceHeight(x, z);
        }
    }

    m_meanSurfaceHeight = sum / static_cast<float>(samples * samples);

    // Перераспределяем частицы по новой области.
    init();
}

const BoxList& ParticleSystem::boxes() const
{
    return m_boxes;
}
