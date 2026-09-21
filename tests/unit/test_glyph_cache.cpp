#include "test_framework.h"
#include "TextBackend.h"
#include "SpriteLib/SpriteLibBackend.h"
#include <memory>
#include <vector>
#include <cstring>

namespace {
using namespace TextSystem;
struct Backend {
	std::unique_ptr<TextBackend> value;
	FontHandle font;
	explicit Backend(GlyphCacheLimits limits = {}) : value(CreateSDLTextBackend(limits))
	{
		CHECK_EQ(0, spritectl_init());
		CHECK(value->Initialize());
		font = value->AcquireFont(FontDesc{});
		CHECK(font.IsValid());
	}
};

class Target : public RenderTarget {
public:
	spritectl_surface_t surface = spritectl_create_surface(48, 48, SPRITECTL_FORMAT_RGB565);
	~Target() { spritectl_destroy_surface(surface); }
	void* GetNative(NativeTargetType) const override { return surface; }
	int GetWidth() const override { return 48; }
	int GetHeight() const override { return 48; }
	std::vector<uint16_t> Draw(TextBackend& backend, const Glyph& glyph, uint8_t alpha)
	{
		spritectl_surface_info_t info{};
		CHECK_EQ(0, spritectl_lock_surface(surface, &info));
		std::memset(info.pixels, 0, info.pitch * info.height);
		spritectl_unlock_surface(surface);
		backend.DrawGlyph(*this, glyph, 4, 4, alpha);
		std::vector<uint16_t> pixels;
		CHECK_EQ(0, spritectl_lock_surface(surface, &info));
		for (int y = 0; y < info.height; ++y) {
			const auto* row = reinterpret_cast<const uint16_t*>(static_cast<char*>(info.pixels) + y * info.pitch);
			pixels.insert(pixels.end(), row, row + info.width);
		}
		spritectl_unlock_surface(surface);
		return pixels;
	}
};
}

TEST(GlyphCache, ColorChurnEvictsColdEntriesAndPreservesHits)
{
	Backend backend({2, 1024 * 1024});
	const auto white = ColorFromRGB(0xFFFFFF);
	CHECK(backend.value->GetGlyph(backend.font, 'A', white));
	CHECK(backend.value->GetGlyph(backend.font, 'B', white));
	const auto bytes = backend.value->GetGlyphCacheStats().pixelBytes;
	CHECK(bytes > 0);
	for (int color = 1; color < 20; ++color) {
		const auto rendered = backend.value->GetGlyphCacheStats().rasterizations;
		CHECK(backend.value->GetGlyph(backend.font, 'A', white));
		CHECK_EQ(rendered, backend.value->GetGlyphCacheStats().rasterizations);
		CHECK(backend.value->GetGlyph(backend.font, 'A', ColorFromRGB(color)));
		CHECK_EQ(2, backend.value->GetGlyphCacheStats().entries);
		CHECK(backend.value->GetGlyphCacheStats().pixelBytes <= bytes * 2);
	}
	const auto rendered = backend.value->GetGlyphCacheStats().rasterizations;
	CHECK(backend.value->GetGlyph(backend.font, 'B', white));
	CHECK_EQ(rendered + 1, backend.value->GetGlyphCacheStats().rasterizations);
}

TEST(GlyphCache, PixelBudgetBoundsSpritesAndRejectsAnOversizedGlyph)
{
	Backend probe;
	CHECK(probe.value->GetGlyph(probe.font, 'W', ColorFromRGB(0xFFFFFF)));
	const auto oneGlyph = probe.value->GetGlyphCacheStats().pixelBytes;
	CHECK(oneGlyph > 0);
	if (!oneGlyph) return;
	Backend bounded({100, oneGlyph * 2});
	for (int color = 0; color < 12; ++color) {
		CHECK(bounded.value->GetGlyph(bounded.font, 'W', ColorFromRGB(color)));
		CHECK(bounded.value->GetGlyphCacheStats().pixelBytes <= oneGlyph * 2);
		CHECK(bounded.value->GetGlyphCacheStats().entries <= 2);
	}
	Backend tooSmall({100, oneGlyph - 1});
	CHECK(tooSmall.value->GetGlyph(tooSmall.font, 'W', ColorFromRGB(0xFFFFFF)) == nullptr);
	CHECK_EQ(0, tooSmall.value->GetGlyphCacheStats().entries);
	CHECK_EQ(0, tooSmall.value->GetGlyphCacheStats().pixelBytes);
}

