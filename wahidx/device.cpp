#include "wahidx.h"

#include <setupapi.h>
#include <Dbt.h>
EXTERN_C
{
#include <hidsdi.h>
}

#include "tstring.h"
USING_TCHAR_STRING;

#include <unordered_set>
#include <queue>
#include <map>

using namespace wahidx;

typedef DWORD USAGEPAGE_USAGE;
#define TO_USAGEPAGE_USAGE(page, usage)	(((USAGEPAGE_USAGE) page << 16) | usage)

class Device::Path
{
public:
	Tstring m_str;
	Path(LPCTSTR cstr): m_str(cstr) {  }
};

class Device::Info
{
public:
	HANDLE							hDevice;
	PHIDP_PREPARSED_DATA			preparsedData;
	HIDD_ATTRIBUTES					attributes;
	HIDP_CAPS						capabilities;

	struct Report
	{
		std::vector<BYTE>				 buffer;
		std::vector<HIDP_VALUE_CAPS>	ValueCaps;
		std::vector<HIDP_BUTTON_CAPS>	ButtonCaps;
		std::map<USAGEPAGE_USAGE, ULONG> Values;
		std::map<USAGEPAGE_USAGE, LONG> ScaledValues;
		std::vector<USAGE>				ButtonUsagePages;
		std::vector<USAGEPAGE_USAGE>	Buttons;
	};
	Report							inputReport, outputReport, featureReport;

	Info(HANDLE h)
	{
		hDevice = h;
		if (h != INVALID_HANDLE_VALUE)
		{
			HidD_GetAttributes(h, &attributes);

			if (HidD_GetPreparsedData(h, &preparsedData))
			{
				HIDP_CAPS capabilities;
				NTSTATUS status = HidP_GetCaps(preparsedData, &capabilities);
			}
			else
			{
				preparsedData = NULL;
			}
		}
		else
		{
			preparsedData = NULL;
		}
	}

	~Info(void)
	{
		if (preparsedData != NULL)
		{
			HidD_FreePreparsedData(preparsedData);
			preparsedData = NULL;
		}
	}

	void PrepareReportInfo(HIDP_REPORT_TYPE rep_type, Report *r, USHORT length_buffer)
	{
		r->buffer.resize(length_buffer, 0x00);

		USHORT length = 0;
		HIDP_VALUE_CAPS *vcaps_buf;
		HidP_GetValueCaps(rep_type, NULL, &length, preparsedData);
		vcaps_buf = new HIDP_VALUE_CAPS[length];
		HidP_GetValueCaps(rep_type, vcaps_buf, &length, preparsedData);
		r->ValueCaps.reserve(length);
		r->Values.clear();
		r->ScaledValues.clear();

		for (USHORT idx = 0; idx < length; idx++)
		{
			USHORT upage = vcaps_buf[idx].UsagePage;
			USHORT usage = vcaps_buf[idx].NotRange.Usage;
			USAGEPAGE_USAGE upage_usage = ((USAGEPAGE_USAGE)upage << 16) | usage;

			r->Values[upage_usage] = vcaps_buf[idx].LogicalMin;
			r->ScaledValues[upage_usage] = vcaps_buf[idx].PhysicalMin;
			r->ValueCaps.push_back(vcaps_buf[idx]);
		}

		delete[] vcaps_buf;

		length = 0;
		HidP_GetButtonCaps(rep_type, NULL, &length, preparsedData);
		HIDP_BUTTON_CAPS *bcaps_buf = new HIDP_BUTTON_CAPS[length];
		HidP_GetButtonCaps(rep_type, bcaps_buf, &length, preparsedData);
		r->ButtonCaps.reserve(length);
		r->ButtonUsagePages.clear();
		for (USHORT idx = 0; idx < length; idx++)
		{
			USHORT upage = bcaps_buf[idx].UsagePage;
			DWORD size = r->ButtonUsagePages.size();
			r->ButtonCaps.push_back(bcaps_buf[idx]);
			for (USHORT i = 0; i <= size; i++)
			{
				if (i == size)
				{
					r->ButtonUsagePages.push_back(upage);
					break;
				}
				else if (r->ButtonUsagePages[i] == upage)
				{
					break; // same found
				}
				else
					continue;
			}
		}
		r->Buttons.clear();
		delete[] bcaps_buf;
	}

