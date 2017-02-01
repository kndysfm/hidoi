#pragma once


#include "hidoi.h"

#include <functional>

namespace hidoi
{

	class Watcher
	{
	private: // types and variables
		struct Impl;
		std::unique_ptr<Impl> pImpl;

	private: // methods
		Watcher(Watcher const &) = delete;	// copy constructor
		Watcher(Watcher &&) = delete;	// move constructor
		Watcher & operator=(Watcher const &) = delete; // copy operator
		Watcher & operator=(Watcher &&) = delete; // move operator
		Watcher();
		virtual ~Watcher();

	public: // methods
		static Watcher &GetInstance(void); // singleton pattern

		typedef void (DeviceChangeEventListener)(DWORD_PTR dwData);
		BOOL RegisterDeviceChangeEventListener(UINT uEventType, std::function<DeviceChangeEventListener>  const &listener);
		BOOL UnregisterDeviceChangeEventListener(UINT uEventType);
		BOOL RegisterDeviceArrivalEventListener(std::function<DeviceChangeEventListener>  const &listener);
		BOOL UnregisterDeviceArrivalEventListener();
		BOOL RegisterDeviceRemoveEventListener(std::function<DeviceChangeEventListener>  const &listener);
		BOOL UnregisterDeviceRemoveEventListener();


		typedef void (RawInputEventListener)(std::vector<BYTE> const &);
		struct Target
		{
			USHORT VendorId, ProductId;
			USAGE UsagePage, Usage;
		};
		BOOL RegisterRawInputEventListener(Target const &target, std::function<RawInputEventListener>  const &listener);
		BOOL UnregisterRawInputEventListener(Target const &target);

	};

};
