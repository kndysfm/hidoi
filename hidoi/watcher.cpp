#include "watcher.h"

#include <thread>

using namespace hidoi;

struct Watcher::Impl
{
	bool req_stop_;
	std::thread th_watch_;

	static DWORD   g_main_tid;
	static HHOOK   g_kb_hook;

	static BOOL CALLBACK con_handler(DWORD)
	{
		PostThreadMessage(g_main_tid, WM_QUIT, 0, 0);
		return TRUE;
	};

	static LRESULT CALLBACK kb_proc(int code, WPARAM w, LPARAM l)
	{
		PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)l;
		const char *info = NULL;
		if (w == WM_KEYDOWN)
			info = "key dn";
		else if (w == WM_KEYUP)
			info = "key up";
		else if (w == WM_SYSKEYDOWN)
			info = "sys key dn";
		else if (w == WM_SYSKEYUP)
			info = "sys key up";
		printf("%s - vkCode [%04x], scanCode [%04x]\n", info, p->vkCode, p->scanCode);
		// always call next hook
		return CallNextHookEx(g_kb_hook, code, w, l);
	};

	void proc_watch()
	{
		g_main_tid = ::GetCurrentThreadId();
		SetConsoleCtrlHandler(&con_handler, TRUE);
		g_kb_hook = ::SetWindowsHookEx(WH_KEYBOARD_LL, &kb_proc,
			::GetModuleHandle(NULL), // cannot be NULL, otherwise it will fail
			0);
		if (g_kb_hook == NULL)
		{
			printf("SetWindowsHookEx failed with error %d\n", ::GetLastError());
			return;
		};

		MSG msg;
		while (::GetMessage(&msg, NULL, 0, 0) && !req_stop_)
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		};
		::UnhookWindowsHookEx(g_kb_hook);

		return;
	}

	bool start()
	{
		if (th_watch_.joinable()) return true;

		req_stop_ = false;
		std::thread th(std::bind(&Impl::proc_watch, this));
		th_watch_.swap(th);
		return th_watch_.joinable();
	}

	void stop()
	{
		if (th_watch_.joinable())
		{
			req_stop_ = true;
			th_watch_.join();
			if (!th_watch_.joinable())
			{

			}
		}
	}
};

Watcher::Watcher(): pImpl(new Impl)
{

}

Watcher::~Watcher()
{
}

BOOL Watcher::Start()
{
	return pImpl->start();
}

void Watcher::Stop()
{
	pImpl->stop();
}
