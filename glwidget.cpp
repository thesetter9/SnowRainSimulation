#include "glwidget.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QVector3D>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "materials.h"
#include "scenedata.h"

namespace
{

using BenchClock = std::chrono::steady_clock;

double msSince(const BenchClock::time_point& t0)
{
    return std::chrono::duration<double, std::milli>(BenchClock::now() - t0).count();
}

// Направление НА источник света (пасмурное небо сверху-сбоку).
const QVector3D kLightDir = QVector3D(0.45f, 0.80f, 0.35f).normalized();

// Цвет неба и тумана зависит от погоды:
// при дожде темнее и холоднее, при снеге светлее.
QVector3D fogColorFor(ParticleType type)
{
    return (type == ParticleType::RAIN)
    ? QVector3D(0.13f, 0.16f, 0.21f)
    : QVector3D(0.24f, 0.28f, 0.34f);
}

float lightScaleFor(ParticleType type)
{
    return (type == ParticleType::RAIN) ? 0.75f : 1.0f;
}

QMatrix4x4 glmToQt(const glm::mat4& m)
{
    QMatrix4x4 result;
    const float* data = glm::value_ptr(m);

    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            result(row, col) = data[col * 4 + row];
        }
    }

    return result;
}

void pushVertex(
    std::vector<float>& v,
    const glm::vec3& p,
    const glm::vec3& c)
{
    v.push_back(p.x);
    v.push_back(p.y);
    v.push_back(p.z);
    v.push_back(c.r);
    v.push_back(c.g);
    v.push_back(c.b);
}

// Вершина сцены: позиция, оттенок и номер материала (7 float).
void pushSceneVertex(
    std::vector<float>& v,
    const glm::vec3& p,
    const glm::vec3& c,
    float material)
{
    v.push_back(p.x);
    v.push_back(p.y);
    v.push_back(p.z);
    v.push_back(c.r);
    v.push_back(c.g);
    v.push_back(c.b);
    v.push_back(material);
}

void pushSceneQuad(
    std::vector<float>& v,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c,
    const glm::vec3& d,
    const glm::vec3& color,
    float material)
{
    pushSceneVertex(v, a, color, material);
    pushSceneVertex(v, b, color, material);
    pushSceneVertex(v, c, color, material);

    pushSceneVertex(v, a, color, material);
    pushSceneVertex(v, c, color, material);
    pushSceneVertex(v, d, color, material);
}

void pushSceneBox(
    std::vector<float>& v,
    const SceneBox& box,
    float bottom)
{
    const float x0 = box.x0;
    const float x1 = box.x1;
    const float z0 = box.z0;
    const float z1 = box.z1;
    const float y0 = bottom;
    const float y1 = box.height;
    const float m = static_cast<float>(box.material);

    const glm::vec3 a(x0, y0, z0), b(x1, y0, z0);
    const glm::vec3 c(x1, y0, z1), d(x0, y0, z1);
    const glm::vec3 e(x0, y1, z0), f(x1, y1, z0);
    const glm::vec3 g(x1, y1, z1), h(x0, y1, z1);

    pushSceneQuad(v, e, f, g, h, box.color, m); // верх
    pushSceneQuad(v, a, b, f, e, box.color, m); // перед
    pushSceneQuad(v, d, c, g, h, box.color, m); // зад
    pushSceneQuad(v, a, d, h, e, box.color, m); // лево
    pushSceneQuad(v, b, c, g, f, box.color, m); // право
}

void setSceneUniforms(
    QOpenGLShaderProgram* shader,
    const glm::mat4& view,
    const glm::mat4& projection,
    int primitiveType,
    const QVector3D& fogColor,
    const glm::vec3& cameraPosition,
    float lightScale)
{
    shader->setUniformValue("model", glmToQt(glm::mat4(1.0f)));
    shader->setUniformValue("view", glmToQt(view));
    shader->setUniformValue("projection", glmToQt(projection));
    shader->setUniformValue("primitiveType", primitiveType);
    shader->setUniformValue("fogColor", fogColor);
    shader->setUniformValue("lightDir", kLightDir);
    shader->setUniformValue(
        "cameraPos",
        QVector3D(cameraPosition.x, cameraPosition.y, cameraPosition.z)
        );
    shader->setUniformValue("lightScale", lightScale);

    // Текстура покрытия лежит в слоте 0.
    shader->setUniformValue("cover", 0);
}

} // namespace

