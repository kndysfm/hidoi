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
#include <mutex>
#include <thread>
#include <cstdio>

using namespace wahidx;

typedef DWORD USAGEPAGE_USAGE;
#define TO_USAGEPAGE_USAGE(page, usage)	(((USAGEPAGE_USAGE) page << 16) | usage)

class Device::Path
{
public:
	Tstring tstr;
	std::string str;

	Path(LPCTSTR ctstr): tstr(ctstr)
	{
#ifdef  UNICODE 
		size_t size_dst = (_tcslen(ctstr) + 2) * sizeof(_TCHAR);
		std::unique_ptr<char[]> dst(new char[size_dst]);
		size_t num_converted = 0;
		wcstombs_s(&num_converted, dst.get(), size_dst, ctstr, size_dst);
		str = dst.get();
#else
		str = tstr;
#endif
	}
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
		//std::map<USAGEPAGE_USAGE, ULONG> Values;
		//std::map<USAGEPAGE_USAGE, LONG> ScaledValues;
		//std::vector<USAGE>				ButtonUsagePages;
		//std::vector<USAGEPAGE_USAGE>	Buttons;
	};
	Report							inputReport, outputReport, featureReport;

	Info(HANDLE h, BOOL with_detailed_caps)
	{
		hDevice = h;
		if (h != INVALID_HANDLE_VALUE)
		{
			HidD_GetAttributes(h, &attributes);

			if (HidD_GetPreparsedData(h, &preparsedData))
			{
				HIDP_CAPS capabilities;
				NTSTATUS status = HidP_GetCaps(preparsedData, &capabilities);
				if (with_detailed_caps) PrepareReportInfo();
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

	void _PrepareReportInfo(HIDP_REPORT_TYPE rep_type, Report *r, USHORT length_buffer)
	{
		r->buffer.resize(length_buffer, 0x00);

		USHORT length = 0;
		HIDP_VALUE_CAPS *vcaps_buf;
		HidP_GetValueCaps(rep_type, NULL, &length, preparsedData);
		vcaps_buf = new HIDP_VALUE_CAPS[length];
		HidP_GetValueCaps(rep_type, vcaps_buf, &length, preparsedData);
		r->ValueCaps.reserve(length);
		//r->Values.clear();
		//r->ScaledValues.clear();

		for (USHORT idx = 0; idx < length; idx++)
		{
			USHORT upage = vcaps_buf[idx].UsagePage;
			USHORT usage = vcaps_buf[idx].NotRange.Usage;
			USAGEPAGE_USAGE upage_usage = TO_USAGEPAGE_USAGE(upage, usage);

			//r->Values[upage_usage] = vcaps_buf[idx].LogicalMin;
			//r->ScaledValues[upage_usage] = vcaps_buf[idx].PhysicalMin;
			r->ValueCaps.push_back(vcaps_buf[idx]);
		}

		delete[] vcaps_buf;

		length = 0;
		HidP_GetButtonCaps(rep_type, NULL, &length, preparsedData);
		HIDP_BUTTON_CAPS *bcaps_buf = new HIDP_BUTTON_CAPS[length];
		HidP_GetButtonCaps(rep_type, bcaps_buf, &length, preparsedData);
		r->ButtonCaps.reserve(length);
		//r->ButtonUsagePages.clear();
		for (USHORT idx = 0; idx < length; idx++)
		{
			USHORT upage = bcaps_buf[idx].UsagePage;
			//DWORD size = r->ButtonUsagePages.size();
			r->ButtonCaps.push_back(bcaps_buf[idx]);
			//for (USHORT i = 0; i <= size; i++)
			//{
			//	if (i == size)
			//	{
			//		r->ButtonUsagePages.push_back(upage);
			//		break;
			//	}
			//	else if (r->ButtonUsagePages[i] == upage)
			//	{
			//		break; // same found
			//	}
			//	else
			//		continue;
			//}
		}
		//r->Buttons.clear();
		delete[] bcaps_buf;
	}

	void PrepareReportInfo()
	{
		_PrepareReportInfo(HidP_Input, &inputReport, capabilities.InputReportByteLength);
		_PrepareReportInfo(HidP_Output, &outputReport, capabilities.OutputReportByteLength);
		_PrepareReportInfo(HidP_Feature, &featureReport, capabilities.FeatureReportByteLength);
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
		dev.pImpl->open(devicePath, FALSE);

		BOOL vid_ok = vendorId == 0 || vendorId == dev.GetVendorId();
		BOOL pid_ok = productId == 0 || productId == dev.GetProductId();
		BOOL page_ok = usagePage == 0 || usagePage == dev.GetUsagePage();
		BOOL usage_ok = usage == 0 || usage == dev.GetUsage();

		dev.Close();
		return vid_ok && pid_ok && page_ok && usage_ok;
	}

	//!TEST
	std::FILE *file_;
	//!TEST

	Tstring device_path_;
	std::unique_ptr<Info> info_;
	std::mutex mtx_;

	std::queue<std::vector<BYTE>> fifo_;

	static int copyBuffer(std::vector<BYTE> *dst, std::vector<BYTE> const *src)
	{
		int cnt = dst->size() < src->size() ? dst->size() : src->size();
		memmove_s(dst->data(), cnt, src->data(), cnt);
		return cnt;
	}

	Device *self_;
	BOOL is_requested_stop_reading_;
	std::thread reader_thread_;

	typedef std::function<void(std::vector<BYTE> const &)> handler_bytearray;

	std::unique_ptr<handler_bytearray> OnInput;

	Impl(Device *pSelf) :self_(pSelf), file_(NULL)
	{
	}

	virtual ~Impl(void)
	{
		close();
	}

	static const DWORD TIMEOUT_DEFAULT = 1000; // 1sec

	BOOL open(Path devicePath, BOOL with_detailed_caps = TRUE)
	{
		close();
		
		HANDLE h = ::CreateFile(devicePath.tstr.c_str(),
								(GENERIC_READ|GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), 
								NULL,        // no SECURITY_ATTRIBUTES structure
								OPEN_EXISTING, // No special create flags
								0,   // Open device as non-overlapped so we can get data
								NULL);       // No template file
		file_ = std::fopen(devicePath.str.c_str(), "rb+");
		if (h != INVALID_HANDLE_VALUE)
		{
			info_.reset(new Info(h, with_detailed_caps));
			setTimeout(TIMEOUT_DEFAULT, TIMEOUT_DEFAULT);

			return TRUE;
		}

		return FALSE;
	}

	BOOL is_opened(void)
	{
		std::lock_guard<std::mutex> lock(mtx_);

		return (bool)info_ && info_->hDevice != INVALID_HANDLE_VALUE;
	}

	BOOL is_opened(Path const &p)
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return p.tstr == device_path_;
	}

	void close(void)
	{
		if (info_)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			//if (Updater::IsAlive())
			//	Updater::EndThread(INFINITE);

			//if (hEvent != INVALID_HANDLE_VALUE)
			//{
			//	::ClosePath(hEvent);
			//	hEvent = INVALID_HANDLE_VALUE;
			//}

			std::fclose(file_);
			file_ = NULL;

			HANDLE h = info_->hDevice;
			info_.reset();
			::CloseHandle(h);
			device_path_.clear();
		}
	}

	BOOL setFeature(std::vector<BYTE> const &src)
	{

		BOOL bRet = (bool)info_;
		if (bRet)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			copyBuffer(&info_->featureReport.buffer, &src);
			bRet = (BOOL)::HidD_SetFeature(
				info_->hDevice, 
				info_->featureReport.buffer.data(), info_->capabilities.FeatureReportByteLength);
		}

		return bRet;
	}

	BOOL getFeature(BYTE report_id, std::vector<BYTE> *dst = NULL)
	{

		BOOL bRet = (bool)info_;
		if (bRet)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			info_->featureReport.buffer[0] = report_id;
			bRet = ::HidD_GetFeature(info_->hDevice,
				info_->featureReport.buffer.data(), info_->capabilities.FeatureReportByteLength);
			if (dst != NULL)
			{
				dst->assign(info_->featureReport.buffer.begin(), info_->featureReport.buffer.end());
			}
		}

		return bRet;
	}

	BOOL write(std::vector<BYTE> const &src)
	{

		BOOL bRet = (bool)info_;
		if (bRet)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			copyBuffer(&info_->outputReport.buffer, &src);
			DWORD bytesWritten = 0;
			bRet = ::WriteFile(
				info_->hDevice,
				info_->outputReport.buffer.data(), info_->capabilities.OutputReportByteLength,
				&bytesWritten, NULL) && bytesWritten == info_->capabilities.OutputReportByteLength;
		}

		return bRet;

	}

	BOOL startFifo(void)
	{
		if (!info_) return FALSE;

		std::lock_guard<std::mutex> lock(mtx_);

		::HidD_FlushQueue(info_->hDevice);

		is_requested_stop_reading_ = FALSE;
		std::thread th(std::bind(&Impl::read_async, this));

		reader_thread_.swap(th);
		return reader_thread_.joinable();
	}

	void stopFifo(void)
	{
		if (!info_) return;

		is_requested_stop_reading_ = TRUE;
		reader_thread_.join();
		if (!reader_thread_.joinable())
		{
			std::lock_guard<std::mutex> lock(mtx_);
			::HidD_FlushQueue(info_->hDevice);
		}
	}

	BOOL usesFifo(void)
	{
		return reader_thread_.joinable();
	}

	BOOL setTimeout(DWORD ms_timeout_read, DWORD ms_timeout_write)
	{
		if (!info_) return FALSE;
		std::lock_guard<std::mutex> lock(mtx_);

		COMMTIMEOUTS to = { 0 };
		to.ReadTotalTimeoutConstant = ms_timeout_read;
		to.WriteTotalTimeoutConstant = ms_timeout_write;
		
		return SetCommTimeouts(info_->hDevice, &to);
	}

	BOOL getTimeout(DWORD *ms_timeout_read, DWORD *ms_timeout_write)
	{
		if (!info_) return FALSE;
		std::lock_guard<std::mutex> lock(mtx_);

		COMMTIMEOUTS to;
		BOOL ret = GetCommTimeouts(info_->hDevice, &to);
		*ms_timeout_read  = to.ReadTotalTimeoutConstant;
		*ms_timeout_write = to.WriteTotalTimeoutConstant;

		return ret;
	}

	BOOL read(std::vector<BYTE> *dst = NULL)
	{
		BOOL bRet = (bool)info_;
		if (bRet)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			DWORD bytesRead = 0;
			bRet = ::ReadFile(info_->hDevice,
				info_->inputReport.buffer.data(), info_->capabilities.InputReportByteLength,
				&bytesRead, NULL);
			if (dst != NULL)
			{
				dst->assign(info_->inputReport.buffer.begin(), info_->inputReport.buffer.end());
			}
		}

		return bRet;
	}

	//(LPBYTE dst = NULL, int size = 0)
	//{
	//	if (dst == NULL)
	//	{
	//		dst = pDevice->InputReportBuffer;
	//		size = pDevice->Caps.InputReportByteLength;
	//	}
	//	Lock();
	//	BOOL bRet = (pDevice != NULL &&
	//		pDevice->InputReportBuffer != NULL &&
	//		pDevice->Caps.InputReportByteLength != 0 &&
	//		size >= (int)pDevice->Caps.InputReportByteLength);
	//	if (bRet)
	//	{
	//		if (Updater::IsAlive())
	//		{ // Asynchronous reading
	//			bRet = fifo_.size() > 0;
	//			if (bRet)
	//			{ // Queued data exist
	//				std::vector<BYTE> data = fifo_.front();
	//				m_fifo.pop();
	//				copyBuffer(dst, size, &data.front(), data.size());
	//			}
	//		}
	//		else
	//		{ // Synchronous reading
	//			bRet = ::ReadInputReport(pDevice);
	//			if (bRet && dst != pDevice->InputReportBuffer)
	//				copyBuffer(dst, size, pDevice->InputReportBuffer, pDevice->Caps.InputReportByteLength);
	//		}
	//	}
	//	Unlock();
	//	return bRet;
	//}

	//BOOL readAsync(void)
	//{
	//	Lock();
	//	BOOL bRet = (pDevice != NULL &&
	//		pDevice->InputReportBuffer != NULL &&
	//		pDevice->Caps.InputReportByteLength != 0 &&
	//		!Updater::IsDying());
	//	if (bRet)
	//	{
	//		if (hEvent != INVALID_HANDLE_VALUE) ::ClosePath(hEvent);
	//		hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	//		bRet = (hEvent != INVALID_HANDLE_VALUE && (BOOL)::ReadInputReportOverlapped(pDevice, hEvent));
	//	}
	//	Unlock();
	//	return bRet;
	//}

	//DWORD waitForRead(LPBYTE dst, int size, DWORD timeout)
	//{
	//	if (pDevice == NULL ||
	//		pDevice->InputReportBuffer == NULL ||
	//		pDevice->Caps.InputReportByteLength == 0 ||
	//		hEvent == INVALID_HANDLE_VALUE ||
	//		Updater::IsDying()) return WAIT_FAILED;

	//	DWORD waitStatus = ::WaitForSingleObject(hEvent, timeout);
	//	Lock();
	//	if (waitStatus != WAIT_TIMEOUT)
	//	{
	//		::ClosePath(hEvent);
	//		hEvent = INVALID_HANDLE_VALUE;
	//	}
	//	if (waitStatus == WAIT_OBJECT_0 && size >= (int)pDevice->Caps.InputReportByteLength)
	//	{
	//		copyBuffer(dst, size, pDevice->InputReportBuffer, pDevice->Caps.InputReportByteLength);
	//	}
	//	Unlock();

	//	return waitStatus;
	//}

	void read_async(void)
	{
		do
		{
			if (read())
			{
				std::lock_guard<std::mutex> lock(mtx_);
				fifo_.push(info_->inputReport.buffer);
				if (OnInput) (*OnInput)(info_->inputReport.buffer);
			}
			std::this_thread::yield();
		} while (!is_requested_stop_reading_);

		is_requested_stop_reading_ = FALSE;
	}
};

