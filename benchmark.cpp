#include "benchmark.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <locale>
#include <random>
#include <sstream>

namespace
{

// Сетка экспериментов. Можно менять под свои нужды.
const int kParticleCounts[] = { 1000, 5000, 15000, 50000, 100000 };
const int kObjectCounts[]   = { 0, 200, 1000, 3000 }; // 0 = базовая сцена
const CollisionAlgorithm kAlgorithms[] =
    {
        CollisionAlgorithm::BruteForce,
        CollisionAlgorithm::HeightMap,
        CollisionAlgorithm::UniformGrid
};

// ASCII-идентификаторы алгоритмов для CSV.
const char* algorithmId(CollisionAlgorithm a)
{
    switch (a)
    {
    case CollisionAlgorithm::HeightMap:   return "height_map";
    case CollisionAlgorithm::UniformGrid: return "uniform_grid";
    case CollisionAlgorithm::BruteForce:
    default:                              return "brute_force";
    }
}

int actualObjects(int objects)
{
    return objects <= 0 ? static_cast<int>(defaultSceneBoxes().size()) : objects;
}

} // namespace

double Benchmark::measureQueryNs(CollisionAlgorithm algorithm, int objects)
{
    using clock = std::chrono::steady_clock;

    const BoxList boxes =
        objects <= 0 ? defaultSceneBoxes() : makeRandomBoxes(objects);
    const std::unique_ptr<ICollision> collision =
        makeCollision(algorithm, boxes);

    // Случайные точки заранее, чтобы генератор не попал в замер.
    const int kBatch = 20000;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> uni(-19.5f, 19.5f);
    std::vector<float> xs(kBatch), zs(kBatch);
    for (int i = 0; i < kBatch; ++i)
    {
        xs[i] = uni(rng);
        zs[i] = uni(rng);
    }

    float sink = 0.0f;
    long long done = 0;
    const auto t0 = clock::now();
    double elapsedNs = 0.0;

    // Крутим батчи, пока не наберётся 150 мс.
    do
    {
        for (int i = 0; i < kBatch; ++i)
            sink += collision->heightAt(xs[i], zs[i]);
        done += kBatch;
        elapsedNs = std::chrono::duration<double, std::nano>(clock::now() - t0).count();
    }
    while (elapsedNs < 150e6);

    // Не даём компилятору выбросить цикл.
    volatile float keep = sink;
    (void)keep;

    return elapsedNs / static_cast<double>(done);
}

void Benchmark::start(const std::string& weatherId)
{
    m_weather = weatherId;
    m_configs.clear();
    m_queryNs.clear();
    m_rows.clear();

    for (int objects : kObjectCounts)
    {
        for (CollisionAlgorithm algorithm : kAlgorithms)
        {
            const double ns = measureQueryNs(algorithm, objects);
            for (int particles : kParticleCounts)
            {
                m_configs.push_back({ particles, algorithm, objects });
                m_queryNs.push_back(ns);
            }
        }
    }

    m_index = 0;
    m_active = true;
    m_finished = false;
    resetPhase();
}

void Benchmark::stop()
{
    m_active = false;
    m_finished = false;
}

const BenchmarkConfig& Benchmark::config() const
{
    return m_configs[std::min<std::size_t>(m_index, m_configs.size() - 1)];
}

void Benchmark::resetPhase()
{
    m_warmup = true;
    m_frames = 0;
    m_sumFrame = 0.0;
    m_sumUpdate = 0.0;
    m_sumPaint = 0.0;
    m_samples.clear();
}

bool Benchmark::addFrame(double updateMs, double paintMs, double frameMs)
{
    if (!m_active)
        return false;

    // Прогрев: JIT драйвера, заполнение кэшей, пересборка структур.
    if (m_warmup)
    {
        ++m_frames;
        m_sumFrame += frameMs;
        if (m_frames >= kWarmupFrames && m_sumFrame >= kWarmupMs)
        {
            m_warmup = false;
            m_frames = 0;
            m_sumFrame = 0.0;
        }
        return false;
    }

    ++m_frames;
    m_sumFrame += frameMs;
    m_sumUpdate += updateMs;
    m_sumPaint += paintMs;
    m_samples.push_back(frameMs);

    if (m_frames < kMeasureFrames || m_sumFrame < kMeasureMs)
        return false;

    // Конфигурация измерена: считаем статистику.
    std::vector<double> sorted = m_samples;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t p95 =
        static_cast<std::size_t>(0.95 * static_cast<double>(sorted.size() - 1));

    BenchmarkRow row;
    row.config = m_configs[m_index];
    row.frames = m_frames;
    row.updateMs = m_sumUpdate / m_frames;
    row.paintMs = m_sumPaint / m_frames;
    row.frameMs = m_sumFrame / m_frames;
    row.p95FrameMs = sorted[p95];
    row.fps = 1000.0 / row.frameMs;
    row.nsPerQuery = m_queryNs[m_index];
    m_rows.push_back(row);

    ++m_index;
    if (m_index >= total())
    {
        m_active = false;
        m_finished = true;
        return false;
    }

    resetPhase();
    return true;
}

std::string Benchmark::toCsv() const
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(3);

    out << "weather;particles;algorithm;objects;frames;"
           "update_ms;paint_ms;frame_ms;p95_frame_ms;fps;ns_per_query\n";

    for (const BenchmarkRow& r : m_rows)
    {
        out << m_weather << ';'
            << r.config.particles << ';'
            << algorithmId(r.config.algorithm) << ';'
            << actualObjects(r.config.objects) << ';'
            << r.frames << ';'
            << r.updateMs << ';'
            << r.paintMs << ';'
            << r.frameMs << ';'
            << r.p95FrameMs << ';'
            << r.fps << ';'
            << r.nsPerQuery << '\n';
    }

    return out.str();
}
