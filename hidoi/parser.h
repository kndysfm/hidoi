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

		public:
			Report();
			Report(Report const &);
			Report & operator=(Report const &);
			Report(Report &&);
			Report & operator=(Report &&);
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

		//! subset of HIDP_VALUE_CAPS structure
		struct ValueCaps
		{
			ULONG    UnitsExp, Units;
			LONG     LogicalMin, LogicalMax;
			LONG     PhysicalMin, PhysicalMax;
		};

		BOOL HasInputValue(USAGE page, USAGE usage) const;
		BOOL HasInputButton(USAGE page, USAGE usage) const;
		BOOL GetInputReportId(USAGE page, USAGE usage, LPBYTE report_id) const;
		BOOL GetInputValueCaps(USAGE page, USAGE usage, ValueCaps *caps) const;

		BOOL HasFeatureValue(USAGE page, USAGE usage) const;
		BOOL HasFeatureButton(USAGE page, USAGE usage) const;
		BOOL GetFeatureReportId(USAGE page, USAGE usage, LPBYTE report_id) const;
		BOOL GetFeatureValueCaps(USAGE page, USAGE usage, ValueCaps *caps) const;

		BOOL HasOutputValue(USAGE page, USAGE usage) const;
		BOOL HasOutputButton(USAGE page, USAGE usage) const;
		BOOL GetOutputReportId(USAGE page, USAGE usage, LPBYTE report_id) const;
		BOOL GetOutputValueCaps(USAGE page, USAGE usage, ValueCaps *caps) const;

		Report const * ParseInput(std::vector<BYTE> const &report);

		std::vector<BYTE> DeparseInput(BYTE report_id, Report const &report);

		Report const * ParseFeature(std::vector<BYTE> const &report);

		std::vector<BYTE> DeparseFeature(BYTE report_id, Report const &report);

		Report const * ParseOutput(std::vector<BYTE> const &report);

		std::vector<BYTE> DeparseOutput(BYTE report_id, Report const &report);

	};

} // namespace wahidx