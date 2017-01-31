#include "parser.h"

EXTERN_C
{
#include <hidsdi.h>
}
#include <map>

#include "tstring.h"

using namespace hidoi;

typedef DWORD USAGEPAGE_USAGE;
#define TO_USAGEPAGE_USAGE(page, usage)	(((USAGEPAGE_USAGE) page << 16) | usage)

struct Parser::Impl
{
private:
	HANDLE handle_;
	PHIDP_PREPARSED_DATA			pp_;

	struct ReportCaps
	{
		std::vector<HIDP_VALUE_CAPS>	ValueCaps;
		std::vector<HIDP_BUTTON_CAPS>	ButtonCaps;
	};

	ReportCaps							caps_input_, caps_output_, caps_feature_;

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
			USHORT upage = vcaps_buf[idx].UsagePage;
			USHORT usage = vcaps_buf[idx].NotRange.Usage;
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
			USHORT upage = bcaps_buf[idx].UsagePage;
			r->ButtonCaps.push_back(bcaps_buf[idx]);
		}
		delete[] bcaps_buf;
	}

public:
	Impl(HANDLE h):
		handle_(h), pp_(NULL)
	{
		if (h != INVALID_HANDLE_VALUE)
		{
			init(h);
		}
	}

	~Impl()
	{
		if (pp_ != NULL)
		{
			HidD_FreePreparsedData(pp_);
			pp_ = NULL;
		}
	}

	void init(HANDLE h)
	{
		// get the report descriptor info
		HidD_GetPreparsedData(h, &pp_);
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

	void parse(HIDP_REPORT_TYPE rep_type, std::vector<BYTE> &report)
	{
		std::map<USAGEPAGE_USAGE, ULONG> Values;
		std::map<USAGEPAGE_USAGE, LONG> ScaledValues;
		std::vector<USAGE>				ButtonUsagePages;
		std::vector<USAGEPAGE_USAGE>	Buttons;

		ReportCaps *caps;
		switch (rep_type)
		{
		case HidP_Input: caps = &caps_input_; 
		break;
		case HidP_Output: caps = &caps_output_; 
		break;
		case HidP_Feature: caps = &caps_feature_; 
		break;
		default: return;
		}

		ULONG length = caps->ValueCaps.size();
		for (ULONG idx = 0; idx < length; idx++)
		{
			PHIDP_VALUE_CAPS cap = &caps->ValueCaps[idx];
			USHORT upage = cap->UsagePage;
			USHORT usage = cap->NotRange.Usage;
			USAGEPAGE_USAGE upage_usage = TO_USAGEPAGE_USAGE(upage, usage);
			ULONG val = 0;
			LONG phys_val = 0;
			NTSTATUS status = HIDP_STATUS_SUCCESS;
			if (cap->ReportCount == 1)
			{
				status = HidP_GetUsageValue(rep_type, upage, 0, usage, &val, pp_, (PCHAR)report.data(), report.size());
				Values[upage_usage] = val;
				status = HidP_GetScaledUsageValue(rep_type, upage, 0, usage, &phys_val, pp_, (PCHAR)report.data(), report.size());
				ScaledValues[upage_usage] = phys_val;
			}
			else if (cap->ReportCount > 1)
			{
				std::vector<CHAR> buf;
				USHORT size_array = cap->ReportCount * ((cap->BitSize + 7) >> 3);
				buf.resize(size_array);
				status = HidP_GetUsageValueArray(rep_type, upage, 0, usage, buf.data(), size_array, pp_, (PCHAR)report.data(), report.size());
			}
		}

		length = caps->ButtonCaps.size();
		for (ULONG idx = 0; idx < length; idx++)
		{
			PHIDP_BUTTON_CAPS cap = &caps->ButtonCaps[idx];
			USHORT upage = cap->UsagePage;
			
			std::vector<USAGE_AND_PAGE> list_btns;
			ULONG length_btns;
			HidP_GetUsagesEx(rep_type, 0, NULL, &length_btns, pp_, (PCHAR)report.data(), report.size());
			list_btns.resize(length_btns);
			HidP_GetUsagesEx(rep_type, 0, list_btns.data(), &length_btns, pp_, (PCHAR)report.data(), report.size());

			Buttons.clear();
			for (ULONG i = 0; i < length_btns; i++)
			{
				USAGEPAGE_USAGE upage_usage = TO_USAGEPAGE_USAGE(list_btns[i].UsagePage, list_btns[i].Usage);
				Buttons.push_back(upage_usage);
			}
		}

	}

};

Parser::Parser(HANDLE hDevice):
	pImpl(new Impl(hDevice))
{
}


Parser::~Parser()
{
}

void Parser::Init(HANDLE hDevice)
{
	pImpl->init(hDevice);
}