GLWidget::GLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_particleSystem(100000) // запас под бенчмарк; слайдер по-прежнему до 15000
    , m_shader(nullptr)
    , m_sceneVbo(QOpenGLBuffer::VertexBuffer)
    , m_particleVbo(QOpenGLBuffer::VertexBuffer)
    , m_splashVbo(QOpenGLBuffer::VertexBuffer)
    , m_width(1)
    , m_height(1)
    , m_fps(0.0f)
    , m_frameCounter(0)
    , m_fpsTime(0)
    , m_sceneVertexCount(0)
    , m_coverTexture(0)
    , m_cameraPosition(0.0f, 10.0f, 25.0f)
    , m_yaw(25.0f)
    , m_pitch(20.0f)
    , m_distance(26.0f)
{
    setMinimumSize(700, 500);

    m_timer.setInterval(16);

    connect(
        &m_timer,
        &QTimer::timeout,
        this,
        [this]()
        {
            if (m_benchmark.active() || m_benchmark.finished())
            {
                benchmarkStep();
                return;
            }

            updateParticles();
            update();
        }
        );

    m_timer.start();
}

GLWidget::~GLWidget()
{
    makeCurrent();

    delete m_shader;

    if (m_coverTexture != 0)
        glDeleteTextures(1, &m_coverTexture);

    m_particleVbo.destroy();
    m_particleVao.destroy();
    m_splashVbo.destroy();
    m_splashVao.destroy();
    m_sceneVbo.destroy();
    m_sceneVao.destroy();

    doneCurrent();
}

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Прозрачность нужна для мягких снежинок и полупрозрачных струй дождя.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initializeShaders();
    initializeScene();

    // Текстура покрытия поверхностей: R — снег, G — влажность.
    glGenTextures(1, &m_coverTexture);
    glBindTexture(GL_TEXTURE_2D, m_coverTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        SurfaceCover::kSize,
        SurfaceCover::kSize,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        m_particleSystem.cover().pixels()
        );

    m_elapsedTimer.start();
    m_fpsTime = 0;

    m_particleSystem.setIntensity(5000);
    m_particleSystem.setWindAngle(0.0f);
}

