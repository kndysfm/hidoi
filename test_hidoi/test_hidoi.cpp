// test_wahidx.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <hidoi/hidoi.h>
#include <hidoi/tstring.h>

#include <thread>

#include <Dbt.h>

//static void OnDeviceNodesChange(DWORD_PTR dwData)
//{
//	Tcout << _T("\t DBT_DEVNODES_CHANGED received !! \r\n");
//}

#include <iomanip>
static void RawInputEventListener(std::vector<BYTE> const& dat)
{
	Tcout << std::hex << std::setfill(_T('0'));
	for (BYTE b : dat) Tcout << std::setw(2) << b << _T(" ");
	Tcout << _T("\r\n");
}

int main()
{
	auto &watcher = hidoi::Watcher::GetInstance();
	//watcher.RegisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED, OnDeviceNodesChange);
	watcher.RegisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED, [](DWORD_PTR) {Tcout << _T("\t DBT_DEVNODES_CHANGED received !! \r\n"); });
	watcher.RegisterDeviceChangeEventListener(DBT_DEVICEARRIVAL, [](DWORD_PTR) {Tcout << _T("\t DBT_DEVICEARRIVAL received !! \r\n"); });
	watcher.RegisterDeviceChangeEventListener(DBT_DEVICEREMOVECOMPLETE, [](DWORD_PTR) {Tcout << _T("\t DBT_DEVICEREMOVECOMPLETE received !! \r\n"); });
	hidoi::Watcher::Target tgt;
	tgt.VendorId = 0; tgt.ProductId = 0; tgt.UsagePage = HID_USAGE_PAGE_DIGITIZER; tgt.Usage = HID_USAGE_DIGITIZER_PEN;
	watcher.RegisterRawInputEventListener(tgt, RawInputEventListener);

	auto paths = hidoi::Device::SearchByID(0x056A, 0x0000);
	for (auto &p : paths)
	{
		hidoi::Device dev;
		if (dev.Open(p))
		{
			_tprintf(_T("Wacom HID is found:\r\n"));
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

	auto ris = hidoi::RawInput::SearchByUsage(HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_PEN);
	for (auto &dev : ris)
	{
		_tprintf(_T("Raw input device is found:\r\n"));
		_tprintf(_T("\t Vendor ID: %04xH\r\n"), dev.GetVendorId());
		_tprintf(_T("\t Product ID: %04xH\r\n"), dev.GetProductId());
		_tprintf(_T("\t Usage Page - Usage: %04xH - %04xH\r\n"), dev.GetUsagePage(), dev.GetUsage());
	}

	_tprintf(_T("Press enter key\r\n"));
	std::vector<_TCHAR> buf; buf.resize(16);
	_getts_s(buf.data(), buf.size());

	watcher.UnregisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED);
	watcher.UnregisterDeviceChangeEventListener(DBT_DEVICEARRIVAL);
	watcher.UnregisterDeviceChangeEventListener(DBT_DEVICEREMOVECOMPLETE);
	watcher.UnregisterRawInputEventListener(tgt);

	_getts_s(buf.data(), buf.size());

    return 0;
}

