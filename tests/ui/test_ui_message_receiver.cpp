#include "test_framework.h"
#include "VS_UI_ui_result_receiver.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
C_VS_UI_UI_RESULT_RECEIVER* activeReceiver = nullptr;
std::vector<intptr_t> received;
bool throwOnce = false;
bool dispatchAgain = false;
std::vector<std::string> receivedText;
void* receivedPointer = nullptr;
intptr_t receivedRight = 0;

void ReceiveText(DWORD, intptr_t, intptr_t, void* text)
{
	receivedText.emplace_back(static_cast<const char*>(text));
	if (dispatchAgain)
	{
		dispatchAgain = false;
		activeReceiver->_DispatchMessage();
		receivedText.emplace_back(static_cast<const char*>(text));
	}
}

void ReceivePointer(DWORD, intptr_t, intptr_t right, void* pointer)
{
	receivedPointer = pointer;
	receivedRight = right;
}

void Receive(DWORD, intptr_t left, intptr_t, void*)
{
	received.push_back(left);
	if (throwOnce)
	{
		throwOnce = false;
		throw std::runtime_error("test receiver failure");
	}
	if (dispatchAgain)
	{
		dispatchAgain = false;
		activeReceiver->_DispatchMessage();
	}
}
}

TEST(UIMessageReceiver, QueuesUntilAReceiverIsInstalledAndDispatchesInOrder)
{
	C_VS_UI_UI_RESULT_RECEIVER receiver;
	received.clear();
	receiver._SendMessage(UI_CHAT_RETURN, 1);
	receiver._SendMessage(UI_CHAT_RETURN, 2);
	receiver._DispatchMessage();
	CHECK(received.empty());
	receiver.SetResultReceiver(Receive);
	receiver._DispatchMessage();
	receiver._DispatchMessage();
	receiver._DispatchMessage();
	CHECK(received == std::vector<intptr_t>({1, 2}));
}

TEST(UIMessageReceiver, AThrowingCallbackDoesNotLeaveTheMessageQueued)
{
	C_VS_UI_UI_RESULT_RECEIVER receiver;
	received.clear();
	receiver.SetResultReceiver(Receive);
	receiver._SendMessage(UI_CHAT_RETURN, 1);
	receiver._SendMessage(UI_CHAT_RETURN, 2);
	throwOnce = true;
	bool threw = false;
	try { receiver._DispatchMessage(); }
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	receiver._DispatchMessage();
	CHECK(received == std::vector<intptr_t>({1, 2}));
}

TEST(UIMessageReceiver, ReentrantDispatchConsumesTheNextMessageOnce)
{
	C_VS_UI_UI_RESULT_RECEIVER receiver;
	activeReceiver = &receiver;
	received.clear();
	receiver.SetResultReceiver(Receive);
	receiver._SendMessage(UI_CHAT_RETURN, 1);
	receiver._SendMessage(UI_CHAT_RETURN, 2);
	dispatchAgain = true;
	receiver._DispatchMessage();
	receiver._DispatchMessage();
	CHECK(received == std::vector<intptr_t>({1, 2}));
	activeReceiver = nullptr;
}

TEST(UIMessageReceiver, TextMessagesOwnCopiesUntilDeferredDispatch)
{
	C_VS_UI_UI_RESULT_RECEIVER receiver;
	receivedText.clear();
	receiver.SetResultReceiver(ReceiveText);
	char localText[] = "original";
	receiver._SendTextMessage(UI_CHAT_RETURN, 0, 0, localText);
	localText[0] = 'X';
	receiver._SendTextMessage(UI_CHAT_RETURN, 0, 0, "literal");
	receiver._SendTextMessage(UI_CHAT_RETURN, 0, 0, "");
	receiver._SendTextMessage(UI_CHAT_RETURN, 0, 0, std::string(4096, 'x'));
	for (int i = 0; i < 4; ++i)
		receiver._DispatchMessage();
	CHECK(receivedText == std::vector<std::string>({"original", "literal", "", std::string(4096, 'x')}));
}

TEST(UIMessageReceiver, RawPayloadsRemainBorrowedAndKeepPointerSizedArguments)
{
	C_VS_UI_UI_RESULT_RECEIVER receiver;
	int object = 42;
	receivedPointer = nullptr;
	receivedRight = 0;
	receiver.SetResultReceiver(ReceivePointer);
	const intptr_t address = reinterpret_cast<intptr_t>(&object);
	receiver._SendMessage(UI_CHAT_RETURN, 0, address, &object);
	receiver._DispatchMessage();
	CHECK(receivedPointer == &object);
	CHECK_EQ(address, receivedRight);
	CHECK_EQ(42, object);
}

TEST(UIMessageReceiver, ReentrantTextDispatchKeepsTheOuterPayloadAlive)
{
	C_VS_UI_UI_RESULT_RECEIVER receiver;
	activeReceiver = &receiver;
	receivedText.clear();
	receiver.SetResultReceiver(ReceiveText);
	receiver._SendTextMessage(UI_CHAT_RETURN, 0, 0, "first");
	receiver._SendTextMessage(UI_CHAT_RETURN, 0, 0, "second");
	dispatchAgain = true;
	receiver._DispatchMessage();
	CHECK(receivedText == std::vector<std::string>({"first", "second", "first"}));
	activeReceiver = nullptr;
}
