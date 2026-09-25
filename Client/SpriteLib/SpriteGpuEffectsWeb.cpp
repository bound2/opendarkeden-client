#include "SpriteGpuEffects.h"
#include "SpriteGpuShaders.h"
#include "SpriteGpuXbrz.h"
#include <GLES3/gl3.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>

namespace SpriteGpuEffects {
namespace {
SDL_Renderer* device = nullptr;
GLuint program = 0, vertexArray = 0, vertexBuffer = 0;
GLuint classifyProgram = 0, scaleProgram = 0, distanceTexture = 0;
struct Uniforms {
    GLint targetSize = -1, effect = -1, value = -1, gradation = -1;
    GLint sourceSize = -1, factor = -1;
    void Init(GLuint shader)
    {
        targetSize = glGetUniformLocation(shader, "targetSize");
        effect = glGetUniformLocation(shader, "effect");
        value = glGetUniformLocation(shader, "value");
        gradation = glGetUniformLocation(shader, "gradation");
        sourceSize = glGetUniformLocation(shader, "sourceSize");
        factor = glGetUniformLocation(shader, "factor");
    }
} effectUniforms, classifyUniforms, scaleUniforms;
struct Vertex { float x, y, u, v, light; };
constexpr size_t MaxVertices = 6 * 256;

constexpr const char* vertexSource = R"glsl(#version 300 es
precision highp float;
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 coordinate;
layout(location = 2) in float brightness;
uniform vec2 targetSize;
out vec2 uv;
out float light;
void main() {
    uv = coordinate;
    light = brightness;
    gl_Position = vec4(position * 2.0 / targetSize - 1.0, 0.0, 1.0);
}
)glsl";

// SDL caches GL state. Use a private VAO and restore every state we change,
// including each texture binding, before returning control to its renderer.
class State {
    GLint oldProgram{}, activeTexture{}, array{}, buffer{};
    GLint viewport[4]{}, scissor[4]{}, textures[4]{}, oldDistanceTexture{};
    GLboolean colorMask[4]{};
    static constexpr std::array<GLenum, 6> capabilities{
        GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_DITHER};
    std::array<GLboolean, capabilities.size()> enabled{};
public:
    State()
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &array);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
        }
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &oldDistanceTexture);
        for (size_t i = 0; i < capabilities.size(); ++i)
            enabled[i] = glIsEnabled(capabilities[i]);
    }
    ~State()
    {
        glUseProgram(oldProgram);
        glBindVertexArray(array);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, textures[i]);
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY, oldDistanceTexture);
        glActiveTexture(activeTexture);
        for (size_t i = 0; i < capabilities.size(); ++i) {
            if (enabled[i]) glEnable(capabilities[i]);
            else glDisable(capabilities[i]);
        }
    }
};

GLuint Compile(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "WebGL sprite shader: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Link(const char* fragment)
{
    GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource);
    GLuint pixel = Compile(GL_FRAGMENT_SHADER, fragment);
    GLuint result = 0;
    if (vertex && pixel) {
        result = glCreateProgram();
        glAttachShader(result, vertex);
        glAttachShader(result, pixel);
        glLinkProgram(result);
    }
    if (vertex) glDeleteShader(vertex);
    if (pixel) glDeleteShader(pixel);
    if (!result) return 0;
    GLint success = 0;
    glGetProgramiv(result, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048]{};
        glGetProgramInfoLog(result, sizeof(log), nullptr, log);
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "WebGL sprite program: %s", log);
        glDeleteProgram(result);
        return 0;
    }
    return result;
}

bool Bind(SDL_Texture* texture, GLenum unit)
{
    glActiveTexture(unit);
    float w = 0, h = 0;
    return texture && SDL_GL_BindTexture(texture, &w, &h) == 0 && w == 1 && h == 1;
}

void Prepare(int width, int height, Effect effect)
{
    glUseProgram(program);
    glUniform2f(effectUniforms.targetSize, float(width), float(height));
    glUniform1i(effectUniforms.effect, static_cast<int>(effect));
    glViewport(0, 0, width, height);
    for (GLenum cap : {GL_SCISSOR_TEST, GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_CULL_FACE, GL_DITHER})
        glDisable(cap);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

bool DrawVertices(std::span<const Vertex> vertices, GLenum mode)
{
    if (vertices.size() > MaxVertices) return false;
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size_bytes(), vertices.data());
    glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size()));
    // Querying WebGL errors here synchronizes with the browser GPU process
    // for every sprite/effect. Validate resources when created; the pixel
    // oracle checks draw results without a synchronous query per command.
    return true;
}

