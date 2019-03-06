//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "JpegEncoderBase.h"

JpegEncoderBase::JpegEncoderBase()
{
	mQualitySetting = 0;

	MemoryFile = NULL;
	MemoryFileCapacity = 0;
	MemoryFileNumBytesWritten = 0;

	ComputeDCTMatrices(DCT_matrix, DCT_matrix_transpose);

   // Compute the Huffman tables used for encoding
	memset(Y_DC_Huffman_Table, 0, sizeof(Y_DC_Huffman_Table));
    ComputeHuffmanTable(Y_DC_Huffman_Table, StandardDCLuminanceValues, StandardDCLuminanceNRCodes);
	
	memset(Y_AC_Huffman_Table, 0, sizeof(Y_AC_Huffman_Table));
    ComputeHuffmanTable(Y_AC_Huffman_Table, StandardACLuminanceValues, StandardACLuminanceNRCodes);
	
	memset(Cb_DC_Huffman_Table, 0, sizeof(Cb_DC_Huffman_Table));
    ComputeHuffmanTable(Cb_DC_Huffman_Table, StandardDCChromianceValues, StandardDCChromianceNRCodes);
	
	memset(Cb_AC_Huffman_Table, 0, sizeof(Cb_AC_Huffman_Table));
    ComputeHuffmanTable(Cb_AC_Huffman_Table, StandardACChromianceValues, StandardACChromianceNRCodes);

	int tmp;
	for(int i = 0; i < 32767; i++)
	{
		NumBitsInUShort[i] = i == 0 ? 0 : 1;
		tmp = i;
		while(tmp>>=1)
			NumBitsInUShort[i]++;
	}
}

JpegEncoderBase::~JpegEncoderBase()
{
	if(MemoryFile && MemoryFileCapacity > 0)
		delete [] MemoryFile;
}

void JpegEncoderBase::Reset()
{
	mByteBuffer = 0;
	mCurrentBytePos = 0;
}

bool JpegEncoderBase::ValidateMemoryFile(unsigned char* targetMemory)
{
	//user external memory?
	if(!targetMemory)
	{
		//need to increase memory file size?
		if(MemoryFileCapacity < mComputationWidthY * mComputationHeightY * 4)
		{
			if(MemoryFile)
				delete [] MemoryFile;

			MemoryFileCapacity = mComputationWidthY * mComputationHeightY * 4;
			MemoryFile = new BYTE[MemoryFileCapacity];

			if(!MemoryFile)
				return false;
		}
	}
	else
	{
		MemoryFile = targetMemory;
	}

	MemoryFileWalker = MemoryFile;
	MemoryFileNumBytesWritten = 0;
	return true;
}

bool JpegEncoderBase::ValidateQuantizationTables(int quality)
{
	//do we have to recalculate quantization tables?
	if(mQualitySetting != quality)
	{
		if(quality < 1 || quality > 100)
			return false;

		ComputeQuantizationTable(Y_Quantization_Table,
			StandardLuminanceQuantizationTable, quality);

		ComputeQuantizationTable(CbCr_Quantization_Table,
			StandardChromianceQuantizationTable, quality);

		mQualitySetting = quality;

		//notify other parts of the change
		QuantizationTablesChanged();
	}

	return true;
}

