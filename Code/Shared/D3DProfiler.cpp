#include "D3DProfiler.h"


D3DProfiler::D3DProfiler(ID3D11Device * pDev, ID3D11DeviceContext * pDevContext)
{
	m_deviceDX11 = pDev;
	m_deviceContext = pDevContext;
}

D3DProfiler::D3DProfiler(ID3D12Device * pDev,  ID3D12GraphicsCommandList * pCommandList, ID3D12CommandQueue * pCommandQueue)
{
	m_deviceDX12 = pDev;
	m_commandList = pCommandList;
	m_commandQueue = pCommandQueue;
}

D3DProfiler::~D3DProfiler()
{
	CleanUp();
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

			if (FAILED(m_deviceDX11->CreateQuery(&desc, &m_gpuFrequencyQuery)))
				OutputDebugStringA("\nDX11 Profiler failed to create GPU Frequency Query\n");
		}
		break;
	case DX12:
		if (m_queryHeap == nullptr)
		{
			D3D12_QUERY_HEAP_DESC desc{};
			desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			desc.Count = MAX_QUERIES;
			desc.NodeMask = 0;

			if (FAILED(m_deviceDX12->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_queryHeap))))
				OutputDebugStringA("\nDX12 Profiler failed to create Query Heap\n");

			D3D12_RESOURCE_DESC resDesc{};
			resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			resDesc.Width = sizeof(UINT64) * MAX_QUERIES;
			resDesc.Height = 1;
			resDesc.DepthOrArraySize = 1;
			resDesc.MipLevels = 1;
			resDesc.Format = DXGI_FORMAT_UNKNOWN;
			resDesc.SampleDesc.Count = 1;
			resDesc.SampleDesc.Quality = 0;
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			D3D12_HEAP_PROPERTIES heapProp{};
			heapProp.Type = D3D12_HEAP_TYPE_READBACK;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;

			if (FAILED(m_deviceDX12->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&resDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				NULL,
				IID_PPV_ARGS(&m_buffer))
			))
			{
				OutputDebugStringA("\nFailed to create buffer\n");
			}

		}
		break;
	default:
		return;
	}
}

void D3DProfiler::CleanUp()
{
	if (m_profiler == DX11)
	{
		for (auto & pair : m_TimestampPairs)
		{
			pair.begin->Release();
			pair.end->Release();
		}
		if (m_gpuFrequencyQuery)
			m_gpuFrequencyQuery->Release();
		m_gpuFrequencyQuery = nullptr;
	}
	m_TimestampPairs.clear();

	if (m_profiler == DX12)
	{
		if (m_queryHeap)
			m_queryHeap->Release();
		m_queryHeap = nullptr;
		if (m_buffer)
			m_buffer->Release();
		m_buffer = nullptr;
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
		m_commandQueue->GetTimestampFrequency(&m_gpuFrequency);
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
		m_commandList->ResolveQueryData(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, MAX_QUERIES, m_buffer, 0);
		m_commandQueue->GetTimestampFrequency(&m_gpuFrequency);
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
			TimestampPair tsPair{};
			D3D11_QUERY_DESC desc{};
			desc.Query = D3D11_QUERY::D3D11_QUERY_TIMESTAMP;

			m_deviceDX11->CreateQuery(&desc, &tsPair.begin);
			m_deviceDX11->CreateQuery(&desc, &tsPair.end);

			tsPair.regionName = str;

			m_TimestampPairs.push_back(tsPair);

			return (unsigned int)(m_TimestampPairs.size() - 1);
		}	
	case DX12:
		{
			if (m_TimestampPairs.size() >= 64) //Max capacity is 64 Timestamp pairs because the heap is set to contain max 128 Queries
				return -1;

			TimestampPair tsPair{};
			tsPair.regionName = str;
			m_TimestampPairs.push_back(tsPair);

			return (unsigned int)(m_TimestampPairs.size() - 1);
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
		m_deviceContext->End(m_TimestampPairs[index].begin);
		break;
	case DX12:
		m_commandList->EndQuery(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, (2 * index));
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
		m_deviceContext->End(m_TimestampPairs[index].end);
		break;
	case DX12:
		m_commandList->EndQuery(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, (2 * index) + 1);
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
		for (auto & pair : m_TimestampPairs)
		{
			m_deviceContext->GetData(pair.begin, (void*)&begin, sizeof(UINT64), 0);
			m_deviceContext->GetData(pair.end, (void*)&end, sizeof(UINT64), 0);
			pair.duration = (double)(((float)(end - begin)) / float(tsDisjoint.Frequency) * 1000.f); //convert from Seconds to Milliseconds
			m_recordings.push_back({ pair.regionName, pair.duration });
		}
	}
	if (m_profiler == DX12)
	{
		D3D12_RANGE range{};
		range.Begin = 0;
		range.End = sizeof(UINT64) * MAX_QUERIES;

		m_buffer->Map(0, &range, &m_queryData);
		
		UINT64 * data = (UINT64*)m_queryData;
		UINT64 begin, end;
		for (size_t i = 0; i < m_TimestampPairs.size(); i++)
		{
			begin = data[i * 2];
			end = data[(i * 2) + 1];
			m_TimestampPairs[i].duration = (double)(((float)(end - begin)) / float(m_gpuFrequency) * 1000.f);
			if (m_recordings.size() < m_frameLimit)
				m_recordings.push_back({ m_TimestampPairs[i].regionName, m_TimestampPairs[i].duration });
		}

		m_buffer->Unmap(0, NULL);
	}
}

void D3DProfiler::PrintAllToDebugOutput()
{
	std::stringstream ss;
	ss << "\n------------------ D3D Profiler | " << m_name << " ------------------\n";

	for (auto &pair : m_TimestampPairs)
	{
		ss << "\n" << pair.regionName << ": " << pair.duration << " ms";
	}

	ss << "\n";
	OutputDebugStringA(ss.str().c_str());
}

void D3DProfiler::Update()
{
	static clock_t t = clock();
	clock_t dur = clock() - t;

	if ( (double)(dur / (double(CLOCKS_PER_SEC))) >= TIME_LIMIT)
	{
		FlushRecordingsToFile();
		t = clock();
	}

}

void D3DProfiler::FlushRecordingsToFile()
{
	std::ofstream ofs;
	std::string filename = m_name + ".txt";
	ofs.open(filename.c_str(), std::ofstream::out);

	if (ofs.is_open())
	{
		for (auto &pair : m_recordings)
		{
			ofs << pair.first << " " << pair.second << std::endl;
		}
	}

	ofs.close();
	m_recordings.clear();
}


