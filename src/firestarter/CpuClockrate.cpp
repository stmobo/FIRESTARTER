#include <firestarter/Logging/Log.hpp>
#include <firestarter/Firestarter.hpp>

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/SmallVector.h>

#include <ctime>

extern "C" {
#include <firestarter/Compat/x86.h>
}

using namespace firestarter;

std::unique_ptr<llvm::MemoryBuffer> Firestarter::getScalingGovernor(void) {
	return this->getFileAsStream("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
}

int Firestarter::genericGetCpuClockrate(void) {
	auto procCpuinfo = this->getFileAsStream("/proc/cpuinfo");
	if (nullptr == procCpuinfo) {
		return EXIT_FAILURE;
	}

	llvm::SmallVector<llvm::StringRef, _HW_DETECT_MAX_OUTPUT> lines;
	llvm::SmallVector<llvm::StringRef, 2> clockrateVector;
	procCpuinfo->getBuffer().split(lines, "\n");
	
	for (size_t i = 0; i < lines.size(); i++) {
		if (lines[i].startswith("cpu MHz")) {
			lines[i].split(clockrateVector, ':');
			break;
		}
	}

	std::string clockrate;

	if (clockrateVector.size() == 2) {
		clockrate = clockrateVector[1].str();
		clockrate.erase(0, 1);
	} else {
		firestarter::log::fatal() << "Can't determine clockrate from /proc/cpuinfo";
	}

	std::unique_ptr<llvm::MemoryBuffer> scalingGovernor;
	if (nullptr == (scalingGovernor = this->getScalingGovernor())) {
		return EXIT_FAILURE;
	}

	std::string governor = scalingGovernor->getBuffer().str();
	
	auto scalingCurFreq = this->getFileAsStream("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
	auto cpuinfoCurFreq = this->getFileAsStream("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq");
	auto scalingMaxFreq = this->getFileAsStream("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");
	auto cpuinfoMaxFreq = this->getFileAsStream("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");

	if (governor.compare("performance") || governor.compare("powersave")) {
		if (nullptr == scalingCurFreq) {
			if (nullptr != cpuinfoCurFreq) {
				clockrate = cpuinfoCurFreq->getBuffer().str();
			}
		} else {
			clockrate = scalingCurFreq->getBuffer().str();
		}
	} else {
		if (nullptr == scalingMaxFreq) {
			if(nullptr != cpuinfoMaxFreq) {
				clockrate = cpuinfoMaxFreq->getBuffer().str();
			}
		} else {
			clockrate = scalingMaxFreq->getBuffer().str();
		}
	}

	this->clockrate = std::stoi(clockrate);
	this->clockrate *= 1000;

	return EXIT_SUCCESS;
}


#ifdef __ARCH_X86

// measures clockrate using the Time-Stamp-Counter
// only constant TSCs will be used (i.e. power management indepent TSCs)
// save frequency in highest P-State or use generic fallback if no invarient TSC is available
int Firestarter::getCpuClockrate(void) {
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::microseconds ticks;

	unsigned long long start1_tsc,start2_tsc,end1_tsc,end2_tsc;
	unsigned long long time_diff;
	unsigned long long clock_lower_bound,clock_upper_bound,clock;
	unsigned long long clockrate=0;
	int i,num_measurements=0,min_measurements;

	Clock::time_point start_time, end_time;

	auto scalingGovernor = this->getScalingGovernor();
	if (nullptr == scalingGovernor) {
		return EXIT_FAILURE;
	}

	std::string governor = scalingGovernor->getBuffer().str();

	/* non invariant TSCs can be used if CPUs run at fixed frequency */
	if (!x86_has_invariant_rdtsc(vendor.c_str()) && governor.compare("performance") && governor.compare("powersave")) {
		return this->genericGetCpuClockrate();
	}

	min_measurements=5;

	if (!x86_has_rdtsc()) {
		return this->genericGetCpuClockrate();
	}

	i = 3;

	do {
			//start timestamp
			start1_tsc = x86_timestamp();
			start_time = Clock::now();
			start2_tsc = x86_timestamp();

			//waiting
			do {
					end1_tsc = x86_timestamp();
			}
			while (end1_tsc<start2_tsc+1000000*i);   /* busy waiting */

			//end timestamp
			do{
				end1_tsc = x86_timestamp();
				end_time = Clock::now();
				end2_tsc = x86_timestamp();

				time_diff = std::chrono::duration_cast<ticks>(end_time - start_time).count();
			}
			while (0 == time_diff);

			clock_lower_bound=(((end1_tsc-start2_tsc)*1000000)/(time_diff));
			clock_upper_bound=(((end2_tsc-start1_tsc)*1000000)/(time_diff));

			// if both values differ significantly, the measurement could have been interrupted between 2 rdtsc's
			if (((double)clock_lower_bound>(((double)clock_upper_bound)*0.999))&&((time_diff)>2000))
			{
					num_measurements++;
					clock=(clock_lower_bound+clock_upper_bound)/2;
					if(clockrate==0) clockrate=clock;
					else if (clock<clockrate) clockrate=clock;
			}
			i+=2;
	} while (((time_diff)<10000)||(num_measurements<min_measurements));

	this->clockrate = clockrate;

	return EXIT_SUCCESS;
}

#elif __ARCH_UNKNOWN

int Firestarter::getCpuClockrate(void) {
	return this->genericGetCpuClockrate();
}
#endif