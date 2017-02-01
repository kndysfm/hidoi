#pragma once
#include "hidoi.h"

namespace hidoi
{
	class RawInput
	{
	public: // types and variables

	private: // types and variables
		struct Impl;
		std::unique_ptr<Impl> pImpl;

	private: // methods
		RawInput();
	public:
		RawInput(RawInput const &);
		RawInput & operator=(RawInput const &);
		virtual ~RawInput();

		static std::vector<RawInput> Enumerate();

		static std::vector<RawInput> Search(
			USHORT vendor_id = 0x0000, USHORT product_id = 0x0000, USAGE usage_page = 0x0000, USAGE usage = 0x0000);
		static std::vector<RawInput> SearchByID(USHORT vendor_id, USHORT product_id = 0x0000)
		{
			return Search(vendor_id, product_id);
		}
		static std::vector<RawInput> SearchByUsage(USAGE usage_page, USAGE usage)
		{
			return Search(0x0000, 0x0000, usage_page, usage);
		}

		static std::vector<RawInput> SearchByHandle(HANDLE handle);

		BOOL Test(USHORT vendor_id = 0x0000, USHORT product_id = 0x0000, USAGE usage_page = 0x0000, USAGE usage = 0x0000);
		BOOL const & TestByID(USHORT vendor_id, USHORT product_id = 0x0000)
		{
			return Test(vendor_id, product_id);
		}
		BOOL const & TestByUsage(USAGE usage_page, USAGE usage)
		{
			return Test(0x0000, 0x0000, usage_page, usage);
		}

		BOOL Register(HWND hWnd);

		BOOL Unregister();

		BOOL IsRegistered();

		static void UnregisterAll();

		USHORT GetVendorId(void) const;

		USHORT GetProductId(void) const;

		USHORT GetVersionNumber(void) const;

		USAGE GetUsagePage(void) const;

		USAGE GetUsage(void) const;

		Parser GetParser(void) const;

	};
}

