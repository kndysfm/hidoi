#include "ri.h"
#include <mutex>
#include <map>
#include <list>

#include "tstring.h"
USING_TCHAR_STRING;

using namespace hidoi;

namespace
{
	std::mutex mtx_;

	std::vector<RAWINPUTDEVICELIST>  rid_lists_;

	struct rid_info_t
	{
		Tstring name;
		RID_DEVICE_INFO info;
	};

	std::map<HANDLE, rid_info_t> rid_infos_;

	std::map<HANDLE, HWND> registered_handles_;

}

struct RawInput::Impl
{

	HANDLE handle_;
	rid_info_t info_;

	static void update()
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

				::GetRawInputDeviceInfo(list.hDevice, RIDI_DEVICENAME, NULL, &info_size);
				_TCHAR *name = new _TCHAR[info_size];
				::GetRawInputDeviceInfo(list.hDevice, RIDI_DEVICENAME, name, &info_size);
				rid.name = name;
				delete[] name;

				rid_infos_[list.hDevice] = rid;
			}
		}
	}

	static std::vector<RawInput> get_handles()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		std::vector<RawInput> ris;
		for (auto const &inf : rid_infos_)
		{
			RawInput ri;
			ri.pImpl->handle_ = inf.first;
			ri.pImpl->info_.name = inf.second.name;
			ri.pImpl->info_.info = inf.second.info;
			ris.push_back(ri);
		}
		return ris;
	}

	static std::vector<RawInput> search(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		std::vector<RawInput> ris;
		for (auto const &inf: rid_infos_)
		{
			auto const &inf_hid = inf.second.info.hid;
			if ((vendor_id	== 0x0000 || vendor_id	== inf_hid.dwVendorId) &&
				(product_id	== 0x0000 || product_id	== inf_hid.dwProductId) &&
				(usage_page	== 0x0000 || usage_page	== inf_hid.usUsagePage) &&
				(usage		== 0x0000 || usage		== inf_hid.usUsage))
			{
				RawInput ri;
				ri.pImpl->handle_ = inf.first;
				ri.pImpl->info_.name = inf.second.name;
				ri.pImpl->info_.info = inf.second.info;
				ris.push_back(ri);
			}
		}
		return ris;
	}

	static std::vector<RawInput> search(HANDLE handle)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		std::vector<RawInput> ris;
		for (auto const &inf: rid_infos_)
		{
			auto const &inf_hid = inf.second.info.hid;
			if (inf.first == handle)
			{
				RawInput ri;
				ri.pImpl->handle_ = inf.first;
				ri.pImpl->info_.name = inf.second.name;
				ri.pImpl->info_.info = inf.second.info;
				ris.push_back(ri);
			}
		}
		return ris;
	}

	BOOL test(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
	{
		auto const &inf_hid = info_.info.hid;
		if ((vendor_id == 0x0000 || vendor_id == inf_hid.dwVendorId) &&
			(product_id == 0x0000 || product_id == inf_hid.dwProductId) &&
			(usage_page == 0x0000 || usage_page == inf_hid.usUsagePage) &&
			(usage == 0x0000 || usage == inf_hid.usUsage))
		{
			return TRUE;
		}
		return FALSE;
	}

	BOOL register_rid(HWND hWnd)
	{
		std::lock_guard<std::mutex> lock(mtx_);

		auto const &inf_hid = info_.info.hid;
		RAWINPUTDEVICE rid;
		rid.hwndTarget = hWnd;
		rid.dwFlags = RIDEV_INPUTSINK;
		rid.usUsagePage = inf_hid.usUsagePage;
		rid.usUsage = inf_hid.usUsage;
		if (::RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
		{
			registered_handles_[handle_] = hWnd;
			return TRUE;
		}

		return FALSE;
	}

	BOOL unregister_rid()
	{
		std::lock_guard<std::mutex> lock(mtx_);

		auto const &inf_hid = info_.info.hid;
		RAWINPUTDEVICE rid;
		rid.hwndTarget = NULL; // must be NULL
		rid.dwFlags = RIDEV_REMOVE;
		rid.usUsagePage = inf_hid.usUsagePage;
		rid.usUsage = inf_hid.usUsage;
		if (::RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
		{
			registered_handles_.erase(handle_);
			return TRUE;
		}

		return FALSE;
	}

	BOOL is_registered()
	{
		for (auto const &h : registered_handles_)
		{
			if (h.first == handle_) return TRUE;
		}
		return FALSE;
	}

	static void unregister_all()
	{
		std::lock_guard<std::mutex> lock(mtx_);

		RAWINPUTDEVICE rid;
		rid.hwndTarget = NULL; // must be NULL
		rid.dwFlags = RIDEV_REMOVE;

		for (auto const &itr : rid_infos_)
		{
			auto const &inf = itr.second;
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
		registered_handles_.clear();
	}
};

RawInput::RawInput(): pImpl(new Impl) { }

RawInput::RawInput(RawInput const &ri) : pImpl(new Impl)
{
	*pImpl = *ri.pImpl;
}

RawInput & RawInput::operator=(RawInput const & ri)
{
	*pImpl = *ri.pImpl;
	return *this;
}

RawInput::~RawInput() { }

std::vector<RawInput> RawInput::Enumerate()
{
	RawInput::Impl::update();
	return RawInput::Impl::get_handles();
}

std::vector<RawInput> RawInput::Search(
	USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
{
	RawInput::Impl::update();
	return RawInput::Impl::search(vendor_id, product_id, usage_page, usage);
}

std::vector<RawInput> RawInput::SearchByHandle(HANDLE handle)
{
	RawInput::Impl::update();
	return RawInput::Impl::search(handle);
}

BOOL RawInput::Test(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
{
	return pImpl->test(vendor_id, product_id, usage_page, usage);
}

BOOL RawInput::Register(HWND hWnd)
{
	return pImpl->register_rid(hWnd);
}

BOOL RawInput::Unregister()
{
	return pImpl->unregister_rid();
}

BOOL RawInput::IsRegistered()
{
	return pImpl->is_registered();
}

void RawInput::UnregisterAll()
{
	RawInput::Impl::unregister_all();
}

USHORT RawInput::GetVendorId(void) const
{
	return (USHORT) pImpl->info_.info.hid.dwVendorId;
}

USHORT RawInput::GetProductId(void) const
{
	return (USHORT)pImpl->info_.info.hid.dwProductId;
}

USHORT RawInput::GetVersionNumber(void) const
{
	return (USHORT)pImpl->info_.info.hid.dwVersionNumber;
}

USAGE RawInput::GetUsagePage(void) const
{
	return (USAGE)pImpl->info_.info.hid.usUsagePage;
}

USAGE RawInput::GetUsage(void) const
{
	return (USAGE)pImpl->info_.info.hid.usUsage;
}

Parser RawInput::GetParser(void) const
{
	return Parser(pImpl->info_.name.c_str());
}
