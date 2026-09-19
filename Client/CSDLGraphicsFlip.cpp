//----------------------------------------------------------------------
// CSDLGraphicsFlip.cpp
//----------------------------------------------------------------------
// Frame presentation stays in the executable, which owns g_pBack and
// connects dxlib's graphics wrapper to the SpriteLib back buffer.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "ClientMain.h"
#include "DXLib/CDirectDraw.h"
#include "SpriteLib/CSpriteSurface.h"

extern CSpriteSurface* g_pBack;

// Set by the lifecycle watch in SDLMain.cpp between
// SDL_APP_WILLENTERBACKGROUND and SDL_APP_DIDENTERFOREGROUND: a mobile
// OS kills a process that touches its window while it is in the
// background, so the frame is composed as usual and not presented.
// Atomic because the watch runs on the thread that raised the event -
// the Java UI thread on Android - not the game's.
std::atomic<bool> g_bPresentSuspended(false);

//----------------------------------------------------------------------
// Flip
//----------------------------------------------------------------------
// Shows one frame: uploads g_pBack (the back buffer the game draws into
// every frame) to the SDL2 renderer and presents it.
//----------------------------------------------------------------------
void CSDLGraphics::Flip()
{
	if (m_pSDLRenderer == NULL || g_bPresentSuspended.load())
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