void JpegEncoderBase::CalculateComputationDimensions(int imageWidth, int imageHeight)
{
	if(mImageWidth != imageWidth || mImageHeight != imageHeight)
	{
		mImageWidth = imageWidth;
		mImageHeight = imageHeight;

		int remainder = 0;
		if(mSubsampleType == JENC_CHROMA_SUBSAMPLE_4_4_4)
		{
			remainder = mImageWidth % 8;
			mComputationWidthY = mImageWidth + (remainder > 0 ? 8 - remainder : 0);
			remainder = mImageHeight % 8;
			mComputationHeightY = mImageHeight + (remainder > 0 ? 8 - remainder : 0);
			

			remainder = mImageWidth % 8;
			mComputationWidthCbCr = mImageWidth + (remainder > 0 ? 8 - remainder : 0);
			mNumComputationBlocks_CbCr[0] = mComputationWidthCbCr / 8;
			remainder = mImageHeight % 8;
			mComputationHeightCbCr = mImageHeight + (remainder > 0 ? 8 - remainder : 0);
			mNumComputationBlocks_CbCr[1] = mComputationHeightCbCr / 8;
		}
		else if(mSubsampleType == JENC_CHROMA_SUBSAMPLE_4_2_2)
		{
			remainder = mImageWidth % 16;
			mComputationWidthY = mImageWidth + (remainder > 0 ? 16 - remainder : 0);
			remainder = mImageHeight % 8;
			mComputationHeightY = mImageHeight + (remainder > 0 ? 8 - remainder : 0);

			remainder = mImageWidth % 16;
			mComputationWidthCbCr = mImageWidth + (remainder > 0 ? 16 - remainder : 0);
			mNumComputationBlocks_CbCr[0] = mComputationWidthCbCr / 16;
			remainder = mImageHeight % 8;
			mComputationHeightCbCr = mImageHeight + (remainder > 0 ? 8 - remainder : 0);
			mNumComputationBlocks_CbCr[1] = mComputationHeightCbCr / 8;
		}
		else if(mSubsampleType == JENC_CHROMA_SUBSAMPLE_4_2_0)
		{
			remainder = mImageWidth % 16;
			mComputationWidthY = mImageWidth + (remainder > 0 ? 16 - remainder : 0);
			remainder = mImageHeight % 16;
			mComputationHeightY = mImageHeight + (remainder > 0 ? 16 - remainder : 0);

			remainder = mImageWidth % 16;
			mComputationWidthCbCr = mImageWidth + (remainder > 0 ? 16 - remainder : 0);
			mNumComputationBlocks_CbCr[0] = mComputationWidthCbCr / 16;
			remainder = mImageHeight % 16;
			mComputationHeightCbCr = mImageHeight + (remainder > 0 ? 16 - remainder : 0);
			mNumComputationBlocks_CbCr[1] = mComputationHeightCbCr / 16;
		}

		//always full resolution Y component
		mNumComputationBlocks_Y[0] = mComputationWidthY / 8;
		mNumComputationBlocks_Y[1] = mComputationHeightY / 8;

		//notify subclasses about change
		ComputationDimensionsChanged();
	}
}

JEncResult JpegEncoderBase::Encode(JEncRGBDataDesc rgbDataDesc, int quality)
{
	JEncResult result;
	memset(&result, 0, sizeof(result));
	
	//mSubsampleType = rgbDataDesc.SubsampleType;

	if(!ValidateQuantizationTables(quality))
		return result;

	CalculateComputationDimensions(rgbDataDesc.Width, rgbDataDesc.Height);

	if(!ValidateMemoryFile(rgbDataDesc.TargetMemory))
		return result;

	Reset();

	WriteHeader();
	result.HeaderSize = unsigned int(MemoryFileWalker - MemoryFile);

	WriteImageData(rgbDataDesc);
	result.DataSize = unsigned int(MemoryFileWalker - MemoryFile) - result.HeaderSize;
	result.Bits = (void*)MemoryFile;
	
	return result;
}

JEncResult JpegEncoderBase::Encode(JEncD3DDataDesc d3dDataDesc, int quality)
{
	JEncResult result;
	memset(&result, 0, sizeof(result));

	//mSubsampleType = d3dDataDesc.SubsampleType;

	if(!ValidateQuantizationTables(quality))
		return result;

	CalculateComputationDimensions(d3dDataDesc.Width, d3dDataDesc.Height);

	if(!ValidateMemoryFile(d3dDataDesc.TargetMemory))
		return result;

	Reset();

	WriteHeader();
	result.HeaderSize = unsigned int(MemoryFileWalker - MemoryFile);

	WriteImageData(d3dDataDesc);

	result.DataSize = unsigned int(MemoryFileWalker - MemoryFile) - result.HeaderSize;
	result.Bits = (void*)MemoryFile;
	
	return result;
}

