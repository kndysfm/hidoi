#include "parser.h"

EXTERN_C
{
#include <hidsdi.h>
}
#include <map>

using namespace hidoi;

typedef DWORD USAGEPAGE_USAGE;
#define TO_USAGEPAGE_USAGE(page, usage)	(((USAGEPAGE_USAGE) page << 16) | usage)

struct Parser::Report::Impl
{
public:
	std::map<USAGEPAGE_USAGE, ULONG>				Values;
	std::map<USAGEPAGE_USAGE, LONG>					ScaledValues;
	std::map<USAGEPAGE_USAGE, std::vector<CHAR>>	ValueArrays;
	std::map<USAGEPAGE_USAGE, BOOL>					Buttons;

	void clear()
	{
		Values.clear(); ScaledValues.clear(); ValueArrays.clear();
		Buttons.clear();
	}

	BOOL has_value(USAGEPAGE_USAGE upu) const
	{
		return Values.count(upu) != 0;
	}
	ULONG get_value(USAGEPAGE_USAGE upu)
	{
		return Values[upu];
	}
	BOOL set_value(USAGEPAGE_USAGE upu, ULONG v)
	{
		if (Values.count(upu) == 0)
		{
			Values[upu] = v; return TRUE;
		}
		else return FALSE;
	}

	BOOL has_scaled_value(USAGEPAGE_USAGE upu) const
	{
		return ScaledValues.count(upu) != 0;
	}
	LONG get_scaled_value(USAGEPAGE_USAGE upu)
	{
		return ScaledValues[upu];
	}
	BOOL set_scaled_value(USAGEPAGE_USAGE upu, LONG v)
	{
		if (ScaledValues.count(upu) == 0)
		{
			ScaledValues[upu] = v; return TRUE;
		}
		else return FALSE;
	}

	BOOL has_value_array(USAGEPAGE_USAGE upu) const
	{
		return ValueArrays.count(upu) != 0;
	}
	std::vector<CHAR> get_value_array(USAGEPAGE_USAGE upu)
	{
		return ValueArrays[upu];
	}
	BOOL set_value_array(USAGEPAGE_USAGE upu, std::vector<CHAR> a)
	{
		if (ValueArrays.count(upu) == 0)
		{
			ValueArrays[upu] = a; return TRUE;
		}
		else return FALSE;
	}

	BOOL has_button(USAGEPAGE_USAGE upu) const
	{
		return Buttons.count(upu) != 0;
	}
	BOOL get_button(USAGEPAGE_USAGE upu)
	{
		return Buttons[upu];
	}
	BOOL set_button(USAGEPAGE_USAGE upu, BOOL b)
	{
		if (Buttons.count(upu) == 0)
		{
			Buttons[upu] = b; return TRUE;
		}
		else return FALSE;
	}
};

Parser::Report::Report(): pImpl(new Parser::Report::Impl) { }
Parser::Report::Report(Parser::Report &&r) 
{
	this->pImpl.swap(r.pImpl); 
}
Parser::Report & Parser::Report::operator=(Parser::Report &&r)
{
	this->pImpl.swap(r.pImpl);
	return *this; 
}
Parser::Report::~Report() { }

void Parser::Report::Clear() { pImpl->clear(); }

BOOL Parser::Report::HasValue(USAGE page, USAGE usage) const
{
	return pImpl->has_value(TO_USAGEPAGE_USAGE(page, usage));
}
ULONG Parser::Report::GetValue(USAGE page, USAGE usage) const
{
	return pImpl->get_value(TO_USAGEPAGE_USAGE(page, usage));
}
BOOL Parser::Report::SetValue(USAGE page, USAGE usage, ULONG value)
{
	return pImpl->set_value(TO_USAGEPAGE_USAGE(page, usage), value);
}

