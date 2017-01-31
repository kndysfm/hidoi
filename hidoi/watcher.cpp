#include "watcher.h"
#include "tstring.h"
#include "ri.h"

#include <thread>
#include <mutex>
#include <map>
#include <condition_variable>

EXTERN_C
{
#include <hidsdi.h>
#include <Dbt.h>
}

using namespace hidoi;

struct Watcher::Impl
{
	std::mutex mtx_listeners_, mtx_th_watch_;
	bool req_stop_;
	std::thread th_watch_;
	HWND h_dlg_;
	HANDLE h_reg_notif_;
	std::condition_variable cond_started_;

	typedef std::pair<Watcher::Target, std::function<Watcher::RawInputEventListener>> wm_input_listner_t;
	std::vector<wm_input_listner_t> listeners_;

	
	wm_input_listner_t * get_wm_input_listner(RawInput &ri)
	{
		for (auto &l : listeners_)
		{
			if (ri.Test(l.first.VendorId, l.first.ProductId, l.first.UsagePage, l.first.Usage))
			{
				return &l;
			}
		}

		return nullptr;
	}

	void register_wm_input_targets(DWORD_PTR)
	{
		auto rids = RawInput::Enumerate();
		for (auto &r : rids)
		{
			if (r.IsRegistered(h_dlg_))
			{	// already registered
				bool is_target = false;
				for (auto &l : listeners_)
				{
					if (r.Test(l.first.VendorId, l.first.ProductId, l.first.UsagePage, l.first.Usage))
					{
						is_target = true;
						break;
					}
				}
				if (!is_target) r.Unregister(h_dlg_); // this device should become non target
			}
			else
			{	// not registered yet
				for (auto &l : listeners_)
				{
					if (r.Test(l.first.VendorId, l.first.ProductId, l.first.UsagePage, l.first.Usage))
					{	// it's target to listen event
						r.Register(h_dlg_);
						break;
					}
				}
			}
		}
	}

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
			DWORD cnt = pRawInput->data.hid.dwCount;
			DWORD size = pRawInput->data.hid.dwSizeHid;
			BYTE const *raw = pRawInput->data.hid.bRawData;
			std::vector<BYTE> rep_in;
			rep_in.resize(size);
			auto ris = RawInput::SearchByHandle(pRawInput->header.hDevice);
			for (auto &ri : ris)
			{
				std::lock_guard<std::mutex> lock(mtx_listeners_);
				wm_input_listner_t *lsnr = get_wm_input_listner(ri);
				if (lsnr)
				{
					for (DWORD idx = 0; idx < cnt; idx++)
					{
						BYTE const *src = raw;
						for (DWORD i = 0; i < size; i++, src++)
						{
							rep_in[i] = *src;
						}
						// pass dat vector to registerd functional object
						lsnr->second(rep_in);
						raw += size;
					}
				}
			}
		}
		delete[] data;
	}

	bool add_wm_input_func(Watcher::Target const &tgt, std::function<RawInputEventListener>  const &listener)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::mutex> lock(mtx_listeners_);

		wm_input_listner_t l(tgt, listener);
		listeners_.push_back(l);
		register_wm_input_targets(0);

		return true;
	}

	bool remove_wm_input_func(Watcher::Target const &tgt)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::mutex> lock(mtx_listeners_);
		for (auto itr = listeners_.begin(); itr != listeners_.end(); itr++)
		{
			auto const &t = itr->first;
			if (t.VendorId == tgt.VendorId && t.ProductId == tgt.ProductId &&
				t.UsagePage == tgt.UsagePage && t.Usage == tgt.Usage)
			{
				listeners_.erase(itr);
				register_wm_input_targets(0); // update
				return true;
			}
		}

		return false;
	}

	typedef std::function<Watcher::DeviceChangeEventListener> wm_devicechange_func;

	std::map<UINT, std::pair<wm_devicechange_func, std::thread>> map_wm_devicechange_listeners_;

	bool add_wm_devicechange_func(UINT uEventType, wm_devicechange_func const &f)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::mutex> lock(mtx_listeners_);
		if (map_wm_devicechange_listeners_.count(uEventType) != 0) return false;

		map_wm_devicechange_listeners_[uEventType].first = f;
		return true;
	}

	bool remove_wm_devicechange_func(UINT uEventType)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::mutex> lock(mtx_listeners_);
		if (map_wm_devicechange_listeners_.count(uEventType) != 1) return false;

		map_wm_devicechange_listeners_.erase(uEventType);
		return true;
	}

	void proc_wm_devicechange_async(UINT nEventType, DWORD_PTR dwData)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		std::lock_guard<std::mutex> lock(mtx_listeners_);
		map_wm_devicechange_listeners_[nEventType].first(dwData);
		map_wm_devicechange_listeners_[nEventType].second.detach();
	}

