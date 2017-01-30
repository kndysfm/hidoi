#pragma once
#include "hidoi.h"

namespace hidoi
{
	class RawInput
	{
	private: // types and variables
		struct Impl;
		std::unique_ptr<Impl> pImpl;

	private: // methods
		RawInput(RawInput const &) = delete;	// copy constructor
		RawInput(RawInput &&) = delete;	// move constructor
		RawInput & operator=(RawInput const &) = delete; // copy operator
		RawInput & operator=(RawInput &&) = delete; // move operator
		RawInput();
		virtual ~RawInput();

	public:
		static RawInput &GetInstance();

		void Update();

		BOOL Search(RID_DEVICE_INFO_HID const &query);

		RID_DEVICE_INFO_HID const *Search(HANDLE h);

		BOOL Register(HWND hWnd, RID_DEVICE_INFO_HID const &query);

		BOOL Unregister(HWND hWnd, RID_DEVICE_INFO_HID const &query);

		void UnregisterAll(HWND hWnd);
	};
}

