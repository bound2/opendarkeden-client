//////////////////////////////////////////////////////////////////////
// 
// SocketEncryptOutputStream.h 
// 
// by Reiot
// 
//////////////////////////////////////////////////////////////////////

#ifndef __SOCKET_ENCRYPT_OUTPUT_STREAM_H__
#define __SOCKET_ENCRYPT_OUTPUT_STREAM_H__

// include files
#include "Types.h"
#include "Exception.h"
#include "SocketOutputStream.h"
#include "Encrypter.h"

const unsigned int DefaultSocketEncryptOutputBufferSize = 81920;

//////////////////////////////////////////////////////////////////////
//
// class SocketEncryptOutputStream
//
//////////////////////////////////////////////////////////////////////

class SocketEncryptOutputStream : public SocketOutputStream {

//////////////////////////////////////////////////
// constructor/destructor
//////////////////////////////////////////////////
public :
	
	// constructor
	SocketEncryptOutputStream (Socket* sock, uint BufferSize = DefaultSocketEncryptOutputBufferSize);
	~SocketEncryptOutputStream() {}
	
//////////////////////////////////////////////////
// methods
//////////////////////////////////////////////////
public :
	
	// write data to stream (output buffer)
	// *CAUTION*
	// std::string 을 버퍼에 writing 할 때, 자동으로 size 를 앞에 붙일 수도 있다.
	// 그러나, std::string 의 크기를 BYTE/WORD 중 어느 것으로 할 건지는 의문이다.
	// 패킷의 크기는 작을 수록 좋다는 정책하에서 필요에 따라서 std::string size 값을
	// BYTE 또는 WORD 를 수동으로 사용하도록 한다.
    uint writeEncrypt (bool   buf) { return write(m_Encrypter.convert(buf)); }
    uint writeEncrypt (char   buf) { return write(static_cast<char>(m_Encrypter.convert(buf))); }
    uint writeEncrypt (uchar  buf) { buf = m_Encrypter.convert(buf); return writeWire(buf); }
    uint writeEncrypt (short  buf) { buf = m_Encrypter.convert(buf); return writeWire(buf); }
    uint writeEncrypt (ushort buf) { buf = m_Encrypter.convert(buf); return writeWire(buf); }
    uint writeEncrypt (int    buf) { buf = m_Encrypter.convert(buf); return writeWire(buf); }
    uint writeEncrypt (uint   buf) { buf = m_Encrypter.convert(buf); return writeWire(buf); }
    uint writeEncrypt (long   buf) {
        int32_t tmp = static_cast<int32_t>(buf);
        tmp = static_cast<int32_t>(m_Encrypter.convert(static_cast<long>(tmp)));
        return writeWire(tmp);
    }
    uint writeEncrypt (ulong  buf) {
        uint32_t tmp = static_cast<uint32_t>(buf);
        tmp = static_cast<uint32_t>(m_Encrypter.convert(static_cast<ulong>(tmp)));
        return writeWire(tmp);
    }

	void	setEncryptCode(uchar code)	{ m_Encrypter.setCode(code); }
	uchar   getEncryptCode() const      { return m_Encrypter.getCode(); }

//////////////////////////////////////////////////
// attributes
//////////////////////////////////////////////////
private :
	Encrypter m_Encrypter;
	
};

#endif
