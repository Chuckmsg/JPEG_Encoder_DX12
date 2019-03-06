# JPEG_Encoder_DX12

--------------------------------------------------------
JPEG Compression using DirectCompute, Demo Source Code
--------------------------------------------------------
Stefan Petersson (spr@bth.se)
September, 2012

--------------------------------------------------------
ABOUT THE DEMO
--------------------------------------------------------
The backbuffer is filled by a compute shader invokation where the backbuffer was created with the DXGI_USAGE_UNORDERED_ACCESS flag. The backbuffer data can thereafter be saved to a JPEG image file or appended to a MJPEG movie sequence. The challenges covered in the book chapter are (hopefully) solved by the SurfacePreparation class.

Keys:
F1 - Save to JPEG file. Files are named 001.jpg, 002.jpg and so on.
F2 - Start recording to a MJPEG movie file. Files are named 001.avi, 002.avi and so on.
F3 - Stop recording to the MJPEG movie file.

Ctrl + Numpad minus: Increase JPEG quality
Ctrl + Numpad plus: Decrease JPEG quality

Shift + Numpad minus: Increase JPEG quality
Shift + Numpad plus: Decrease JPEG quality

Numpad 1: Set 4:4:4 chroma subsampling
Numpad 2: Set 4:2:2 chroma subsampling
Numpad 3: Set 4:2:0 chroma subsampling

Esc - Exit application

--------------------------------------------------------
REQUIREMENTS
--------------------------------------------------------
* GPU supporting DirectX 11.0 or higher. Ref device used if not.
* Windows Software Development Kit (SDK) for Windows 8 or newer (project configured with "C:\Program Files (x86)\Windows Kits\8.0\")
* DirectX SDK June 2010 may also be used, just need to reconfigure project settings.
* Visual Studio 2010 and 2012 - project configured with both x86 and x64 platform targets.


--------------------------------------------------------
NOTE!
--------------------------------------------------------
* When recording movie sequences the framerate is locked at 24 frames per second. The fps lock is set in the code and is easy to change, so please try to do so :-)
* The code is not structured, commented or optimized in a very good way. It's a demo...
* If encoded image has artifacts they may be caused by driver bugs (I had artifacts on a GeForce 690 card). Please try ref device to verify.
* Proper error checks are NOT performed!

--------------------------------------------------------
FREE CODE?
--------------------------------------------------------
You can use and/or improve the code in any way you like. The only thing I request is that you mention me as the original author, thank you!

This work is provided on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND. You are solely responsible for the consequences of using or redistributing this code and assume any risks associated with these or related activities.

---------------------------------------------------
ABOUT THE AUTHOR
---------------------------------------------------
Stefan Petersson holds M.Sc. in computer science from Blekinge Institute of Technology. He is currently a Lecturer and PhD student in the School of Computing (COM) at Blekinge Institute of Technology. He is program manager for the Master of Science in Game and Software Engineering programme and teaches courses in 3D programming, game projects, GPGPU and so on.
