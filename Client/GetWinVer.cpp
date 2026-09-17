#include "Client_PCH.h"

BOOL GetWinVersion(char *szVersion, size_t nSize)
{
   if (szVersion == NULL || nSize == 0)
      return FALSE;

#ifdef PLATFORM_WINDOWS
   // Windows implementation - simplified version
   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;

   // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
   {
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) )
         return FALSE;
   }

   switch (osvi.dwPlatformId)
   {
      // Test for the Windows NT product family.
      case VER_PLATFORM_WIN32_NT:
         if ( osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 )
            snprintf(szVersion, nSize, "%s", "Windows 10/11");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3 )
            snprintf(szVersion, nSize, "%s", "Windows 8.1");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2 )
            snprintf(szVersion, nSize, "%s", "Windows 8");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1 )
            snprintf(szVersion, nSize, "%s", "Windows 7");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 )
            snprintf(szVersion, nSize, "%s", "Windows Vista");
         else if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
            snprintf(szVersion, nSize, "%s", "Windows XP");
         else
            snprintf(szVersion, nSize, "Windows NT %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
         break;

      // Test for the Windows 95 product family.
      case VER_PLATFORM_WIN32_WINDOWS:
         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
             snprintf(szVersion, nSize, "%s", "Windows 95");
         else if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
             snprintf(szVersion, nSize, "%s", "Windows 98");
         else if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
             snprintf(szVersion, nSize, "%s", "Windows ME");
         else
             snprintf(szVersion, nSize, "%s", "Windows 9x");
         break;

      default:
         snprintf(szVersion, nSize, "%s", "Unknown Windows");
         break;
   }

   // Add build number if available
   if (osvi.dwBuildNumber > 0)
   {
      const size_t used = strlen(szVersion);
      if (used < nSize)
         snprintf(szVersion + used, nSize - used, " (Build %d)", osvi.dwBuildNumber & 0xFFFF);
   }

   return TRUE;

#else
   // Non-Windows platforms
   snprintf(szVersion, nSize, "%s", "Non-Windows Platform");
   return TRUE;
#endif
}
