#define _HIDOI_PARSER_REPORT_CPP_
#include "parser.h"

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
	void set_value(USAGEPAGE_USAGE upu, ULONG v)
	{
		Values[upu] = v; 
	}

	BOOL has_scaled_value(USAGEPAGE_USAGE upu) const
	{
		return ScaledValues.count(upu) != 0;
	}
	LONG get_scaled_value(USAGEPAGE_USAGE upu)
	{
		return ScaledValues[upu];
	}
	void set_scaled_value(USAGEPAGE_USAGE upu, LONG v)
	{
		ScaledValues[upu] = v; 
	}

	BOOL has_value_array(USAGEPAGE_USAGE upu) const
	{
		return ValueArrays.count(upu) != 0;
	}
	std::vector<CHAR> const &get_value_array(USAGEPAGE_USAGE upu)
	{
		return ValueArrays[upu];
	}
	void set_value_array(USAGEPAGE_USAGE upu, std::vector<CHAR> a)
	{
		ValueArrays[upu] = a; 
	}

	BOOL has_button(USAGEPAGE_USAGE upu) const
	{
		return Buttons.count(upu) != 0;
	}
	BOOL get_button(USAGEPAGE_USAGE upu)
	{
		return Buttons[upu];
	}
	void set_button(USAGEPAGE_USAGE upu, BOOL b)
	{
		Buttons[upu] = b;
	}
};

Parser::Report::Report() : pImpl(new Parser::Report::Impl) { }
Parser::Report::Report(Parser::Report &&r)
{
	this->pImpl.swap(r.pImpl);
}
Parser::Report & Parser::Report::operator=(Parser::Report &&r)
{
	this->pImpl.swap(r.pImpl);
	return *this;
}
Parser::Report::Report(Parser::Report const &r)
{
	*this->pImpl = *r.pImpl;
}
Parser::Report & Parser::Report::operator=(Parser::Report const &r)
{
	*this->pImpl = *r.pImpl;
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
void Parser::Report::SetValue(USAGE page, USAGE usage, ULONG value)
{
	pImpl->set_value(TO_USAGEPAGE_USAGE(page, usage), value);
}

BOOL Parser::Report::HasScaledValue(USAGE page, USAGE usage) const
{
	return pImpl->has_scaled_value(TO_USAGEPAGE_USAGE(page, usage));
}
LONG Parser::Report::GetScaledValue(USAGE page, USAGE usage) const
{
	return pImpl->get_scaled_value(TO_USAGEPAGE_USAGE(page, usage));
}
void Parser::Report::SetScaledValue(USAGE page, USAGE usage, LONG value)
{
	pImpl->set_scaled_value(TO_USAGEPAGE_USAGE(page, usage), value);
}

BOOL Parser::Report::HasValueArray(USAGE page, USAGE usage) const
{
	return pImpl->has_value_array(TO_USAGEPAGE_USAGE(page, usage));
}
std::vector<CHAR> const &Parser::Report::GetValueArray(USAGE page, USAGE usage) const
{
	return pImpl->get_value_array(TO_USAGEPAGE_USAGE(page, usage));
}

void Parser::Report::SetValueArray(USAGE page, USAGE usage, std::vector<CHAR> values)
{
	pImpl->set_value_array(TO_USAGEPAGE_USAGE(page, usage), values);
}


BOOL Parser::Report::HasButton(USAGE page, USAGE usage) const
{
	return pImpl->has_button(TO_USAGEPAGE_USAGE(page, usage));
}
BOOL Parser::Report::GetButton(USAGE page, USAGE usage) const
{
	return pImpl->get_button(TO_USAGEPAGE_USAGE(page, usage));
}
void Parser::Report::SetButton(USAGE page, USAGE usage, BOOL pressed)
{
	pImpl->set_button(TO_USAGEPAGE_USAGE(page, usage), pressed);
}