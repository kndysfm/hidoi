// test_wahidx.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <hidoi/device.h>
#include <hidoi/tstring.h>
#include <hidoi/watcher.h>

#include <thread>
//
//static bool req_stop_;
//static std::thread th_watch_;
//
//static DWORD	g_main_tid, g_proc_tid;
//static HHOOK	g_hook;
//static HWND		g_hDlg;
//
//static BOOL CALLBACK con_handler(DWORD)
//{
//	PostThreadMessage(g_proc_tid, WM_QUIT, 0, 0);
//	return TRUE;
//};
//
//static LRESULT CALLBACK kb_proc(int code, WPARAM w, LPARAM l)
//{
//	PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)l;
//	LPCTSTR info = NULL;
//	if (w == WM_KEYDOWN)
//		info = _T("key dn");
//	else if (w == WM_KEYUP)
//		info = _T("key up");
//	else if (w == WM_SYSKEYDOWN)
//		info = _T("sys key dn");
//	else if (w == WM_SYSKEYUP)
//		info = _T("sys key up");
//	_tprintf(_T("%s - vkCode [%04x], scanCode [%04x]\n"), info, p->vkCode, p->scanCode);
//	// always call next hook
//	return CallNextHookEx(g_hook, code, w, l);
//};
//
//static LRESULT CALLBACK msg_proc(int code, WPARAM w, LPARAM l)
//{
//	LPCTSTR str_w;
//	if (w == PM_NOREMOVE) str_w = _T("PM_NOREMOVE");
//	else if (w == PM_REMOVE) str_w = _T("PM_REMOVE");
//	else str_w = _T("*******");
//	PMSG p = (PMSG)l;
//	LPCTSTR str_msg;
//	if (p->message == WM_INPUT) str_msg = _T("WM_INPUT");
//	else if (p->message == WM_DEVICECHANGE) str_msg = _T("WM_DEVICECHANGE");
//	else str_msg = _T("WM_****");
//
//	_tprintf(_T("[%s] %s(%04x) - wParam [%04x], lParam [%08x]\n"), str_w, str_msg, p->message, p->wParam, p->lParam);
//	// always call next hook
//	return CallNextHookEx(g_hook, code, w, l);
//}
//
//static BOOL CALLBACK proc_enum(HWND h, LPARAM)
//{
//	Tcout << _T("EnumWindows callback:") << h << _T("\r\n"); 
//
//	return TRUE;
//}
//
//static void proc_watch()
//{
//	_tprintf(_T("start thread \n"));
//
//	g_proc_tid = ::GetCurrentThreadId();
//	SetConsoleCtrlHandler(&con_handler, TRUE);
//	//g_hook = ::SetWindowsHookEx(WH_KEYBOARD_LL, &kb_proc,
//	g_hook = ::SetWindowsHookEx(WH_GETMESSAGE, &msg_proc,
//		::GetModuleHandle(NULL), // cannot be NULL, otherwise it will fail
//		::GetCurrentThreadId());// 0);
//	if (g_hook == NULL)
//	{
//		_tprintf(_T("SetWindowsHookEx failed with error %d\n"), ::GetLastError());
//		return;
//	};
//
//	//::EnumWindows(&proc_enum, NULL);
//
//	MSG msg;
//	//while (::GetMessage(&msg, NULL, 0, 0) && !req_stop_)
//	while (!req_stop_)
//	{
//		if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
//		{
//			::TranslateMessage(&msg);
//			::DispatchMessage(&msg);
//		}
//		std::this_thread::sleep_for(std::chrono::milliseconds(1));
//	};
//	::UnhookWindowsHookEx(g_hook);
//
//	_tprintf(_T("thread ended successfully \n"));
//	return;
//}
//
//static bool start_th()
//{
//	if (th_watch_.joinable()) return true;
//
//	req_stop_ = false;
//	std::thread th(proc_watch);
//	th_watch_.swap(th);
//	return th_watch_.joinable();
//}
//
//static void stop_th()
//{
//	if (th_watch_.joinable())
//	{
//		req_stop_ = true;
//		th_watch_.join();
//		if (!th_watch_.joinable())
//		{
//		}
//	}
//}
//
//#include <iostream>
//
//long __stdcall WindowProcedure(HWND window, unsigned int msg, WPARAM wp, LPARAM lp)
//{
//	switch (msg)
//	{
//	case WM_INPUT:
//		std::cout << "WM_INPUT \n";
//		break;
//	case WM_DEVICECHANGE:
//		std::cout << "WM_DEVICECHANGE \n";
//		break;
//
//	case WM_DESTROY:
//		std::cout << "\ndestroying window\n";
//		PostQuitMessage(0);
//		return 0L;
//	case WM_LBUTTONDOWN:
//		std::cout << "\nmouse left button down at (" << LOWORD(lp)
//			<< ',' << HIWORD(lp) << ")\n";
//		break;
//	default:
//		std::cout << '.';
//		break;
//	}
//	return DefWindowProc(window, msg, wp, lp);
//}

