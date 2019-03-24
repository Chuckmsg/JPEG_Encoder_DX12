#pragma once
#include <d3d11.h>
#include <d3d12.h>
#include <vector>
#include <sstream>

class D3DProfiler
{
public:
	enum PROFILER
	{
		DX11 = 0,
		DX12 = 1
	};

	D3DProfiler(ID3D11Device * pDev, ID3D11DeviceContext * pDevContext);
	D3DProfiler(ID3D12CommandList * pCommandList, ID3D12CommandQueue * pCommandQueue);
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
private:
	struct DX11TimestampPair
	{
		ID3D11Query * begin = nullptr;
		ID3D11Query * end = nullptr;
		std::string regionName = " ";
		double duration = -1.0;
	};
	//DX11
	ID3D11Device * m_device = nullptr;
	ID3D11DeviceContext * m_deviceContext = nullptr;
	ID3D11Query * m_gpuFrequencyQuery = nullptr;
	std::vector<DX11TimestampPair> m_timestampPairs;
private:
	//DX12

private:
	//Shared
	PROFILER m_profiler;
};