	void PrepareReportInfo()
	{
		PrepareReportInfo(HidP_Input, &inputReport, capabilities.InputReportByteLength);
		PrepareReportInfo(HidP_Output, &outputReport, capabilities.OutputReportByteLength);
		PrepareReportInfo(HidP_Feature, &featureReport, capabilities.FeatureReportByteLength);
	}
};

struct Device::Impl
{
public:
	static std::vector<Path> enumerate()
	{
		std::vector<Path> paths;
		GUID guid;
		HDEVINFO deviceInfo;

		HidD_GetHidGuid(&guid);
		// 渡されたGUIDに該当するデバイス情報を取得
		deviceInfo = SetupDiGetClassDevs(&guid,
			NULL,
			NULL,
			(DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

		if (deviceInfo != INVALID_HANDLE_VALUE)
		{
			SP_DEVICE_INTERFACE_DATA deviceInfoData;

			deviceInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
			// インターフェースを列挙
			for (DWORD index = 0;
				SetupDiEnumDeviceInterfaces(deviceInfo,
					0,
					&guid,
					index,
					&deviceInfoData);
				index++)
			{
				DWORD requiredLength;
				// デバイスパスを含む構造体のサイズを取得
				SetupDiGetDeviceInterfaceDetail(deviceInfo,
					&deviceInfoData,
					NULL,
					0,
					&requiredLength,
					NULL);

				LPBYTE deviceDetailDataBuffer;
				PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetailData;

				// 取得したサイズを元にバッファを確保
				deviceDetailDataBuffer = new BYTE[requiredLength];
				deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)deviceDetailDataBuffer;

				deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
				// デバイスパスを取得
				SetupDiGetDeviceInterfaceDetail(deviceInfo,
					&deviceInfoData,
					deviceDetailData,
					requiredLength,
					&requiredLength,
					NULL);
				// デバイスパスをPathインスタンスメンバにコピー
				paths.push_back(deviceDetailData->DevicePath);

				// バッファ削除
				deviceDetailData = NULL;
				delete[] deviceDetailDataBuffer;
			}
			// 使用したデバイス情報の破棄
			SetupDiDestroyDeviceInfoList(deviceInfo);
		}

		return paths;
	}

	static std::vector<Path> search(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
	{
		std::vector<Path> paths;
		auto paths_enum = enumerate();
		for (const auto& p : paths_enum)
		{
			if (test(p, vendor_id, product_id, usage_page, usage))
			{
				paths.push_back(p);
			}
		}
		return paths;
	}


	static BOOL test(Path devicePath, USHORT vendorId, USHORT productId, USAGE usagePage, USAGE usage)
	{
		Device dev;
		dev.Open(devicePath);

		BOOL vid_ok = vendorId == 0 || vendorId == dev.GetVendorId();
		BOOL pid_ok = productId == 0 || productId == dev.GetProductId();
		BOOL page_ok = usagePage == 0 || usagePage == dev.GetUsagePage();
		BOOL usage_ok = usage == 0 || usage == dev.GetUsage();

		dev.Close();
		return vid_ok && pid_ok && page_ok && usage_ok;
	}

	Tstring device_path;
	HIDD_ATTRIBUTES      Attributes;
	std::unique_ptr<Info> info;

	std::queue<std::vector<BYTE>> m_fifo;

	static int copyBuffer(void *dst, int dst_size, const void *src, int src_size)
	{
		int cnt = dst_size < src_size ? dst_size : src_size;
		memmove_s(dst, cnt, src, cnt);
		return cnt;
	}

	Device *self;

	Impl(Device *pSelf) :self(pSelf)
	{
	}

	virtual ~Impl(void)
	{
		close();
	}

	BOOL open(Path devicePath)
	{
		close();
		
		HANDLE h = ::CreateFile(devicePath.m_str.c_str(),
								(GENERIC_READ|GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), 
								NULL,        // no SECURITY_ATTRIBUTES structure
								OPEN_EXISTING, // No special create flags
								0,   // Open device as non-overlapped so we can get data
								NULL);       // No template file
		if (h != INVALID_HANDLE_VALUE)
		{
			info.reset(new Info(h));
			return TRUE;
		}

		return FALSE;
	}

	BOOL is_opened(void)
	{
		Lock();
		BOOL b = pDevice != NULL &&
			pDevice->HidDevice != INVALID_HANDLE_VALUE &&
			pDevice->HidDevice != 0;
		Unlock();

		return b;
	}

	BOOL is_opened(LPCTSTR devicePath)
	{
		Lock();
		Tstring path = devicePath;
		BOOL b = (path == m_path);
		Unlock();

		return b;
	}

	void close(void)
	{
		if (pDevice)
		{
			if (Updater::IsAlive())
				Updater::EndThread(INFINITE);

			Lock();
			if (hEvent != INVALID_HANDLE_VALUE)
			{
				::ClosePath(hEvent);
				hEvent = INVALID_HANDLE_VALUE;
			}
			::CloseHidDevice(pDevice);
			delete pDevice;
			pDevice = NULL;
			m_path.clear();
			Unlock();
		}
	}

	BOOL setFeature(const BYTE * src, int size)
	{
		Lock();
		BOOL bRet = (pDevice != NULL &&
			pDevice->FeatureReportBuffer != NULL &&
			pDevice->Caps.FeatureReportByteLength != 0 &&
			size <= (int)pDevice->Caps.FeatureReportByteLength);
		if (bRet)
		{
			if (pDevice->FeatureReportBuffer != src)
				copyBuffer(pDevice->FeatureReportBuffer, pDevice->Caps.FeatureReportByteLength, src, size);
			bRet = (BOOL)::SetFeatureReportWithSize(pDevice, size);
		}
		Unlock();
		return bRet;
	}

	BOOL getFeature(LPBYTE dst = NULL, int size = 0)
	{
		if (dst == NULL)
		{
			dst = pDevice->FeatureReportBuffer;
			size = pDevice->Caps.FeatureReportByteLength;
		}
		else
		{
			pDevice->FeatureReportBuffer[0] = dst[0];
			if (size == 0)
				size = pDevice->Caps.FeatureReportByteLength;
		}
		Lock();
		BOOL bRet = (pDevice != NULL &&
			pDevice->FeatureReportBuffer != NULL &&
			pDevice->Caps.FeatureReportByteLength != 0);
		if (bRet)
		{
			bRet = ::GetFeatureReportWithSize(pDevice, size);
			if (bRet && dst != pDevice->FeatureReportBuffer)
				copyBuffer(dst, size, pDevice->FeatureReportBuffer, pDevice->Caps.FeatureReportByteLength);
		}

		Unlock();
		return bRet;
	}

	BOOL write(const BYTE * src, int size)
	{
		Lock();
		BOOL bRet = (pDevice != NULL &&
			pDevice->OutputReportBuffer != NULL &&
			pDevice->Caps.OutputReportByteLength != 0 &&
			size <= (int)pDevice->Caps.OutputReportByteLength);
		if (bRet)
		{
			if (pDevice->OutputReportBuffer != src)
				copyBuffer(pDevice->OutputReportBuffer, pDevice->Caps.OutputReportByteLength, src, size);
			bRet = (BOOL)::WriteOutputReport(pDevice);
		}
		Unlock();
		return bRet;
	}

	BOOL startFifo(void)
	{
		if (pDevice == NULL ||
			pDevice->InputReportBuffer == NULL ||
			pDevice->Caps.InputReportByteLength == 0) return FALSE;

		Lock();
		::HidD_FlushQueue(pDevice->HidDevice);
		Unlock();
		return Updater::StartThread();
	}

	void stopFifo(void)
	{
		if (pDevice == NULL ||
			pDevice->InputReportBuffer == NULL ||
			pDevice->Caps.InputReportByteLength == 0) return;

		Updater::EndThread();
		Lock();
		::HidD_FlushQueue(pDevice->HidDevice);
		Unlock();
	}

	BOOL usesFifo(void)
	{
		return Updater::IsAlive();
	}

	BOOL read(LPBYTE dst = NULL, int size = 0)
	{
		if (dst == NULL)
		{
			dst = pDevice->InputReportBuffer;
			size = pDevice->Caps.InputReportByteLength;
		}
		Lock();
		BOOL bRet = (pDevice != NULL &&
			pDevice->InputReportBuffer != NULL &&
			pDevice->Caps.InputReportByteLength != 0 &&
			size >= (int)pDevice->Caps.InputReportByteLength);
		if (bRet)
		{
			if (Updater::IsAlive())
			{ // Asynchronous reading
				bRet = m_fifo.size() > 0;
				if (bRet)
				{ // Queued data exist
					std::vector<BYTE> data = m_fifo.front();
					m_fifo.pop();
					copyBuffer(dst, size, &data.front(), data.size());
				}
			}
			else
			{ // Synchronous reading
				bRet = ::ReadInputReport(pDevice);
				if (bRet && dst != pDevice->InputReportBuffer)
					copyBuffer(dst, size, pDevice->InputReportBuffer, pDevice->Caps.InputReportByteLength);
			}
		}
		Unlock();
		return bRet;
	}

	BOOL readAsync(void)
	{
		Lock();
		BOOL bRet = (pDevice != NULL &&
			pDevice->InputReportBuffer != NULL &&
			pDevice->Caps.InputReportByteLength != 0 &&
			!Updater::IsDying());
		if (bRet)
		{
			if (hEvent != INVALID_HANDLE_VALUE) ::ClosePath(hEvent);
			hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			bRet = (hEvent != INVALID_HANDLE_VALUE && (BOOL)::ReadInputReportOverlapped(pDevice, hEvent));
		}
		Unlock();
		return bRet;
	}

	DWORD waitForRead(LPBYTE dst, int size, DWORD timeout)
	{
		if (pDevice == NULL ||
			pDevice->InputReportBuffer == NULL ||
			pDevice->Caps.InputReportByteLength == 0 ||
			hEvent == INVALID_HANDLE_VALUE ||
			Updater::IsDying()) return WAIT_FAILED;

		DWORD waitStatus = ::WaitForSingleObject(hEvent, timeout);
		Lock();
		if (waitStatus != WAIT_TIMEOUT)
		{
			::ClosePath(hEvent);
			hEvent = INVALID_HANDLE_VALUE;
		}
		if (waitStatus == WAIT_OBJECT_0 && size >= (int)pDevice->Caps.InputReportByteLength)
		{
			copyBuffer(dst, size, pDevice->InputReportBuffer, pDevice->Caps.InputReportByteLength);
		}
		Unlock();

		return waitStatus;
	}

	virtual void onUpdate(void)
	{
		// if hEvent is not invalid, last waitForRead returned WAIT_TIMEOUT
		if (hEvent != INVALID_HANDLE_VALUE || readAsync())
		{
			int buf_size = pDevice->Caps.InputReportByteLength;
			std::vector<BYTE> data(buf_size);
			if (waitForRead(&data.front(), buf_size, 1) == WAIT_OBJECT_0)
			{
				//KzUtils::Trace(_T("waitForRead() == WAIT_OBJECT_0"));
				Lock();
				if (pDevice != NULL &&
					pDevice->InputReportBuffer != NULL &&
					pDevice->Caps.InputReportByteLength != 0 &&
					!Updater::IsDying())
				{
					m_fifo.push(data);
					self->OnAsyncInputEvent(*self, data.data());
					self->OnAsyncInput();
				}
				Unlock();
			}
			//KzUtils::Trace(_T("waitForRead() != WAIT_OBJECT_0"));
		}
	}
};

std::vector<Device::Path> Device::Enumerate()
{
	return Impl::enumerate();
}

std::vector<Device::Path> Device::Search(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
{
	return Impl::search(vendor_id, product_id, usage_page, usage)
}

BOOL Device::Test(Device::Path const &p,
	USHORT vendor_id = 0x0000, USHORT product_id = 0x0000, USAGE usage_page = 0x0000, USAGE usage = 0x0000)
{
	return Impl::test(p, vendor_id, product_id, usage_page, usage);
}

static std::map<HANDLE, HDEVNOTIFY> registeredPaths;

BOOL Device::RegisterNotification(HWND hwnd)
{
	if (registeredPaths.count(hwnd) != 0) return FALSE;

	DEV_BROADCAST_DEVICEINTERFACE    broadcastInterface;

	broadcastInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	broadcastInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	::HidD_GetHidGuid(&broadcastInterface.dbcc_classguid);

	HANDLE handle = ::RegisterDeviceNotification(hwnd,
		&broadcastInterface,
		DEVICE_NOTIFY_WINDOW_HANDLE
	);
	if (handle != INVALID_HANDLE_VALUE)
	{
		registeredPaths[hwnd] = handle;
		return TRUE;
	}
	else return FALSE;
}

void Device::UnregisterNotification(HWND hwnd)
{
	if (registeredPaths.count(hwnd) != 0)
	{
		::UnregisterDeviceNotification(registeredPaths[hwnd]);
		registeredPaths.erase(hwnd);
	}
}

Device::Device() :
	pImpl(new Impl(this))
{
}

Device::~Device()
{
}

BOOL Device::Open(Path const & path)
{
	return pImpl->open(path);
}

BOOL Device::IsOpened(void) const
{
	return pImpl->is_opened();
}

BOOL Device::IsOpenedPath(LPCTSTR devicePath) const
{
	return pImpl->is_opened(devicePath);
}

void Device::Close(void)
{
	pImpl->close();
}

BOOL Device::SetFeature(const BYTE * src, int size)
{
	return pImpl-> setFeature(src, size);
}

BOOL Device::GetFeature(LPBYTE dst, int size)
{
	return pImpl->getFeature(dst, size);
}

const BYTE * Device::GetFeature(BYTE id)
{
	KzUtils::Assert(pImpl->pDevice != NULL);
	pImpl->pDevice->FeatureReportBuffer[0] = id;
	return pImpl->getFeature() ? pImpl->pDevice->FeatureReportBuffer : NULL;
}

BOOL Device::Write(const BYTE * src, int size)
{
	return pImpl->write(src, size);
}

BOOL Device::Read(LPBYTE dst, int size)
{
	return pImpl->read(dst, size);
}

const BYTE * Device::Read(void)
{
	return pImpl->read() ? pImpl->pDevice->InputReportBuffer : NULL;
}

USHORT Device::GetVendorId(void) const
{
	return pImpl->info->attributes.VendorID;
}

USHORT Device::GetProductId(void) const
{
	return pImpl->info->attributes.ProductID;
}

USHORT Device::GetUsagePage(void) const
{
	return pImpl->info->capabilities.UsagePage;
}

USHORT Device::GetUsage(void) const
{
	return pImpl->info->capabilities.Usage;
}

USHORT Device::GetFeatureReportLength(void) const
{
	return pImpl->info->capabilities.FeatureReportByteLength;
}

USHORT Device::GetInputReportLength(void) const
{
	return pImpl->info->capabilities.InputReportByteLength;
}

USHORT Device::GetOutputReportLength(void) const
{
	return pImpl->info->capabilities.OutputReportByteLength;
}
