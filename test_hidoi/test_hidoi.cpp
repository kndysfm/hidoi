// test_wahidx.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <hidoi/hidoi.h>
USING_TCHAR_CONSOLE

#include <thread>

#include <Dbt.h>

static void OnDeviceNodesChange(DWORD_PTR dwData)
{
	Tcout << _T("\t DBT_DEVNODES_CHANGED received !! \r\n");
}

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
		hidoi::Parser::ValueCaps x_caps, y_caps;
		p->GetInputValueCaps(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X, &x_caps);
		p->GetInputValueCaps(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y, &y_caps);

		auto x_logi = r->GetValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X);
		auto x_phys = r->GetScaledValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X);
		auto x_rate = (double) (x_logi - x_caps.LogicalMin) / (x_caps.LogicalMax - x_caps.LogicalMin);
		auto y_logi = r->GetValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y);
		auto y_phys = r->GetScaledValue(HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y);
		auto y_rate = (double)(y_logi - y_caps.LogicalMin) / (y_caps.LogicalMax - y_caps.LogicalMin);
		auto p_logi = r->GetValue(HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TIP_PRESSURE);

		Tcout << std::dec;
		Tcout << _T("x = ") << x_phys << _T("(") << std::setw(2) << x_rate * 100.0 << _T("%)\r\n");
		Tcout << _T("y = ") << y_phys << _T("(") << std::setw(2) << y_rate * 100.0 << _T("%)\r\n");
		Tcout << _T("p = ") << p_logi << _T("\r\n");

#if 0 // test code for deparsing
		std::vector<BYTE> deparsed = p->DeparseInput(dat[0], *r);
		if (deparsed[1] == dat[1] && deparsed[2] == dat[2])
		{
			Tcout << _T("Good deparsing !\r\n");
		}
		else
		{
			Tcout << _T("Bad deparsing !\r\n");
		}
#endif
	}

}


int main()
{
	TCHAR c_dmy;
	Tcout << _T("Enter a charactor\r\n");
	Tcin >> c_dmy;
	Tcout << _T("Test phase 1: Handling WM_DEVICECHANGE and WM_INPUT in background\r\n");

	hidoi::Watcher::Target tgt_all = { 0x056A, 0x011E, 0xFF11, HID_USAGE_DIGITIZER_PEN };
	auto &watcher = hidoi::Watcher::GetInstance();
	// add handlers for WM_DEVICECHANGE about HID devices
	watcher.WatchConnection(tgt_all,
		[](hidoi::Device::Path) {Tcout << _T("\t DBT_DEVICEARRIVAL received !! \r\n"); },
		[]() {Tcout << _T("\t DBT_DEVICEREMOVECOMPLETE received !! \r\n"); });
	watcher.WatchDeviceChange([]() {Tcout << _T("Device Nodes Has Changed !!(1) \r\n"); });
	watcher.WatchDeviceChange([]() {Tcout << _T("Device Nodes Has Changed !!(2) \r\n"); });

	// add handler for WM_INPUT about HID devices
	hidoi::Watcher::Target tgt;
	tgt.VendorId = 0; tgt.ProductId = 0; tgt.UsagePage = HID_USAGE_PAGE_DIGITIZER; tgt.Usage = HID_USAGE_DIGITIZER_PEN;
	Tcout << _T("Start watching Raw Input of HID and connection of device(s)\r\n");
	watcher.WatchRawInput(tgt, PenInputEventListener);

	auto ris = hidoi::RawInput::SearchByUsage(HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_PEN);
	std::vector<hidoi::Parser> parsers;
	parsers.reserve(ris.size()); // important reservation in advance to prevent changing where "parser.back()" is pointing 
	for (auto &ri : ris)
	{
		Tcout << _T("Raw input device is found:\r\n");
		Tcout << std::hex << std::setfill(_T('0'));
		Tcout << _T("\t Vendor ID: ") <<  std::setw(4) << ri.GetVendorId() << _T("H\r\n");
		Tcout << _T("\t Product ID: ") << std::setw(4) << ri.GetProductId() << _T("H\r\n");
		Tcout << _T("\t Usage Page - Usage: ") <<
			std::setw(4) << ri.GetUsagePage() << _T("H - ") << std::setw(4) << ri.GetUsage() << _T("H\r\n");
		parsers.push_back(ri.GetParser());
		// the function object bound with its own parser can parse the HID Input report passed from watcher
		watcher.WatchRawInput(ri, std::bind(ParsingPenEventListener, std::placeholders::_1, &parsers.back()));
	}

	Tcout << _T("Enter a charactor\r\n");
	Tcin >> c_dmy;
	
	// remove handler for WM_INPUT
	watcher.UnwatchRawInput(tgt);
	// remove handlers for WM_DEVICECHANGE
	watcher.UnwatchConnection(tgt_all);
	watcher.UnwatchAllConnections(); // all
	watcher.UnwatchAllRawInputs(); // all
	watcher.UnwatchDeviceChange(); // all

	Tcout << _T("Listeners are removed from the Watcher !!\r\n");
	Tcout << _T("Test phase 2: Contorolling HID with file handle\r\n");

	// search device path for Wacom Active-ES pen I/Fs
	auto paths = hidoi::Device::Search(0x056A, 0x0000, 0xFF11, HID_USAGE_DIGITIZER_PEN);
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

	Tcout << _T("Enter a charactor to quit\r\n");
	Tcin >> c_dmy;

	dev.QuitListeningInput();
	dev.Close();

    return 0;
}

