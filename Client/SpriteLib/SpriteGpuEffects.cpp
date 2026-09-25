#include "SpriteGpuEffects.h"
#include "SpriteGpuXbrz.h"
#include "SpriteGpuShaders.h"
#include <SDL_opengl.h>
#include <algorithm>
#include <cstring>

namespace SpriteGpuEffects {
namespace {
// Load through SDL: no platform-specific OpenGL import library is required.
#define GPU_GL_FUNCTIONS(X) \
	X(GLuint, CreateShader, (GLenum)) \
	X(void, ShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
	X(void, CompileShader, (GLuint)) \
	X(void, GetShaderiv, (GLuint, GLenum, GLint*)) \
	X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
	X(void, DeleteShader, (GLuint)) \
	X(GLuint, CreateProgram, ()) \
	X(void, AttachShader, (GLuint, GLuint)) \
	X(void, LinkProgram, (GLuint)) \
	X(void, GetProgramiv, (GLuint, GLenum, GLint*)) \
	X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
	X(void, DeleteProgram, (GLuint)) \
	X(void, UseProgram, (GLuint)) \
	X(GLint, GetUniformLocation, (GLuint, const GLchar*)) \
	X(void, Uniform1i, (GLint, GLint)) \
	X(void, Uniform1f, (GLint, GLfloat)) \
	X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
	X(void, Uniform3fv, (GLint, GLsizei, const GLfloat*)) \
	X(void, GetIntegerv, (GLenum, GLint*)) \
	X(GLenum, GetError, ()) \
	X(void, PushAttrib, (GLbitfield)) \
	X(void, PopAttrib, ()) \
	X(void, ActiveTexture, (GLenum)) \
	X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
	X(void, Scissor, (GLint, GLint, GLsizei, GLsizei)) \
	X(void, Enable, (GLenum)) \
	X(void, Disable, (GLenum)) \
	X(void, ColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
	X(void, Color4ub, (GLubyte, GLubyte, GLubyte, GLubyte)) \
	X(void, Begin, (GLenum)) \
	X(void, End, ()) \
	X(void, TexCoord2f, (GLfloat, GLfloat)) \
	X(void, Vertex2f, (GLfloat, GLfloat))
#define DECLARE_GL(result, name, args) result (APIENTRY* name) args = nullptr;
GPU_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL

SDL_Renderer* device = nullptr;
GLuint program = 0;
GLuint xbrzClassifyProgram = 0, xbrzScaleProgram = 0;
GLint sizeUniform = -1, effectUniform = -1, valueUniform = -1, gradationUniform = -1;

const char* vertexSource = R"glsl(#version 120
uniform vec2 targetSize;
varying vec2 uv;
varying float light;
void main() {
	uv = gl_MultiTexCoord0.xy;
	light = gl_Color.r * 255.0;
	// SDL's OpenGL render targets store row zero at texture coordinate zero.
	gl_Position = vec4(gl_Vertex.xy * 2.0 / targetSize - 1.0, 0.0, 1.0);
}
)glsl";

const char* fragmentSource = SpriteGpuShaders::Fragment;

GLuint Compile(GLenum type, const char* source, const char* tail = nullptr)
{
	GLuint shader = CreateShader(type);
	if (!shader) return 0;
	const char* sources[]{source, tail};
	ShaderSource(shader, tail ? 2 : 1, sources, nullptr);
	CompileShader(shader);
	GLint success = 0;
	GetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[2048]{};
		GetShaderInfoLog(shader, sizeof(log), nullptr, log);
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Sprite shader compile: %s", log);
		DeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint Link(const char* fragmentSource, const char* tail = nullptr)
{
	GLuint vertex = Compile(GL_VERTEX_SHADER, vertexSource);
	GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentSource, tail);
	GLuint result = 0;
	if (vertex && fragment) {
		result = CreateProgram();
		if (result) {
			AttachShader(result, vertex);
			AttachShader(result, fragment);
			LinkProgram(result);
		}
	}
	if (vertex) DeleteShader(vertex);
	if (fragment) DeleteShader(fragment);
	if (!result) return 0;
	GLint success = 0;
	GetProgramiv(result, GL_LINK_STATUS, &success);
	if (!success) {
		char log[2048]{};
		GetProgramInfoLog(result, sizeof(log), nullptr, log);
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Sprite shader link: %s", log);
		DeleteProgram(result);
		return 0;
	}
	return result;
}

// SDL caches GL state. Restore everything we change, including the program
// (not part of glPushAttrib), before handing control back to SDL.
struct State {
	GLint previousProgram = 0, activeTexture = 0;
	State()
	{
		GetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
		GetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
		PushAttrib(GL_ALL_ATTRIB_BITS);
	}
	~State()
	{
		UseProgram(previousProgram);
		PopAttrib();
		ActiveTexture(activeTexture);
	}
};
}

bool Attach(SDL_Renderer* renderer)
{
	Detach();
	SDL_RendererInfo info{};
	if (!renderer || SDL_GetRendererInfo(renderer, &info) != 0
		|| std::strcmp(info.name, "opengl") != 0 || !SDL_GL_GetCurrentContext()) return false;
#define LOAD_GL(result, name, args) \
	name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name)); \
	if (!name) return false;
	GPU_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
	program = Link(fragmentSource);
	if (!program) return false;
	if (SDL_GL_ExtensionSupported("GL_ARB_gpu_shader_fp64")) {
		xbrzClassifyProgram = Link(SpriteGpuXbrz::Common, SpriteGpuXbrz::Classify);
		xbrzScaleProgram = Link(SpriteGpuXbrz::Common, SpriteGpuXbrz::Scale);
	}
	device = renderer;
	State restore;
	// SDL's optimized RGB565 conversion uses lookup-table rounding which can
	// differ from bit replication. Capture the local converter once, so GPU
	// xBRZ receives the same colors as FrameUpscaler's SDL_ConvertPixels call.
	Uint16 samples[64];
	Uint32 converted[64];
	for (int i = 0; i < 64; ++i) {
		const int rb = (std::min)(i, 31);
		samples[i] = Uint16((rb << 11) | (i << 5) | rb);
	}
	if (SDL_ConvertPixels(64, 1, SDL_PIXELFORMAT_RGB565, samples, sizeof(samples),
		SDL_PIXELFORMAT_RGB888, converted, sizeof(converted)) != 0) {
		if (xbrzClassifyProgram) DeleteProgram(xbrzClassifyProgram);
		if (xbrzScaleProgram) DeleteProgram(xbrzScaleProgram);
		xbrzClassifyProgram = xbrzScaleProgram = 0;
	} else {
		GLfloat expansion[64 * 3];
		for (int i = 0; i < 64; ++i) {
			expansion[i * 3] = GLfloat((converted[i] >> 16) & 255);
			expansion[i * 3 + 1] = GLfloat((converted[i] >> 8) & 255);
			expansion[i * 3 + 2] = GLfloat(converted[i] & 255);
		}
		for (GLuint shader : {xbrzClassifyProgram, xbrzScaleProgram}) {
			if (!shader) continue;
			UseProgram(shader);
			Uniform3fv(GetUniformLocation(shader, "expansion"), 64, expansion);
		}
	}
	UseProgram(program);
	Uniform1i(GetUniformLocation(program, "source"), 0);
	Uniform1i(GetUniformLocation(program, "background"), 1);
	Uniform1i(GetUniformLocation(program, "palette"), 2);
	sizeUniform = GetUniformLocation(program, "targetSize");
	effectUniform = GetUniformLocation(program, "effect");
	valueUniform = GetUniformLocation(program, "value");
	gradationUniform = GetUniformLocation(program, "gradation");
	SDL_Log("Sprite effects: OpenGL shaders");
	SDL_Log("xBRZ: %s", XbrzAvailable() ? "OpenGL shaders" : "CPU fallback (shader unavailable)");
	return true;
}

