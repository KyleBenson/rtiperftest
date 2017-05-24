#include "CpuMonitor.h"

CpuMonitor::CpuMonitor()
{
    _counter = 0;
    _cpuUsageTotal = 0.0;

#if defined(RTI_LINUX)

    FILE* file;
    char line[128];

    file = fopen("/proc/cpuinfo", "r");
    _numProcessors = 0;
    while (fgets(line, 128, file) != NULL) {
        if (strncmp(line, "processor", 9) == 0) {
            _numProcessors++;
        }
    }
    fclose(file);

#elif defined(RTI_DARWIN)

    int mib[4];
    std::size_t len = sizeof(_numProcessors);

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &_numProcessors, &len, NULL, 0);

    if (_numProcessors < 1) {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &_numProcessors, &len, NULL, 0);
        if (_numProcessors < 1) {
            _numProcessors = 1;
        }
    }

#elif defined(RTI_WIN32)

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    _numProcessors = sysInfo.dwNumberOfProcessors;

#else
    fprintf(stderr, "[WARNING] get CPU consumption feature, is not available in that Operator System\n");

#endif
}

void CpuMonitor::initialize()
{

#if defined(RTI_LINUX) || defined(RTI_DARWIN)

    struct tms timeSample;
    lastCPU = times(&timeSample);
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

#elif defined(RTI_WIN32)

    FILETIME ftime, fsys, fuser;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCPU, &ftime, sizeof(FILETIME));

    self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));

#endif
}

std::string CpuMonitor::get_cpu_instant()
{
    double percent = 0.0;

#if defined(RTI_LINUX) || defined(RTI_DARWIN)

    struct tms timeSample;
    clock_t now;
    now = times(&timeSample);
    if (now > lastCPU && timeSample.tms_stime >= lastSysCPU &&
            timeSample.tms_utime >= lastUserCPU) {
        percent = (timeSample.tms_stime - lastSysCPU) +
                (timeSample.tms_utime - lastUserCPU);
        percent /= (now - lastCPU);
        percent /= _numProcessors;
        percent *= 100;
        lastCPU = now;
        lastSysCPU = timeSample.tms_stime;
        lastUserCPU = timeSample.tms_utime;
    }

#elif defined(RTI_WIN32)

    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));
    if (now.QuadPart - lastCPU.QuadPart>0.0) {
        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&sys, &fsys, sizeof(FILETIME));
        memcpy(&user, &fuser, sizeof(FILETIME));
        percent = (double)(sys.QuadPart - lastSysCPU.QuadPart) +
                (user.QuadPart - lastUserCPU.QuadPart);
        percent /= (double)(now.QuadPart - lastCPU.QuadPart);
        lastCPU = now;
        lastUserCPU = user;
        lastSysCPU = sys;
        percent /= _numProcessors;
        percent *= 100;
    }

#endif

    _cpuUsageTotal += percent;
    _counter++;
    std::ostringstream strs;
    strs <<  std::fixed << std::setprecision(2)  << percent;
    std::string output = "CPU: "+ strs.str()+"%";
    return output;
}

std::string CpuMonitor::get_cpu_average()
{
    std::ostringstream strs;
    if (_counter==0) {
        //in the case that the CpuMonitor was just initialize, get_cpu_instant
        get_cpu_instant();
    }
    strs <<  std::fixed << std::setprecision(2)  << (double)(_cpuUsageTotal/_counter);
    std::string output = "CPU: "+ strs.str()+"%";
    return output;
}