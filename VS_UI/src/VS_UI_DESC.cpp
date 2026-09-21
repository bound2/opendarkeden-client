// Owned descriptor text and validated resource images.
#include "Client_PCH.h"
#include "VS_UI_ExtraDialog.h"
#include "VS_UI_filepath.h"
#include "DescriptorText.h"
#include "TextWrap.h"
#include "ResourceText.h"
#include "TextSystem/TextService.h"

#include <algorithm>
#include <limits>
#include <memory>

extern RECT g_GameRect;

namespace {
constexpr size_t MaxDescriptionSprites = 1024;

CSprite* CheckedSprite(CSpritePack* pack, int number)
{
	if (!pack || number < 0 || static_cast<unsigned>(number) >= pack->GetSize()) return nullptr;
	auto& sprite = (*pack)[static_cast<WORD>(number)];
	return sprite.IsInit() && sprite.GetWidth() > 0 && sprite.GetHeight() > 0 ? &sprite : nullptr;
}

void PrintDescription(long long x, long long y, const char* text, PrintInfo& info, COLORREF color)
{
	if (x < (std::numeric_limits<int>::min)() || x > (std::numeric_limits<int>::max)() ||
		y < (std::numeric_limits<int>::min)() || y > (std::numeric_limits<int>::max)()) return;
	g_PrintColorStr(static_cast<int>(x), static_cast<int>(y), text, info, color);
}
}

C_VS_UI_DESC::C_VS_UI_DESC()
{
	m_desc_col = m_desc_row = m_desc_scroll = 0;
	m_desc_x = m_desc_y = m_desc_title_x = m_desc_title_y = 0;
	m_desc_y_distance = 18;
	fontx = 6;
	m_color = BLACK;
	m_pi = gpC_base->m_dialog_msg_pi;
	m_title_color = RGB_WHITE;
	m_title_pi = gpC_base->m_desc_menu_pi;
	m_delimiter_pack = m_delimiter_sprite = 0;
}

C_VS_UI_DESC::~C_VS_UI_DESC()
{
	for (auto* picture : m_pC_inpicture) delete picture;
}

CSprite* C_VS_UI_DESC::GetDescSprite(int pack, int number) const
{
	if (pack < 0) return nullptr;
	const auto index = static_cast<size_t>(pack);
	if (index < m_pC_inpicture.size()) return CheckedSprite(m_pC_inpicture[index], number);
	const auto loaded = index - m_pC_inpicture.size();
	return loaded < m_descPictures.size() ? CheckedSprite(m_descPictures[loaded].get(), number) : nullptr;
}

