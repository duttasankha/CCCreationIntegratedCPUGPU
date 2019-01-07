#include "CC_ALL_HEADER.h"

bool m_IncludeMaxValues = true;

CloseMetricsDevice_fn  CloseMetricsDevice;
OpenMetricsDevice_fn   OpenMetricsDevice;
//IMetricsDevice_1_5*    m_MetricsDevice;

static const char * cMDLIBFILENAME = "/home/duttasankha/Desktop/SANKHA_ALL/INTEL_METRICS_DISCOVERY/metrics-discovery/dump/linux64/release/md/libmd.so";

/*
void printMetricNameWrapper(IMetricSet_1_1* m_MetricSet){

	PrintMetricNames(m_MetricSet,cout);
}

void PrintMetricUnitsWrapper(IMetricSet_1_1* m_MetricSet){

	PrintMetricUnits(m_MetricSet,cout)
}*/

inline uint32_t GetMetricsConfiguration(IMetricSet_1_1* m_MetricSet)
{
    return ( m_MetricSet != NULL ) ? m_MetricSet->GetParams()->ApiSpecificId.OCL : 0;
}

/************************************************************************/
/* GetQueryReportSize                                                   */
/************************************************************************/
inline uint32_t GetQueryReportSize(IMetricSet_1_1* m_MetricSet)
{
    return ( m_MetricSet != NULL ) ? m_MetricSet->GetParams()->QueryReportSize : 0;
}

bool initMetrics(IMetricSet_1_1* m_MetricSet,IMetricsDevice_1_5* m_MetricsDevice){

	TCompletionCode res = CC_OK;

	void *mLibrary = OpenLibrary(cMDLIBFILENAME);   
	if(mLibrary == NULL){
		printf("Error in opening the library\n");
		return false;
	}

	CloseMetricsDevice = (CloseMetricsDevice_fn)GetFunctionAddress(mLibrary, "CloseMetricsDevice");
	if (CloseMetricsDevice == NULL)
	{
		printf("CloseMetricsDevice NULL, error: %s", GetLastError());
		return false;
	}

	OpenMetricsDevice = (OpenMetricsDevice_fn)GetFunctionAddress(mLibrary, "OpenMetricsDevice");
	if (OpenMetricsDevice == NULL)
	{
		printf("OpenMetricsDevice NULL, error: %s", GetLastError());
		return false;
	}

	res = OpenMetricsDevice(&m_MetricsDevice);
	if (res != CC_OK)
	{
		printf("OpenMetricsDevice failed, res: %d", res);
		return false;
	}

	TMetricsDeviceParams_1_0 * deviceParams = m_MetricsDevice->GetParams();
	if(deviceParams == NULL){
		printf("Device Params NULL\n");
		return false;
	}

	for(uint32_t cg = 0;cg < deviceParams->ConcurrentGroupsCount;cg++){

		IConcurrentGroup_1_1 *group = m_MetricsDevice->GetConcurrentGroup(cg);
		TConcurrentGroupParams_1_0* groupParams = group->GetParams(); 

		if(groupParams){

			for(uint32_t ms = 0;ms < groupParams->MetricSetsCount;ms++){
				IMetricSet_1_1* metricSet = group->GetMetricSet(ms);
				TMetricSetParams_1_0* setParams = metricSet->GetParams();

				if(setParams){
					printf("Group: %s || Metric Set: %s || Metric Count: %d || API: %x || Category; %x\n",groupParams->SymbolName,setParams->SymbolName,setParams->MetricsCount,setParams->ApiMask,setParams->CategoryMask);
				}
			    
				if(!strcmp(setParams->SymbolName,"GPUTimestamp")){
			   		std::cout<<"Setting up the time stamp"<<std::endl;
			   		m_MetricSet = metricSet;
				}

			
		        }

		}


	}

	return true;
}

bool ActivateMetricSet(IMetricSet_1_1* m_MetricSet){

	TCompletionCode res = m_MetricSet->Activate();
	if( res != CC_OK ){ 
		printf("ActivateMetricSet failed!\n"); 
		return false;
	}

	return res == CC_OK;
}

void DeactivateMetricSet(IMetricSet_1_1* m_MetricSet)
{
	//	if( !m_Initialized || !m_MetricSet )
	//	{
	//		DebugPrint("Can't DeactivateMetricSet!\n");
	//		return;
	//	}

	TCompletionCode res = m_MetricSet->Deactivate();
	if( res != CC_OK ) { 
		printf("DeactivateMetricSet failed!\n"); 

	}
}