bool BindDistances()
{
    glActiveTexture(GL_TEXTURE3);
    if (distanceTexture) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, distanceTexture);
        return true;
    }
    // Retain no CPU copy: fill and upload one 256 KiB layer at a time. This
    // work happens on first xBRZ use after device creation, never per frame.
    std::vector<float> layer(256 * 256, 0.0f);
    glGenTextures(1, &distanceTexture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, distanceTexture);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32F, 256, 256, 128, 0, GL_RED, GL_FLOAT, nullptr);
    constexpr double kb = 0.0593, kr = 0.2627, kg = 1 - kb - kr;
    constexpr double scaleB = 0.5 / (1 - kb), scaleR = 0.5 / (1 - kr);
    for (int r = 0; r < 128; ++r) {
        for (int b = -127; b <= 127; ++b) {
            for (int g = -127; g <= 127; ++g) {
                const double y = kr * (r * 2) + kg * (g * 2) + kb * (b * 2);
                const double cb = scaleB * (b * 2 - y), cr = scaleR * (r * 2 - y);
                layer[(b + 127) * 256 + g + 127] = float(std::sqrt(y * y + cb * cb + cr * cr));
            }
        }
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, r, 256, 256, 1, GL_RED, GL_FLOAT, layer.data());
    }
    if (glGetError() == GL_NO_ERROR) return true;
    glDeleteTextures(1, &distanceTexture);
    distanceTexture = 0;
    return false;
}
}

bool Attach(SDL_Renderer* renderer)
{
    Detach();
    SDL_RendererInfo info{};
    if (!renderer || SDL_GetRendererInfo(renderer, &info) != 0 ||
        std::strcmp(info.name, "opengles2") != 0 || !SDL_GL_GetCurrentContext()) return false;
    State restore;
    program = Link(SpriteGpuShaders::Fragment);
    if (!program) return false;
    classifyProgram = Link((std::string(SpriteGpuXbrz::Common) + SpriteGpuXbrz::Classify).c_str());
    scaleProgram = Link((std::string(SpriteGpuXbrz::Common) + SpriteGpuXbrz::Scale).c_str());
    effectUniforms.Init(program);
    if (classifyProgram) classifyUniforms.Init(classifyProgram);
    if (scaleProgram) scaleUniforms.Init(scaleProgram);
    Uint16 samples[64];
    Uint32 converted[64];
    for (int i = 0; i < 64; ++i) {
        const int rb = std::min(i, 31);
        samples[i] = Uint16((rb << 11) | (i << 5) | rb);
    }
    if (SDL_ConvertPixels(64, 1, SDL_PIXELFORMAT_RGB565, samples, sizeof(samples),
        SDL_PIXELFORMAT_RGB888, converted, sizeof(converted)) != 0) {
        Detach();
        return false;
    }
    GLfloat expansion[64 * 3];
    for (int i = 0; i < 64; ++i) {
        expansion[i * 3] = GLfloat((converted[i] >> 16) & 255);
        expansion[i * 3 + 1] = GLfloat((converted[i] >> 8) & 255);
        expansion[i * 3 + 2] = GLfloat(converted[i] & 255);
    }
    for (GLuint shader : {classifyProgram, scaleProgram}) {
        if (!shader) continue;
        glUseProgram(shader);
        glUniform3fv(glGetUniformLocation(shader, "expansion"), 64, expansion);
        glUniform1i(glGetUniformLocation(shader, "distances"), 3);
        glUniform1i(glGetUniformLocation(shader, "source"), 0);
        glUniform1i(glGetUniformLocation(shader, "corners"), 1);
    }
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, MaxVertices * sizeof(Vertex), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, light)));
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "source"), 0);
    glUniform1i(glGetUniformLocation(program, "background"), 1);
    glUniform1i(glGetUniformLocation(program, "palette"), 2);
    if (glGetError() != GL_NO_ERROR) { Detach(); return false; }
    device = renderer;
    SDL_Log("Sprite effects: WebGL 2 shaders");
    SDL_Log("xBRZ: %s", XbrzAvailable() ? "WebGL 2 shaders" : "CPU fallback (shader unavailable)");
    return true;
}

void Detach()
{
    if (program) glDeleteProgram(program);
    if (classifyProgram) glDeleteProgram(classifyProgram);
    if (scaleProgram) glDeleteProgram(scaleProgram);
    if (distanceTexture) glDeleteTextures(1, &distanceTexture);
    if (vertexArray) glDeleteVertexArrays(1, &vertexArray);
    if (vertexBuffer) glDeleteBuffers(1, &vertexBuffer);
    program = vertexArray = vertexBuffer = 0;
    classifyProgram = scaleProgram = distanceTexture = 0;
    device = nullptr;
}

