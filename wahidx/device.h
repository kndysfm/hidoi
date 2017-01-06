#pragma once

#include <memory>
#include <vector>

namespace wahidx
{

class Device
{
public:

	class Path;

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

	class Info;

private:

	struct Impl;
	std::unique_ptr<Impl> pImpl;

public:

	Device();
	virtual ~Device();

	BOOL Open(Path const &p);

	BOOL IsOpened(void) const;

	BOOL IsOpenedPath(Path const &p) const;

	void Close(void);

	BOOL SetFeature(std::vector<BYTE> const &src);

	BOOL GetFeature(std::vector<BYTE> * dst);

	std::vector<BYTE> const * GetFeature(BYTE id);

	BOOL Write(std::vector<BYTE> const &src);

	BOOL Read(std::vector<BYTE> * dst);

	std::vector<BYTE> const * Read(void);

	BOOL IsAsyncReading(void) const;

	USHORT GetVendorId(void) const;

	USHORT GetProductId(void) const;

	USHORT GetUsagePage(void) const;

	USHORT GetUsage(void) const;

	USHORT GetFeatureReportLength(void) const;

	USHORT GetInputReportLength(void) const;

	USHORT GetOutputReportLength(void) const;
};

} // namespace wahidx