void Detach()
{
	if (program) DeleteProgram(program);
	if (xbrzClassifyProgram) DeleteProgram(xbrzClassifyProgram);
	if (xbrzScaleProgram) DeleteProgram(xbrzScaleProgram);
	program = 0;
	xbrzClassifyProgram = xbrzScaleProgram = 0;
	device = nullptr;
}

bool Active() { return device && program; }
bool XbrzAvailable() { return Active() && xbrzClassifyProgram && xbrzScaleProgram; }

bool LightGrid(SDL_Texture* source, int width, int height, std::span<const LightCell> cells)
{
	if (!Active() || !source || !SDL_GetRenderTarget(device) || width <= 0 || height <= 0) return false;
	if (SDL_RenderFlush(device) != 0) return false;
	State restore;
	ActiveTexture(GL_TEXTURE0);
	float texWidth = 0, texHeight = 0;
	if (SDL_GL_BindTexture(source, &texWidth, &texHeight) != 0 || texWidth != 1 || texHeight != 1) return false;
	UseProgram(program);
	Uniform2f(sizeUniform, float(width), float(height));
	Uniform1i(effectUniform, static_cast<int>(Effect::LightGrid));
	Viewport(0, 0, width, height);
	Disable(GL_SCISSOR_TEST);
	Disable(GL_BLEND);
	Disable(GL_DEPTH_TEST);
	Disable(GL_STENCIL_TEST);
	Disable(GL_CULL_FACE);
	Disable(GL_ALPHA_TEST);
	Disable(GL_DITHER);
	ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	Begin(GL_QUADS);
	for (const auto& cell : cells) {
		const float x0 = float(cell.rect.x), y0 = float(cell.rect.y);
		const float x1 = x0 + cell.rect.w, y1 = y0 + cell.rect.h;
		Color4ub(cell.light, 0, 0, 255);
		TexCoord2f(x0 / width, y0 / height); Vertex2f(x0, y0);
		TexCoord2f(x1 / width, y0 / height); Vertex2f(x1, y0);
		TexCoord2f(x1 / width, y1 / height); Vertex2f(x1, y1);
		TexCoord2f(x0 / width, y1 / height); Vertex2f(x0, y1);
	}
	End();
	return GetError() == GL_NO_ERROR;
}

