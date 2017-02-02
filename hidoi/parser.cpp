#include "parser.h"

EXTERN_C
{
#include <hidsdi.h>
}
#include <map>

using namespace hidoi;

struct Parser::Impl
{
private:
	LPCTSTR path_;
	HANDLE handle_;
	PHIDP_PREPARSED_DATA			pp_;
	HIDP_CAPS caps_hid_;

	struct ReportCaps
	{
		std::vector<HIDP_VALUE_CAPS>	ValueCaps;
		std::vector<HIDP_BUTTON_CAPS>	ButtonCaps;
	};

	ReportCaps		caps_input_,	caps_output_,	caps_feature_;
	Report			rep_input_,		rep_output_,		rep_feature_;

	void init_caps(HIDP_REPORT_TYPE rep_type, ReportCaps *r)
	{
		USHORT length = 0;
		HIDP_VALUE_CAPS *vcaps_buf;
		HidP_GetValueCaps(rep_type, NULL, &length, pp_);
		vcaps_buf = new HIDP_VALUE_CAPS[length];
		HidP_GetValueCaps(rep_type, vcaps_buf, &length, pp_);
		r->ValueCaps.reserve(length);
		for (USHORT idx = 0; idx < length; idx++)
		{
			r->ValueCaps.push_back(vcaps_buf[idx]);
		}
		delete[] vcaps_buf;

		length = 0;
		HidP_GetButtonCaps(rep_type, NULL, &length, pp_);
		HIDP_BUTTON_CAPS *bcaps_buf = new HIDP_BUTTON_CAPS[length];
		HidP_GetButtonCaps(rep_type, bcaps_buf, &length, pp_);
		r->ButtonCaps.reserve(length);
		for (USHORT idx = 0; idx < length; idx++)
		{
			r->ButtonCaps.push_back(bcaps_buf[idx]);
		}
		delete[] bcaps_buf;
	}

	ReportCaps * get_caps(HIDP_REPORT_TYPE rep_type)
	{
		switch (rep_type)
		{
		case HidP_Input: return &caps_input_;
		case HidP_Output: return &caps_output_; 
		case HidP_Feature: return &caps_feature_; 
		default: return nullptr;
		}
	}

	Report * get_report(HIDP_REPORT_TYPE rep_type)
	{
		switch (rep_type)
		{
		case HidP_Input: return &rep_input_;
		case HidP_Output: return &rep_output_;
		case HidP_Feature: return &rep_feature_;
		default: return nullptr;
		}
	}

	USHORT get_byte_length(HIDP_REPORT_TYPE rep_type) const
	{
		switch (rep_type)
		{
		case HidP_Input: return caps_hid_.InputReportByteLength;
		case HidP_Output: return caps_hid_.OutputReportByteLength;
		case HidP_Feature: return caps_hid_.FeatureReportByteLength;
		default: return 0;
		}
	}

public:
	Impl(LPCTSTR path):
		path_(path), pp_(NULL)
	{
		handle_ = ::CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (handle_ != INVALID_HANDLE_VALUE)
		{
			init(handle_);
		}
	}

	~Impl()
	{
		if (pp_ != NULL)
		{
			HidD_FreePreparsedData(pp_);
			pp_ = NULL;
		}
		if (handle_ != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(handle_);
			handle_ = INVALID_HANDLE_VALUE;
		}
	}

	void init(HANDLE h)
	{
		// get the report descriptor info
		::HidD_GetPreparsedData(h, &pp_);
		::HidP_GetCaps(pp_, &caps_hid_);
		// make capabilities objects for each report type
		init_caps(HidP_Input, &caps_input_);
		init_caps(HidP_Output, &caps_output_);
		init_caps(HidP_Feature, &caps_feature_);
	}

	BOOL has_value(HIDP_REPORT_TYPE rep_type, USAGE page, USAGE usage)
	{
		auto *caps = get_caps(rep_type);
		if (!caps) return FALSE;
		
		for (auto const &c : caps->ValueCaps)
		{
			if (page == c.UsagePage && usage == c.NotRange.Usage) return TRUE;
		}

		return FALSE;
	}