TEST(GlyphCache, AlphaIsAppliedAtDrawTimeAndNeverBakedIntoTheCachedSprite)
{
	Backend backend;
	Target target;
	CHECK(target.surface != nullptr);
	if (!target.surface) return;
	auto transparent = ColorFromRGB(0xFFFFFF);
	transparent.a = 0;
	const auto* glyph = backend.value->GetGlyph(backend.font, 'W', transparent);
	CHECK(glyph != nullptr);
	if (!glyph) return;
	const auto invisible = target.Draw(*backend.value, *glyph, 0);
	const auto faded = target.Draw(*backend.value, *glyph, 96);
	const auto opaque = target.Draw(*backend.value, *glyph, 255);
	CHECK(invisible == std::vector<uint16_t>(48 * 48, 0));
	CHECK(faded != invisible);
	CHECK(opaque != faded);
	CHECK(opaque == target.Draw(*backend.value, *glyph, 255));
	CHECK(glyph == backend.value->GetGlyph(backend.font, 'W', ColorFromRGB(0xFFFFFF)));
	CHECK_EQ(1, backend.value->GetGlyphCacheStats().entries);
	CHECK_EQ(1, backend.value->GetGlyphCacheStats().rasterizations);
}

TEST(GlyphCache, EvictedGlyphsRecreateTheSamePixelsAndMetrics)
{
	Backend backend({1, 1024 * 1024});
	Target target;
	CHECK(target.surface != nullptr);
	if (!target.surface) return;
	const auto white = ColorFromRGB(0xFFFFFF);
	const auto* glyph = backend.value->GetGlyph(backend.font, 'W', white);
	CHECK(glyph != nullptr);
	if (!glyph) return;
	const auto metrics = glyph->metrics;
	const auto before = target.Draw(*backend.value, *glyph, 255);
	CHECK(backend.value->GetGlyph(backend.font, 'A', white));
	glyph = backend.value->GetGlyph(backend.font, 'W', white);
	CHECK(glyph != nullptr);
	if (!glyph) return;
	CHECK(before == target.Draw(*backend.value, *glyph, 255));
	CHECK_EQ(metrics.advance, glyph->metrics.advance);
	CHECK_EQ(metrics.width, glyph->metrics.width);
	CHECK_EQ(metrics.height, glyph->metrics.height);
	CHECK_EQ(3, backend.value->GetGlyphCacheStats().rasterizations);
	CHECK_EQ(1, backend.value->GetGlyphCacheStats().entries);
}

TEST(GlyphCache, ZeroBudgetsAndInvalidFontsDoNotRetainSprites)
{
	for (const auto limits : {GlyphCacheLimits{0, 1024}, GlyphCacheLimits{10, 0}}) {
		Backend backend(limits);
		CHECK(backend.value->GetGlyph(backend.font, 'W', ColorFromRGB(0xFFFFFF)) == nullptr);
		CHECK(backend.value->GetGlyph(FontHandle{}, 'W', ColorFromRGB(0xFFFFFF)) == nullptr);
		CHECK_EQ(0, backend.value->GetGlyphCacheStats().entries);
		CHECK_EQ(0, backend.value->GetGlyphCacheStats().pixelBytes);
	}
}