void GLWidget::initializeShaders()
{
    m_shader = new QOpenGLShaderProgram(this);

    // Внимание: в шейдерах только ASCII-символы, включая комментарии.
    // Часть драйверов (в том числе AMD) не принимает кириллицу в GLSL.
    const char* vertexShader = R"(
        #version 330 core

        layout(location = 0) in vec3 position;
        layout(location = 1) in vec3 color;
        layout(location = 2) in float material;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform float pointScale;

        out vec3 vertexColor;
        out vec3 worldPos;
        out float viewDist;
        flat out float materialId;

        void main()
        {
            vec4 world = model * vec4(position, 1.0);
            vec4 viewPos = view * world;

            gl_Position = projection * viewPos;

            vertexColor = color;
            worldPos = world.xyz;
            viewDist = length(viewPos.xyz);
            materialId = material;

            // Far particles are smaller, near ones are bigger.
            gl_PointSize = clamp(pointScale / viewDist, 2.0, 14.0);
        }
)";

    // primitiveType: 0 - surface, 1 - round point sprite, 2 - rain line
    const char* fragmentShader = R"(
        #version 330 core

        in vec3 vertexColor;
        in vec3 worldPos;
        in float viewDist;
        flat in float materialId;

        uniform int primitiveType;
        uniform vec3 fogColor;
        uniform vec3 lightDir;
        uniform vec3 cameraPos;
        uniform float lightScale;

        // Surface cover: R - snow depth, G - wetness.
        uniform sampler2D cover;

        // Per material optics: x - darkening when wet,
        // y - gloss, z - specular power. Must match kMaterialCount.
        uniform vec3 matOptics[5];

        out vec4 fragColor;

        float hash(vec3 p)
        {
            return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        }

        float hash2(vec2 p)
        {
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
        }

        float valueNoise(vec2 p)
        {
            vec2 i = floor(p);
            vec2 f = fract(p);
            f = f * f * (3.0 - 2.0 * f);

            float a = hash(vec3(i, 0.0));
            float b = hash(vec3(i + vec2(1.0, 0.0), 0.0));
            float c = hash(vec3(i + vec2(0.0, 1.0), 0.0));
            float d = hash(vec3(i + vec2(1.0, 1.0), 0.0));

            return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
        }

        // Fractal noise: four octaves of value noise, result in 0..1.
        float fbm(vec2 p)
        {
            float sum = 0.0;
            float amp = 0.5;

            for (int i = 0; i < 4; ++i)
            {
                sum += amp * valueNoise(p);
                p *= 2.03;
                amp *= 0.5;
            }

            return sum / 0.9375;
        }

        // Planar projection along the dominant axis of the normal.
        // Coordinates are world meters, so texture scale is the same
        // on every object.
        vec2 planarUV(vec3 p, vec3 n)
        {
            vec3 a = abs(n);

            if (a.y >= a.x && a.y >= a.z)
                return p.xz;

            if (a.x >= a.z)
                return p.zy;

            return p.xy;
        }

        // ---- Procedural albedo. base is the per-object tint. ----

        vec3 grassAlbedo(vec3 base, vec2 uv)
        {
            float patches = fbm(uv * 0.8);
            float blades = valueNoise(uv * vec2(35.0, 55.0));
            vec3 tone = mix(vec3(0.70, 0.85, 0.60), vec3(1.25, 1.20, 0.85), patches);
            return base * tone * (0.80 + 0.40 * blades);
        }

        vec3 stoneAlbedo(vec3 base, vec2 uv)
        {
            vec2 s = uv * 0.9;
            vec2 cell = floor(s);
            vec2 f = fract(s);
            float edge = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
            float joint = smoothstep(0.0, 0.05, edge);
            float tone = 0.80 + 0.40 * hash2(cell);
            float grain = 0.75 + 0.50 * fbm(uv * 9.0);
            return base * tone * grain * mix(0.45, 1.0, joint);
        }

        vec3 woodAlbedo(vec3 base, vec2 uv)
        {
            float plank = floor(uv.y * 2.5);
            float fy = fract(uv.y * 2.5);
            float gap = smoothstep(0.0, 0.06, min(fy, 1.0 - fy));
            float shift = hash2(vec2(plank, 3.0)) * 20.0;
            float grain = fbm(vec2(uv.x * 1.5 + shift, uv.y * 40.0));
            float tone = 0.80 + 0.35 * hash2(vec2(plank, 7.0));
            return base * tone * (0.65 + 0.70 * grain) * mix(0.40, 1.0, gap);
        }

        vec3 brickAlbedo(vec3 base, vec2 uv)
        {
            vec2 b = uv * vec2(2.5, 5.0);
            float row = floor(b.y);
            b.x += 0.5 * mod(row, 2.0);

            vec2 cell = floor(b);
            vec2 f = fract(b);

            float mortar = max(
                1.0 - smoothstep(0.0, 0.07, f.x),
                1.0 - smoothstep(0.0, 0.14, f.y));

            float tone = 0.80 + 0.40 * hash2(cell);
            vec3 brick = base * tone * (0.85 + 0.30 * valueNoise(uv * 25.0));

            return mix(brick, vec3(0.62, 0.60, 0.56), mortar);
        }

        vec3 asphaltAlbedo(vec3 base, vec2 uv)
        {
            float coarse = 0.80 + 0.40 * fbm(uv * 3.0);
            float speck = 0.80 + 0.40 * hash2(floor(uv * 70.0));
            return base * coarse * speck;
        }

        // Material ids: 0 grass, 1 stone, 2 wood, 3 brick, 4 asphalt.
        vec3 materialAlbedo(int mat, vec3 base, vec2 uv)
        {
            if (mat == 1)
                return stoneAlbedo(base, uv);

            if (mat == 2)
                return woodAlbedo(base, uv);

            if (mat == 3)
                return brickAlbedo(base, uv);

            if (mat == 4)
                return asphaltAlbedo(base, uv);

            return grassAlbedo(base, uv);
        }

        void main()
        {
            vec3 color = vertexColor;
            float alpha = 1.0;

            if (primitiveType == 1)
            {
                // Soft round sprite.
                vec2 p = gl_PointCoord * 2.0 - 1.0;
                float d = dot(p, p);
                if (d > 1.0)
                    discard;
                alpha = 1.0 - smoothstep(0.25, 1.0, d);
            }
            else if (primitiveType == 2)
            {
                alpha = 0.55;
            }
            else
            {
                // Flat normal from screen-space derivatives of world position,
                // so no normals are needed in the vertex buffer.
                vec3 n = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
                vec3 L = normalize(lightDir);
                vec3 V = normalize(cameraPos - worldPos);

                float diffuse = max(dot(n, L), 0.0);

                int mat = clamp(int(materialId + 0.5), 0, 4);
                vec3 optics = matOptics[mat];

                // Procedural texture; fine detail fades with distance
                // to avoid shimmering (poor man's mipmapping).
                vec2 uv = planarUV(worldPos, n);
                float lod = 1.0 - smoothstep(14.0, 45.0, viewDist);
                color = vertexColor * 0.95;
                if (lod > 0.001)
                    color = mix(color, materialAlbedo(mat, vertexColor, uv), lod);

                // Cover data for this point (world XZ -> texture UV).
                // 20.0 must match SurfaceCover::kHalfExtent.
                vec2 cuv = (worldPos.xz + 20.0) / 40.0;
                vec4 cov = texture(cover, cuv);
                float snow = cov.r;
                float wet = cov.g;

                // Wet surface is darker; how much depends on the material.
                color *= 1.0 - optics.x * wet;

                // Snow lies only on upward-facing surfaces,
                // with an uneven edge from noise.
                float up = smoothstep(0.55, 0.90, n.y);
                float breakup = valueNoise(worldPos.xz * 2.5);
                float snowMask = smoothstep(0.12, 0.50, snow + (breakup - 0.5) * 0.45) * up;
                color = mix(color, vec3(0.90, 0.94, 1.0), snowMask);

                // Lighting.
                color *= (0.35 + 0.75 * diffuse) * lightScale;

                // Wet glossy surfaces: sharp highlight plus a faint
                // reflection of the sky at grazing angles.
                float wetGloss = wet * optics.y * (1.0 - snowMask);
                float spec = pow(max(dot(n, normalize(L + V)), 0.0), optics.z);
                float fresnel = pow(1.0 - max(dot(n, V), 0.0), 3.0);

                color += vec3(spec * wetGloss * 0.8);
                color = mix(color, fogColor * 1.5, clamp(fresnel * wetGloss * 0.6, 0.0, 0.6));
            }

            // Fog: far things fade into the sky color.
            float fog = 1.0 - exp(-viewDist * viewDist * 0.0007);
            color = mix(color, fogColor, clamp(fog, 0.0, 1.0));

            fragColor = vec4(color, alpha);
        }
)";

    if (!m_shader->addShaderFromSourceCode(
            QOpenGLShader::Vertex,
            vertexShader))
    {
        qDebug() << "Vertex shader error:"
                 << m_shader->log();
    }

    if (!m_shader->addShaderFromSourceCode(
            QOpenGLShader::Fragment,
            fragmentShader))
    {
        qDebug() << "Fragment shader error:"
                 << m_shader->log();
    }

    if (!m_shader->link())
    {
        qDebug() << "Shader link error:"
                 << m_shader->log();
    }
}