	BOOL has_button(HIDP_REPORT_TYPE rep_type, USAGE page, USAGE usage)
	{
		auto *caps = get_caps(rep_type);
		if (!caps) return FALSE;

		for (auto const &c : caps->ButtonCaps)
		{
			if (page == c.UsagePage && usage == c.NotRange.Usage) return TRUE;
		}

		return FALSE;
	}

	BOOL get_report_id(HIDP_REPORT_TYPE rep_type, USAGE page, USAGE usage, LPBYTE report_id)
	{
		auto *caps = get_caps(rep_type);
		if (!caps) return FALSE;

		for (auto const &c : caps->ValueCaps)
		{
			if (page == c.UsagePage && usage == c.NotRange.Usage)
			{
				*report_id = c.ReportID;
				return TRUE;
			}
		}
		for (auto const &c : caps->ButtonCaps)
		{
			if (page == c.UsagePage && usage == c.NotRange.Usage) 
			{
				*report_id = c.ReportID;
				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL get_value_caps(HIDP_REPORT_TYPE rep_type, USAGE page, USAGE usage, ValueCaps *dst)
	{
		auto *caps = get_caps(rep_type);
		if (!caps) return FALSE;

		for (auto const &c : caps->ValueCaps)
		{
			if (page == c.UsagePage && usage == c.NotRange.Usage)
			{
				dst->UnitsExp		= c.UnitsExp;
				dst->Units			= c.Units;
				dst->LogicalMin		= c.LogicalMin;
				dst->LogicalMax		= c.LogicalMax;
				dst->PhysicalMin	= c.PhysicalMin;
				dst->PhysicalMax	= c.PhysicalMax;
				return TRUE;
			}
		}
		return FALSE;
	}

	static bool handle_hidp_status(NTSTATUS s)
	{
#define CASE_HIDP_STATUS(S)	case S: utils::trace(_T(#S));
		switch (s)
		{
		CASE_HIDP_STATUS(HIDP_STATUS_SUCCESS)
		return true;
		CASE_HIDP_STATUS(HIDP_STATUS_NULL)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_INVALID_PREPARSED_DATA)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_INVALID_REPORT_TYPE)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_INVALID_REPORT_LENGTH)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_USAGE_NOT_FOUND)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_VALUE_OUT_OF_RANGE)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_BAD_LOG_PHY_VALUES)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_BUFFER_TOO_SMALL)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_INTERNAL_ERROR)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_I8042_TRANS_UNKNOWN)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_INCOMPATIBLE_REPORT_ID)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_NOT_VALUE_ARRAY)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_IS_VALUE_ARRAY)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_DATA_INDEX_NOT_FOUND)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_DATA_INDEX_OUT_OF_RANGE)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_BUTTON_NOT_PRESSED)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_REPORT_DOES_NOT_EXIST)
		break;
		CASE_HIDP_STATUS(HIDP_STATUS_NOT_IMPLEMENTED)
		break;
		}
		return false;
