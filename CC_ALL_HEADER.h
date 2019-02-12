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
#include <iostream>
#include <metrics_discovery_api.h>
#include <vector>

#define OpenLibrary(__filename) dlopen(__filename,RTLD_LAZY | RTLD_LOCAL)
#define GetFunctionAddress(_handle, _name) dlsym(_handle, _name)
#define GetLastError() dlerror()

#define KB(X) ((X)*1024)
#define MB(X) ((X)*1024*1024)

typedef int type_1;
//static const char * cMDLIBFILENAME = "libmd.so";

//extern void printMetricNameWrapper(IMetricSet_1_1* m_MetricSet);
//extern void PrintMetricUnitsWrapper(IMetricSet_1_1* m_MetricSet);

extern "C++" {
using namespace MetricsDiscovery;
using namespace std;
//IMetricsDevice_1_5* m_MetricsDevice;
//IMetricSet_1_1* m_MetricSet;


extern bool initMetrics(IMetricSet_1_1* m_MetricSet,IMetricsDevice_1_5* m_MetricsDevice);
bool ActivateMetricSet(IMetricSet_1_1* m_MetricSet);
void DeactivateMetricSet(IMetricSet_1_1* m_MetricSet);
void GetMetricsFromReport(IMetricSet_1_1* m_MetricSet,const char* pReportData,std::vector<TTypedValue_1_0>& results,std::vector<TTypedValue_1_0>& maxValues);
void PrintMetricNames(IMetricSet_1_1* m_MetricSet , std::ostream& os );
void PrintMetricValues(	IMetricSet_1_1* m_MetricSet,std::ostream& os,const std::string& name,const std::vector<TTypedValue_1_0>& results,
			const std::vector<TTypedValue_1_0>& maxValues );
void PrintValue( std::ostream& os, const TTypedValue_1_0& value );
void PrintMetricUnits(IMetricSet_1_1* m_MetricSet , std::ostream& os);
TTypedValue_1_0* GetGlobalSymbolValue( const char* SymbolName ,IMetricsDevice_1_5* m_MetricsDevice);
}
