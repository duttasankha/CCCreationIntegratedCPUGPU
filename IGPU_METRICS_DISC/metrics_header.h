#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <CL/cl.h>
#include <CL/cl_platform.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>
#include <metrics_discovery_api.h>
#include <vector>

#define OpenLibrary(__filename) dlopen(__filename,RTLD_LAZY | RTLD_LOCAL)
#define GetFunctionAddress(_handle, _name) dlsym(_handle, _name)
#define GetLastError() dlerror()

//static const char * cMDLIBFILENAME = "libmd.so";

using namespace MetricsDiscovery;

CloseMetricsDevice_fn  CloseMetricsDevice;
OpenMetricsDevice_fn   OpenMetricsDevice;
IMetricsDevice_1_5*    m_MetricsDevice;


extern bool InitMetrics();