void JpegEncoderBase::WriteHeader()
{
	//JPG header
	WriteAPP0Info();

	WriteQuantizationInfo();
	WriteHuffmanInfo();
	WriteS0FInfo();
	WriteS0SInfo();
}

inline void JpegEncoderBase::Write(BYTE b)
{
	//MemoryFile[MemoryFileNumBytesWritten++] = b;
	*MemoryFileWalker++ = b;
}

inline void JpegEncoderBase::Write(char c)
{
	//MemoryFile[MemoryFileNumBytesWritten++] = (BYTE)c;
	*MemoryFileWalker++ = (BYTE)c;
}

inline void JpegEncoderBase::WriteHex(unsigned short data)
{
	Write((BYTE)((data & 0xff00) >> 8));
	Write((BYTE)(data & 0x00ff));
}

inline void JpegEncoderBase::WriteByteArray(BYTE* arr, size_t size)
{
	memcpy(MemoryFileWalker, arr, size);
	MemoryFileWalker += size;
	//MemoryFileNumBytesWritten += size;
}

void JpegEncoderBase::ComputeHuffmanTable(BitString* outTable, BYTE* inTable, BYTE* nrCodes)
{
    BYTE k, j;
    BYTE pos_in_table;
    USHORT code_value;

    code_value = 0;
    pos_in_table = 0;
    for (k = 1; k <= 16; k++)
    {
        for (j = 1; j <= nrCodes[k]; j++)
        {
			outTable[inTable[pos_in_table]].value = code_value;
            outTable[inTable[pos_in_table]].length = k;

			pos_in_table++;
            code_value++;
        }
        code_value <<= 1;
    }            
}

void JpegEncoderBase::WriteBits(const BitString& bs)
{
	static BYTE b = 0;
	mByteBuffer |= bs.value << (32 - bs.length - mCurrentBytePos);
	mCurrentBytePos += bs.length;

	while(mCurrentBytePos >= 8)
	{
		b = mByteBuffer >> 24;

		Write(b);


        // Write to stream
        if (b == 0xFF)
			Write((BYTE)0x00);

		mByteBuffer <<= 8;
		mCurrentBytePos -= 8;
	}
}

//JPG header
void JpegEncoderBase::WriteAPP0Info()
{
	USHORT marker = 0xFFE0;
	USHORT length = 16; // = 16 for usual JPEG, no thumbnail		
	BYTE versionhi = 1; // 1
	BYTE versionlo = 1; // 1
	BYTE xyunits = 0;   // 0 = no units, normal density
	USHORT xdensity = 1;  // 1
	USHORT ydensity = 1;  // 1
	BYTE thumbnwidth = 0; // 0
	BYTE thumbnheight = 0; // 0

	WriteHex(0xFFD8); // JPEG INIT
	WriteHex(marker);
	WriteHex(length);

	Write('J');
    Write('F');
    Write('I');
    Write('F');
    Write((BYTE)0x0);
    Write(versionhi);
    Write(versionlo);
    Write(xyunits);
    WriteHex(xdensity);
    WriteHex(ydensity);
    Write(thumbnheight);
    Write(thumbnwidth);
}

void JpegEncoderBase::WriteQuantizationInfo()
{
	USHORT marker = 0xFFDB;
	USHORT length = 132;  // = 132
	BYTE QTYinfo = 0;// = 0:  bit 0..3: number of QT = 0 (table for Y)
	//       bit 4..7: precision of QT, 0 = 8 bit		 
	BYTE QTCbinfo = 1; // = 1 (quantization table for Cb,Cr}     
 
	WriteHex(marker);
    WriteHex(length);                
    Write(QTYinfo);
	WriteByteArray(Y_Quantization_Table, sizeof(Y_Quantization_Table));
    Write(QTCbinfo);
    WriteByteArray(CbCr_Quantization_Table, sizeof(CbCr_Quantization_Table));
}

