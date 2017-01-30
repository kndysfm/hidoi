#pragma once


#include "hidoi.h"

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

	public: // methods
		Watcher();

		virtual ~Watcher();

		BOOL Start();

		void Stop();
	};

};
