#include "D3DProfiler.h"


D3DProfiler::D3DProfiler(ID3D11Device * pDev, ID3D11DeviceContext * pDevContext)
{
	m_device = pDev;
	m_deviceContext = pDevContext;
}

D3DProfiler::D3DProfiler(ID3D12CommandList * pCommandList, ID3D12CommandQueue * pCommandQueue)
{

}

D3DProfiler::~D3DProfiler()
{
}

void D3DProfiler::Init(PROFILER profiler)
{
	m_profiler = profiler;
	switch (profiler)
	{
	case DX11:
		if (m_gpuFrequencyQuery == NULL)
		{
			D3D11_QUERY_DESC desc{};
			desc.Query = D3D11_QUERY::D3D11_QUERY_TIMESTAMP_DISJOINT;

			if (FAILED(m_device->CreateQuery(&desc, &m_gpuFrequencyQuery)))
				OutputDebugStringA("\nDX11 Profiler failed to create GPU Frequency Query\n");
		}
		break;
	case DX12:

		break;
	default:
		return;
	}
}

void D3DProfiler::CleanUp()
{
	if (m_profiler == DX11)
	{
		for (auto & pair : m_timestampPairs)
		{
			pair.begin->Release();
			pair.end->Release();
		}
		m_timestampPairs.clear();
		m_gpuFrequencyQuery->Release();
		m_gpuFrequencyQuery = nullptr;
	}
}

void D3DProfiler::StartProfiler()
{
	switch (m_profiler)
	{
	case DX11:
		m_deviceContext->Begin(m_gpuFrequencyQuery);
		break;
	case DX12:

		break;
	default:
		break;
	}
}

void D3DProfiler::EndProfiler()
{
	switch (m_profiler)
	{
	case DX11:
		m_deviceContext->End(m_gpuFrequencyQuery);
		break;
	case DX12:

		break;
	default:
		break;
	}
}

unsigned int D3DProfiler::CreateTimestamp(std::string str)
{
	switch (m_profiler)
	{
	case DX11:
		{
			DX11TimestampPair tsPair{};
			D3D11_QUERY_DESC desc{};
			desc.Query = D3D11_QUERY::D3D11_QUERY_TIMESTAMP;

			m_device->CreateQuery(&desc, &tsPair.begin);
			m_device->CreateQuery(&desc, &tsPair.end);

			tsPair.regionName = str;

			m_timestampPairs.push_back(tsPair);

			return (unsigned int)(m_timestampPairs.size() - 1);
		}	
	case DX12:
		{
			return -1;
		}
	default:
		return -1;
	}
	
}

void D3DProfiler::BeginTimestamp(unsigned int index)
{
	switch (m_profiler)
	{
	case DX11:
		m_deviceContext->End(m_timestampPairs[index].begin);
		break;
	default:
		break;
	}
}

void D3DProfiler::EndTimestamp(unsigned int index)
{
	switch (m_profiler)
	{
	case DX11:
		m_deviceContext->End(m_timestampPairs[index].end);
		break;
	default:
		break;
	}
}

void D3DProfiler::CalculateAllDurations()
{
	if (m_profiler == DX11)
	{
		while (m_deviceContext->GetData(m_gpuFrequencyQuery, NULL, 0, 0) == S_FALSE)
		{
			//basically wait for GPU 
		}
		//check if timestamps where disjointed during the last frame
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
		m_deviceContext->GetData(m_gpuFrequencyQuery, (void*)&tsDisjoint, sizeof(tsDisjoint), 0);
		if (tsDisjoint.Disjoint)
			return;

		UINT64 begin, end;
		for (auto & pair : m_timestampPairs)
		{
			m_deviceContext->GetData(pair.begin, (void*)&begin, sizeof(UINT64), 0);
			m_deviceContext->GetData(pair.end, (void*)&end, sizeof(UINT64), 0);
			pair.duration = (double)(((float)(end - begin)) / float(tsDisjoint.Frequency) * 1000.f); //convert from Seconds to Milliseconds
		}
	}
	if (m_profiler == DX12)
		return;
}

void D3DProfiler::PrintAllToDebugOutput()
{
	std::stringstream ss;
	ss << "\n------------------ D3D Profiler ------------------\n";

	switch (m_profiler)
	{
	case DX11:
		for (auto &pair : m_timestampPairs)
		{
			ss << "\n" << pair.regionName << ": " << pair.duration << " ms";
		}
		break;
	default:
		break;
	}
	ss << "\n";
	OutputDebugStringA(ss.str().c_str());
}


