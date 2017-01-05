#pragma once

#include <memory>

namespace wahidx
{

class Device
{
private:
	struct Impl;
	std::unique_ptr<Impl> pImpl;

public:
	Device();
	virtual ~Device();
};

} // namespace wahidx