// Строит геометрию сцены (рельеф + объекты) и загружает её в GPU.
// Функцию можно вызывать повторно: например, при смене числа объектов.
void GLWidget::initializeScene()
{
    std::vector<float> vertices;

    // Базовый рельеф.
    const int N = 40;
    const float size = SurfaceCover::kHalfExtent;
    const glm::vec3 grass(0.14f, 0.34f, 0.17f);

    for (int z = 0; z < N; ++z)
    {
        for (int x = 0; x < N; ++x)
        {
            const float x0 = -size + (2.0f * size * x) / N;
            const float x1 = -size + (2.0f * size * (x + 1)) / N;
            const float z0 = -size + (2.0f * size * z) / N;
            const float z1 = -size + (2.0f * size * (z + 1)) / N;

            const glm::vec3 a(x0, terrainBaseHeight(x0, z0), z0);
            const glm::vec3 b(x1, terrainBaseHeight(x1, z0), z0);
            const glm::vec3 c(x1, terrainBaseHeight(x1, z1), z1);
            const glm::vec3 d(x0, terrainBaseHeight(x0, z1), z1);

            pushSceneQuad(vertices, a, b, c, d, grass,
                          static_cast<float>(Material::Grass));
        }
    }

    // Объекты сцены берутся из ParticleSystem — тот же список видит физика.
    // Низ опущен ниже рельефа, чтобы не было просветов.
    for (const SceneBox& box : m_particleSystem.boxes())
        pushSceneBox(vertices, box, -0.3f);

    m_sceneVertexCount =
        static_cast<int>(vertices.size() / 7);

    if (!m_sceneVao.isCreated())
        m_sceneVao.create();
    m_sceneVao.bind();

    if (!m_sceneVbo.isCreated())
        m_sceneVbo.create();
    m_sceneVbo.bind();

    m_sceneVbo.allocate(
        vertices.data(),
        static_cast<int>(vertices.size() * sizeof(float))
        );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        7 * sizeof(float),
        nullptr
        );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        7 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
        );

    // Номер материала.
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE,
        7 * sizeof(float),
        reinterpret_cast<void*>(6 * sizeof(float))
        );

    m_sceneVao.release();
}

