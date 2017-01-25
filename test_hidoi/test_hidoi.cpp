// test_wahidx.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <hidoi/device.h>

int main()
{
	auto paths = hidoi::Device::Enumerate();
	for (auto p: paths)
	{
		hidoi::Device dev;
		if (dev.Open(p))
		{
			_tprintf(_T("Succeeded to open a device:\r\n"));
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

    return 0;
}

