//----------------------------------------------------------------------
// CSDLGraphicsFlip.cpp
//----------------------------------------------------------------------
// Frame presentation stays in the executable, which owns g_pBack and
// connects dxlib's graphics wrapper to the SpriteLib back buffer.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "DXLib/CDirectDraw.h"
#include "SpriteLib/CSpriteSurface.h"

extern CSpriteSurface* g_pBack;

//----------------------------------------------------------------------
// Flip
//----------------------------------------------------------------------
// 한 프레임을 화면에 보여준다. g_pBack(게임이 매 프레임 그려넣는 백버퍼)을
// SDL2 렌더러에 올린 뒤 present한다.
//----------------------------------------------------------------------
void CSDLGraphics::Flip()
{
	if (m_pSDLRenderer == NULL)
	{
		return;
	}

	SDL_RenderClear(m_pSDLRenderer);

	if (g_pBack != NULL)
	{
		spritectl_surface_t backendSurface = g_pBack->GetBackendSurface();
		if (backendSurface != SPRITECTL_INVALID_SURFACE)
		{
			spritectl_present_surface(backendSurface, m_pSDLRenderer);
		}
	}

	SDL_RenderPresent(m_pSDLRenderer);
}