void GetMetricsFromReport(
		IMetricSet_1_1* m_MetricSet,
		const char* pReportData,
		std::vector<TTypedValue_1_0>& results,
		std::vector<TTypedValue_1_0>& maxValues )
{
	if(!m_MetricSet )
	{
		printf("Can't GetMetricsFromReport!\n");
		return;
	}

	const uint32_t reportSize       = m_MetricSet->GetParams()->QueryReportSize;

	const uint32_t metricsCount     = m_MetricSet->GetParams()->MetricsCount;
	const uint32_t informationCount = m_MetricSet->GetParams()->InformationCount;

	results.resize( metricsCount + informationCount );

	TCompletionCode res = MetricsDiscovery::CC_ERROR_GENERAL;
	//	if( m_IncludeMaxValues )
	//	{
	uint32_t    outReportCount = 0;
	maxValues.resize( metricsCount );
	res = ((MetricsDiscovery::IMetricSet_1_5*)m_MetricSet)->CalculateMetrics(
			(const unsigned char*)pReportData,
			reportSize,
			results.data(),
			(uint32_t)(results.size() * sizeof(TTypedValue_1_0)),
			&outReportCount,
			maxValues.data(),
			(uint32_t)(maxValues.size() * sizeof(TTypedValue_1_0)) );
	//	}
	//	else
	//	{
	//		uint32_t    outReportCount = 0;
	//		res = m_MetricSet->CalculateMetrics(
	//				(const unsigned char*)pReportData,
	//				reportSize,
	//				results.data(),
	//				(uint32_t)(results.size() * sizeof(TTypedValue_1_0)),
	//				&outReportCount,
	//				false );
	//	}
	//
	if( res != CC_OK )
	{
		printf("CalculateMetrics failed!\n");
		return;
	}
}


void PrintMetricNames(IMetricSet_1_1* m_MetricSet , std::ostream& os )
{
	if( !m_MetricSet || !os.good() )
	{
		printf("Can't PrintMetricNames!\n");
		return;
	}

	os << "kernel,";

	for( uint32_t i = 0; i < m_MetricSet->GetParams( )->MetricsCount; i++ )
	{	
		os << m_MetricSet->GetMetric( i )->GetParams()->SymbolName << ",";
		if( m_IncludeMaxValues )
		{
			os << "max_" << m_MetricSet->GetMetric( i )->GetParams()->SymbolName << ",";
		}
		

	}

	os << ",";

	for(uint32_t i = 0; i < m_MetricSet->GetParams()->InformationCount; i++)
	{
		os << m_MetricSet->GetInformation( i )->GetParams()->SymbolName << ",";
	}

	os << std::endl;
}

void PrintMetricUnits(IMetricSet_1_1* m_MetricSet , std::ostream& os)
{
	if (!m_MetricSet || !os.good()) return;

	os << " ,";

	for (uint32_t i = 0; i < m_MetricSet->GetParams()->MetricsCount; i++)
	{
		const char* unit = m_MetricSet->GetMetric(i)->GetParams()->MetricResultUnits;
		os << ( unit ? unit : " " ) << ( m_IncludeMaxValues ? ", ," : "," );
	}

	os << ",";

	for (uint32_t i = 0; i < m_MetricSet->GetParams()->InformationCount; i++)
	{
		const char* unit = m_MetricSet->GetInformation(i)->GetParams()->InfoUnits;
		os << ( unit ? unit : " " ) << ",";
	}

	os << std::endl;
}



void PrintMetricValues(
		IMetricSet_1_1* m_MetricSet,
		std::ostream& os,
		const std::string& name,
		const std::vector<TTypedValue_1_0>& results,
		const std::vector<TTypedValue_1_0>& maxValues )
{

	os << name << ",";

	uint32_t metricsCount = m_MetricSet->GetParams()->MetricsCount;
	for( uint32_t i = 0; i < metricsCount; i++ )
	{
		PrintValue( os, results[ i ] );
		if( m_IncludeMaxValues )
		{
			PrintValue( os, maxValues[ i ] );
		}
	}

	os << ",";

	for( uint32_t i = 0; i < m_MetricSet->GetParams()->InformationCount; i++ )
	{
		PrintValue( os, results[ metricsCount + i ] );
	}

	os << std::endl;
}


void PrintValue( std::ostream& os, const TTypedValue_1_0& value )
{
    switch( value.ValueType )
    {
    case VALUE_TYPE_UINT64:
        os << value.ValueUInt64 << ",";
        break;

    case VALUE_TYPE_FLOAT:
        os << value.ValueFloat << ",";
        break;

    case VALUE_TYPE_BOOL:
        os << (value.ValueBool ? "TRUE" : "FALSE") << ",";
        break;

    case VALUE_TYPE_UINT32:
        os << value.ValueUInt32 << ",";
        break;

    default:
       break;// CLI_ASSERT(false);
    }
}

TTypedValue_1_0* GetGlobalSymbolValue( const char* SymbolName ,IMetricsDevice_1_5* m_MetricsDevice)
{
    for( uint32_t i = 0; i < m_MetricsDevice->GetParams()->GlobalSymbolsCount; i++ )
    {
        TGlobalSymbol_1_0* symbol = m_MetricsDevice->GetGlobalSymbol( i );
        if( strcmp( symbol->SymbolName, SymbolName ) == 0 )
        {
            return &( symbol->SymbolTypedValue );
        }
    }
    return NULL;
}

