#define _HIDOI_DEVICE_CPP_
#include "device.h"

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

using namespace hidoi;

struct Device::Path::Impl
{
	Tstring tstr;
	std::string str;

	Impl(LPCTSTR ctstr) :tstr(ctstr)
	{
#ifdef  UNICODE 
		size_t size_dst = (_tcslen(ctstr) + 2) * sizeof(_TCHAR);
		char* dst = new char[size_dst];
		memset(dst, 0, size_dst);
		size_t num_converted = 0;
		wcstombs_s(&num_converted, dst, size_dst, ctstr, size_dst);
		str = dst;
		delete[] dst;
#else
		str = tstr;
#endif
	}
};

Device::Path::Path(Device::Path const& p) : pImpl(new Device::Path::Impl(p.pImpl->tstr.c_str()))
{
}

Device::Path & Device::Path::operator=(Device::Path const& r)
{
	*this->pImpl = *r.pImpl;
	return *this;
}

Device::Path & Device::Path::operator=(Path && r)
{
	this->pImpl = r.pImpl;
	r.pImpl = nullptr;
	return *this;
}

Device::Path::Path(LPCTSTR ctstr): pImpl(new Device::Path::Impl(ctstr))
{
}

Device::Path::~Path()  { delete pImpl; }

bool Device::Path::operator==(Device::Path const& r) const
{
	return this->pImpl->tstr == r.pImpl->tstr;
}

struct Device::Impl
{
public:
	class Info
	{
	public:
		BOOL							valid;
		HANDLE							hDevice;
		HIDD_ATTRIBUTES					attributes;
		HIDP_CAPS						capabilities;

		std::vector<BYTE>				inputReportBuffer;
		std::vector<BYTE>				outputReportBuffer;
		std::vector<BYTE>				featureReportBuffer;

		Info(HANDLE h, BOOL with_detailed_caps)
		{
			valid = FALSE;
			hDevice = h;
			if (h != INVALID_HANDLE_VALUE)
			{	// handle valid
				if (HidD_GetAttributes(h, &attributes))
				{	// attributes available
					PHIDP_PREPARSED_DATA preparsedData;
					if (HidD_GetPreparsedData(h, &preparsedData))
					{	// pre-parsed data available
						if (HIDP_STATUS_SUCCESS == HidP_GetCaps(preparsedData, &capabilities))
						{	// capabilities available
							valid = TRUE;
							if (with_detailed_caps) PrepareReportInfo();
						}
						else utils::trace(_T("Failed to get HID capabilities"));
						HidD_FreePreparsedData(preparsedData);
					}
					else utils::trace(_T("Failed to get HID preparsed data"));
				}
				else utils::trace(_T("Failed to get HID attributes"));
			}
		}

		~Info(void)
		{
		}

		void PrepareReportInfo()
		{
			inputReportBuffer.resize(capabilities.InputReportByteLength);
			outputReportBuffer.resize(capabilities.OutputReportByteLength);
			featureReportBuffer.resize(capabilities.FeatureReportByteLength);
		}
	};