void C_VS_UI_DESC::ShowDesc(int x, int y)
{
	if (m_desc_col <= 0 || m_desc_y_distance <= 0) return;
	const auto viewTop = static_cast<long long>(m_desc_y) + y;
	const auto viewBottom = viewTop + static_cast<long long>(m_desc_col) * m_desc_y_distance;
	int delimiterX = 0;
	if (!m_Sprite.empty()) {
		if (auto* delimiter = GetDescSprite(m_delimiter_pack, m_delimiter_sprite))
			delimiterX = delimiter->GetWidth() + PICTURE_INDENT;
	}
	for (const auto& picture : m_Sprite) {
		auto* sprite = GetDescSprite(picture.pack_num, picture.sprite_num);
		if (!sprite) continue;
		// The Christmas tree is top-aligned with an extra 200-pixel x offset.
		const bool tree = picture.pos == -300;
		const auto left = static_cast<long long>(m_desc_x) + x + (tree ? 200 : 0);
		const auto top = viewTop + (static_cast<long long>(tree ? 0 : picture.pos) - m_desc_scroll) * m_desc_y_distance;
		if (!gpC_base->m_p_DDSurface_back->Lock()) continue;
		S_SURFACEINFO surface{};
		gpC_base->m_p_DDSurface_back->GetSurfaceInfo(&surface);
		if (surface.p_surface && surface.width > 0 && surface.height > 0 &&
			surface.pitch > 0 && surface.pitch <= (std::numeric_limits<WORD>::max)() &&
			surface.pitch % sizeof(WORD) == 0 &&
			static_cast<long long>(surface.pitch) >= static_cast<long long>(surface.width) * sizeof(WORD)) {
			const auto right = (std::min)(static_cast<long long>(surface.width), static_cast<long long>(g_GameRect.right));
			const auto bottom = (std::min)(static_cast<long long>(surface.height), static_cast<long long>(g_GameRect.bottom));
			const auto clipLeft = (std::max)(0LL, -left);
			const auto clipTop = (std::max)({0LL, -top, viewTop - top});
			const auto clipRight = (std::min)(static_cast<long long>(sprite->GetWidth()), right - left);
			const auto clipBottom = (std::min)({static_cast<long long>(sprite->GetHeight()), bottom - top, viewBottom - top});
			if (clipLeft < clipRight && clipTop < clipBottom) {
				RECT source{static_cast<LONG>(clipLeft), static_cast<LONG>(clipTop),
					static_cast<LONG>(clipRight), static_cast<LONG>(clipBottom)};
				const size_t offset = static_cast<size_t>(top + clipTop) * static_cast<size_t>(surface.pitch) +
					static_cast<size_t>(left + clipLeft) * sizeof(WORD);
				auto* destination = reinterpret_cast<WORD*>(static_cast<BYTE*>(surface.p_surface) + offset);
				sprite->BltClipWidth(destination, static_cast<WORD>(surface.pitch), &source);
			}
		}
		gpC_base->m_p_DDSurface_back->Unlock();
	}
	if (!g_FL2_GetDC()) return;
	if (!m_desc_title.empty())
		PrintDescription(static_cast<long long>(m_desc_title_x) + x, static_cast<long long>(m_desc_title_y) + y,
			m_desc_title.c_str(), m_title_pi, m_title_color);
	const auto first = static_cast<size_t>(m_desc_scroll);
	const auto end = (std::min)(m_desc.size(), first + static_cast<size_t>(m_desc_col));
	for (size_t i = first; i < end; ++i) {
		const auto& row = m_desc[i];
		const bool tab = !row.empty() && row.front() == '\t';
		PrintDescription(static_cast<long long>(m_desc_x) + x + (tab ? delimiterX : 0),
			viewTop + static_cast<long long>(i - first) * m_desc_y_distance,
			row.c_str() + (tab ? 1 : 0), m_pi, m_color);
	}
	g_FL2_ReleaseDC();
}

bool C_VS_UI_DESC::LoadDesc(const char* filename, int row, int col, bool title, int coreZap)
{
	if (!m_pack_file.OpenText(filename)) return false;
	struct CloseReader {
		CRarFile& reader;
		~CloseReader() { reader.Release(); }
	} close{m_pack_file};
	return LoadDescText(std::string_view(m_pack_file.GetFilePointer(), m_pack_file.GetRemainingSize()),
		row, col, title, coreZap, true);
}

bool C_VS_UI_DESC::LoadDescFromString(const char* text, int row, int col, bool title, int coreZap)
{
	if (!text || std::string_view(text).size() > ResourceText::MaxFileBytes) return false;
	return LoadDescText(TextSystem::TextService::NormalizeText(text), row, col, title, coreZap, false);
}

