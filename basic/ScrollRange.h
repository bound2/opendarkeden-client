#pragma once

#include <algorithm>

namespace Basic {

// Position state shared by the UI scrollbar's input and drawing paths.
class ScrollRange
{
protected:
	int m_pos_max = -1;
	int m_pos = 0;
	bool m_bl_reverse = false;

public:
	void ScrollUp(int pos = 1)
	{
		if (m_bl_reverse)
			m_pos = (std::min)(m_pos_max - 1, m_pos + pos);
		else
			m_pos = (std::max)(0, m_pos - pos);
	}

	void ScrollDown(int pos = 1)
	{
		if (m_bl_reverse)
			m_pos = (std::max)(0, m_pos - pos);
		else
			m_pos = (std::min)(m_pos_max - 1, m_pos + pos);
	}

	void SetScrollPos(int pos)
	{
		m_pos = (std::max)(0, (std::min)(m_pos_max - 1, pos));
	}

	void SetPixelPosition(int pixel, int origin, int extent, int tagExtent)
	{
		SetScrollPos((pixel - origin - tagExtent / 2) * m_pos_max / (extent - tagExtent));
	}

	int GetScrollPos() const { return m_pos; }

	// Number of possible positions, including the zero position.
	void SetPosMax(int maximum)
	{
		m_pos = 0;
		m_pos_max = maximum;
	}

	void SetReverse(bool reverse) { m_bl_reverse = reverse; }
};

} // namespace Basic
