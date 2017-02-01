#include "utils.h"

#if defined(_DEBUG)
#include <stdarg.h>

void hidoi::utils::trace(LPCTSTR fmt, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, fmt);

	size_t len = (1 + _tcslen(fmt)) * 8;
	_TCHAR *str = new _TCHAR[len];
	_vstprintf_s(str, len, fmt, arg_ptr);
	_tcscat_s(str, len, _T("\r\n"));
	::OutputDebugString(str);
	delete[] str;

	va_end(arg_ptr);
}

void hidoi::utils::assert(BOOL condition, LPCTSTR message)
{
	if (!condition)
	{
		if (message) trace(_T("[Assert]: %s"), message);
		::DebugBreak();
	}
}

#else
void hidoi::utils::Trace(LPCTSTR fmt, ...) {}
void Assert(BOOL expression, LPCTSTR message) { }
#endif