void JpegEncoderBase::WriteS0FInfo()
{
	USHORT marker = 0xFFC0; /* SOF Huff  - Baseline DCT*/
	USHORT length = 17; // = 17 for a truecolor YCbCr JPG
	BYTE precision = 8;// Should be 8: 8 bits/sample            
	BYTE nrofcomponents = 3;//Should be 3: We encode a truecolor JPG
	BYTE IdY = 1;  // = 1

	BYTE HVY = 0; // vertical sampling in high nibble, horizontal in low nibble
	if(mSubsampleType == JENC_CHROMA_SUBSAMPLE_4_4_4)
		HVY = 0x11;
	else if(mSubsampleType == JENC_CHROMA_SUBSAMPLE_4_2_2)
		HVY = (0x11 << 1) - 1;
	else if(mSubsampleType == JENC_CHROMA_SUBSAMPLE_4_2_0)
		HVY = (0x11 << 1);
	
	BYTE QTY = 0;  // Quantization Table number for Y = 0
	BYTE IdCb = 2; // = 2
	BYTE HVCb = 0x11;
	BYTE QTCb = 1; // 1
	BYTE IdCr = 3; // = 3
	BYTE HVCr = 0x11;
	BYTE QTCr = 1; // Normally equal to QTCb = 1

    WriteHex(marker);
    WriteHex(length);
    Write(precision);
	WriteHex(mImageHeight);
    WriteHex(mImageWidth);
    Write(nrofcomponents);
    Write(IdY);
    Write(HVY);
    Write(QTY);
    Write(IdCb);
    Write(HVCb);
    Write(QTCb);
    Write(IdCr);
    Write(HVCr);
    Write(QTCr);
}

void JpegEncoderBase::WriteHuffmanInfo()
{
	USHORT marker = 0xFFC4;
	USHORT length = 0x01A2;
	BYTE HTYDCinfo = 0x00; // bit 0..3: number of HT (0..3), for Y =0
	//bit 4  :type of HT, 0 = DC table,1 = AC table
	//bit 5..7: not used, must be 0
	BYTE HTYACinfo = 0x10; // = 0x10
	BYTE HTCbDCinfo = 0x01; // = 1
	BYTE HTCbACinfo = 0x11; //  = 0x11

	WriteHex(marker);
	WriteHex(length);              
	Write(HTYDCinfo);
	WriteByteArray(&StandardDCLuminanceNRCodes[1], sizeof(StandardDCLuminanceNRCodes) - 1);
	WriteByteArray(&StandardDCLuminanceValues[0], sizeof(StandardDCLuminanceValues));
	Write(HTYACinfo);
	WriteByteArray(&StandardACLuminanceNRCodes[1], sizeof(StandardACLuminanceNRCodes) - 1);
	WriteByteArray(&StandardACLuminanceValues[0], sizeof(StandardACLuminanceValues));
	Write(HTCbDCinfo);
	WriteByteArray(&StandardDCChromianceNRCodes[1], sizeof(StandardDCChromianceNRCodes) - 1);
	WriteByteArray(&StandardDCChromianceValues[0], sizeof(StandardDCChromianceValues));
	Write(HTCbACinfo);
	WriteByteArray(&StandardACChromianceNRCodes[1], sizeof(StandardACChromianceNRCodes) - 1);
	WriteByteArray(&StandardACChromianceValues[0], sizeof(StandardACChromianceValues));
}

void JpegEncoderBase::WriteS0SInfo()
{
	USHORT marker = 0xFFDA;
	USHORT length = 12;
	BYTE nrofcomponents = 3; // Should be 3: truecolor JPG
	BYTE IdY = 1; 
	BYTE HTY = 0; // bits 0..3: AC table (0..3)
						// bits 4..7: DC table (0..3)
	BYTE IdCb = 2; 
	BYTE HTCb = 0x11; 
	BYTE IdCr = 3; 
	BYTE HTCr = 0x11; 
			
	BYTE Ss = 0;
	BYTE Se = 0x3F;
	BYTE Bf = 0; // not interesting, they should be 0,63,0

    WriteHex(marker);
    WriteHex(length);
    Write(nrofcomponents);
    Write(IdY);
    Write(HTY);
    Write(IdCb);
    Write(HTCb);
    Write(IdCr);
    Write(HTCr);
    Write(Ss);
    Write(Se);
    Write(Bf);
}