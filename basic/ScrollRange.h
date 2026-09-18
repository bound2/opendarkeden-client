#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Basic {

// Position state shared by the UI scrollbar's input and drawing paths.
class ScrollRange
{
private:
	int m_pos_max = 0;
	int m_pos = 0;
	bool m_bl_reverse = false;

	void ClampPosition(int64_t position)
	{
		const int last = m_pos_max > 0 ? m_pos_max - 1 : 0;
		m_pos = static_cast<int>((std::clamp)(position, int64_t{0}, int64_t{last}));
	}

public:
	void ScrollUp(int pos = 1)
	{
		ClampPosition(int64_t{m_pos} + (m_bl_reverse ? int64_t{pos} : -int64_t{pos}));
	}

	void ScrollDown(int pos = 1)
	{
		ClampPosition(int64_t{m_pos} + (m_bl_reverse ? -int64_t{pos} : int64_t{pos}));
	}

	void SetScrollPos(int pos)
	{
		ClampPosition(pos);
	}

	void SetPixelPosition(int pixel, int origin, int extent, int tagExtent)
	{
		if (!CanScroll() || tagExtent < 0 || extent <= tagExtent)
		{
			m_pos = 0;
			return;
		}
		const int64_t travel = int64_t{extent} - tagExtent;
		const int64_t offset = (std::clamp)(int64_t{pixel} - origin - tagExtent / 2,
			int64_t{0}, travel);
		// Clamp before multiplying. Both positive factors are at most INT_MAX.
		ClampPosition(offset * m_pos_max / travel);
		if (m_bl_reverse) m_pos = m_pos_max - 1 - m_pos;
	}

	// Drawing reads the same state as dragging without temporarily mutating it.
	int GetThumbOffset(int extent, int tagExtent) const
	{
		if (!CanScroll() || tagExtent < 0 || extent <= tagExtent) return 0;
		const int position = m_bl_reverse ? m_pos_max - 1 - m_pos : m_pos;
		return static_cast<int>(int64_t{position} * (int64_t{extent} - tagExtent) / (m_pos_max - 1));
	}

	bool CanScroll() const { return m_pos_max > 1; }

	int GetScrollPos() const { return m_pos; }

	// Number of possible positions, including the zero position.
	void SetPosMax(int maximum)
	{
		m_pos = 0;
		m_pos_max = (std::max)(0, maximum);
	}

	// Counts from containers are capped before narrowing to the UI's int indices.
	void SetPositionCount(size_t count)
	{
		SetPosMax(static_cast<int>((std::min)(count,
			static_cast<size_t>((std::numeric_limits<int>::max)()))));
	}

	void SetItemCount(size_t count, size_t visibleRows)
	{
		if (count == 0 || visibleRows == 0)
			SetPosMax(0);
		else if (count <= visibleRows)
			SetPosMax(1);
		else
			SetPositionCount(count - visibleRows + 1);
	}

	void SetReverse(bool reverse) { m_bl_reverse = reverse; }
};

} // namespace Basic
