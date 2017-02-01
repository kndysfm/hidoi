#pragma once

#include "hidoi.h"

namespace hidoi
{
	class Parser
	{
	public:
		class Report
		{
		private:
			struct Impl;
			std::unique_ptr<Report::Impl> pImpl;
			Report(Report const &) = delete;
			Report & operator=(Report const &) = delete;
			Report(Report &&);
			Report & operator=(Report &&);

		public:
			Report();
			virtual ~Report();

			void Clear();

			BOOL HasValue(USAGE page, USAGE usage) const;
			ULONG GetValue(USAGE page, USAGE usage) const;
			void SetValue(USAGE page, USAGE usage, ULONG value);

			BOOL HasScaledValue(USAGE page, USAGE usage) const;
			LONG GetScaledValue(USAGE page, USAGE usage) const;
			void SetScaledValue(USAGE page, USAGE usage, LONG value);

			BOOL HasValueArray(USAGE page, USAGE usage) const;
			std::vector<CHAR> const &GetValueArray(USAGE page, USAGE usage) const;
			void SetValueArray(USAGE page, USAGE usage, std::vector<CHAR> values);

			BOOL HasButton(USAGE page, USAGE usage) const;
			BOOL GetButton(USAGE page, USAGE usage) const;
			void SetButton(USAGE page, USAGE usage, BOOL pressed);
		};

	private:
		struct Impl;
		std::unique_ptr<Impl> pImpl;

		Parser(Parser const &) = delete;
		Parser & operator=(Parser const &) = delete;

	public:
		Parser();
		Parser(LPCTSTR name);
		Parser(Parser &&);
		Parser & operator=(Parser &&);
		virtual ~Parser();


		Report const * ParseInput(std::vector<BYTE> const &report);

		Report const * ParseFeature(std::vector<BYTE> const &report);

	};

} // namespace wahidx