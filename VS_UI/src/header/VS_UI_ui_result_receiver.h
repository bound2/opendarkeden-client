#ifndef __VS_UI_UI_RESULT_RECEIVER_H__
#define __VS_UI_UI_RESULT_RECEIVER_H__

#include "Typedef.h"
#include "VS_UI_UIMessage.h"
#include <deque>
#include <optional>
#include <string>


//
// left and right are intptr_t, not int, because several senders put a pointer
// in them - an MItem* or a c_str() - and int is 32 bits on 64-bit Windows, so
// the pointer arrived at the handler with its top half gone. The dispatch runs
// through a function pointer table, so every handler signature has to match
// this width or the assignment is a compile error, which is what keeps the two
// ends honest.
//
struct MESSAGE
{
	DWORD				message;
	intptr_t			left;
	intptr_t			right;
	void *			void_ptr;
	std::optional<std::string> text;
};

/*-----------------------------------------------------------------------------
  Class VS UI - UI Result Receiver

  `  message   .   process 
    message     process kill   .
	 message message queue .
-----------------------------------------------------------------------------*/
class C_VS_UI_UI_RESULT_RECEIVER
{
private:
	std::deque<MESSAGE> m_message_queue;

	void (*m_fp_result_receiver)(DWORD, intptr_t, intptr_t, void *);

public:
	C_VS_UI_UI_RESULT_RECEIVER();
	~C_VS_UI_UI_RESULT_RECEIVER();
	C_VS_UI_UI_RESULT_RECEIVER(const C_VS_UI_UI_RESULT_RECEIVER&) = delete;
	C_VS_UI_UI_RESULT_RECEIVER& operator=(const C_VS_UI_UI_RESULT_RECEIVER&) = delete;

	void _SendMessage(DWORD message, intptr_t left = 0, intptr_t right = 0, void *void_ptr = NULL);
	// Own a copy until dispatch finishes. The callback borrows void_ptr and
	// must neither retain nor delete it. Raw _SendMessage payloads stay borrowed.
	void _SendTextMessage(DWORD message, intptr_t left, intptr_t right, std::string text);
	void	_DispatchMessage();

/*-----------------------------------------------------------------------------
  Set.
-----------------------------------------------------------------------------*/
	void SetResultReceiver(void (*fp)(DWORD, intptr_t, intptr_t, void *));

#ifndef _LIB
	int	GetMessageSize() const { return static_cast<int>(m_message_queue.size()); }
#endif
};

#endif