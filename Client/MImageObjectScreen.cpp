#include "Client_PCH.h"
#include "MImageObject.h"
#include "MTopView.h"

bool
MImageObject::IsWallTransPosition(int sX, int sY) const
{
		switch (m_X)
		{
			//-------------------------------------------------------------
			// 오른쪽으로 가면서 아래로 내려가는 벽
			//-------------------------------------------------------------
			case WALL_RIGHTDOWN :
			{
				// imageObject의 sector X좌표
				int objectSX = MTopView::PixelToMapX( m_PixelX );
		
				// x - y
				int value = objectSX - (int)m_Viewpoint;			
				
				if (sX-sY > value)
				{
					return true;
				}

				return false;
			}
			break;

			//-------------------------------------------------------------
			// 오른쪽으로 가면서 위로 올라가는 벽
			//-------------------------------------------------------------
			case WALL_RIGHTUP :
			{
				// imageObject의 sector X좌표
				int objectSX = MTopView::PixelToMapX( m_PixelX );
				
				// x + y
				int value = objectSX + (int)m_Viewpoint;			

				if (sX+sY < value)
				{
					return true;
				}

				return false;
			}
			break;
		}

	return true;
}
