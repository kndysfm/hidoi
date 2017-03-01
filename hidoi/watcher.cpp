#define _HIDOI_WATCHER_CPP_
#include "watcher.h"

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

static uint64_t target_to_ui64(Watcher::Target const &t)
{
	return (uint64_t)t.VendorId << 48 | (uint64_t)t.ProductId << 32 | t.UsagePage << 16 | t.Usage;
}

bool Watcher::Target::operator==(Target const & x) const
{
	return target_to_ui64(*this) == target_to_ui64(x);
}

bool Watcher::Target::operator!=(Target const & x) const
{
	return target_to_ui64(*this) != target_to_ui64(x);
}

bool Watcher::Target::operator<(Target const & x) const
{
	return target_to_ui64(*this) < target_to_ui64(x);
}

bool Watcher::Target::operator>(Target const & x) const
{
	return target_to_ui64(*this) > target_to_ui64(x);
}

bool Watcher::Target::operator<=(Target const & x) const
{
	return target_to_ui64(*this) <= target_to_ui64(x);
}

bool Watcher::Target::operator>=(Target const & x) const
{
	return target_to_ui64(*this) >= target_to_ui64(x);
}


struct Watcher::Impl
{
	std::recursive_mutex mtx_listeners_, mtx_th_watch_;
	std::thread th_watch_;
	HWND h_dlg_;

	typedef Watcher::RawInputHandler wm_input_listner_t;
	std::map<Watcher::Target, wm_input_listner_t> rawinput_listeners_;

	std::vector<wm_input_listner_t> get_wm_input_listners(std::vector<RawInput> &ris)
	{
		std::vector<wm_input_listner_t> ls;
		for (auto & ri : ris)
		{
			for (auto &l : rawinput_listeners_)
			{
				if (ri.Test(l.first.VendorId, l.first.ProductId, l.first.UsagePage, l.first.Usage))
				{
					ls.push_back(l.second);
				}
			}
		}

		return ls;
	}

	void register_wm_input_targets()
	{
		auto rids = RawInput::Enumerate();
		for (auto &r : rids)
		{
			if (r.IsRegistered())
			{	// already registered
				bool is_target = false;
				for (auto &l : rawinput_listeners_)
				{
					if (r.Test(l.first.VendorId, l.first.ProductId, l.first.UsagePage, l.first.Usage))
					{
						is_target = true;
						break;
					}
				}
				if (!is_target) r.Unregister(); // this device should become non target
			}
			else
			{	// not registered yet
				for (auto &l : rawinput_listeners_)
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
			auto ls = get_wm_input_listners(ris);
			if (ls.size() > 0)
			{	// listeners are registered
				for (DWORD idx = 0; idx < cnt; idx++)
				{
					BYTE const *src = raw;
					for (DWORD i = 0; i < size; i++, src++)
					{
						rep_in[i] = *src;
					}
					// pass dat vector to registerd functional object
					for (auto &l : ls) l(rep_in);
					raw += size;
				}
			}
		}
		delete[] data;
	}

	bool add_wm_input_func(Watcher::Target const &tgt, RawInputHandler  const &listener)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);

		wm_input_listner_t l(listener);
		rawinput_listeners_[tgt] = (l);
		register_wm_input_targets(); // update