TEST(GlyphCache, RepeatedNonBmpMeasurementAndColorsReuseMetrics)
{
	Backend backend;
	constexpr uint32_t smile = 0x1F600;
	GlyphMetrics expected{};
	CHECK(backend.value->GetGlyphMetrics(backend.font, smile, expected));
	for (int i = 0; i < 12; ++i) {
		GlyphMetrics measured{};
		CHECK(backend.value->GetGlyphMetrics(backend.font, smile, measured));
		CHECK_EQ(expected.advance, measured.advance);
		CHECK_EQ(expected.width, measured.width);
		const auto* glyph = backend.value->GetGlyph(backend.font, smile, ColorFromRGB(i));
		CHECK(glyph != nullptr);
		if (glyph) CHECK_EQ(expected.advance, glyph->metrics.advance);
	}
	const auto stats = backend.value->GetGlyphCacheStats();
	CHECK_EQ(1, stats.metricEntries);
	CHECK_EQ(1, stats.metricLookups);
	CHECK_EQ(1, stats.metricRasterizations);
}

TEST(GlyphCache, MetricEntriesHaveAnIndependentBoundAndKeepRecentHits)
{
	Backend backend({1, 1024 * 1024, 2});
	GlyphMetrics metrics{};
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'A', metrics));
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'B', metrics));
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'A', metrics));
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'C', metrics));
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'A', metrics));
	CHECK_EQ(2, backend.value->GetGlyphCacheStats().metricEntries);
	CHECK_EQ(3, backend.value->GetGlyphCacheStats().metricLookups);
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'B', metrics));
	CHECK_EQ(4, backend.value->GetGlyphCacheStats().metricLookups);
	CHECK_EQ(2, backend.value->GetGlyphCacheStats().metricEntries);
}

TEST(GlyphCache, MetricKeysSeparateFontsAndZeroBudgetRetainsNothing)
{
	Backend backend;
	FontDesc larger;
	larger.size = 32;
	const auto second = backend.value->AcquireFont(larger);
	CHECK(second.IsValid());
	GlyphMetrics firstMetrics{}, secondMetrics{};
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'W', firstMetrics));
	CHECK(backend.value->GetGlyphMetrics(second, 'W', secondMetrics));
	CHECK(secondMetrics.advance > firstMetrics.advance);
	CHECK_EQ(2, backend.value->GetGlyphCacheStats().metricEntries);
	CHECK(backend.value->GetGlyphMetrics(backend.font, 'W', firstMetrics));
	CHECK_EQ(2, backend.value->GetGlyphCacheStats().metricLookups);
	CHECK(!backend.value->GetGlyphMetrics(FontHandle{}, 'W', firstMetrics));
	CHECK_EQ(2, backend.value->GetGlyphCacheStats().metricLookups);

	Backend disabled({1, 1024 * 1024, 0});
	for (int i = 0; i < 2; ++i)
		CHECK(disabled.value->GetGlyphMetrics(disabled.font, 'W', firstMetrics));
	CHECK_EQ(0, disabled.value->GetGlyphCacheStats().metricEntries);
	CHECK_EQ(2, disabled.value->GetGlyphCacheStats().metricLookups);
}

TEST(GlyphCache, DrawingFirstSeedsMetricsWithoutAnExtraFallbackRasterization)
{
	Backend backend;
	const auto* glyph = backend.value->GetGlyph(backend.font, 0x1F600, ColorFromRGB(0xFFFFFF));
	CHECK(glyph != nullptr);
	if (!glyph) return;
	const auto expected = glyph->metrics;
	GlyphMetrics measured{};
	CHECK(backend.value->GetGlyphMetrics(backend.font, 0x1F600, measured));
	CHECK_EQ(expected.width, measured.width);
	CHECK_EQ(expected.height, measured.height);
	CHECK_EQ(expected.advance, measured.advance);
	CHECK_EQ(expected.bearingX, measured.bearingX);
	CHECK_EQ(expected.bearingY, measured.bearingY);
	const auto stats = backend.value->GetGlyphCacheStats();
	CHECK_EQ(1, stats.metricLookups);
	CHECK_EQ(0, stats.metricRasterizations);
	CHECK_EQ(1, stats.rasterizations);
}
