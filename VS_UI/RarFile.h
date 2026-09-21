// RarFile.h: interface for the CRarFile class.
// Modified for cross-platform support without RAR dependency
//////////////////////////////////////////////////////////////////////

#ifndef _RAR_FILE_HEADER_
#define _RAR_FILE_HEADER_

#pragma warning(disable:4786)

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include "Platform.h"
#endif
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * CRarFile - Cross-platform file reader that works with extracted RAR content
 *
 * Files are read beside the named archive: Data/Info/infodata.rpk selects
 * Data/Info/. Archive decompression is not implemented here.
 *
 * This avoids dependency on unrar library and improves cross-platform compatibility
 */

class CRarFile
{
private:
	std::string m_rar_filename;
	std::string m_password;
	std::string m_base_dir;      // Extracted directory path

	char *m_data;
	char *m_file_pointer;
	int m_size;
	bool m_text = false;
	bool OpenLimited(const char* filename, bool text);

public:
	// Constructor
	CRarFile();
	CRarFile(const char *rar_filename, const char *pass);

	// Destructor
	~CRarFile();
	CRarFile(const CRarFile&) = delete;
	CRarFile& operator=(const CRarFile&) = delete;

	// Release resources
	void Release();

	// Set RAR file path (converted to directory path)
	void SetRAR(const char *rar_filename, const char *pass);

	// Open raw bytes from the extracted directory. Failed opens close old data.
	bool Open(const char *in_filename);
	// Decode once using the resource page or UTF-8 BOM, with a 16 MiB input cap.
	// GetString then clips only at UTF-8 scalar boundaries, consuming each line.
	bool OpenText(const char *in_filename);

	// Exact reads reject negative/oversized requests without changing the cursor
	// or destination. The caller supplies at least size writable destination bytes.
	char*	Read(char *buf, int size);
	char*	Read(int size);
	bool	GetString(char* buf, int size);

	// Check if file is ready
	bool	IsSet()	{ return (m_data != NULL); }

	// Check if EOF
	bool	IsEOF(int plus = 0);

	// Caller owns the returned list, even when empty. Archive listing is a stub.
	std::vector<std::string> *GetList(char *filter = NULL);

	char* GetFilePointer(){return m_file_pointer;};
	size_t GetRemainingSize() const {
		return m_file_pointer ? static_cast<size_t>(m_size - (m_file_pointer - m_data)) : 0;
	}
};

#endif
