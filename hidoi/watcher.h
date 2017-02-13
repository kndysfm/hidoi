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
		typedef void (RawInputHandler)(std::vector<BYTE> const &);

		typedef void (DeviceArrivalHandler)(hidoi::Device::Path const &);

		typedef void (DeviceRemoveHandler)(void);

		typedef void (DeviceChangeHandler)(void);

	private: // methods
		Watcher(Watcher const &) = delete;	// copy constructor
		Watcher(Watcher &&) = delete;	// move constructor
		Watcher & operator=(Watcher const &) = delete; // copy operator
		Watcher & operator=(Watcher &&) = delete; // move operator
		Watcher();
		virtual ~Watcher();

	public: // methods
		static Watcher &GetInstance(void); // singleton pattern

		BOOL WatchRawInput(Target const &target, std::function<RawInputHandler>  const &listener);
		BOOL UnwatchRawInput(Target const &target);

		BOOL WatchRawInput(RawInput const &ri, std::function<RawInputHandler>  const &listener);
		BOOL UnwatchRawInput(RawInput const &ri);

		BOOL UnwatchAllRawInputs();

		BOOL WatchConnection(Target const &target, std::function<DeviceArrivalHandler> const &on_arrive, std::function<DeviceRemoveHandler> const &on_remove);
		BOOL UnwatchConnection(Target const &target);
		BOOL UnwatchAllConnections();
		
		BOOL WatchDeviceChange(std::function<DeviceChangeHandler> const &on_devchange);
		BOOL UnwatchDeviceChange();

	};

};
