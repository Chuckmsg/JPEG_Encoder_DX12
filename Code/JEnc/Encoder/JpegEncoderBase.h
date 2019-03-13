//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <Windows.h>
#include <fstream>
#include <iostream>
#include <iomanip>

#include "../Include/JEnc.h"
#include "../../Shared/JpegCommon.h"

struct BitString
{
	USHORT value;
	USHORT length;
};

class JpegEncoderBase : public JEnc
{
public:
	JpegEncoderBase();
	virtual ~JpegEncoderBase();

	JEncResult Encode(JEncRGBDataDesc rgbDataDesc, int quality);
	JEncResult Encode(JEncD3DDataDesc d3dDataDesc, int quality);
	JEncResult Encode(DX12_JEncD3DDataDesc d3dDataDesc, int quality);

	virtual bool Init() { return true; }

protected:

	virtual void WriteImageData(JEncRGBDataDesc rgbDataDesc) = 0;
	virtual void WriteImageData(JEncD3DDataDesc d3dDataDesc) = 0;
	virtual void WriteImageData(DX12_JEncD3DDataDesc d3dDataDesc) = 0;
	virtual void Reset();

	int				MemoryFileCapacity;
	int				MemoryFileNumBytesWritten;
	BYTE*			MemoryFile;
	BYTE*			MemoryFileWalker;

	BYTE			NumBitsInUShort[32767];

	BitString Y_DC_Huffman_Table[12];
	BitString Cb_DC_Huffman_Table[12];
	BitString Y_AC_Huffman_Table[256];
	BitString Cb_AC_Huffman_Table[256];

	float DCT_matrix[64];
	float DCT_matrix_transpose[64];

	// Quantization data tables
	int	mQualitySetting;
	BYTE Y_Quantization_Table[64];
	BYTE CbCr_Quantization_Table[64];

	//fstream helpers
	inline void Write(BYTE b);
	inline void Write(char c);
	inline void WriteHex(unsigned short data);
	inline void WriteByteArray(BYTE* arr, size_t size);
	void WriteBits(const BitString& bs);


	UINT mByteBuffer;
	signed char mCurrentBytePos;


	int mImageWidth;
	int mImageHeight;
	int mComputationWidthY;
	int mComputationHeightY;
	int mComputationWidthCbCr;
	int mComputationHeightCbCr;
	int mNumComputationBlocks_Y[2];
	int mNumComputationBlocks_CbCr[2];

	JENC_CHROMA_SUBSAMPLE mSubsampleType;

private:

	void CalculateComputationDimensions(int imageWidth, int imageHeight);
	virtual void ComputationDimensionsChanged() {};

	bool ValidateQuantizationTables(int quality);
	virtual void QuantizationTablesChanged() {};

	bool ValidateMemoryFile(unsigned char* targetMemory);
	void WriteHeader();

	void ComputeHuffmanTable(BitString* outTable, BYTE* inTable, BYTE* nrCodes);

	//JPG header
	void WriteAPP0Info();
	void WriteQuantizationInfo();
	void WriteS0FInfo();
	void WriteHuffmanInfo();
	void WriteS0SInfo();
};