	static std::vector<Device::Path> enumerate()
	{
		std::vector<Device::Path> paths;
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

	static std::vector<Device::Path> search(USHORT vendor_id, USHORT product_id, USAGE usage_page, USAGE usage)
	{
		std::vector<Device::Path> paths;
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


	static BOOL test(Path const & devicePath, USHORT vendorId, USHORT productId, USAGE usagePage, USAGE usage)
	{
		Device dev;
		if (dev.pImpl->open(devicePath, FALSE))
		{
			BOOL vid_ok = vendorId == 0 || vendorId == dev.GetVendorId();
			BOOL pid_ok = productId == 0 || productId == dev.GetProductId();
			BOOL page_ok = usagePage == 0 || usagePage == dev.GetUsagePage();
			BOOL usage_ok = usage == 0 || usage == dev.GetUsage();

			dev.Close();
			return vid_ok && pid_ok && page_ok && usage_ok;
		}

		return FALSE;
	}

	Tstring device_path_;
	std::unique_ptr<Info> info_;
	std::mutex mtx_;

	static int copyBuffer(std::vector<BYTE> *dst, std::vector<BYTE> const *src)
	{
		int cnt = dst->size() < src->size() ? dst->size() : src->size();
		memmove_s(dst->data(), cnt, src->data(), cnt);
		return cnt;
	}

	Device *self_;
	BOOL is_requested_stop_reading_;
	std::thread reader_thread_;
	OVERLAPPED ol_write_, ol_read_;
	bool is_read_pending_;
	DWORD milsec_timeout_;

	static const DWORD DEFAULT_MILSEC_TIMEOUT = 1000;

	Impl(Device *pSelf) :self_(pSelf), milsec_timeout_(DEFAULT_MILSEC_TIMEOUT)
	{
		memset(&ol_write_, 0, sizeof(OVERLAPPED));
		memset(&ol_read_, 0, sizeof(OVERLAPPED));
		is_read_pending_ = false;
	}

	virtual ~Impl(void)
	{
		close();
	}

	BOOL open(Path const &devicePath, BOOL with_detailed_caps = TRUE)
	{
		close();
		
		HANDLE h = ::CreateFile(devicePath.pImpl->tstr.c_str(),
								(GENERIC_READ|GENERIC_WRITE), (FILE_SHARE_READ | FILE_SHARE_WRITE), 
								NULL,        // no SECURITY_ATTRIBUTES structure
								OPEN_EXISTING, // No special create flags
								FILE_FLAG_OVERLAPPED,   // Open device as overlapped
								NULL);       // No template file
		//file_ = std::fopen(devicePath.pImpl->str.c_str(), "rb+");
		if (h != INVALID_HANDLE_VALUE)
		{
			info_.reset(new Info(h, with_detailed_caps));
			if (info_->valid)
			{
				ol_read_.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
				ol_write_.hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

				return TRUE;
			}
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
		return p.pImpl->tstr == device_path_;
	}

	void close(void)
	{
		if (info_)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			stop_async();

			::CloseHandle(ol_write_.hEvent);
			::CloseHandle(ol_read_.hEvent);
			HANDLE h = info_->hDevice;
			info_.reset();
			::CloseHandle(h);
			device_path_.clear();
		}
	}

	BOOL set_feature(std::vector<BYTE> const &src)
	{
		if (!info_) return FALSE;

		std::lock_guard<std::mutex> lock(mtx_);
		copyBuffer(&info_->featureReportBuffer, &src);
		if (::HidD_SetFeature(
			info_->hDevice,
			info_->featureReportBuffer.data(), info_->capabilities.FeatureReportByteLength))
		{
			return TRUE;
		}
		
		utils::trace(_T("Failed to set feature"));
		return FALSE;
	}

	BOOL get_feature(BYTE report_id, std::vector<BYTE> *dst = NULL)
	{
		if (!info_) return FALSE;

		std::lock_guard<std::mutex> lock(mtx_);
		info_->featureReportBuffer[0] = report_id;
		if (::HidD_GetFeature(info_->hDevice,
			info_->featureReportBuffer.data(), info_->capabilities.FeatureReportByteLength))
		{
			if (dst != NULL)
			{
				dst->assign(info_->featureReportBuffer.begin(), info_->featureReportBuffer.end());
			}
			return TRUE;
		}

		utils::trace(_T("Failed to get feature"));
		return FALSE;
	}

	BOOL write(std::vector<BYTE> const &src)
	{
		if (!info_) return FALSE;

		std::lock_guard<std::mutex> lock(mtx_);
		DWORD bytes_written;
		copyBuffer(&info_->outputReportBuffer, &src);
		::ResetEvent(ol_write_.hEvent);
		if (::WriteFile(
			info_->hDevice,
			info_->outputReportBuffer.data(), info_->capabilities.OutputReportByteLength,
			&bytes_written, &ol_write_))
		{
			if (::WaitForSingleObject(ol_write_.hEvent, milsec_timeout_) != WAIT_OBJECT_0)
			{
				::CancelIo(info_->hDevice);
				utils::trace(_T("Timeout in writing Output report"));
				return FALSE;
			}
			if (::GetOverlappedResult(info_->hDevice, &ol_write_, &bytes_written, TRUE))
			{
				return TRUE;
			}
		}
			
		utils::trace(_T("Failed to set Output report"));
		return FALSE;

	}

	InputListener listener_;

	BOOL start_async(InputListener const &listener)
	{
		if (!info_) return FALSE;

		std::lock_guard<std::mutex> lock(mtx_);

		::HidD_FlushQueue(info_->hDevice);

		listener_ = listener;
		is_requested_stop_reading_ = FALSE;
		std::thread th(std::bind(&Impl::read_async, this));

		reader_thread_.swap(th);
		return reader_thread_.joinable();
	}

	void stop_async(void)
	{
		if (!info_) return;

		is_requested_stop_reading_ = TRUE;
		if (reader_thread_.joinable())
		{
			reader_thread_.join();
			if (!reader_thread_.joinable())
			{
				std::lock_guard<std::mutex> lock(mtx_);
				::HidD_FlushQueue(info_->hDevice);
			}
			listener_ = nullptr;
		}
	}

	BOOL is_reading_async(void)
	{
		return reader_thread_.joinable();
	}

	BOOL read(std::vector<BYTE> *dst = nullptr, bool async = false)
	{
		if (!info_) return FALSE;

		DWORD bytes_read = 0;
		if (!is_read_pending_)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			::ResetEvent(ol_read_.hEvent);
			if (::ReadFile(info_->hDevice,
				info_->inputReportBuffer.data(), info_->capabilities.InputReportByteLength,
				&bytes_read, &ol_read_))
			{
			}
			else
			{
				if (::GetLastError() == ERROR_IO_PENDING)
				{
					utils::trace(_T("HID IO is pending"));
				}
				else
				{
					utils::trace(_T("Failed to get Input report"));
					return FALSE;
				}
			}
			is_read_pending_ = true;
		}

		if (!async && ::WaitForSingleObject(ol_write_.hEvent, milsec_timeout_) != WAIT_OBJECT_0)
		{	// when synchronous reading, it will be timeout
			std::lock_guard<std::mutex> lock(mtx_);
			::CancelIo(info_->hDevice);
			is_read_pending_ = false;
			utils::trace(_T("Timeout in reading Input report"));
			return FALSE;
		}

		if (is_read_pending_)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			if (::GetOverlappedResult(info_->hDevice, &ol_read_, &bytes_read, async ? FALSE/*no wait*/ : TRUE/*wait*/))
			{
				is_read_pending_ = false;
				if (bytes_read > 0)
				{
					if (dst)
					{
						dst->assign(info_->inputReportBuffer.begin(), info_->inputReportBuffer.end());
					}
					return TRUE;
				}
			}
		}
		return FALSE;
	}

