//----------------------------------------------------------------------
// MMusic.h
//----------------------------------------------------------------------
/*

  notify처리 방법

  윈도우 핸들(g_hWnd)을 대입한 상태이면..
  그 윈도우로 MM_MCINOTIFY가 간다.

	case MM_MCINOTIFY :
		{
			// Mid가 다 연주되었다는 말이다.
			if (wParam==MCI_NOTIFY_SUCCESSFUL)
			{
				// 반복해서 연주한다.
				g_Music.RePlay();
			}
		}
*/
//----------------------------------------------------------------------
#ifndef __MMUSIC_H__
#define __MMUSIC_H__

/* See MMusic.cpp: __USE_REAL_MIDI__ is never defined, so the real Win32 MCI/
   MIDI implementation (and its <MMSystem.h> dependency) is never built. */
#if defined(PLATFORM_WINDOWS) && defined(__USE_REAL_MIDI__)
#include <MMSystem.h>
#endif


// 클래스
class MMusic
{
protected:
	HWND		m_hwnd;


	char		MIDI_ERROR_MSG[80];				// the last MCI error text

public:
	bool		m_bInit;
	bool		m_bLoad;						// 화일이 로드중인지?
	bool		m_bPlay;						// 음악이 진행중인지?
	bool		m_bPause;						// 정지시켰나?
	WORD		m_Volume;

	
protected:
	bool		ErrorMsg();						// 메세지 출력


public:
	MMusic();
	~MMusic();
	
	bool		Init(HWND);
	void		UnInit();

	bool		IsInit() const		{ return m_bInit; }

	bool		IsPlay() const		{ return m_bPlay; }
	bool		IsPause() const		{ return m_bLoad && m_bPause; }//m_bLoad && !m_bPlay; }

	bool		Play(LPCSTR filename);
	bool		Stop();
	bool		Pause();
	bool		Resume();
	bool		RePlay();

	void		SetVolume(WORD volume);
	WORD		GetVolume()			{ return m_Volume; }

};

extern MMusic		g_Music;

#endif