BOOL Parser::Report::HasScaledValue(USAGE page, USAGE usage) const
{
	return pImpl->has_scaled_value(TO_USAGEPAGE_USAGE(page, usage));
}
LONG Parser::Report::GetScaledValue(USAGE page, USAGE usage) const
{
	return pImpl->get_scaled_value(TO_USAGEPAGE_USAGE(page, usage));
}
BOOL Parser::Report::SetScaledValue(USAGE page, USAGE usage, LONG value)
{
	return pImpl->set_scaled_value(TO_USAGEPAGE_USAGE(page, usage), value);
}

BOOL Parser::Report::HasValueArray(USAGE page, USAGE usage) const
{
	return pImpl->has_value_array(TO_USAGEPAGE_USAGE(page, usage));
}
std::vector<CHAR> const &Parser::Report::GetValueArray(USAGE page, USAGE usage) const
{
	return pImpl->get_value_array(TO_USAGEPAGE_USAGE(page, usage));
}

BOOL Parser::Report::SetValueArray(USAGE page, USAGE usage, std::vector<CHAR> values)
{
	return pImpl->set_value_array(TO_USAGEPAGE_USAGE(page, usage), values);
}


BOOL Parser::Report::HasButton(USAGE page, USAGE usage) const
{
	return pImpl->has_button(TO_USAGEPAGE_USAGE(page, usage));
}
BOOL Parser::Report::GetButton(USAGE page, USAGE usage) const
{
	return pImpl->get_button(TO_USAGEPAGE_USAGE(page, usage));
}
BOOL Parser::Report::SetButton(USAGE page, USAGE usage, BOOL pressed)
{
	return pImpl->set_button(TO_USAGEPAGE_USAGE(page, usage), pressed);
}


struct Parser::Impl
{
private:
	LPCTSTR path_;
	HANDLE handle_;
	PHIDP_PREPARSED_DATA			pp_;

	struct ReportCaps
	{
		std::vector<HIDP_VALUE_CAPS>	ValueCaps;
		std::vector<HIDP_BUTTON_CAPS>	ButtonCaps;
	};

	ReportCaps		caps_input_,	caps_output_,	caps_feature_;
	Report			rep_input_,		rep_output,		rep_feature_;

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
		// make capabilities objects for each report type
		init_caps(HidP_Input, &caps_input_);
		init_caps(HidP_Output, &caps_output_);
		init_caps(HidP_Feature, &caps_feature_);
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
		Report *r;
		ReportCaps *caps;
		switch (rep_type)
		{
		case HidP_Input: caps = &caps_input_; r = &rep_input_;
		break;
		case HidP_Output: caps = &caps_output_;  r = &rep_output;
		break;
		case HidP_Feature: caps = &caps_feature_; r = &rep_feature_;
		break;
		default: return nullptr;
		}

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
				status = HidP_GetUsageValue(rep_type, page, 0, usage, &val, pp_, (PCHAR)report.data(), report.size()); handle_hidp_status(status);
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

		for (auto const &bc: caps->ButtonCaps)
		{
			if (bc.ReportID != report[0]) continue; ///BUGBUG not considering report without ID

			USHORT page = bc.UsagePage;
			USHORT usage = bc.NotRange.Usage; ///BUGBUG not considering buttons as range of usage

			ULONG val;
			NTSTATUS status = HidP_GetUsageValue(rep_type, page, 0, usage, &val, pp_, (PCHAR)report.data(), report.size()); handle_hidp_status(status);
			if (status == HIDP_STATUS_SUCCESS) r->SetButton(page, usage, TRUE);
			else if (status == HIDP_STATUS_BUTTON_NOT_PRESSED) r->SetButton(page, usage, FALSE);
		}

		return r;
	}

};

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

Parser::Report const & Parser::ParseInput(std::vector<BYTE> const &report)
{
	std::vector<BYTE> buf = report;
	return *pImpl->parse(HidP_Input, buf);
}

Parser::Report const & Parser::ParseFeature(std::vector<BYTE> const &report)
{
	std::vector<BYTE> buf = report;
	return *pImpl->parse(HidP_Feature, buf);
}
