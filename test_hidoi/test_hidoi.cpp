// test_wahidx.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <hidoi/hidoi.h>
USING_TCHAR_CONSOLE

#include <thread>

#include <Dbt.h>

//static void OnDeviceNodesChange(DWORD_PTR dwData)
//{
//	Tcout << _T("\t DBT_DEVNODES_CHANGED received !! \r\n");
//}

#include <iomanip>
static void PenInputEventListener(std::vector<BYTE> const& dat)
{
	Tcout << std::hex << std::setfill(_T('0'));
	for (BYTE b : dat) Tcout << std::setw(2) << b << _T(" ");
	Tcout << _T("\r\n");
}

static void ParsingPenEventListener(std::vector<BYTE> const& dat, hidoi::Parser *p)
{
	auto const *r = p->ParseInput(dat);

	if (r)
	{
		auto x_logi = r->GetValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X);
		auto x_phys = r->GetScaledValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X);
		auto y_logi = r->GetValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y);
		auto y_phys = r->GetScaledValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y);
		auto p_logi = r->GetValue(HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TIP_PRESSURE);

		Tcout << std::dec;
		Tcout << _T("x = ") << x_phys << _T("(") << x_logi << _T(")\r\n");
		Tcout << _T("y = ") << y_phys << _T("(") << y_logi << _T(")\r\n");
		Tcout << _T("p = ") << p_logi << _T("\r\n");
	}

}


int main()
{
	auto &watcher = hidoi::Watcher::GetInstance();
	//watcher.RegisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED, OnDeviceNodesChange);
	watcher.RegisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED, [](DWORD_PTR) {Tcout << _T("\t DBT_DEVNODES_CHANGED received !! \r\n"); });
	watcher.RegisterDeviceArrivalEventListener([](DWORD_PTR) {Tcout << _T("\t DBT_DEVICEARRIVAL received !! \r\n"); });
	watcher.RegisterDeviceRemoveEventListener([](DWORD_PTR) {Tcout << _T("\t DBT_DEVICEREMOVECOMPLETE received !! \r\n"); });
	hidoi::Watcher::Target tgt;
	tgt.VendorId = 0; tgt.ProductId = 0; tgt.UsagePage = HID_USAGE_PAGE_DIGITIZER; tgt.Usage = HID_USAGE_DIGITIZER_PEN;
	Tcout << _T("Start watching Raw Input of HID and connection of device(s)\r\n");
	watcher.RegisterRawInputEventListener(tgt, PenInputEventListener);

	auto ris = hidoi::RawInput::SearchByUsage(HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_PEN);
	std::vector<hidoi::Parser> parsers;
	for (auto &ri : ris)
	{
		Tcout << _T("Raw input device is found:\r\n");
		Tcout << std::hex << std::setfill(_T('0'));
		Tcout << _T("\t Vendor ID: ") <<  std::setw(4) << ri.GetVendorId() << _T("H\r\n");
		Tcout << _T("\t Product ID: ") << std::setw(4) << ri.GetProductId() << _T("H\r\n");
		Tcout << _T("\t Usage Page - Usage: ") <<
			std::setw(4) << ri.GetUsagePage() << _T("H - ") << std::setw(4) << ri.GetUsage() << _T("H\r\n");
		parsers.push_back(ri.GetParser());
		watcher.RegisterRawInputEventListener(ri, std::bind(ParsingPenEventListener, std::placeholders::_1, &parsers.back()));
	}

	Tcout << _T("Press enter an any charactor\r\n");
	TCHAR c_dmy;
	Tcin >> c_dmy;

	watcher.UnregisterDeviceChangeEventListener(DBT_DEVNODES_CHANGED);
	watcher.UnregisterDeviceArrivalEventListener();
	watcher.UnregisterDeviceRemoveEventListener();
	watcher.UnregisterDeviceChangeEventListener(); // all
	watcher.UnregisterRawInputEventListener(tgt);
	watcher.UnregisterRawInputEventListener(); // all
	Tcout << _T("Listeners are removed from the Watcher !!\r\n");

	auto paths = hidoi::Device::Search(0x056A, 0x0000, 0x0000, HID_USAGE_DIGITIZER_PEN);
	if (paths.size() == 0) Tcout << _T("No Wacom Pen device was found !!\r\n");
	hidoi::Device dev;
	for (auto &p : paths)
	{
		if (dev.Open(p))
		{
			Tcout << _T("A Wacom device is found:\r\n");
			Tcout << std::hex << std::setfill(_T('0'));
			Tcout << _T("\t Vendor ID: ") << std::setw(4) << dev.GetVendorId() << _T("H\r\n");
			Tcout << _T("\t Product ID: ") << std::setw(4) << dev.GetProductId() << _T("H\r\n");
			Tcout << _T("\t Usage Page - Usage: ") <<
				std::setw(4) << dev.GetUsagePage() << _T("H - ") << std::setw(4) << dev.GetUsage() << _T("H\r\n");
			break;
		}
		else
		{
			Tcout << _T("Failed to open a device\r\n");
		}
	}
	if (dev.IsOpened())
	{
		Tcout << _T("Start asynchronous reading HID input reports..\r\n");
		dev.StartListeningInput(PenInputEventListener);
		dev.SetFeature({ 0x0b, 0x01 }); // switch I/F for interrupt report
	}

	Tcout << _T("Press enter an any charactor\r\n");
	Tcin >> c_dmy;

	dev.QuitListeningInput();
	dev.Close();

    return 0;
}

