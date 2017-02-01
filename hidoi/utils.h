#pragma once

#include "hidoi.h"

namespace hidoi
{
	namespace utils
	{
		void trace(LPCTSTR fmt, ...);
		void assert(BOOL condition, LPCTSTR message = NULL);
	};
} // namespace hidoi

#ifndef _STRINGIZE
#define _STRINGIZEX(x)	#x
#define _STRINGIZE(x)	_STRINGIZEX(x)
#endif
#ifndef HERE
#define __TCSLINE__		_T(_STRINGIZE(__LINE__))
#define __TCSFILE__		_T(__FILE__)
#define _HERE			__TCSFILE__ _T("(") __TCSLINE__ _T(")")
#define _HERE_T(x)		_HERE _T(x) _T(":\r\n\t")
#endif

#define HIDOI_UTILS_TRACE_HERE()	hidoi::utils::trace(_HERE)
#define HIDOI_UTILS_TRACE_STR(exp)	hidoi::utils::trace(_HERE_T(exp))
#define HIDOI_UTILS_TRACE_INT(exp)	hidoi::utils::trace(_HERE_T(#exp"\t= %d"),		(int)exp)
#define HIDOI_UTILS_TRACE_HEX(exp)	hidoi::utils::trace(_HERE_T(#exp"\t= %08XH"),	(int)exp)
#define HIDOI_UTILS_TRACE_BOOL(exp)	hidoi::utils::trace(_HERE_T(#exp"\t= %s"),		((BOOL)exp)?_T("TRUE"):_T("FALSE"))
#define HIDOI_UTILS_ASSERT(cond)	hidoi::utils::assert(cond, _HERE_T(#cond) _T(" must be TRUE"))

