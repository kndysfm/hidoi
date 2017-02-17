#pragma once

#include "hidoi.h"

namespace hidoi
{
	class Watcher
	{
	private: // types and variables
		struct Impl;
		std::unique_ptr<Impl> pImpl;

	public: // types
		struct Target
		{
			const USHORT VendorId, ProductId;
			const USAGE UsagePage, Usage;

			Target(USHORT vid, USHORT pid, USAGE usagePage, USAGE usage) :
				VendorId(vid), ProductId(pid), UsagePage(usagePage), Usage(usage) { }

			Target(USAGE usagePage, USAGE usage) :
				VendorId(0x0000), ProductId(0x0000), UsagePage(usagePage), Usage(usage) { }

			Target() = delete;// :
				//VendorId(0x0000), ProductId(0x0000), UsagePage(0x0000), Usage(0x0000) { }

			bool operator==(Target const & x) const;
			bool operator!=(Target const & x) const;
			bool operator<(Target const & x) const;
			bool operator>(Target const & x) const;
			bool operator<=(Target const & x) const;
			bool operator>=(Target const & x) const;
		};
		typedef std::function<void (std::vector<BYTE> const &)> RawInputHandler;

		typedef std::function<void (hidoi::Device::Path const &)> DeviceArrivalHandler;

		typedef std::function<void (void)> DeviceRemoveHandler;

		typedef std::function<void (void)> DeviceChangeHandler;

	private: // methods
		Watcher(Watcher const &) = delete;	// copy constructor
		Watcher(Watcher &&) = delete;	// move constructor
		Watcher & operator=(Watcher const &) = delete; // copy operator
		Watcher & operator=(Watcher &&) = delete; // move operator
		Watcher();
		virtual ~Watcher();

	public: // methods
		static Watcher &GetInstance(void); // singleton pattern

		BOOL WatchRawInput(Target const &target, RawInputHandler const &listener);
		BOOL UnwatchRawInput(Target const &target);

		BOOL WatchRawInput(RawInput const &ri, RawInputHandler const &listener);
		BOOL UnwatchRawInput(RawInput const &ri);

		BOOL UnwatchAllRawInputs();

		BOOL WatchConnection(Target const &target, DeviceArrivalHandler const &on_arrive, DeviceRemoveHandler const &on_remove);
		BOOL UnwatchConnection(Target const &target);
		BOOL UnwatchAllConnections();
		
		BOOL WatchDeviceChange(DeviceChangeHandler const &on_devchange);
		BOOL UnwatchDeviceChange();

	};

};
