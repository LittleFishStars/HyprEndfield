#include "sourcedial.hpp"
#include "centerlogo.hpp"
#include <QImageReader>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QQuickOpenGLUtils>
#include <array>
#include <memory>

namespace
{
QByteArray resource(const QString& name)
{
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot read source dial resource: %s", qPrintable(name));
    return file.readAll();
}
struct Mesh
{
    GLuint vertices = 0, indices = 0;
    int count = 0;
};
struct Draw
{
    QString id, mesh, material, texture;
    bool isMesh = false;
    float opacity = 1;
    QMatrix4x4 projection;
    QVector4D color;
    std::array<float, 80> uniforms { };
};
class DialRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLExtraFunctions
{
public:
    DialRenderer()
    {
        catalog = QJsonDocument::fromJson(resource(":/assets/watch-dial/dial.json")).object();
        for (const auto& v : catalog.value("draws").toArray())
        {
            const auto o = v.toObject();
            Draw d;
            d.id = o.value("id").toString();
            d.mesh = o.value("mesh").toString();
            d.material = o.value("material").toString();
            d.texture = o.value("texture").toString();
            d.isMesh = o.value("isMesh").toBool();
            d.opacity = o.value("opacity").toDouble(1);
            draws.append(d);
        }
        clock.start();
    }
    ~DialRenderer() override
    {
        if (!initialized)
            return;
        for (const auto& m : meshes)
        {
            glDeleteBuffers(1, &m.vertices);
            glDeleteBuffers(1, &m.indices);
        }
        for (auto texture : textures)
            glDeleteTextures(1, &texture);
        glDeleteVertexArrays(1, &vao);
    }
    void synchronize(QQuickFramebufferObject* item) override
    {
        const auto* dial = static_cast<SourceDial*>(item);
        if (logo.cacheKey() != dial->logoImage().cacheKey()) {
            logo = dial->logoImage(); logoDirty = true;
        }
        if (!dial->animation)
            return;
        const auto p = dial->pose();
        const int phase = p.value("phase").toInt();
        const double time = p.value("time").toDouble(), ambient = p.value("ambient", -1).toDouble();
        const auto gyro = p.value("gyro").value<QQuaternion>();
        const auto materials = catalog.value("materials").toObject();
        for (auto& d : draws)
        {
            d.projection = dial->animation->projectSourceGeometry(d.id, phase, time, gyro, ambient);
            d.color = dial->animation->sourceGraphicColor(d.id, phase, time, d.isMesh);
            d.color.setW(d.color.w() * d.opacity);
            if (!logo.isNull()) {
                if (d.id.endsWith(":-8595532247694873385")) d.color.setW(0);
                if (d.id.endsWith(":469684920047201495")) d.projection.scale(float(65. * logo.width() / logo.height() / 300.), 1, 1);
            }
            const auto material = materials.value(d.material).toObject();
            const auto u = material.value("uniforms").toArray();
            for (int i = 0; i < 80; ++i)
                d.uniforms[i] = float(u[i].toDouble());
            // The six watchline meshes animate their original UV window.
            if (d.isMesh)
            {
                const QStringList axes { "x", "y", "z", "w" };
                for (int i = 0; i < 4; ++i)
                    d.uniforms[8 + i] = float(dial->animation->sourceProperty(
                        d.id, phase, time, "material._MainTex_ST." + axes[i], d.uniforms[8 + i]));
            }
        }
    }
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setInternalTextureFormat(GL_RGBA8);
        return new QOpenGLFramebufferObject(size, format);
    }
    void render() override
    {
        if (!initialized)
            initialize();
        if (logoDirty) {
            if (textures.contains("__logo")) { const auto texture = textures.take("__logo"); glDeleteTextures(1, &texture); }
            if (!logo.isNull()) {
                GLuint texture; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                const auto pixels = logo.flipped(Qt::Vertical);
#else
                const auto pixels = logo.mirrored(false, true);
#endif
                glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, pixels.width(), pixels.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.constBits());
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                textures.insert("__logo", texture);
            }
            logoDirty = false;
        }
        const QSize size = framebufferObject()->size();
        if (!scene || scene->size() != size)
        {
            QOpenGLFramebufferObjectFormat f;
            f.setInternalTextureFormat(GL_RGBA16F);
            f.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            f.setSamples(4);
            scene = std::make_unique<QOpenGLFramebufferObject>(size, f);
            f.setSamples(0);
            f.setAttachment(QOpenGLFramebufferObject::NoAttachment);
            resolved = std::make_unique<QOpenGLFramebufferObject>(size, f);
        }
        scene->bind();
        glViewport(0, 0, size.width(), size.height());
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilMask(255);
        glClearColor(0, 0, 0, 0);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_BLEND);
        glEnable(GL_STENCIL_TEST);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBindVertexArray(vao);
        program.bind();
        program.setUniformValue("mainTexture", 0);
        program.setUniformValue("maskTexture", 1);
        program.setUniformValue("dissolveTexture", 2);
        program.setUniformValue("effectTime", float(clock.elapsed() / 1000.0));
        const auto materials = catalog.value("materials").toObject();
        for (const auto& d : draws)
        {
            if (d.color.w() <= 0)
                continue;
            const auto m = materials.value(d.material).toObject(), bindings = m.value("textures").toObject();
            const bool customLogo = !logo.isNull() && d.id.endsWith(":469684920047201495");
            program.setUniformValue("projection", d.projection);
            program.setUniformValue("color", d.color);
            program.setUniformValue("fx", m.value("fx").toBool());
            program.setUniformValueArray("parameters", d.uniforms.data(), 20, 4);
            const QStringList names { "_MainTex", "_MaskTex", "_DissolveTex" };
            for (int i = 0; i < 3; ++i)
            {
                const auto id = i == 0 && customLogo ? QString("__logo")
                    : i == 0 && !d.texture.isEmpty() ? d.texture : bindings.value(names[i]).toString();
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, textures.value(id, textures.value("__white")));
                glBindSampler(i, 0);
            }
            const auto mesh = meshes.value(customLogo ? "__logo" : d.mesh);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
            glVertexAttribPointer(
                1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
            for (const auto& p : m.value("passes").toArray())
            {
                const auto pass = p.toObject(), blend = pass.value("blend").toObject(),
                           stencil = pass.value("stencil").toObject();
                glBlendFuncSeparate(blendFactor(blend.value("srcBlend").toInt()),
                    blendFactor(blend.value("destBlend").toInt()),
                    blendFactor(blend.value("srcBlendAlpha").toInt()),
                    blendFactor(blend.value("destBlendAlpha").toInt()));
                glStencilFunc(stencil.value("comp").toInt() == 3 ? GL_EQUAL : GL_ALWAYS,
                    pass.value("reference").toInt(), pass.value("readMask").toInt());
                glStencilMask(pass.value("writeMask").toInt());
                glStencilOp(GL_KEEP, GL_KEEP, stencil.value("pass").toInt() == 2 ? GL_REPLACE : GL_KEEP);
                program.setUniformValue("stencilAlpha", float(pass.value("stencilAlpha").toDouble(1)));
                glDrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
            }
        }
        program.release();
        QOpenGLFramebufferObject::blitFramebuffer(
            resolved.get(), scene.get(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        framebufferObject()->bind();
        glViewport(0, 0, size.width(), size.height());
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        composite.bind();
        composite.setUniformValue("source", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resolved->texture());
        glDrawArrays(GL_TRIANGLES, 0, 3);
        composite.release();
        glBindVertexArray(0);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    static GLenum blendFactor(int value)
    {
        switch (value)
        {
        case 0:
            return GL_ZERO;
        case 1:
            return GL_ONE;
        case 5:
            return GL_SRC_ALPHA;
        case 10:
            return GL_ONE_MINUS_SRC_ALPHA;
        }
        qFatal("Unsupported original dial blend factor: %d", value);
    }
    void initialize()
    {
        initializeOpenGLFunctions();
        const auto compile = [](QOpenGLShaderProgram& p, const QString& v, const QString& f)
        {
            if (!p.addShaderFromSourceCode(QOpenGLShader::Vertex, resource(v))
                || !p.addShaderFromSourceCode(QOpenGLShader::Fragment, resource(f)) || !p.link())
                qFatal("Source dial shader failed: %s", qPrintable(p.log()));
        };
        compile(program, ":/shaders/dial.vert", ":/shaders/dial.frag");
        compile(composite, ":/shaders/dial-composite.vert", ":/shaders/dial-composite.frag");
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        const auto geometry = catalog.value("meshes").toObject();
        for (auto it = geometry.begin(); it != geometry.end(); ++it)
        {
            const auto o = it.value().toObject();
            QVector<float> v;
            QVector<GLuint> indices;
            for (const auto& p : o.value("vertices").toArray())
                for (const auto& c : p.toArray())
                    v.append(float(c.toDouble()));
            for (const auto& i : o.value("indices").toArray())
                indices.append(GLuint(i.toInt()));
            Mesh mesh;
            mesh.count = indices.size();
            glGenBuffers(1, &mesh.vertices);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
            glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.constData(), GL_STATIC_DRAW);
            glGenBuffers(1, &mesh.indices);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.constData(),
                GL_STATIC_DRAW);
            meshes.insert(it.key(), mesh);
        }
        const auto sourceTextures = catalog.value("textures").toObject();
        // Untrimmed replacement quad; the authored quad uses atlas-specific UVs.
        const float quad[]{-150, -32.5, 0, 0, 0, -150, 32.5, 0, 0, 1, 150, 32.5, 0, 1, 1, 150, -32.5, 0, 1, 0};
        const GLuint indices[]{0, 1, 2, 2, 3, 0};
        Mesh logoMesh; logoMesh.count = 6;
        glGenBuffers(1, &logoMesh.vertices); glBindBuffer(GL_ARRAY_BUFFER, logoMesh.vertices); glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glGenBuffers(1, &logoMesh.indices); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, logoMesh.indices); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        meshes.insert("__logo", logoMesh);
        for (auto it = sourceTextures.begin(); it != sourceTextures.end(); ++it)
        {
            const auto t = it.value().toObject(), sampler = t.value("sampler").toObject();
            const auto bytes = resource(":/assets/watch-dial/" + t.value("file").toString());
            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            const int levels = t.value("mipCount").toInt(), filter = sampler.value("m_FilterMode").toInt();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter == 0 ? GL_NEAREST : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                levels <= 1       ? (filter == 0 ? GL_NEAREST : GL_LINEAR)
                    : filter == 2 ? GL_LINEAR_MIPMAP_LINEAR
                    : filter == 0 ? GL_NEAREST_MIPMAP_NEAREST
                                  : GL_LINEAR_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                sampler.value("m_WrapU").toInt() == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                sampler.value("m_WrapV").toInt() == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            // GLES uploads RGBA. Swizzle the original BGRA bytes in the sampler
            // without rewriting the authored mip chain or changing its precision.
            if (t.value("bgra").toBool())
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
            }
            int w = t.value("width").toInt(), h = t.value("height").toInt();
            qsizetype offset = 0;
            for (int level = 0; level < levels; ++level)
            {
                const qsizetype count = qsizetype(w) * h * 4;
                if (offset + count > bytes.size())
                    qFatal("Truncated original dial texture");
                glTexImage2D(GL_TEXTURE_2D, level, t.value("srgb").toBool() ? GL_SRGB8_ALPHA8 : GL_RGBA8, w,
                    h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    bytes.constData() + offset);
                offset += count;
                w = std::max(1, w / 2);
                h = std::max(1, h / 2);
            }
            textures.insert(it.key(), texture);
        }
        GLuint white;
        glGenTextures(1, &white);
        glBindTexture(GL_TEXTURE_2D, white);
        const GLuint pixel = 0xffffffff;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        textures.insert("__white", white);
        initialized = true;
    }
    QJsonObject catalog;
    QVector<Draw> draws;
    QHash<QString, Mesh> meshes;
    QHash<QString, GLuint> textures;
    QOpenGLShaderProgram program, composite;
    std::unique_ptr<QOpenGLFramebufferObject> scene, resolved;
    QElapsedTimer clock;
    GLuint vao = 0;
    bool initialized = false;
    QImage logo;
    bool logoDirty = false;
};
}
SourceDial::SourceDial(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
{
    setMirrorVertically(true);
    connect(this, &SourceDial::animationChanged, this, &SourceDial::update);
}
QQuickFramebufferObject::Renderer* SourceDial::createRenderer() const
{
    return new DialRenderer;
}
void SourceDial::setPose(const QVariantMap& pose)
{
    if (m_pose == pose)
        return;
    m_pose = pose;
    emit poseChanged();
    update();
}
void SourceDial::setLogo(const QUrl &url) {
    if (m_logo == url) return;
    m_logo = url;
    QImageReader reader(url.scheme() == "qrc" ? ':' + url.path() : url.toLocalFile());
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (size.isValid()) reader.setScaledSize(size.scaled(768, 768, Qt::KeepAspectRatio));
    m_logoImage = url.isEmpty() ? QImage() : centerLogoArtwork(reader.read());
    emit logoChanged(); update();
}
