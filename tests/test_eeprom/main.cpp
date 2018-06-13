#include <iostream>
#include <random>
#include "Storage.h"



std::random_device rd;     // only used once to initialise (seed) engine
std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)
std::uniform_int_distribution<uint32_t> uni(0, 0xFFFFFFFF); // guaranteed unbiased


int main(int argc, char **argv)
{
	EEPROMStorage<Data> storage(4);
	
	Data d;
	for (size_t i = 0; i < 100; i++)
	{
		d.value0 = d.value1 = uni(rng);
		storage.add(d);

		EEPROMStorage<Data> storage2(4);
		Data d2;
		storage2.get(d2);
		if (d.value0 != d2.value0 || d.value1 != d2.value1)
		{
			std::cout << "ERROR: i=" << i;
			return 1;
		}

	}
	std::cout << "OK";
	return 0;
}