void GLWidget::resizeGL(int width, int height)
{
    m_width = std::max(width, 1);
    m_height = std::max(height, 1);

    glViewport(0, 0, m_width, m_height);
}

void GLWidget::updateParticles()
{
    static qint64 lastTime =
        m_elapsedTimer.elapsed();

    const qint64 now =
        m_elapsedTimer.elapsed();

    float deltaTime =
        static_cast<float>(now - lastTime) / 1000.0f;

    lastTime = now;
    deltaTime = std::min(deltaTime, 0.033f);

    m_particleSystem.update(deltaTime);

    // FPS.
    ++m_frameCounter;

    if (now - m_fpsTime >= 1000)
    {
        m_fps =
            static_cast<float>(m_frameCounter) * 1000.0f /
            static_cast<float>(now - m_fpsTime);

        m_frameCounter = 0;
        m_fpsTime = now;
    }
}

void GLWidget::uploadVertices(
    QOpenGLVertexArrayObject& vao,
    QOpenGLBuffer& vbo,
    const std::vector<float>& vertices)
{
    if (!vao.isCreated())
        vao.create();

    if (!vbo.isCreated())
        vbo.create();

    vao.bind();
    vbo.bind();

    vbo.allocate(
        vertices.data(),
        static_cast<int>(vertices.size() * sizeof(float))
        );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        nullptr
        );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        6 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
        );

    vao.release();
}