#undef CASE_HIDP_STATUS
	}

	Report const * parse(HIDP_REPORT_TYPE rep_type, std::vector<BYTE> &report)
	{
		Report *r = get_report(rep_type);
		ReportCaps *caps = get_caps(rep_type);
		if (!r || !caps) return nullptr;

		r->Clear();

		ULONG length = caps->ValueCaps.size();
		for (auto const &vc: caps->ValueCaps)
		{
			if (vc.ReportID != report[0]) continue; ///BUGBUG not considering report without ID

			USHORT page = vc.UsagePage;
			USHORT usage = vc.NotRange.Usage; ///BUGBUG not considering a field with range
			NTSTATUS status = HIDP_STATUS_SUCCESS;
			if (vc.ReportCount == 1)
			{
				ULONG val = 0;
				LONG phys_val = 0;
				status = HidP_GetUsageValue(rep_type, page, 0, usage, &val, pp_, (PCHAR)report.data(), report.size());
				if (status == HIDP_STATUS_SUCCESS) r->SetValue(page, usage, val);
				status = HidP_GetScaledUsageValue(rep_type, page, 0, usage, &phys_val, pp_, (PCHAR)report.data(), report.size());
				if (status == HIDP_STATUS_SUCCESS) r->SetScaledValue(page, usage, phys_val);
			}
			else if (vc.ReportCount > 1)
			{
				std::vector<CHAR> buf;
				USHORT size_array = vc.ReportCount * ((vc.BitSize + 7) >> 3);
				buf.resize(size_array);
				status = HidP_GetUsageValueArray(rep_type, page, 0, usage, buf.data(), size_array, pp_, (PCHAR)report.data(), report.size());
				if (status == HIDP_STATUS_SUCCESS) r->SetValueArray(page, usage, buf);
			}
		}

		ULONG len = 0;
		std::vector<USAGE_AND_PAGE> btns_pressed;
		::HidP_GetUsagesEx(rep_type, 0, NULL, &len, pp_, (PCHAR)report.data(), report.size());
		if (len > 0)
		{
			btns_pressed.resize(len);
			::HidP_GetUsagesEx(rep_type, 0, btns_pressed.data(), &len, pp_, (PCHAR)report.data(), report.size());
		}

		for (auto const &bc: caps->ButtonCaps)
		{
			if (bc.ReportID != report[0]) continue; ///BUGBUG not considering report without ID

			USHORT page = bc.UsagePage;
			USHORT usage = bc.NotRange.Usage; ///BUGBUG not considering buttons as range of usage

			for (auto const &b : btns_pressed)
			{
				r->SetButton(page, usage, (b.UsagePage == page && b.Usage == usage));
			}
		}

		return r;
	}

	std::vector<BYTE> deparse(HIDP_REPORT_TYPE rep_type, UCHAR report_id, Report const &report)
	{
		std::vector<BYTE> rep_raw;

		ReportCaps *caps = get_caps(rep_type);
		if (!caps) return rep_raw;
		
		ULONG len = get_byte_length(rep_type);
		if (len == 0) return rep_raw;
		rep_raw.resize(len, 0x00);

		ULONG length = caps->ValueCaps.size();
		for (auto const &vc : caps->ValueCaps)
		{
			if (vc.ReportID != report_id) continue; ///BUGBUG not considering report without ID

			USHORT page = vc.UsagePage;
			USHORT usage = vc.NotRange.Usage; ///BUGBUG not considering a field with range
			NTSTATUS status = HIDP_STATUS_SUCCESS;
			if (vc.ReportCount == 1)
			{
				status = HidP_SetUsageValue(rep_type, page, 0, usage, report.GetValue(page, usage), pp_, (PCHAR)rep_raw.data(), len);
				if (status != HIDP_STATUS_SUCCESS) handle_hidp_status(status);
			}
			else if (vc.ReportCount > 1)
			{
				std::vector<CHAR> buf = report.GetValueArray(page, usage);
				status = HidP_SetUsageValueArray(rep_type, page, 0, usage, buf.data(), (USHORT)buf.size(), pp_, (PCHAR)rep_raw.data(), len);
				if (status != HIDP_STATUS_SUCCESS) handle_hidp_status(status);
			}
		}

		for (auto const &bc : caps->ButtonCaps)
		{
			if (bc.ReportID != report_id) continue; ///BUGBUG not considering report without ID

			USHORT page = bc.UsagePage;
			USHORT usage = bc.NotRange.Usage; ///BUGBUG not considering buttons as range of usage

			if (report.GetButton(page, usage))
			{
				ULONG len_btn = 1;
				::HidP_SetUsages(rep_type, page, 0, &usage, &len_btn, pp_, (PCHAR)rep_raw.data(), len);
			}
		}

		return rep_raw;
	}

};

Parser::Parser(): pImpl(nullptr) { }

Parser::Parser(LPCTSTR name):
	pImpl(new Impl(name))
{
}

