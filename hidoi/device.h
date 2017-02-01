#pragma once

#include "hidoi.h"

namespace hidoi
{
	class RawInput;
	class Parser;

	class Device
	{

	public: // types and variables
		struct Path
		{
			struct Impl;
			Impl *pImpl;
			Path & operator=(Path const&);
			Path(Path const&);
			Path(LPCTSTR ctstr);
			~Path();
		};

	private: // types and variables

		struct Impl;
		std::unique_ptr<Impl> pImpl;

	public: // static methods

		static std::vector<Path> Enumerate();

		static std::vector<Path> Search(
			USHORT vendor_id = 0x0000, USHORT product_id = 0x0000, USAGE usage_page = 0x0000, USAGE usage = 0x0000);
		static std::vector<Path> SearchByID(USHORT vendor_id, USHORT product_id = 0x0000)
		{
			return Search(vendor_id, product_id);
		}
		static std::vector<Path> SearchByUsage(USAGE usage_page, USAGE usage)
		{
			return Search(0x0000, 0x0000, usage_page, usage);
		}

		static BOOL Test(Path const &p,
			USHORT vendor_id = 0x0000, USHORT product_id = 0x0000, USAGE usage_page = 0x0000, USAGE usage = 0x0000);
		static BOOL const & TestByID(Path const &p, USHORT vendor_id, USHORT product_id = 0x0000)
		{
			return Test(p, vendor_id, product_id);
		}
		static BOOL const & TestByUsage(Path const &p, USAGE usage_page, USAGE usage)
		{
			return Test(p, 0x0000, 0x0000, usage_page, usage);
		}

	private: // class methods

		Device(Device const&) = delete;
		Device(Device &&) = delete;
		Device& operator=(Device const&) = delete;
		Device& operator=(Device &&) = delete;

	public: // class methods

		Device();

		virtual ~Device();

		BOOL Open(Path const &p);

		BOOL IsOpened(void) const;

		BOOL IsOpenedPath(Path const &p) const;

		void Close(void);

		BOOL SetFeature(std::vector<BYTE> const &src);

		BOOL GetFeature(std::vector<BYTE> * dst);

		std::vector<BYTE> const * GetFeature(BYTE id);

		BOOL SetOutput(std::vector<BYTE> const &src);

		BOOL GetInput(std::vector<BYTE> * dst);

		std::vector<BYTE> const * GetInput(void);

		typedef void (InputListener)(std::vector<BYTE> const &);
		BOOL StartListeningInput(std::function<InputListener> const &listener);

		BOOL IsListeningInput(void) const;

		void QuitListeningInput(void);

		USHORT GetVendorId(void) const;

		USHORT GetProductId(void) const;

		USHORT GetVersionNumber(void) const;

		USAGE GetUsagePage(void) const;

		USAGE GetUsage(void) const;

		USHORT GetFeatureReportLength(void) const;

		USHORT GetInputReportLength(void) const;

		USHORT GetOutputReportLength(void) const;

		Parser GetParser(void) const;
	};

}; // namespace wahidx