void GLWidget::uploadParticles()
{
    const auto& particles =
        m_particleSystem.getParticles();

    const std::size_t count =
        m_particleSystem.getActiveCount();

    m_particleVertices.clear();

    const bool rain =
        m_particleSystem.getType() == ParticleType::RAIN;

    if (rain)
    {
        m_particleVertices.reserve(count * 2 * 6);

        for (std::size_t i = 0; i < count; ++i)
        {
            const Particle& p = particles[i];

            const glm::vec3 direction =
                glm::normalize(p.velocity);

            const glm::vec3 tail =
                p.position - direction * 0.45f;

            // Голова капли (ярче).
            pushVertex(
                m_particleVertices,
                p.position,
                glm::vec3(0.75f, 0.85f, 1.0f)
                );

            // Хвост (темнее).
            pushVertex(
                m_particleVertices,
                tail,
                glm::vec3(0.40f, 0.55f, 0.80f)
                );
        }
    }
    else
    {
        m_particleVertices.reserve(count * 6);

        for (std::size_t i = 0; i < count; ++i)
        {
            pushVertex(
                m_particleVertices,
                particles[i].position,
                glm::vec3(0.95f, 0.98f, 1.0f)
                );
        }
    }

    uploadVertices(m_particleVao, m_particleVbo, m_particleVertices);
}

void GLWidget::uploadSplashes()
{
    m_splashVertices.clear();

    for (const Particle& s : m_particleSystem.getSplashes())
    {
        if (s.life <= 0.0f)
            continue;

        pushVertex(
            m_splashVertices,
            s.position,
            glm::vec3(0.78f, 0.88f, 1.0f)
            );
    }

    if (!m_splashVertices.empty())
        uploadVertices(m_splashVao, m_splashVbo, m_splashVertices);
}

void GLWidget::uploadCover()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_coverTexture);

    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        SurfaceCover::kSize,
        SurfaceCover::kSize,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        m_particleSystem.cover().pixels()
        );
}

void GLWidget::paintGL()
{
    const BenchClock::time_point paintStart = BenchClock::now();

    // Цвет неба зависит от типа осадков.
    const QVector3D fog =
        fogColorFor(m_particleSystem.getType());

    glClearColor(fog.x(), fog.y(), fog.z(), 1.0f);

    glClear(
        GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT
        );

    // Орбитальная камера вокруг центра сцены.
    const float yaw = glm::radians(m_yaw);
    const float pitch = glm::radians(m_pitch);

    const glm::vec3 target(0.0f, 2.5f, 0.0f);

    m_cameraPosition =
        target +
        m_distance *
            glm::vec3(
                std::cos(pitch) * std::sin(yaw),
                std::sin(pitch),
                std::cos(pitch) * std::cos(yaw)
                );

    const glm::mat4 view =
        glm::lookAt(
            m_cameraPosition,
            target,
            glm::vec3(0.0f, 1.0f, 0.0f)
            );

    const glm::mat4 projection =
        glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(m_width) /
                static_cast<float>(m_height),
            0.1f,
            120.0f
            );

    uploadCover();

    drawTerrain(view, projection);

    uploadParticles();
    uploadSplashes();

    drawParticles(view, projection);

    if (m_benchmark.active())
    {
        // Ждём, пока GPU реально закончит кадр, иначе замер покажет
        // только время постановки команд в очередь.
        glFinish();

        const BenchClock::time_point now = BenchClock::now();
        const double paintMs =
            std::chrono::duration<double, std::milli>(now - paintStart).count();
        const double frameMs =
            std::chrono::duration<double, std::milli>(now - m_benchLastFrame).count();
        m_benchLastFrame = now;

        // Новую конфигурацию применяем не здесь (внутри paintGL нельзя
        // вызывать makeCurrent), а на следующем тике таймера.
        if (m_benchmark.addFrame(m_benchUpdateMs, paintMs, frameMs))
            m_benchApply = true;

        m_benchPainted = true;
    }
}

