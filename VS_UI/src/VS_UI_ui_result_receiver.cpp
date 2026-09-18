// VS_UI_ui_result_receiver.cpp

#include "Client_PCH.h"
#include <assert.h>
#include "VS_UI_ui_result_receiver.h"
#include <utility>

/*-----------------------------------------------------------------------------
- C_VS_UI_UI_RESULT_RECEIVER
-
-----------------------------------------------------------------------------*/
C_VS_UI_UI_RESULT_RECEIVER::C_VS_UI_UI_RESULT_RECEIVER()
{
	m_fp_result_receiver = NULL;
}

/*-----------------------------------------------------------------------------
- ~C_VS_UI_UI_RESULT_RECEIVER
-
-----------------------------------------------------------------------------*/
C_VS_UI_UI_RESULT_RECEIVER::~C_VS_UI_UI_RESULT_RECEIVER()
{

}

/*-----------------------------------------------------------------------------
- SetResultReceiver
-
-----------------------------------------------------------------------------*/
void C_VS_UI_UI_RESULT_RECEIVER::SetResultReceiver(void (*fp)(DWORD, intptr_t, intptr_t, void *))
{
	assert(fp);
	
	m_fp_result_receiver = fp;
}

/*-----------------------------------------------------------------------------
- SendMessage
- Message queue에 message를 넣는다.
-----------------------------------------------------------------------------*/
void C_VS_UI_UI_RESULT_RECEIVER::_SendMessage(DWORD message, intptr_t left, intptr_t right,
															 void *void_ptr)
{
	assert(message != INVALID_MESSAGE);

	m_message_queue.push_back({message, left, right, void_ptr, std::nullopt});
}

void C_VS_UI_UI_RESULT_RECEIVER::_SendTextMessage(
	DWORD message, intptr_t left, intptr_t right, std::string text)
{
	assert(message != INVALID_MESSAGE);
	m_message_queue.push_back({message, left, right, nullptr, std::move(text)});
}

/*-----------------------------------------------------------------------------
- DispatchMessage
- 가장 빨리 저장된 message를 한 개 보낸다. 그리고 그것을 queue에서 삭제한다.
-----------------------------------------------------------------------------*/
void C_VS_UI_UI_RESULT_RECEIVER::_DispatchMessage()
{
	if (m_fp_result_receiver == nullptr || m_message_queue.empty())
		return;

	// Consume before invoking application code: callbacks may dispatch again
	// or throw. The local message keeps owned text alive through either path.
	MESSAGE data = std::move(m_message_queue.front());
	m_message_queue.pop_front();
	m_fp_result_receiver(data.message, data.left, data.right,
		data.text ? data.text->data() : data.void_ptr);
}
