//--------------------------------------------------------------------------------------
// File: MJPEG.h
//
// Code to create MJPEG movies by appending a sequence of JPEG image frames.
//
// Copyright (c) 2012 Stefan Petersson. All rights reserved.
//--------------------------------------------------------------------------------------
#include "../stdafx.h"
#include <vfw.h>

#pragma comment(lib, "Vfw32.lib")

#define MJPG mmioFOURCC('M','J','P','G')

class MotionJpeg
{
	PAVISTREAM		mAviVideo; // Pointer to the AVI stream
	AVISTREAMINFO	mAviStreamInfo; // AVI stream informations
	OFSTRUCT		mAviOutputFileStruct; // File struct required to delete AVI file
	PAVIFILE		mAviOutputFile; // Pointer to the AVI file

	int				mCurrentFrame;
	bool			mRecording;
public:
	MotionJpeg() : mAviVideo(NULL), mCurrentFrame(0), mRecording(false)
	{

	}

	bool IsRecording()
	{
		return mRecording;
	}

	HRESULT StartRecording(char* filename, int framwWidth, int frameHeight, int frameRate)
	{
		HRESULT hr = E_FAIL;

		if(!mRecording)
		{
			mCurrentFrame = 0;

			// init AVIFile functions
			AVIFileInit();

			// delete the previously captured file
			OpenFile(filename, &mAviOutputFileStruct, OF_DELETE);

			// create and open the the capture file
			AVIFileOpenA(&mAviOutputFile, filename, OF_CREATE | OF_WRITE | OF_SHARE_DENY_NONE, NULL);

			// set stream info
			memset(&mAviStreamInfo, 0, sizeof(mAviStreamInfo));  
			mAviStreamInfo.fccType = streamtypeVIDEO;
			mAviStreamInfo.fccHandler = MJPG;
			mAviStreamInfo.dwFlags = 0;
			mAviStreamInfo.dwCaps = 0;
			mAviStreamInfo.wPriority = 0;
			mAviStreamInfo.wLanguage = 0;

			mAviStreamInfo.dwScale = 1; 
			mAviStreamInfo.dwRate = frameRate;

			mAviStreamInfo.dwStart = 0;
			mAviStreamInfo.dwLength = 0;
			mAviStreamInfo.dwInitialFrames = 0;
			mAviStreamInfo.dwSuggestedBufferSize = 0;
			mAviStreamInfo.dwQuality = (DWORD)-1;
			mAviStreamInfo.dwSampleSize = 0;
			mAviStreamInfo.rcFrame.left = 0;
			mAviStreamInfo.rcFrame.top = 0;
			mAviStreamInfo.rcFrame.right = framwWidth;
			mAviStreamInfo.rcFrame.bottom = frameHeight;
			mAviStreamInfo.dwEditCount = 0;
			mAviStreamInfo.dwFormatChangeCount = 0;
			wcscpy_s(mAviStreamInfo.szName, L"DirectCompute M-JPEG Demo Video");

			// create stream
			hr = AVIFileCreateStream(mAviOutputFile, &mAviVideo, &mAviStreamInfo);

			if(hr == S_OK)
			{
				struct
				{
					EXBMINFOHEADER bi;
					JPEGINFOHEADER jpeg;
				}header;

				memset(&header, 0, sizeof(header));
				header.bi.bmi.biSize         = sizeof(header);
				header.bi.bmi.biWidth        = framwWidth;
				header.bi.bmi.biHeight       = frameHeight;
				header.bi.bmi.biPlanes       = 1;
				header.bi.bmi.biBitCount     = 24;
				header.bi.bmi.biCompression  = MJPG_DIB;
				header.bi.biExtDataOffset = sizeof(header.bi);

				header.jpeg.JPEGSize = sizeof(header.jpeg);
				header.jpeg.JPEGProcess = JPEG_PROCESS_BASELINE;
				header.jpeg.JPEGColorSpaceID = JPEG_YCbCr; // <-- YCbCr as define by CCIR 601
				header.jpeg.JPEGBitsPerSample = 8;
				header.jpeg.JPEGHSubSampling = 1;
				header.jpeg.JPEGVSubSampling = 1;
	
				hr = AVIStreamSetFormat(mAviVideo,
										0,   
										(void*)&header, 
										sizeof(header));

				if(hr == S_OK)
					mRecording = true;
			}
		}
		return hr;
	}

	HRESULT AppendFrame(void* frameData, int dataSize)
	{
		HRESULT hr = E_FAIL;

		if(mRecording)
		{
			LONG nSamplesWritten = 0;
			LONG nBytesWritten = 0; 

			// append compressed frame to stream
			hr = AVIStreamWrite(
				mAviVideo,
				mCurrentFrame,
				1,
				frameData,
				dataSize,
				AVIIF_KEYFRAME,
				&nSamplesWritten, 
				&nBytesWritten);

			mCurrentFrame++;
		}
		return hr;
	}

	HRESULT StopRecording()
	{
		HRESULT hr = E_FAIL;

		if(mRecording)
		{
			AVIStreamRelease(mAviVideo);
			AVIFileRelease(mAviOutputFile);
			AVIFileExit();

			mRecording = false;

			hr = S_OK;
		}

		return hr;
	}
};