		return true;
	}

	bool remove_wm_input_func(Watcher::Target const &tgt)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		if (rawinput_listeners_.count(tgt) == 1)
		{
			rawinput_listeners_.erase(tgt);
			register_wm_input_targets(); // update
			return true;
		}

		return false;
	}

	bool remove_wm_input_func() // remove all
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		rawinput_listeners_.clear();
		register_wm_input_targets(); // update

		return true;
	}

	typedef std::pair<DeviceArrivalHandler, DeviceRemoveHandler> connection_handlers_t;
	std::map< Watcher::Target, connection_handlers_t> path_handlers_;

	typedef std::pair<std::function<void(void)>, std::thread> pair_function_thread_t;

	bool add_handler_connection(Target const &target, DeviceArrivalHandler const &on_arrive, DeviceRemoveHandler const &on_remove)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		path_handlers_[target] = connection_handlers_t(on_arrive, on_remove);
		proc_wm_devicechange();

		return true;
	}

	bool remove_handler_connection(Target const &target)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		if (path_handlers_.count(target) == 1)
		{
			path_handlers_.erase(target);
			return true;
		}

		return false;
	}

	bool remove_handler_connection()
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		path_handlers_.clear();

		return true;
	}

	std::vector<DeviceChangeHandler> handlers_on_devchange;

	BOOL add_handler_devchange(DeviceChangeHandler const &on_devchange)
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		handlers_on_devchange.push_back(on_devchange);
		proc_wm_devicechange();

		return true;
	}

	BOOL remove_handlers_devchange()
	{
		if (h_dlg_ == NULL || !th_watch_.joinable()) return false;

		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		handlers_on_devchange.clear();

		return true;
	}

	std::map< Watcher::Target, bool> count_connections_;
	std::mutex mtx_connection_;

	void handle_connection_change(bool by_wm)
	{
		mtx_connection_.lock();
		
		if (by_wm) std::this_thread::sleep_for(std::chrono::milliseconds(500));
		else std::this_thread::sleep_for(std::chrono::milliseconds(100));
		
		std::lock_guard<std::recursive_mutex> lock(mtx_listeners_);
		// store the last connection counts
		auto last_conn_status = count_connections_;
		count_connections_.clear();

		auto paths = Device::Enumerate();

		// check all the devices connected
		for (auto &p : paths)
		{
			for (auto &h : path_handlers_)
			{
				Target t = h.first;
				if (Device::Test(p, t.VendorId, t.ProductId, t.UsagePage, t.Usage))
				{
					if (last_conn_status.count(t) == 0 || !last_conn_status[t])
					{	// target has come
						if (h.second.first) h.second.first(p);
					}
					count_connections_[t] = true;
				}
			}
		}

		// check the devices disconnected
		for (auto &h : path_handlers_)
		{
			Target t = h.first;
			if (count_connections_.count(t) == 0)
			{	// target not found
				count_connections_[t] = false;
				if (last_conn_status.count(t) != 0 && last_conn_status[t])
				{	// target has gone
					if (h.second.second) h.second.second();
				}
			}
		}

		// iterate handler for any type of event
		for (auto &h : handlers_on_devchange)
		{
			h();
		}

		if (by_wm)
		{	// only when the real WM_DEVICECHANGE has come
			register_wm_input_targets();
		}

		mtx_connection_.unlock();
	}

	std::thread th_connection_;
	void proc_wm_devicechange(bool by_wm = false)
	{
		if (mtx_connection_.try_lock())
		{
			mtx_connection_.unlock();
			if (th_connection_.joinable()) th_connection_.join();
			std::thread th(std::bind(&Watcher::Impl::handle_connection_change, this, by_wm));
			th_connection_.swap(th);
		}
	}

	static LRESULT CALLBACK wnd_proc(HWND window, unsigned int msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_INPUT:
			Watcher::GetInstance().pImpl->proc_wm_input(GET_RAWINPUT_CODE_WPARAM(wp), (HRAWINPUT)lp);
			break;
		case WM_DEVICECHANGE:
		{
			if (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE) 
			{	// only when HID device connection has changed
				Watcher::GetInstance().pImpl->proc_wm_devicechange();
			}
			break;
		}
		case WM_DESTROY:
			HIDOI_UTILS_TRACE_STR("WM_DESTROY - Destroying watcher window");
			PostQuitMessage(0);
			return 0L;
		default:
			utils::trace(_T("unknown message(%08xH)"), msg);
			break;
		}
		return DefWindowProc(window, msg, wp, lp);
	}

	HANDLE h_reg_notif_;
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

	bool req_stop_;
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

	std::condition_variable cond_started_;
	std::mutex mtx_cond_;

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
		std::lock_guard<std::recursive_mutex> guard(mtx_th_watch_);
		std::unique_lock<std::mutex> lock(mtx_cond_);
		if (th_watch_.joinable()) return true;

		h_dlg_ = NULL;
		req_stop_ = false;
		std::thread th(std::bind(&Impl::proc_watch, this));
		th_watch_.swap(th);
		cond_started_.wait(lock);
		if (th_watch_.joinable())
		{	// thread started
			return true;
		}
		return false;
	}

	void stop()
	{
		// wait WM_DEVICECHANGE to be handled 
		if (th_connection_.joinable()) th_connection_.join(); 

		std::lock_guard<std::recursive_mutex> lock(mtx_th_watch_);
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

	std::lock_guard<std::recursive_mutex> lock_l(instance.pImpl->mtx_listeners_);
	std::lock_guard<std::recursive_mutex> lock_t(instance.pImpl->mtx_th_watch_);

	return instance;
}

BOOL Watcher::WatchRawInput(Watcher::Target const &target, RawInputHandler  const &listener)
{
	return (BOOL)pImpl->add_wm_input_func(target, listener);
}

BOOL Watcher::UnwatchRawInput(Watcher::Target const &target)
{
	return (BOOL)pImpl->remove_wm_input_func(target);
}

static Watcher::Target _raw_input_to_target(RawInput const &ri)
{
	Watcher::Target tgt(ri.GetVendorId(), ri.GetProductId(), ri.GetUsagePage(), ri.GetUsage());
	return tgt;
}

BOOL Watcher::WatchRawInput(RawInput const &ri, RawInputHandler  const &listener)
{
	return (BOOL)pImpl->add_wm_input_func(_raw_input_to_target(ri), listener);
}

BOOL Watcher::UnwatchRawInput(RawInput const &ri)
{
	return (BOOL)pImpl->remove_wm_input_func(_raw_input_to_target(ri));
}

BOOL Watcher::UnwatchAllRawInputs()
{
	return (BOOL)pImpl->remove_wm_input_func();
}

BOOL Watcher::WatchConnection(Target const &target, DeviceArrivalHandler const &on_arrive, DeviceRemoveHandler const &on_remove)
{
	return (BOOL)pImpl->add_handler_connection(target, on_arrive, on_remove);
}

BOOL Watcher::UnwatchConnection(Target const &target)
{
	return (BOOL)pImpl->remove_handler_connection(target);
}

BOOL Watcher::UnwatchAllConnections()
{
	return (BOOL)pImpl->remove_handler_connection();
}

BOOL Watcher::WatchDeviceChange(DeviceChangeHandler const &on_devchange)
{
	return (BOOL)pImpl->add_handler_devchange(on_devchange);
}

BOOL Watcher::UnwatchDeviceChange()
{
	return (BOOL)pImpl->remove_handlers_devchange();
}