#define DBT_ANY	(0xFFFF)

	void proc_wm_devicechange_start_thread(UINT nEventType, DWORD_PTR dwData)
	{
		std::lock_guard<std::mutex> lock(mtx_listeners_);
		if (map_wm_devicechange_listeners_.count(nEventType) == 1)
		{
			std::thread &th_proc = map_wm_devicechange_listeners_[nEventType].second;
			if (!th_proc.joinable())
			{
				std::thread th(std::bind(&Watcher::Impl::proc_wm_devicechange_async, this, nEventType, dwData));
				th_proc.swap(th);
			}
		}
	}
	
	void proc_wm_devicechange(UINT nEventType, DWORD_PTR dwData)
	{
		proc_wm_devicechange_start_thread(nEventType, dwData);
		proc_wm_devicechange_start_thread(DBT_ANY, 0);

		switch (nEventType)
		{
		default: utils::trace(_T("DBT_unknown(%04xH)"), nEventType); break;
#define CASE_DBT(S)	case S: utils::trace(_T(#S) _T("(%04xH)"), S); break
		CASE_DBT(DBT_APPYBEGIN);
		CASE_DBT(DBT_APPYEND);
		CASE_DBT(DBT_DEVNODES_CHANGED);
		CASE_DBT(DBT_QUERYCHANGECONFIG);
		CASE_DBT(DBT_CONFIGCHANGED);
		CASE_DBT(DBT_CONFIGCHANGECANCELED);
		CASE_DBT(DBT_MONITORCHANGE);
		CASE_DBT(DBT_SHELLLOGGEDON);
		CASE_DBT(DBT_CONFIGMGAPI32);
		CASE_DBT(DBT_VXDINITCOMPLETE);
		CASE_DBT(DBT_VOLLOCKQUERYLOCK);
		CASE_DBT(DBT_VOLLOCKLOCKTAKEN);
		CASE_DBT(DBT_VOLLOCKLOCKFAILED);
		CASE_DBT(DBT_VOLLOCKQUERYUNLOCK);
		CASE_DBT(DBT_VOLLOCKLOCKRELEASED);
		CASE_DBT(DBT_VOLLOCKUNLOCKFAILED);
		CASE_DBT(DBT_DEVICEARRIVAL);
		CASE_DBT(DBT_DEVICEQUERYREMOVE);
		CASE_DBT(DBT_DEVICEQUERYREMOVEFAILED);
		CASE_DBT(DBT_DEVICEREMOVEPENDING);
		CASE_DBT(DBT_DEVICEREMOVECOMPLETE);
		CASE_DBT(DBT_DEVICETYPESPECIFIC);
#undef CASE_DBT
		}
	}

	static LRESULT CALLBACK wnd_proc(HWND window, unsigned int msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_INPUT:
			utils::trace(_T("WM_INPUT"));
			Watcher::GetInstance().pImpl->proc_wm_input(GET_RAWINPUT_CODE_WPARAM(wp), (HRAWINPUT)lp);
			break;
		case WM_DEVICECHANGE:
			utils::trace(_T("WM_DEVICECHANGE"));
			Watcher::GetInstance().pImpl->proc_wm_devicechange((UINT)wp, (DWORD_PTR)lp);
			break;
		case WM_DESTROY:
			utils::trace(_T("Destroying watcher window"));
			PostQuitMessage(0);
			return 0L;
		default:
			utils::trace(_T("unknown message(%08xH)"), msg);
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

		h_reg_notif_ = ::RegisterDeviceNotification(h_dlg_, &broadcastInterface,
			DEVICE_NOTIFY_WINDOW_HANDLE
		);

		if (h_reg_notif_ == INVALID_HANDLE_VALUE)
		{
			utils::trace(_T("Failed to register HID notification"));
		}
	}

	void unregister_hid_notification(void)
	{
		if (h_reg_notif_ != INVALID_HANDLE_VALUE)
		{
			::UnregisterDeviceNotification(h_reg_notif_);
		}
		h_reg_notif_ = INVALID_HANDLE_VALUE;
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
					utils::trace(_T("Error"));
					return false;
				}
				else if (ret == 0)
				{
					utils::trace(_T("Received WM_QUIT"));
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
			cond_started_.notify_one();
			if (h_dlg_)
			{
				register_hid_notification();
				proc_watch_msg_loop();
				unregister_hid_notification();
			}
			else
			{
				utils::trace(_T("Failed to create a window"));
			}
		}
		else
		{
			cond_started_.notify_one();
			utils::trace(_T("Failed to register a windows class"));
		}

		return;
	}


	bool start()
	{
		std::unique_lock<std::mutex> lock(mtx_th_watch_);
		if (th_watch_.joinable()) return true;

		h_dlg_ = NULL;
		req_stop_ = false;
		std::thread th(std::bind(&Impl::proc_watch, this));
		th_watch_.swap(th);
		cond_started_.wait(lock);
		if (th_watch_.joinable())
		{
			add_wm_devicechange_func(DBT_ANY, std::bind(&Watcher::Impl::register_wm_input_targets, this, std::placeholders::_1));
			register_wm_input_targets(0);
			return true;
		}
		return false;
	}

	void stop()
	{
		std::lock_guard<std::mutex> lock(mtx_th_watch_);
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

	Impl(): h_dlg_(NULL), h_reg_notif_(INVALID_HANDLE_VALUE) 
	{
		start();
	}

	~Impl()
	{
		stop();
	}
};


Watcher::Watcher(): pImpl(new Impl) 
{

}

Watcher::~Watcher() { }


Watcher & Watcher::GetInstance(void)
{
	static Watcher instance;

	std::lock_guard<std::mutex> lock_l(instance.pImpl->mtx_listeners_);
	std::lock_guard<std::mutex> lock_t(instance.pImpl->mtx_th_watch_);

	return instance;
}

BOOL Watcher::RegisterDeviceChangeEventListener(UINT uEventType, std::function<DeviceChangeEventListener>  const & listener)
{
	return (BOOL) pImpl->add_wm_devicechange_func(uEventType, listener);
}

BOOL Watcher::UnregisterDeviceChangeEventListener(UINT uEventType)
{
	return (BOOL) pImpl->remove_wm_devicechange_func(uEventType);
}

BOOL Watcher::RegisterRawInputEventListener(Watcher::Target const &target, std::function<RawInputEventListener>  const &listener)
{
	return (BOOL)pImpl->add_wm_input_func(target, listener);
}

BOOL Watcher::UnregisterRawInputEventListener(Watcher::Target const &target)
{
	return (BOOL)pImpl->remove_wm_input_func(target);
}
