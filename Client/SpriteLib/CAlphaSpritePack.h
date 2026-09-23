#include <fstream>
#include <cstdint>
//----------------------------------------------------------------------
// CAlphaSpritePack.h
//----------------------------------------------------------------------
//
// SpritePack의 Sprite에 대한 File Pointer Index가 필요하다.
// SpriteSet에서 읽기 위해서.
//
//----------------------------------------------------------------------

#ifndef	__CALPHASPRITEPACK_H__
#define	__CALPHASPRITEPACK_H__

#include "DrawTypeDef.h"
#include "CAlphaSprite565.h"
#include "CAlphaSprite555.h"
#include <memory>

class CAlphaSpritePack {
	public :
		CAlphaSpritePack();
		~CAlphaSpritePack();
		CAlphaSpritePack(const CAlphaSpritePack&) = delete;
		CAlphaSpritePack& operator=(const CAlphaSpritePack&) = delete;

		//------------------------------------------------------------
		// Init/Release
		//------------------------------------------------------------
		void		Init(TYPE_SPRITEID count, bool b565);		
		void		Release();
		void		ReleasePart(TYPE_SPRITEID firstSpriteID, TYPE_SPRITEID lastSpriteID);


		//------------------------------------------------------------
		// file I/O
		//------------------------------------------------------------
		bool		SaveToFile(std::ofstream& spkFile, std::ofstream& indexFile);
		bool		SaveToFileSpriteOnly(std::ofstream& spkFile, int32_t &filePosition);
		// Full reloads publish together. Partial loads report rejection and keep
		// earlier complete rows; a rejected decoded row becomes empty.
		bool		LoadFromFile(std::ifstream& file);
		bool		LoadFromFilePart(std::ifstream& file, int32_t filePosition,
								TYPE_SPRITEID firstSpriteID, TYPE_SPRITEID lastSpriteID);

		bool		LoadFromFileSprite(int spriteID, int fileSpriteID, std::ifstream& spkFile, std::ifstream& indexFile);
		bool		LoadFromFileSprite(int spriteID, int fileSpriteID, const char* spkFilename, const char* indexFilename);

		//--------------------------------------------------------
		// size
		//--------------------------------------------------------
		TYPE_SPRITEID	GetSize() const		{ return m_nSprites; }

		//------------------------------------------------------------
		// operator
		//------------------------------------------------------------
		CAlphaSprite&	operator [] (TYPE_SPRITEID n);

	private :
		void Swap(CAlphaSpritePack& other) noexcept;
		TYPE_SPRITEID	m_nSprites;		// CAlphaSprite의 개수
		bool m_rgb565 = true;
		std::unique_ptr<CAlphaSprite565[]> m_sprites565;
		std::unique_ptr<CAlphaSprite555[]> m_sprites555;
};

#endif


