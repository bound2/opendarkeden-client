//----------------------------------------------------------------------
// MShopShelf.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MShopShelf.h"
#include "DebugLog.h"

//----------------------------------------------------------------------
// Static member
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// ItemClass에 맞게 메모리를 잡아주는 class table을 설정..
//----------------------------------------------------------------------
MShopShelf::FUNCTION_NEWSHELFCLASS
MShopShelf::s_NewShelfClassTable[MAX_SHELF] =
{
	MShopFixedShelf::NewShelf,
	MShopSpecialShelf::NewShelf,
	MShopUnknownShelf::NewShelf
};

//----------------------------------------------------------------------
//
//							 MShop Shelf
//
//----------------------------------------------------------------------
MShopShelf::MShopShelf()
{
	m_bShelfEnable = false;

	m_Version = 0;

	// 모두 NULL로 초기화
	for (int i=0; i<SHOP_SHELF_SLOT; i++)
	{
		m_pItems[i] = NULL;
	}
}

MShopShelf::~MShopShelf()
{
	Release();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// New Shelf
//----------------------------------------------------------------------
// ShelfClass에 맞는 class의 객체를 생성해서(new) 넘겨준다.
//----------------------------------------------------------------------
MShopShelf*		
MShopShelf::NewShelf(int ShelfClass)
{
	DEBUG_ADD_FORMAT("MShopShelf::NewShelf %d", ShelfClass);

	// The type arrives in a shop-list packet, so it is validated before it
	// indexes the factory table - one past the table is a code pointer to
	// call through. The two shop-list handlers return on NULL; MNPC and
	// GCShopVersionHandler pass in-range enumerators and never see one.
	if (ShelfClass < 0 || ShelfClass >= (int)MAX_SHELF)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] MShopShelf::NewShelf: invalid shelf type %d", ShelfClass);
		return NULL;
	}

	return (MShopShelf*)(*s_NewShelfClassTable[ShelfClass])();
};

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MShopShelf::Release()
{
	m_bShelfEnable = false;

	m_Version = 0;

	for (int i=0; i<SHOP_SHELF_SLOT; i++)
	{
		// 뭔가 있으면 지우고.. NULL로..
		if (m_pItems[i]!=NULL)
		{
			delete m_pItems[i];
		}

		m_pItems[i] = NULL;
	}
}

//----------------------------------------------------------------------
// Is Empty
//----------------------------------------------------------------------
bool		
MShopShelf::IsEmptySlot(unsigned int slot)
{
	if (slot >= SHOP_SHELF_SLOT)
	{
		return true;
	}
	
	return m_pItems[slot]==NULL;
}

//----------------------------------------------------------------------
// Get item
//----------------------------------------------------------------------
MItem*		
MShopShelf::GetItem(unsigned int slot) const
{
	if (slot >= SHOP_SHELF_SLOT)
	{
		return NULL;
	}

	return m_pItems[slot];
}
//----------------------------------------------------------------------
// Add Item
//----------------------------------------------------------------------
// 앞에서부터 검색해서 빈 곳에 추가한다.
//----------------------------------------------------------------------
bool		
MShopShelf::AddItem(MItem* pItem)
{
	for (int i=0; i<SHOP_SHELF_SLOT; i++)
	{
		// 빈 곳에 추가한다.
		if (m_pItems[i]==NULL)
		{
			if (SetItem( i, pItem ))
			{
				return true;
			}

			return false;
		}
	}

	return false;
}

//----------------------------------------------------------------------
// Set Item
//----------------------------------------------------------------------
bool		
MShopShelf::SetItem(unsigned int  slot, MItem* pItem)
{
	if (slot >= SHOP_SHELF_SLOT)
	{
		return false;
	}

	// 이전에 뭔가 있었으면..
	if (m_pItems[slot]!=NULL)
	{
		delete m_pItems[slot];
	}

	m_pItems[slot] = pItem;

	//------------------------------------------------------
	// Can the player use it?
	//------------------------------------------------------
	MItem::RefreshAffect( pItem );

	return true;
}

//----------------------------------------------------------------------
// Remove - Shelf에서 제거해서 return해준다.(외부에서 delete해야한다.)
//----------------------------------------------------------------------
MItem*		
MShopShelf::RemoveItem(unsigned int slot)
{
	if (slot >= SHOP_SHELF_SLOT)
	{
		return NULL;
	}

	// 임시로 저장하고 
	MItem* pRemoveItem = m_pItems[slot];

	// 내부에 있는 걸 지워준다.
	m_pItems[slot] = NULL;

	return pRemoveItem;
}

//----------------------------------------------------------------------
// Delete - Shelf 내부에 있는걸 지워준다.
//----------------------------------------------------------------------
void		
MShopShelf::DeleteItem(unsigned int slot)
{
	if (slot >= SHOP_SHELF_SLOT)
	{
		return;
	}

	// 이전에 뭔가 있었으면..
	if (m_pItems[slot]!=NULL)
	{
		delete m_pItems[slot];
	}

	// 없애준다.
	m_pItems[slot] = NULL;
}


//----------------------------------------------------------------------
//
//							MShop FixedShelf 
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//
// MShop FixedShelf
//
//----------------------------------------------------------------------
MShopFixedShelf::MShopFixedShelf()
{
	m_Name = "Normal";
}

MShopFixedShelf::~MShopFixedShelf()
{
}

//----------------------------------------------------------------------
//
// MShop SpecialShelf
//
//----------------------------------------------------------------------
MShopSpecialShelf::MShopSpecialShelf()
{
	m_Name = "Special";
}

MShopSpecialShelf::~MShopSpecialShelf()
{
}

//----------------------------------------------------------------------
//
// MShop UnknownShelf
//
//----------------------------------------------------------------------
MShopUnknownShelf::MShopUnknownShelf()
{
	m_Name = "Mysterious";
}

MShopUnknownShelf::~MShopUnknownShelf()
{
}

