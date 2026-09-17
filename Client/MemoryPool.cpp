/********************************************************************
	created:	2003/12/05
	created:	5:12:2003   13:40
	filename: 	MemoryPool.cpp
	file ext:	cpp
	author:		sonee	// 손히승 바보
	
	purpose:	memory pool
				고정된 크기를 빈번하게 new/delete 하는 경우 메모리 풀을 사용하면
				메모리 단편화를 줄일 수 있다.

				메모리 leak 현상을 막을 수 있다.

				Debug 모드인 경우에는 메모리가 MEMORY_POOL_GARBAGE 값으로
				채워진다.
*********************************************************************/
#include "Client_PCH.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <memory.h>
#include "MemoryPool.h"

#include "MNPC.h"
#include "MFakeCreature.h"


MemoryPool g_CreatureMemoryPool( sizeof( MCreature ), 30 );
MemoryPool g_CreatureWearMemoryPool( sizeof( MCreatureWear ), 30 );
MemoryPool g_NPCCreatureMemoryPool( sizeof( MNPC ), 5 );
MemoryPool g_FakeCreatureMemoryPool( sizeof( MFakeCreature ), 30 );

MemoryPool::MemoryPool( int BlockSize, int BlockCount )
: m_pCurrentBlock ( NULL ), m_pFreeBlockList( NULL ), m_BlockSize( BlockSize ), m_BlockCount( BlockCount )
{
	if( BlockSize < sizeof( void* ) )
	{
		// -_- 포인터 크기보다 작으면 문제가 생길 것같은데-_-;
		m_BlockSize = sizeof( void* );
	}
}

MemoryPool::~MemoryPool()
{
	while( m_pCurrentBlock != NULL )
	{
		CBlock *pPrev = m_pCurrentBlock->m_pPrev;

		// Alloc() takes these chunks from ::operator new, so they have to go
		// back through ::operator delete. Pairing them with free() is
		// undefined behaviour.
		::operator delete( m_pCurrentBlock );

		m_pCurrentBlock = pPrev;
	}
}

void*		MemoryPool::Alloc()
{
	void *pMem;

	if( m_pFreeBlockList != NULL )					// FreeList 에 남아있는것이 있다면, 그 메모리 주소를 리턴.
	{
		pMem = m_pFreeBlockList;

		m_pFreeBlockList = m_pFreeBlockList->m_pPrev;
		return pMem;
	}

	if( m_pCurrentBlock == NULL || m_pCurrentBlock->m_leftBlocks <= 0 )
	{
		// The pool has not been allocated yet, or every block handed out by the
		// current chunk is in use: allocate a new chunk and link the previous
		// one behind it.
		// ::operator new throws std::bad_alloc on failure and never returns
		// NULL, so there is no null pointer to test for here. These pools back
		// a throwing operator new (see MCreature::operator new), which must not
		// hand a null block back to its caller either.
		CBlock *pPool = (CBlock*)( ::operator new( sizeof(CBlock) + ( m_BlockSize * m_BlockCount )) );

#ifdef _DEBUG
//		memset( (unsigned char*)(pPool) + sizeof( CBlock ), MEMORY_POOL_GARBAGE, m_BlockSize * m_BlockCount );
#endif
		pPool->m_pPrev = m_pCurrentBlock;
		pPool->m_leftBlocks = m_BlockCount;
		pPool->m_pNextBlock = reinterpret_cast<unsigned char*>( (pPool + 1) );

		m_pCurrentBlock = pPool;
	}	

	pMem = m_pCurrentBlock->m_pNextBlock;
	m_pCurrentBlock->m_pNextBlock += m_BlockSize;
	m_pCurrentBlock->m_leftBlocks --;

	return pMem;
}

void		MemoryPool::Free( void *pMem )
{
#ifdef _DEBUG
//	memset( pMem, MEMORY_POOL_GARBAGE, m_BlockSize );
#endif
	CFreeBlock *pBlock = reinterpret_cast<CFreeBlock*>(pMem);

	pBlock->m_pPrev = m_pFreeBlockList;
	m_pFreeBlockList = pBlock;
}

bool		MemoryPool::IsPtrInPool( void *pMem )
{
	CBlock *pCurBlock = m_pCurrentBlock;
	
	while( pCurBlock != NULL )
	{
        if( ( (unsigned char*)(pCurBlock) + sizeof( CBlock ) ) <= pMem && 
			( (unsigned char*)(pCurBlock) + sizeof( CBlock ) + m_BlockSize * m_BlockCount ) > pMem )
			return true;

		pCurBlock = pCurBlock->m_pPrev;
	}
	return false;
}

//----------------------------------------------------------------------------------
//
// 할당된 메모리안에 있으면서, FreeList 에 없으면 -_- 유효한 메모리이다.
//
//----------------------------------------------------------------------------------
bool		MemoryPool::IsAvailablePtr( void *pMem )
{
	CBlock *pCurBlock = m_pCurrentBlock;

	bool bIsInPool = false;
	
	while( pCurBlock != NULL )
	{
		if( ( (unsigned char*)(pCurBlock) + sizeof( CBlock ) <= pMem ) &&
			( (unsigned char*)(pCurBlock) + sizeof( CBlock ) + m_BlockSize * m_BlockCount > pMem ) )
		{
			bIsInPool = true;
			break;
		}

		pCurBlock = pCurBlock->m_pPrev;
	}
	if( !bIsInPool ) return false;
	
	CFreeBlock *pFreeBlock = m_pFreeBlockList;
	
	while( pFreeBlock != NULL )
	{
		if( pFreeBlock <= pMem && (pFreeBlock + m_BlockSize * m_BlockCount) > pMem )
			return false;

		pFreeBlock = pFreeBlock->m_pPrev;
	}

	return true;
}