void GLWidget::drawTerrain(
    const glm::mat4& view,
    const glm::mat4& projection)
{
    const ParticleType type =
        m_particleSystem.getType();

    m_shader->bind();

    setSceneUniforms(
        m_shader,
        view,
        projection,
        0,
        fogColorFor(type),
        m_cameraPosition,
        lightScaleFor(type)
        );

    m_shader->setUniformValue("pointScale", 0.0f);

    // Оптические свойства материалов: затемнение при намокании,
    // блеск и острота блика.
    QVector3D optics[kMaterialCount];
    for (int i = 0; i < kMaterialCount; ++i)
    {
        const MaterialProps& props =
            materialProps(static_cast<Material>(i));
        optics[i] = QVector3D(props.wetDarken, props.gloss, props.specPower);
    }
    m_shader->setUniformValueArray("matOptics", optics, kMaterialCount);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_coverTexture);

    m_sceneVao.bind();

    glDrawArrays(
        GL_TRIANGLES,
        0,
        m_sceneVertexCount
        );

    m_sceneVao.release();
    m_shader->release();
}

void GLWidget::drawParticles(
    const glm::mat4& view,
    const glm::mat4& projection)
{
    const ParticleType type =
        m_particleSystem.getType();

    const bool rain =
        type == ParticleType::RAIN;

    const QVector3D fog =
        fogColorFor(type);

    m_shader->bind();

    setSceneUniforms(
        m_shader,
        view,
        projection,
        rain ? 2 : 1,
        fog,
        m_cameraPosition,
        lightScaleFor(type)
        );

    m_shader->setUniformValue("pointScale", 90.0f);

    // Частицы не пишут в буфер глубины: полупрозрачные частицы
    // не закрывают друг друга, но корректно скрываются за поверхностью.
    glDepthMask(GL_FALSE);

    m_particleVao.bind();

    const int count =
        static_cast<int>(m_particleSystem.getActiveCount());

    if (rain)
    {
        glLineWidth(1.5f);

        glDrawArrays(
            GL_LINES,
            0,
            count * 2
            );
    }
    else
    {
        glDrawArrays(
            GL_POINTS,
            0,
            count
            );
    }

    m_particleVao.release();

    // Брызги: мелкие точки, рисуются как спрайты.
    if (!m_splashVertices.empty())
    {
        setSceneUniforms(
            m_shader,
            view,
            projection,
            1,
            fog,
            m_cameraPosition,
            lightScaleFor(type)
            );

        m_shader->setUniformValue("pointScale", 45.0f);

        m_splashVao.bind();

        glDrawArrays(
            GL_POINTS,
            0,
            static_cast<int>(m_splashVertices.size() / 6)
            );

        m_splashVao.release();
    }

    glDepthMask(GL_TRUE);

    m_shader->release();
}

void GLWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
}

void GLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;

    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    m_yaw -= delta.x() * 0.4f;
    m_pitch = std::clamp(m_pitch + delta.y() * 0.4f, 3.0f, 85.0f);
}

void GLWidget::wheelEvent(QWheelEvent* event)
{
    m_distance = std::clamp(
        m_distance - event->angleDelta().y() * 0.01f,
        8.0f,
        45.0f
        );
}

void GLWidget::setParticleType(ParticleType type)
{
    m_particleSystem.setType(type);
}

void GLWidget::setIntensity(int value)
{
    // 500 ... 15000
    m_particleSystem.setIntensity(
        static_cast<std::size_t>(
            std::clamp(value, 500, 15000)
            )
        );
}

void GLWidget::setWindAngle(int angle)
{
    m_particleSystem.setWindAngle(
        static_cast<float>(angle)
        );
}

void GLWidget::setWindStrength(int tenths)
{
    m_particleSystem.setWindStrength(
        static_cast<float>(tenths) / 10.0f
        );
}

void GLWidget::setCollisionAlgorithm(CollisionAlgorithm algorithm)
{
    m_particleSystem.setCollisionAlgorithm(algorithm);
}

// count = 0 — базовая сцена из пяти объектов, иначе случайная сцена.
void GLWidget::setObjectCount(int count)
{
    m_objectCount = count;

    // До инициализации OpenGL геометрию строить нельзя:
    // initializeGL() сам возьмёт актуальный список объектов.
    if (!isValid())
    {
        m_particleSystem.setObjectCount(count);
        return;
    }

    makeCurrent();
    m_particleSystem.setObjectCount(count);
    initializeScene();
    doneCurrent();
}

