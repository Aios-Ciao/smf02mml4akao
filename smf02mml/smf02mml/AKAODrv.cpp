
#include "AKAODrv.h"
#include <iomanip>

string AKAODRV::d2h(uint8 value)
{
	stringstream ss;

	ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << int(value);

	return ss.str();

}

string AKAODRV::d2s(uint8 value)
{
	stringstream ss;

	ss << int(value);

	return ss.str();

}