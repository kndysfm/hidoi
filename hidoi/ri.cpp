#include "ri.h"
#include <mutex>

#include "tstring.h"
USING_TCHAR_STRING;

using namespace hidoi;

struct RawInput::Impl
{
	std::mutex mtx_;

	std::vector<RAWINPUTDEVICELIST>  rid_lists_;

	struct rid_info_t
	{
		HANDLE handle;
		Tstring name;
		RID_DEVICE_INFO info;
	};

	std::vector<rid_info_t> rid_infos_;

	void update()
	{
		std::lock_guard<std::mutex> lock(mtx_);

		UINT n;
		::GetRawInputDeviceList(NULL, &n, sizeof(RAWINPUTDEVICELIST));
		rid_lists_.resize(n);
		n = ::GetRawInputDeviceList(rid_lists_.data(), &n, sizeof(RAWINPUTDEVICELIST));
		rid_infos_.clear();

		for (UINT i = 0; i < n; i++)
		{
			RAWINPUTDEVICELIST const &list = rid_lists_[i];

			RID_DEVICE_INFO inf;
			inf.cbSize = sizeof(RID_DEVICE_INFO);
			UINT info_size = sizeof(RID_DEVICE_INFO);
			::GetRawInputDeviceInfo(list.hDevice, RIDI_DEVICEINFO, &inf, &info_size);
			if (inf.dwType == RIM_TYPEHID) // only HID
			{
				rid_info_t rid;
				rid.info = inf;
				rid.handle = list.hDevice;

				::GetRawInputDeviceInfo(list.hDevice, RIDI_DEVICENAME, NULL, &info_size);
				_TCHAR *name = new _TCHAR[info_size];
				::GetRawInputDeviceInfo(list.hDevice, RIDI_DEVICENAME, name, &info_size);
				rid.name = name;
				delete[] name;

				rid_infos_.push_back(rid);
			}
		}

	}

	rid_info_t const * search(RID_DEVICE_INFO_HID const &inf_hid)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		for (auto inf : rid_infos_)
		{
			if ((!inf_hid.dwVendorId || inf_hid.dwVendorId == inf.info.hid.dwVendorId) &&
				(!inf_hid.dwProductId || inf_hid.dwProductId == inf.info.hid.dwProductId) &&
				(!inf_hid.dwVersionNumber || inf_hid.dwVersionNumber == inf.info.hid.dwVersionNumber) &&
				(!inf_hid.usUsagePage || inf_hid.usUsagePage == inf.info.hid.usUsagePage) &&
				(!inf_hid.usUsage || inf_hid.usUsage == inf.info.hid.usUsage))
			{
				return &inf;
			}
		}

		return nullptr;
	}

	RID_DEVICE_INFO_HID const *search(HANDLE h)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		for (auto inf : rid_infos_)
		{
			if (inf.handle == h) return &inf.info.hid;
		}

		return nullptr;
	}

	BOOL register_rid(HWND hWnd, RID_DEVICE_INFO_HID const &inf_hid)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		BOOL bRet = FALSE;

		rid_info_t const *pinf = search(inf_hid);
		if (pinf)
		{
			RAWINPUTDEVICE rid;
			rid.hwndTarget = hWnd;
			rid.dwFlags = RIDEV_INPUTSINK;
			rid.usUsagePage = pinf->info.hid.usUsagePage;
			rid.usUsage = pinf->info.hid.usUsage;
			bRet = ::RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
		}

		return bRet;
	}

	BOOL unregister_rid(HWND hWnd, RID_DEVICE_INFO_HID const &inf_hid)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		BOOL bRet = FALSE;

		rid_info_t const *pinf = search(inf_hid);
		if (pinf)
		{
			RAWINPUTDEVICE rid;
			rid.hwndTarget = hWnd;
			rid.dwFlags = RIDEV_REMOVE;
			rid.usUsagePage = pinf->info.hid.usUsagePage;
			rid.usUsage = pinf->info.hid.usUsage;
			bRet = ::RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
		}

		return bRet;
	}

	void unregister_all(HWND hWnd)
	{
		std::lock_guard<std::mutex> lock(mtx_);

		RAWINPUTDEVICE rid;
		rid.hwndTarget = hWnd;
		rid.dwFlags = RIDEV_REMOVE;

		for (auto inf : rid_infos_)
		{
			switch (inf.info.dwType)
			{
			case RIM_TYPEHID:
				rid.usUsagePage = inf.info.hid.usUsagePage;
				rid.usUsage = inf.info.hid.usUsage;
				break;
			case RIM_TYPEMOUSE:
				rid.usUsagePage = 1;
				rid.usUsage = 2;
				break;
			case RIM_TYPEKEYBOARD:
				rid.usUsagePage = 1;
				rid.usUsage = 6;
				break;
			}
			::RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
		}
	}
};

RawInput::RawInput(): pImpl(new Impl) { }

RawInput::~RawInput() { }

RawInput & RawInput::GetInstance()
{
	static RawInput instance;

	std::lock_guard<std::mutex> lock(instance.pImpl->mtx_);
	return instance;
}

void RawInput::Update()
{
	pImpl->update();
}

BOOL RawInput::Search(RID_DEVICE_INFO_HID const &query)
{
	return (BOOL) pImpl->search(query);
}

RID_DEVICE_INFO_HID const *RawInput::Search(HANDLE h)
{
	return pImpl->search(h);
}

BOOL RawInput::Register(HWND hWnd, RID_DEVICE_INFO_HID const &query)
{
	return pImpl->register_rid(hWnd, query);
}

BOOL RawInput::Unregister(HWND hWnd, RID_DEVICE_INFO_HID const &query)
{
	return pImpl->unregister_rid(hWnd, query);
}

void RawInput::UnregisterAll(HWND hWnd)
{
	pImpl->unregister_all(hWnd);
}