std::vector<Device::Path> Device::Enumerate()
{
	return Impl::enumerate();
}

std::vector<Device::Path> Device::Search(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
{
	return Impl::search(vendor_id, product_id, usage_page, usage);
}

BOOL Device::Test(Device::Path const &p, USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
{
	return Impl::test(p, vendor_id, product_id, usage_page, usage);
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

BOOL Device::IsOpenedPath(Device::Path const &devicePath) const
{
	return pImpl->is_opened(devicePath);
}

void Device::Close(void)
{
	pImpl->close();
}

BOOL Device::SetFeature(std::vector<BYTE> const &src)
{
	return pImpl-> setFeature(src);
}

BOOL Device::GetFeature(std::vector<BYTE> * dst)
{
	return pImpl->getFeature((*dst)[0], dst);
}

std::vector<BYTE> const * Device::GetFeature(BYTE id)
{
	return pImpl->getFeature(id)? &pImpl->info_->featureReport.buffer: NULL;
}

BOOL Device::Write(std::vector<BYTE> const &src)
{
	return pImpl->write(src);
}

BOOL Device::Read(std::vector<BYTE> * dst)
{
	return pImpl->read(dst);
}

std::vector<BYTE> const * Device::Read(void)
{
	return pImpl->read() ? &pImpl->info_->inputReport.buffer : NULL;
}

USHORT Device::GetVendorId(void) const
{
	return pImpl->info_->attributes.VendorID;
}

USHORT Device::GetProductId(void) const
{
	return pImpl->info_->attributes.ProductID;
}

USHORT Device::GetUsagePage(void) const
{
	return pImpl->info_->capabilities.UsagePage;
}

USHORT Device::GetUsage(void) const
{
	return pImpl->info_->capabilities.Usage;
}

USHORT Device::GetFeatureReportLength(void) const
{
	return pImpl->info_->capabilities.FeatureReportByteLength;
}

USHORT Device::GetInputReportLength(void) const
{
	return pImpl->info_->capabilities.InputReportByteLength;
}

USHORT Device::GetOutputReportLength(void) const
{
	return pImpl->info_->capabilities.OutputReportByteLength;
}