bool Xbrz(SDL_Texture* source, SDL_Texture* corners, int width, int height, int factor)
{
	if (!XbrzAvailable() || !source || !SDL_GetRenderTarget(device) || width <= 0 || height <= 0
		|| (corners && (factor < 2 || factor > 4))) return false;
	if (SDL_RenderFlush(device) != 0) return false;
	State restore;
	const GLuint shader = corners ? xbrzScaleProgram : xbrzClassifyProgram;
	const int targetWidth = width * (corners ? factor : 1);
	const int targetHeight = height * (corners ? factor : 1);
	ActiveTexture(GL_TEXTURE0);
	float texWidth = 0, texHeight = 0;
	if (SDL_GL_BindTexture(source, &texWidth, &texHeight) != 0 || texWidth != 1 || texHeight != 1) return false;
	if (corners) {
		ActiveTexture(GL_TEXTURE1);
		if (SDL_GL_BindTexture(corners, nullptr, nullptr) != 0) return false;
	}
	ActiveTexture(GL_TEXTURE0);
	UseProgram(shader);
	Uniform1i(GetUniformLocation(shader, "source"), 0);
	Uniform1i(GetUniformLocation(shader, "corners"), 1);
	Uniform1i(GetUniformLocation(shader, "factor"), factor);
	Uniform2f(GetUniformLocation(shader, "sourceSize"), float(width), float(height));
	Uniform2f(GetUniformLocation(shader, "targetSize"), float(targetWidth), float(targetHeight));
	Viewport(0, 0, targetWidth, targetHeight);
	Disable(GL_SCISSOR_TEST);
	Disable(GL_BLEND);
	Disable(GL_DEPTH_TEST);
	Disable(GL_STENCIL_TEST);
	Disable(GL_CULL_FACE);
	Disable(GL_ALPHA_TEST);
	Disable(GL_DITHER);
	ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	Begin(GL_TRIANGLE_STRIP);
	TexCoord2f(0, 0); Vertex2f(0, 0);
	TexCoord2f(1, 0); Vertex2f(float(targetWidth), 0);
	TexCoord2f(0, 1); Vertex2f(0, float(targetHeight));
	TexCoord2f(1, 1); Vertex2f(float(targetWidth), float(targetHeight));
	End();
	return GetError() == GL_NO_ERROR;
}

bool Draw(SDL_Texture* source, int width, int height, const SDL_Rect& placement,
	const SDL_Rect& clip, Effect effect, int value, const Uint16* gradation,
	SDL_Texture* background, SDL_Texture* palette)
{
	if (!Active() || !source || !SDL_GetRenderTarget(device) || width <= 0 || height <= 0) return false;
	if (SDL_RenderFlush(device) != 0) return false;
	State restore;
	ActiveTexture(GL_TEXTURE0);
	float texWidth = 0, texHeight = 0;
	if (SDL_GL_BindTexture(source, &texWidth, &texHeight) != 0) return false;
	// The shaders require normalized 2D textures, available on supported GPUs.
	if (texWidth != 1.0f || texHeight != 1.0f) return false;
	if (background) {
		ActiveTexture(GL_TEXTURE1);
		if (SDL_GL_BindTexture(background, nullptr, nullptr) != 0) return false;
	}
	if (palette) {
		ActiveTexture(GL_TEXTURE2);
		if (SDL_GL_BindTexture(palette, nullptr, nullptr) != 0) return false;
	}
	ActiveTexture(GL_TEXTURE0);
	UseProgram(program);
	Uniform2f(sizeUniform, float(width), float(height));
	Uniform1i(effectUniform, static_cast<int>(effect));
	Uniform1f(valueUniform, float(value));
	if (effect == Effect::Gradation) {
		if (!gradation) return false;
		GLfloat colors[94 * 3];
		for (int i = 0; i < 94; ++i) {
			colors[i * 3] = GLfloat(gradation[i] >> 11);
			colors[i * 3 + 1] = GLfloat((gradation[i] >> 5) & 63);
			colors[i * 3 + 2] = GLfloat(gradation[i] & 31);
		}
		Uniform3fv(gradationUniform, 94, colors);
	}
	Viewport(0, 0, width, height);
	Enable(GL_SCISSOR_TEST);
	Scissor(clip.x, clip.y, clip.w, clip.h);
	Disable(GL_BLEND);
	Disable(GL_DEPTH_TEST);
	Disable(GL_STENCIL_TEST);
	Disable(GL_CULL_FACE);
	Disable(GL_ALPHA_TEST);
	Disable(GL_DITHER);
	ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	const float x0 = float(placement.x), y0 = float(placement.y);
	const float x1 = x0 + float(placement.w), y1 = y0 + float(placement.h);
	Begin(GL_TRIANGLE_STRIP);
	TexCoord2f(0, 0); Vertex2f(x0, y0);
	TexCoord2f(1, 0); Vertex2f(x1, y0);
	TexCoord2f(0, 1); Vertex2f(x0, y1);
	TexCoord2f(1, 1); Vertex2f(x1, y1);
	End();
	return GetError() == GL_NO_ERROR;
}
}
