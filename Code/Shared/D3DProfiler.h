#pragma once
#include <d3d11.h>
#include <d3d12.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <time.h>

class D3DProfiler
{
public:
	static const UINT MAX_QUERIES = 128;
	const double TIME_LIMIT = 2.0;
	enum PROFILER
	{
		DX11 = 0,
		DX12 = 1
	};

	D3DProfiler(ID3D11Device * pDev, ID3D11DeviceContext * pDevContext);
	D3DProfiler(ID3D12Device * pDev, ID3D12GraphicsCommandList * pCommandList, ID3D12CommandQueue * pCommandQueue);
	~D3DProfiler();
	void Init(PROFILER profiler);
	void CleanUp();

	void StartProfiler();
	void EndProfiler();

	unsigned int CreateTimestamp(std::string str = "UNKNOWN REGION"); //returns the index to the created timestamp

	void BeginTimestamp(unsigned int index);
	void EndTimestamp(unsigned int index);

	void CalculateAllDurations();
	void PrintAllToDebugOutput();

	void SetName(std::string name) { m_name = name; }
	void SetFlushInterval(unsigned int frameLimit) { m_frameLimit = frameLimit; }

	void Update();
private:

	//DX11
	ID3D11Device * m_deviceDX11 = nullptr;
	ID3D11DeviceContext * m_deviceContext = nullptr;
	ID3D11Query * m_gpuFrequencyQuery = nullptr;
private:
	//DX12
	ID3D12QueryHeap * m_queryHeap = nullptr;
	ID3D12Resource * m_buffer = nullptr;
	void * m_queryData = nullptr;
	UINT64 m_gpuFrequency = 0;
	ID3D12Device * m_deviceDX12 = nullptr;
	ID3D12GraphicsCommandList * m_commandList = nullptr;
	ID3D12CommandQueue * m_commandQueue = nullptr;
private:
	//Shared
	PROFILER m_profiler;
	struct TimestampPair
	{
		ID3D11Query * begin = nullptr;
		ID3D11Query * end = nullptr;
		std::string regionName = " ";
		double duration = -1.0;
	};
	std::vector<TimestampPair> m_TimestampPairs;
	std::string m_name = "UNNAMED PROFILER";
	unsigned int m_frameLimit = 25;
	std::vector<std::pair<std::string, double>> m_recordings;

	void FlushRecordingsToFile();
};