bool C_VS_UI_DESC::LoadDescText(std::string_view text, int row, int col, bool title, int coreZap, bool tags)
{
	if (row <= 0 || col <= 0 || fontx <= 0 || m_desc_y_distance <= 0 || coreZap < -1 ||
		text.size() > ResourceText::MaxFileBytes || text.find('\0') != std::string_view::npos ||
		m_pC_inpicture.size() > MaxDescriptionSprites) return false;
	DescriptorText::Layout layout(static_cast<size_t>(row));
	std::vector<DESC_SPRITE> pictures;
	std::vector<size_t> relativePictures;
	std::vector<std::unique_ptr<CSpritePack>> loaded;
	int pack = 0, delimiterPack = 0, delimiterSprite = 0;
	bool delimiterSpecified = false;
	auto getSprite = [&](int packIndex, int spriteIndex) -> CSprite* {
		if (packIndex < 0) return nullptr;
		const auto index = static_cast<size_t>(packIndex);
		if (index < m_pC_inpicture.size()) return CheckedSprite(m_pC_inpicture[index], spriteIndex);
		const auto local = index - m_pC_inpicture.size();
		return local < loaded.size() ? CheckedSprite(loaded[local].get(), spriteIndex) : nullptr;
	};
	auto addPicture = [&](int packIndex, int spriteIndex, int position, bool relative) {
		if (pictures.size() >= MaxDescriptionSprites || !getSprite(packIndex, spriteIndex)) return false;
		if (relative) relativePictures.push_back(pictures.size());
		pictures.push_back({packIndex, spriteIndex, position});
		return true;
	};
	auto heightRows = [&](const CSprite& sprite) {
		return (static_cast<size_t>(sprite.GetHeight()) - 1) / static_cast<size_t>(m_desc_y_distance) + 1;
	};
	auto insetColumns = [&](const CSprite& sprite) {
		const auto width = (static_cast<size_t>(sprite.GetWidth()) + PICTURE_INDENT - 1) / static_cast<size_t>(fontx);
		return width > 0 ? width - 1 : 0;
	};

	CSprite* headerPicture = nullptr;
	size_t headerRows = 0;
	for (const auto& source : m_rep_string) {
		const auto header = TextSystem::TextService::NormalizeText(source);
		if (header.find('\0') != std::string::npos) return false;
		if (!header.empty() && header.front() == '%') {
			int spriteIndex = 0;
			if (!DescriptorText::ReadIndex(std::string_view(header).substr(1), spriteIndex) ||
				!addPicture(0, spriteIndex, 0, false)) return false;
			if (!headerPicture) headerPicture = getSprite(0, spriteIndex);
		} else {
			if (!layout.AppendHeader(header)) return false;
			++headerRows;
		}
	}
	if (headerPicture && !layout.BeginImage(insetColumns(*headerPicture), heightRows(*headerPicture))) return false;

	while (!text.empty()) {
		const auto line = TextSystem::NextUtf8Line(text, text.size(), {.skipSeamSpace = false});
		text.remove_prefix(line.consumed);
		if (tags && !line.text.empty() &&
			(line.text.front() == '&' || line.text.front() == '(' || line.text.front() == ')' || line.text.front() == '%')) {
			int index = 0;
			if (!DescriptorText::ReadIndex(std::string_view(line.text).substr(1), index)) return false;
			switch (line.text.front()) {
			case '&': pack = index; break;
			case '(': delimiterPack = index; delimiterSpecified = true; break;
			case ')': delimiterSprite = index; delimiterSpecified = true; break;
			case '%':
				if (headerRows != 0) {
					if (loaded.size() >= MaxDescriptionSprites) return false;
					auto picture = std::make_unique<CSpritePack>();
					picture->Init(coreZap == -1 ? 1 : 2);
					if (!picture->LoadFromFileData(0, index, SPK_ITEM, SPKI_ITEM) ||
						(coreZap != -1 && !picture->LoadFromFileData(1, coreZap, SPK_ITEM, SPKI_ITEM))) return false;
					const auto packIndex = static_cast<int>(m_pC_inpicture.size() + loaded.size());
					loaded.push_back(std::move(picture));
					if (!addPicture(packIndex, 0, index == 371 ? -300 : 0, false) ||
						(coreZap != -1 && !addPicture(packIndex, 1, 0, false))) return false;
					const auto height = heightRows(*getSprite(packIndex, 0));
					if (!layout.BeginImage(0, height > headerRows ? height - headerRows : 0)) return false;
				} else {
					auto* picture = getSprite(pack, index);
					if (!picture || !layout.BeginImage(insetColumns(*picture), heightRows(*picture)) ||
						!addPicture(pack, index, static_cast<int>(layout.RowCount()), true)) return false;
				}
				break;
			}
		} else if (!layout.AppendLine(line.text)) {
			return false;
		}
	}
	if (delimiterSpecified && !getSprite(delimiterPack, delimiterSprite)) return false;
	std::vector<std::string> rows;
	std::string newTitle = m_desc_title;
	size_t removedRows = 0;
	if (!layout.Complete(title, rows, newTitle, &removedRows)) return false;
	for (const auto index : relativePictures) pictures[index].pos -= static_cast<int>(removedRows);
	m_desc = std::move(rows);
	m_desc_title = std::move(newTitle);
	m_Sprite = std::move(pictures);
	m_descPictures = std::move(loaded);
	m_desc_row = row;
	m_desc_col = col;
	m_desc_scroll = 0;
	m_delimiter_pack = delimiterPack;
	m_delimiter_sprite = delimiterSprite;
	return true;
}