bool Active() { return device && program; }
bool XbrzAvailable() { return Active() && classifyProgram && scaleProgram; }
bool Xbrz(SDL_Texture* source, SDL_Texture* corners, int width, int height, int factor)
{
    if (!XbrzAvailable() || !SDL_GetRenderTarget(device) || width <= 0 || height <= 0 ||
        (corners && (factor < 2 || factor > 4)) || SDL_RenderFlush(device) != 0) return false;
    State restore;
    if (!Bind(source, GL_TEXTURE0) || (corners && !Bind(corners, GL_TEXTURE1)) || !BindDistances()) return false;
    const int targetWidth = width * (corners ? factor : 1), targetHeight = height * (corners ? factor : 1);
    Prepare(targetWidth, targetHeight, Effect::Copy);
    const GLuint shader = corners ? scaleProgram : classifyProgram;
    const auto& uniforms = corners ? scaleUniforms : classifyUniforms;
    glUseProgram(shader);
    glUniform1i(uniforms.factor, factor);
    glUniform2f(uniforms.sourceSize, float(width), float(height));
    glUniform2f(uniforms.targetSize, float(targetWidth), float(targetHeight));
    const Vertex vertices[]{{0, 0, 0, 0, 0}, {float(targetWidth), 0, 1, 0, 0},
        {0, float(targetHeight), 0, 1, 0}, {float(targetWidth), float(targetHeight), 1, 1, 0}};
    return DrawVertices(vertices, GL_TRIANGLE_STRIP);
}

bool Draw(SDL_Texture* source, int width, int height, const SDL_Rect& placement,
    const SDL_Rect& clip, Effect effect, int value, const Uint16* gradation,
    SDL_Texture* background, SDL_Texture* palette)
{
    if (!Active() || !SDL_GetRenderTarget(device) || width <= 0 || height <= 0 ||
        (effect == Effect::Gradation && !gradation) || SDL_RenderFlush(device) != 0) return false;
    State restore;
    if (!Bind(source, GL_TEXTURE0) || (background && !Bind(background, GL_TEXTURE1)) ||
        (palette && !Bind(palette, GL_TEXTURE2))) return false;
    Prepare(width, height, effect);
    glUniform1f(effectUniforms.value, float(value));
    if (effect == Effect::Gradation) {
        GLfloat colors[94 * 3];
        for (int i = 0; i < 94; ++i) {
            colors[i * 3] = GLfloat(gradation[i] >> 11);
            colors[i * 3 + 1] = GLfloat((gradation[i] >> 5) & 63);
            colors[i * 3 + 2] = GLfloat(gradation[i] & 31);
        }
        glUniform3fv(effectUniforms.gradation, 94, colors);
    }
    glEnable(GL_SCISSOR_TEST);
    glScissor(clip.x, clip.y, clip.w, clip.h);
    const float x0 = float(placement.x), y0 = float(placement.y);
    const float x1 = x0 + placement.w, y1 = y0 + placement.h;
    const Vertex vertices[]{{x0, y0, 0, 0, 0}, {x1, y0, 1, 0, 0},
        {x0, y1, 0, 1, 0}, {x1, y1, 1, 1, 0}};
    return DrawVertices(vertices, GL_TRIANGLE_STRIP);
}

bool LightGrid(SDL_Texture* source, int width, int height, std::span<const LightCell> cells)
{
    if (!Active() || !SDL_GetRenderTarget(device) || width <= 0 || height <= 0 ||
        SDL_RenderFlush(device) != 0) return false;
    State restore;
    if (!Bind(source, GL_TEXTURE0)) return false;
    Prepare(width, height, Effect::LightGrid);
    // Bound temporary storage even for an unusually large light grid.
    std::array<Vertex, MaxVertices> vertices;
    size_t count = 0;
    for (const auto& cell : cells) {
        const float x0 = float(cell.rect.x), y0 = float(cell.rect.y);
        const float x1 = x0 + cell.rect.w, y1 = y0 + cell.rect.h, light = float(cell.light);
        const Vertex a{x0, y0, x0 / width, y0 / height, light};
        const Vertex b{x1, y0, x1 / width, y0 / height, light};
        const Vertex c{x0, y1, x0 / width, y1 / height, light};
        const Vertex d{x1, y1, x1 / width, y1 / height, light};
        for (const Vertex v : {a, b, c, b, d, c}) vertices[count++] = v;
        if (count == vertices.size()) {
            if (!DrawVertices(std::span(vertices).first(count), GL_TRIANGLES)) return false;
            count = 0;
        }
    }
    return count == 0 || DrawVertices(std::span(vertices).first(count), GL_TRIANGLES);
}
}