	void read_async(void)
	{
		do
		{
			std::vector<BYTE> rep;
			if (read(&rep, true))
			{
				if (listener_) listener_(rep);
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
	return pImpl-> set_feature(src);
}

BOOL Device::GetFeature(std::vector<BYTE> * dst)
{
	return pImpl->get_feature((*dst)[0], dst);
}

std::vector<BYTE> const * Device::GetFeature(BYTE id)
{
	return pImpl->get_feature(id)? &pImpl->info_->featureReportBuffer: NULL;
}

BOOL Device::SetOutput(std::vector<BYTE> const &src)
{
	return pImpl->write(src);
}

BOOL Device::GetInput(std::vector<BYTE> * dst)
{
	return pImpl->read(dst);
}

std::vector<BYTE> const * Device::GetInput(void)
{
	return pImpl->read() ? &pImpl->info_->inputReportBuffer : NULL;
}

BOOL Device::StartListeningInput(InputListener const &listener)
{
	return pImpl->start_async(listener);
}

BOOL Device::IsListeningInput(void) const
{
	return  pImpl->is_reading_async();
}

void Device::QuitListeningInput(void)
{
	pImpl->stop_async();
}

USHORT Device::GetVendorId(void) const
{
	return pImpl->info_->attributes.VendorID;
}

USHORT Device::GetProductId(void) const
{
	return pImpl->info_->attributes.ProductID;
}

USHORT Device::GetVersionNumber(void) const
{
	return pImpl->info_->attributes.VersionNumber;
}

USAGE Device::GetUsagePage(void) const
{
	return pImpl->info_->capabilities.UsagePage;
}

USAGE Device::GetUsage(void) const
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

Parser Device::GetParser(void) const
{
	return Parser(pImpl->device_path_.c_str());
}