#include <Dbt.h>

static void DeviceChangeEventListener(DWORD_PTR dwData)
{
	Tcout << _T("\t DBT_DEVNODES_CHANGED received !! \n");
}

int main()
{
	//std::cout << "hello world!\n";
	//LPCTSTR myclass = _T("myclass");
	//WNDCLASSEX wndclass = { sizeof(WNDCLASSEX), CS_DBLCLKS, WindowProcedure,
	//	0, 0, GetModuleHandle(0), LoadIcon(0,IDI_APPLICATION),
	//	LoadCursor(0,IDC_ARROW), HBRUSH(COLOR_WINDOW + 1),
	//	0, myclass, LoadIcon(0,IDI_APPLICATION) };

	//if (RegisterClassEx(&wndclass))
	//{
	//	g_hDlg = CreateWindowEx(0, myclass, _T("title"),
	//		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	//		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, GetModuleHandle(0), 0);
	//	if (g_hDlg)
	//	{
	//		ShowWindow(g_hDlg, SW_SHOWDEFAULT);
	//		MSG msg;
	//		while (GetMessage(&msg, 0, 0, 0)) DispatchMessage(&msg);
	//	}
	//}

	//g_main_tid = ::GetCurrentThreadId();

	//start_th();

	auto &watcher = hidoi::Watcher::GetInstance();
	watcher.RegisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED, DeviceChangeEventListener);
	watcher.Start();

	auto paths = hidoi::Device::Enumerate();
	//for (auto p: paths)
	//{
	//	hidoi::Device dev;
	//	if (dev.Open(p))
	//	{
	//		_tprintf(_T("Succeeded to open a device:\r\n"));
	//		_tprintf(_T("\t Vendor ID: %04xH\r\n"), dev.GetVendorId());
	//		_tprintf(_T("\t Product ID: %04xH\r\n"), dev.GetProductId());
	//		_tprintf(_T("\t Usage Page - Usage: %04xH - %04xH\r\n"), dev.GetUsagePage(), dev.GetUsage());
	//		dev.Close();
	//	}
	//	else
	//	{
	//		_tprintf(_T("Failed to open a device\r\n"));
	//	}
	//}
	paths = hidoi::Device::SearchByUsage(0xFF11, 0x0002);
	for (auto p : paths)
	{
		hidoi::Device dev;
		if (dev.Open(p))
		{
			_tprintf(_T("Desired device is found:\r\n"));
			_tprintf(_T("\t Vendor ID: %04xH\r\n"), dev.GetVendorId());
			_tprintf(_T("\t Product ID: %04xH\r\n"), dev.GetProductId());
			_tprintf(_T("\t Usage Page - Usage: %04xH - %04xH\r\n"), dev.GetUsagePage(), dev.GetUsage());
			dev.Close();
		}
		else
		{
			_tprintf(_T("Failed to open a device\r\n"));
		}
	}

	_tprintf(_T("Press enter key\r\n"));
	_TCHAR buf[16];
	_getts_s(buf, sizeof(buf));
	watcher->Stop();
	_getts_s(buf, sizeof(buf));

    return 0;
}

