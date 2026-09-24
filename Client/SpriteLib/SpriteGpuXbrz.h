#pragma once

// GPU adaptation of the vendored xBRZ RGB scaler (2x, 3x, 4x).
// Derived from third_party/xbrz/xbrz.cpp, Copyright (C) Zenju.
// Distributed under GNU GPL v3; see third_party/xbrz/License.txt.
// Modified for this client: fragment passes replace the CPU scanline buffer
// and output matrices. The algorithm, thresholds, rotations and weights are
// retained. The upstream linking exceptions are not extended to this file.
namespace SpriteGpuXbrz {
inline constexpr const char* Common = R"glsl(#version 120
#extension GL_ARB_gpu_shader_fp64 : require
uniform sampler2D source;
uniform sampler2D corners;
uniform vec2 sourceSize;
uniform int factor;
uniform vec3 expansion[64];

vec3 pixel(vec2 p) {
	vec2 uv = (clamp(p, vec2(0.0), sourceSize - 1.0) + 0.5) / sourceSize;
	vec3 rgb = floor(texture2D(source, uv).rgb * 255.0 + 0.5);
	// Match the original RGB565 -> RGB888 input to the CPU frame scaler.
	vec3 c = floor(rgb / vec3(8.0, 4.0, 8.0));
	return vec3(expansion[int(c.r)].r, expansion[int(c.g)].g, expansion[int(c.b)].b);
}

float fastDist(vec3 a, vec3 b) {
	// xBRZ's buffered RGB distance truncates each signed difference / 2.
	vec3 delta = sign(a - b) * floor(abs(a - b) / 2.0) * 2.0;
	float y = dot(delta, vec3(0.2627, 0.6780, 0.0593));
	float cb = (delta.b - y) * (0.5 / (1.0 - 0.0593));
	float cr = (delta.r - y) * (0.5 / (1.0 - 0.2627));
	return sqrt(y * y + cb * cb + cr * cr);
}
double dist(vec3 a, vec3 b) {
	dvec3 delta = dvec3(sign(a - b) * floor(abs(a - b) / 2.0) * 2.0);
	double kb = double(593) / double(10000), kr = double(2627) / double(10000);
	double kg = double(1) - kb - kr;
	double y = kr * delta.r + kg * delta.g + kb * delta.b;
	double cb = (delta.b - y) * (double(0.5) / (double(1) - kb));
	double cr = (delta.r - y) * (double(0.5) / (double(1) - kr));
	// The CPU stores distances as floats, then sums/compares them as doubles.
	return double(float(sqrt(y * y + cb * cb + cr * cr)));
}
bool eq(vec3 a, vec3 b) {
	float d = fastDist(a, b);
	return abs(d - 30.0) < 0.001 ? dist(a, b) < double(30) : d < 30.0;
}
)glsl";

inline constexpr const char* Classify = R"glsl(
void main() {
	vec2 p = floor(gl_FragCoord.xy);
	vec3 b = pixel(p + vec2(0, -1)), c = pixel(p + vec2(1, -1));
	vec3 e = pixel(p + vec2(-1, 0)), f = pixel(p), g = pixel(p + vec2(1, 0)), h = pixel(p + vec2(2, 0));
	vec3 i = pixel(p + vec2(-1, 1)), j = pixel(p + vec2(0, 1)), k = pixel(p + vec2(1, 1)), l = pixel(p + vec2(2, 1));
	vec3 n = pixel(p + vec2(0, 2)), o = pixel(p + vec2(1, 2));
	vec4 blend = vec4(0.0); // F, G, J, K at this crossing of four pixels.
	if (!((f == g && j == k) || (f == j && g == k))) {
		float jgFast = fastDist(i, f) + fastDist(f, c) + fastDist(n, k) + fastDist(k, h) + 4.0 * fastDist(j, g);
		float fkFast = fastDist(e, j) + fastDist(j, o) + fastDist(b, g) + fastDist(g, l) + 4.0 * fastDist(f, k);
		double jg = double(jgFast), fk = double(fkFast);
		// Double precision is only needed near a decision boundary. The margin
		// exceeds float accumulation error for the bounded RGB distance range.
		if (abs(jgFast - fkFast) < 0.01 || abs(3.6 * jgFast - fkFast) < 0.01 || abs(3.6 * fkFast - jgFast) < 0.01) {
			jg = dist(i, f) + dist(f, c) + dist(n, k) + dist(k, h) + double(4) * dist(j, g);
			fk = dist(e, j) + dist(j, o) + dist(b, g) + dist(g, l) + double(4) * dist(f, k);
		}
		if (jg < fk) {
			float strength = (double(36) / double(10)) * jg < fk ? 2.0 : 1.0;
			if (f != g && f != j) blend.r = strength;
			if (k != j && k != g) blend.a = strength;
		} else if (fk < jg) {
			float strength = (double(36) / double(10)) * fk < jg ? 2.0 : 1.0;
			if (j != f && j != k) blend.b = strength;
			if (g != f && g != k) blend.g = strength;
		}
	}
	gl_FragColor = blend / 255.0;
}
)glsl";

inline constexpr const char* Scale = R"glsl(
vec4 crossing(vec2 p) {
	// Clamp-to-edge source pixels imply no blend at crossings outside the image.
	if (p.x < 0.0 || p.y < 0.0 || p.x >= sourceSize.x || p.y >= sourceSize.y) return vec4(0.0);
	return floor(texture2D(corners, (p + 0.5) / sourceSize) * 255.0 + 0.5);
}

