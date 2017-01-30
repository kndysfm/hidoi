#include "watcher.h"
#include "tstring.h"
#include "ri.h"

#include <thread>
#include <mutex>
#include <map>

EXTERN_C
{
#include <hidsdi.h>
#include <Dbt.h>
}

using namespace hidoi;

struct Watcher::Impl
{
	std::mutex mtx_;
	bool req_stop_;
	std::thread th_watch_;
	HWND h_dlg_;
	HANDLE h_reg_not_;

	void proc_wm_input(WPARAM RawInputCode, HRAWINPUT hRawInput)
	{
		BOOL isForeground = (RawInputCode == RIM_INPUT);

		UINT dwSize;
		::GetRawInputData(hRawInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		if (dwSize == 0) return;

		LPBYTE data = new BYTE[dwSize];
		if (::GetRawInputData(hRawInput, RID_INPUT, data, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
		{
			RAWINPUT *pRawInput = (RAWINPUT *)data;
			auto const* inf_hid = RawInput::GetInstance().Search(pRawInput->header.hDevice);
			DWORD cnt = pRawInput->data.hid.dwCount;
			DWORD size = pRawInput->data.hid.dwSizeHid;
			pRawInput->header.hDevice;
			std::vector<BYTE> dat; 
			dat.resize(size);
			for (DWORD idx = 0; idx < cnt; idx++)
			{
				DWORD i_base = idx * size;
				for (DWORD i = 0; i < size; i++, i_base)
				{
					dat[i] = data[i_base];
				}
				//! pass dat vector to registerd functional object
			}
			pRawInput->data.hid.dwSizeHid;
		}
		delete[] data;
	}

	typedef std::function<Watcher::DeviceChangeEventListener> wm_devicechange_func;

	std::map<UINT, wm_devicechange_func> map_wm_devicechange_listeners_;

	bool add_wm_devicechange_func(UINT uEventType, wm_devicechange_func const &f)
	{
		if (map_wm_devicechange_listeners_.count(uEventType) != 0) return false;

		map_wm_devicechange_listeners_[uEventType] = f;
		return true;
	}

	void proc_wm_devicechange(UINT nEventType, DWORD_PTR dwData)
	{
		if (map_wm_devicechange_listeners_.count(nEventType) == 1)
			map_wm_devicechange_listeners_[nEventType](dwData);

		switch (nEventType)
		{
		case DBT_APPYBEGIN:
			::OutputDebugString(_T("\t DBT_APPYBEGIN \n")); break;
		case DBT_APPYEND:
			::OutputDebugString(_T("\t DBT_APPYEND \n")); break;
		case DBT_DEVNODES_CHANGED:
			::OutputDebugString(_T("\t DBT_DEVNODES_CHANGED \n")); break;
		case DBT_QUERYCHANGECONFIG:
			::OutputDebugString(_T("\t DBT_QUERYCHANGECONFIG \n")); break;
		case DBT_CONFIGCHANGED: 
			::OutputDebugString(_T("\t DBT_CONFIGCHANGED \n")); break;
		case DBT_CONFIGCHANGECANCELED:
			::OutputDebugString(_T("\t DBT_CONFIGCHANGECANCELED \n")); break;
		case DBT_MONITORCHANGE:
			::OutputDebugString(_T("\t DBT_MONITORCHANGE \n")); break;
		case DBT_SHELLLOGGEDON:
			::OutputDebugString(_T("\t DBT_SHELLLOGGEDON \n")); break;
		case DBT_CONFIGMGAPI32:
			::OutputDebugString(_T("\t DBT_CONFIGMGAPI32 \n")); break;
		case DBT_VXDINITCOMPLETE: 
			::OutputDebugString(_T("\t DBT_VXDINITCOMPLETE \n")); break;
		case DBT_VOLLOCKQUERYLOCK:
			::OutputDebugString(_T("\t DBT_VOLLOCKQUERYLOCK \n")); break;
		case DBT_VOLLOCKLOCKTAKEN:
			::OutputDebugString(_T("\t DBT_VOLLOCKLOCKTAKEN \n")); break;
		case DBT_VOLLOCKLOCKFAILED:
			::OutputDebugString(_T("\t DBT_VOLLOCKLOCKFAILED \n")); break;
		case DBT_VOLLOCKQUERYUNLOCK:
			::OutputDebugString(_T("\t DBT_VOLLOCKQUERYUNLOCK \n")); break;
		case DBT_VOLLOCKLOCKRELEASED:
			::OutputDebugString(_T("\t DBT_VOLLOCKLOCKRELEASED \n")); break;
		case DBT_VOLLOCKUNLOCKFAILED:
			::OutputDebugString(_T("\t DBT_VOLLOCKUNLOCKFAILED \n")); break;
		case DBT_DEVICEARRIVAL:
			::OutputDebugString(_T("\t DBT_DEVICEARRIVAL \n")); break;
		case DBT_DEVICEQUERYREMOVE:
			::OutputDebugString(_T("\t DBT_DEVICEQUERYREMOVE \n")); break;
		case DBT_DEVICEQUERYREMOVEFAILED:
			::OutputDebugString(_T("\t DBT_DEVICEQUERYREMOVEFAILED \n")); break;
		case DBT_DEVICEREMOVEPENDING:
			::OutputDebugString(_T("\t DBT_DEVICEREMOVEPENDING \n")); break;
		case DBT_DEVICEREMOVECOMPLETE:
			::OutputDebugString(_T("\t DBT_DEVICEREMOVECOMPLETE \n")); break;
		case DBT_DEVICETYPESPECIFIC:
			::OutputDebugString(_T("\t DBT_DEVICETYPESPECIFIC \n")); break;
		}
	}

	static LRESULT CALLBACK wnd_proc(HWND window, unsigned int msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_INPUT:
			::OutputDebugString(_T("WM_INPUT \n"));
			Watcher::GetInstance().pImpl->proc_wm_input(GET_RAWINPUT_CODE_WPARAM(wp), (HRAWINPUT)lp);
			break;
		case WM_DEVICECHANGE:
			::OutputDebugString(_T("WM_DEVICECHANGE \n"));
			Watcher::GetInstance().pImpl->proc_wm_devicechange((UINT)wp, (DWORD_PTR)lp);
			break;
		case WM_DESTROY:
			::OutputDebugString(_T("\nDestroying watcher window\n"));
			PostQuitMessage(0);
			return 0L;
		default:
			std::cout << '.';
			break;
		}
		return DefWindowProc(window, msg, wp, lp);
	}

	void register_hid_notification(void)
	{
		DEV_BROADCAST_DEVICEINTERFACE    broadcastInterface = {0};

		broadcastInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		broadcastInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		::HidD_GetHidGuid(&broadcastInterface.dbcc_classguid);

		h_reg_not_ = ::RegisterDeviceNotification(h_dlg_, &broadcastInterface,
			DEVICE_NOTIFY_WINDOW_HANDLE
		);

		if (h_reg_not_ == INVALID_HANDLE_VALUE)
		{
			::OutputDebugString(_T("Failed to register HID notification"));
		}
	}

	void unregister_hid_notification(void)
	{
		if (h_reg_not_ != INVALID_HANDLE_VALUE)
		{
			::UnregisterDeviceNotification(h_reg_not_);
		}
		h_reg_not_ = INVALID_HANDLE_VALUE;
	}

	bool proc_watch_msg_loop()
	{
		MSG msg;
		while (!req_stop_)
		{
			if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
			{
				BOOL ret = GetMessage(&msg, NULL, 0, 0);
				if (ret == -1)
				{
					::OutputDebugString(_T("Error"));
					return false;
				}
				else if (ret == 0)
				{
					::OutputDebugString(_T("Received WM_QUIT"));
					return false;
				}
				else
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		};
		return true;
	}

	void proc_watch()
	{
		LPCTSTR myclass = _T("hidoi::Watcher::Impl");
		WNDCLASSEX wndclass = { sizeof(WNDCLASSEX), CS_DBLCLKS, wnd_proc,
			0, 0, GetModuleHandle(NULL), LoadIcon(0,IDI_APPLICATION),
			LoadCursor(0,IDC_ARROW), HBRUSH(COLOR_WINDOW + 1),
			0, myclass, LoadIcon(0,IDI_APPLICATION) };

		if (RegisterClassEx(&wndclass))
		{
			h_dlg_ = CreateWindowEx(0, myclass, _T("watcher"),
				WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
				CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, GetModuleHandle(0), 0);
			if (h_dlg_)
			{
				register_hid_notification();
				proc_watch_msg_loop();
				unregister_hid_notification();
			}
			else
			{
				::OutputDebugString(_T("Failed to create a window"));
			}
		}
		else
		{
			::OutputDebugString(_T("Failed to register a windows class"));
		}

		return;
	}


	bool start()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		if (th_watch_.joinable()) return true;

		h_dlg_ = NULL;
		req_stop_ = false;
		std::thread th(std::bind(&Impl::proc_watch, this));
		th_watch_.swap(th);
		return th_watch_.joinable();
	}

	void stop()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		if (th_watch_.joinable())
		{
			if (h_dlg_)
			{
				::DestroyWindow(h_dlg_);
			}
			req_stop_ = true;
			th_watch_.join();
			if (!th_watch_.joinable())
			{
				h_dlg_ = NULL;
			}
		}
	}
};


Watcher::Watcher(): pImpl(new Impl) { }

Watcher::~Watcher() { }


Watcher & Watcher::GetInstance(void)
{
	static Watcher instance;

	std::lock_guard<std::mutex> lock(instance.pImpl->mtx_);
	return instance;
}

BOOL Watcher::Start()
{
	return pImpl->start();
}

void Watcher::Stop()
{
	pImpl->stop();
}

BOOL Watcher::RegisterDeviceChangeEventListener(UINT uEventType, std::function<DeviceChangeEventListener>  const & listener)
{
	return (BOOL) pImpl->add_wm_devicechange_func(uEventType, listener);
}