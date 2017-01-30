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

		BOOL Start();

		void Stop();

		typedef void (DeviceChangeEventListener)(DWORD_PTR dwData);
		BOOL RegisterDeviceChangeEventListener(UINT uEventType, std::function<DeviceChangeEventListener>  const &listener);

	};

};