void GLWidget::resetCover()
{
    m_particleSystem.resetCover();
}

float GLWidget::fps() const
{
    return m_fps;
}

// ---------------------------------------------------------------------
// Бенчмарк
// ---------------------------------------------------------------------

void GLWidget::startBenchmark()
{
    if (m_benchmark.active())
        return;

    // Запоминаем состояние, чтобы вернуть его после замеров.
    m_savedIntensity = m_particleSystem.getActiveCount();
    m_savedAlgorithm = m_particleSystem.collisionAlgorithm();
    m_savedObjects = m_objectCount;
    m_savedYaw = m_yaw;
    m_savedPitch = m_pitch;
    m_savedDistance = m_distance;

    // Камера фиксирована: от неё зависит объём отрисовки.
    m_yaw = 25.0f;
    m_pitch = 20.0f;
    m_distance = 26.0f;

    m_particleSystem.resetCover();

    const bool rain = m_particleSystem.getType() == ParticleType::RAIN;
    m_benchmark.start(rain ? "rain" : "snow"); // ~2 с на замер чистых запросов

    // Без ограничения в 60 кадров/с: кадры идут, как только готовы.
    m_timer.setInterval(0);

    m_benchLastFrame = BenchClock::now();
    m_benchPainted = true;
    m_benchApply = true; // первая конфигурация применяется на первом тике
}

void GLWidget::stopBenchmark()
{
    if (!m_benchmark.active())
        return;

    m_benchmark.stop();
    restoreAfterBenchmark(false);
}

bool GLWidget::benchmarkRunning() const
{
    return m_benchmark.active();
}

int GLWidget::benchmarkIndex() const
{
    return m_benchmark.index();
}

int GLWidget::benchmarkTotal() const
{
    return m_benchmark.total();
}

void GLWidget::applyBenchmarkConfig()
{
    const BenchmarkConfig& cfg = m_benchmark.config();

    // Число объектов меняем только при необходимости: это пересборка сцены.
    if (cfg.objects != m_objectCount)
        setObjectCount(cfg.objects);

    m_particleSystem.setCollisionAlgorithm(cfg.algorithm);

    // Напрямую в систему частиц: слайдер ограничен 15000, а тут нужно больше.
    m_particleSystem.setIntensity(static_cast<std::size_t>(cfg.particles));
}

// Один тик бенчмарка: фиксированный шаг физики и замер её времени.
void GLWidget::benchmarkStep()
{
    if (m_benchmark.finished())
    {
        finishBenchmark();
        return;
    }

    // Пока предыдущий кадр не отрисован, следующий шаг не делаем:
    // иначе физики станет больше, чем кадров, и замеры разойдутся.
    if (!m_benchPainted)
        return;

    if (m_benchApply)
    {
        applyBenchmarkConfig();
        m_benchApply = false;
    }

    // Фиксированный dt: нагрузка в каждом прогоне одинакова.
    const BenchClock::time_point t0 = BenchClock::now();
    m_particleSystem.update(1.0f / 60.0f);
    m_benchUpdateMs = msSince(t0);

    m_benchPainted = false;
    update();
}

void GLWidget::finishBenchmark()
{
    const std::string csv = m_benchmark.toCsv();
    m_benchmark.stop();
    restoreAfterBenchmark(true, csv);
}

void GLWidget::restoreAfterBenchmark(bool completed, const std::string& csv)
{
    m_timer.setInterval(16);

    if (m_savedObjects != m_objectCount)
        setObjectCount(m_savedObjects);

    m_particleSystem.setCollisionAlgorithm(m_savedAlgorithm);
    m_particleSystem.setIntensity(m_savedIntensity);
    m_particleSystem.resetCover();

    m_yaw = m_savedYaw;
    m_pitch = m_savedPitch;
    m_distance = m_savedDistance;

    if (onBenchmarkFinished)
        onBenchmarkFinished(completed, csv);
}