Parser::Parser(Parser &&r)
{
	this->pImpl.swap(r.pImpl);
}

Parser & Parser::operator=(Parser &&r)
{
	this->pImpl.swap(r.pImpl);
	return *this;
}

Parser::~Parser()
{
}

BOOL Parser::HasInputValue(USAGE page, USAGE usage) const
{
	return pImpl ? pImpl->has_value(HidP_Input, page, usage) : FALSE;
}

BOOL Parser::HasInputButton(USAGE page, USAGE usage) const
{
	return pImpl ? pImpl->has_button(HidP_Input, page, usage) : FALSE;
}

BOOL Parser::GetInputReportId(USAGE page, USAGE usage, LPBYTE report_id) const
{
	return pImpl ? pImpl->get_report_id(HidP_Input, page, usage, report_id) : FALSE;
}

BOOL Parser::GetInputValueCaps(USAGE page, USAGE usage, ValueCaps *caps) const
{
	return pImpl ? pImpl->get_value_caps(HidP_Input, page, usage, caps) : FALSE;
}

BOOL Parser::HasOutputValue(USAGE page, USAGE usage) const
{
	return pImpl ? pImpl->has_value(HidP_Output, page, usage) : FALSE;
}

BOOL Parser::HasOutputButton(USAGE page, USAGE usage) const
{
	return pImpl ? pImpl->has_button(HidP_Output, page, usage) : FALSE;
}

BOOL Parser::GetOutputReportId(USAGE page, USAGE usage, LPBYTE report_id) const
{
	return pImpl ? pImpl->get_report_id(HidP_Output, page, usage, report_id) : FALSE;
}

BOOL Parser::GetOutputValueCaps(USAGE page, USAGE usage, ValueCaps *caps) const
{
	return pImpl ? pImpl->get_value_caps(HidP_Output, page, usage, caps) : FALSE;
}

BOOL Parser::HasFeatureValue(USAGE page, USAGE usage) const
{
	return pImpl ? pImpl->has_value(HidP_Feature, page, usage) : FALSE;
}

BOOL Parser::HasFeatureButton(USAGE page, USAGE usage) const
{
	return pImpl ? pImpl->has_button(HidP_Feature, page, usage) : FALSE;
}

BOOL Parser::GetFeatureReportId(USAGE page, USAGE usage, LPBYTE report_id) const
{
	return pImpl ? pImpl->get_report_id(HidP_Feature, page, usage, report_id) : FALSE;
}

BOOL Parser::GetFeatureValueCaps(USAGE page, USAGE usage, ValueCaps *caps) const
{
	return pImpl ? pImpl->get_value_caps(HidP_Feature, page, usage, caps) : FALSE;
}

Parser::Report const * Parser::ParseInput(std::vector<BYTE> const &report)
{
	std::vector<BYTE> buf = report;
	return pImpl? pImpl->parse(HidP_Input, buf): nullptr;
}

std::vector<BYTE> Parser::DeparseInput(BYTE report_id, Report const &report)
{
	return pImpl? pImpl->deparse(HidP_Input, report_id, report): std::vector<BYTE>();
}

Parser::Report const * Parser::ParseFeature(std::vector<BYTE> const &report)
{
	std::vector<BYTE> buf = report;
	return pImpl? pImpl->parse(HidP_Feature, buf): nullptr;
}

std::vector<BYTE> Parser::DeparseFeature(BYTE report_id, Report const &report)
{
	return pImpl ? pImpl->deparse(HidP_Feature, report_id, report) : std::vector<BYTE>();
}

Parser::Report const * Parser::ParseOutput(std::vector<BYTE> const &report)
{
	std::vector<BYTE> buf = report;
	return pImpl ? pImpl->parse(HidP_Output, buf) : nullptr;
}

std::vector<BYTE> Parser::DeparseOutput(BYTE report_id, Report const &report)
{
	return pImpl ? pImpl->deparse(HidP_Output, report_id, report) : std::vector<BYTE>();
}

