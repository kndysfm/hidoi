#pragma once

#include "hidoi.h"

namespace hidoi
{
	class Parser
	{
	private:
		struct Impl;
		std::unique_ptr<Impl> pImpl;

	public:
		Parser(HANDLE hDevice = INVALID_HANDLE_VALUE);
		virtual ~Parser();

		void Init(HANDLE hDevice);

		class Report;

		Report const & UnpackInput(std::vector<BYTE> const &report);

		Report const & UnpackFeature(std::vector<BYTE> const &report);

	};

} // namespace wahidx