// Weights are (numerator, denominator), preserving integer truncation at
// every rotation. Coordinates are (row, column), as in xBRZ's output matrix.
vec2 shallow(vec2 p) {
	if (p == vec2(factor - 1, 0)) return vec2(1, 4);
	if (p == vec2(factor - 1, 1)) return vec2(3, 4);
	if (factor >= 3 && p == vec2(factor - 2, 2)) return vec2(1, 4);
	if (factor == 4 && p == vec2(2, 3)) return vec2(3, 4);
	if (p.x == float(factor - 1) && p.y >= 2.0) return vec2(1, 1);
	return vec2(0, 1);
}
vec2 weight(vec2 p, bool line, bool isShallow, bool isSteep) {
	if (!line) {
		if (factor == 2 && p == vec2(1, 1)) return vec2(21, 100);
		if (factor == 3 && p == vec2(2, 2)) return vec2(45, 100);
		if (factor == 4) {
			if (p == vec2(3, 3)) return vec2(68, 100);
			if (p == vec2(3, 2) || p == vec2(2, 3)) return vec2(9, 100);
		}
	} else if (isShallow && isSteep) {
		if (factor == 2) {
			if (p == vec2(1, 0) || p == vec2(0, 1)) return vec2(1, 4);
			if (p == vec2(1, 1)) return vec2(5, 6);
		} else if (factor == 3) {
			if (p == vec2(2, 0) || p == vec2(0, 2)) return vec2(1, 4);
			if (p == vec2(2, 1) || p == vec2(1, 2)) return vec2(3, 4);
			if (p == vec2(2, 2)) return vec2(1, 1);
		} else {
			if (p == vec2(3, 0) || p == vec2(0, 3)) return vec2(1, 4);
			if (p == vec2(3, 1) || p == vec2(1, 3)) return vec2(3, 4);
			if (p == vec2(2, 2)) return vec2(1, 3);
			if (p == vec2(3, 3) || p == vec2(3, 2) || p == vec2(2, 3)) return vec2(1, 1);
		}
	} else if (isShallow) return shallow(p);
	else if (isSteep) return shallow(p.yx);
	else {
		if (factor == 2 && p == vec2(1, 1)) return vec2(1, 2);
		if (factor == 3) {
			if (p == vec2(1, 2) || p == vec2(2, 1)) return vec2(1, 8);
			if (p == vec2(2, 2)) return vec2(7, 8);
		} else if (factor == 4) {
			if (p == vec2(3, 2) || p == vec2(2, 3)) return vec2(1, 2);
			if (p == vec2(3, 3)) return vec2(1, 1);
		}
	}
	return vec2(0, 1);
}

vec3 blendPixel(vec3 result, vec2 outputPixel, vec4 blend,
	vec3 b, vec3 c, vec3 d, vec3 e, vec3 f, vec3 g, vec3 h, vec3 i) {
	if (blend.b == 0.0) return result; // Top-left, top-right, bottom-right, bottom-left.
	bool line = blend.b >= 2.0 || !(
		(blend.g != 0.0 && !eq(e, g)) || (blend.a != 0.0 && !eq(e, c)) ||
		(!eq(e, i) && eq(g, h) && eq(h, i) && eq(i, f) && eq(f, c)));
	float ef = fastDist(e, f), eh = fastDist(e, h);
	bool chooseF = abs(ef - eh) < 0.001 ? dist(e, f) <= dist(e, h) : ef <= eh;
	vec3 color = chooseF ? f : h;
	float fgFast = fastDist(f, g), hcFast = fastDist(h, c);
	double fg = double(fgFast), hc = double(hcFast);
	if (abs(2.2 * fgFast - hcFast) < 0.01 || abs(2.2 * hcFast - fgFast) < 0.01) {
		fg = dist(f, g); hc = dist(h, c);
	}
	bool isShallow = (double(22) / double(10)) * fg <= hc && e != g && d != g;
	bool isSteep = (double(22) / double(10)) * hc <= fg && e != c && b != c;
	vec2 w = weight(outputPixel, line, isShallow, isSteep);
	// Avoid a floating divide rounding an exact integer infinitesimally down.
	return floor((color * w.x + result * (w.y - w.x)) / w.y + 0.0001);
}

void main() {
	vec2 p = floor(gl_FragCoord.xy / float(factor));
	vec2 sub = mod(floor(gl_FragCoord.yx), float(factor)); // row, column
	vec4 blend = vec4(crossing(p - vec2(1, 1)).a, crossing(p - vec2(0, 1)).b,
		crossing(p).r, crossing(p - vec2(1, 0)).g);
	vec3 a = pixel(p + vec2(-1, -1)), b = pixel(p + vec2(0, -1)), c = pixel(p + vec2(1, -1));
	vec3 d = pixel(p + vec2(-1, 0)), e = pixel(p), f = pixel(p + vec2(1, 0));
	vec3 g = pixel(p + vec2(-1, 1)), h = pixel(p + vec2(0, 1)), i = pixel(p + vec2(1, 1));
	vec3 color = e;
	color = blendPixel(color, sub, blend, b, c, d, e, f, g, h, i);
	sub = vec2(sub.y, float(factor - 1) - sub.x);
	color = blendPixel(color, sub, blend.argb, d, a, h, e, b, i, f, c);
	sub = vec2(sub.y, float(factor - 1) - sub.x);
	color = blendPixel(color, sub, blend.barg, h, g, f, e, d, c, b, a);
	sub = vec2(sub.y, float(factor - 1) - sub.x);
	color = blendPixel(color, sub, blend.gbar, f, i, b, e, h, a, d, g);
	gl_FragColor = vec4(color / 255.0, 1.0);
}
)glsl";
}
