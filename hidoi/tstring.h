#pragma once

#include <tchar.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>

//! Declarations to use TCHAR typed string class in an implementation file
//! Do NOT include this file in HEADER file

#define USING_TCHAR_STRING \
namespace { \
	 /*_TCHAR typed string*/ \
	template class std::basic_string<_TCHAR>;\
	typedef std::basic_string<_TCHAR> Tstring; \
}

#define USING_TCHAR_SSTREAM \
namespace { \
	/*_TCHAR typed ostringstream*/ \
	template class std::basic_ostringstream<_TCHAR>;\
	typedef std::basic_ostringstream<_TCHAR> Tosstream;\
	/*_TCHAR typed istringstream*/ \
	template class std::basic_istringstream<_TCHAR>;\
	typedef std::basic_istringstream<_TCHAR> Tisstream;\
}

#define USING_TCHAR_FSTREAM \
namespace { \
	/*_TCHAR typed ostringstream*/ \
	template class std::basic_ofstream<_TCHAR>;\
	typedef std::basic_ofstream<_TCHAR> Tofstream;\
	/*_TCHAR typed istringstream*/ \
	template class std::basic_ifstream<_TCHAR>;\
	typedef std::basic_ifstream<_TCHAR> Tifstream;\
}

#define USING_TCHAR_STRING_VECTOR \
namespace { \
	/*Tstring vector*/ \
	template class std::vector<std::basic_string<_TCHAR>>;\
	typedef std::vector<std::basic_string<_TCHAR>> TstringVector; \
}
