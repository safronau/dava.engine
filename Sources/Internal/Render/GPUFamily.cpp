
/*==================================================================================
    Copyright (c) 2008, DAVA Consulting, LLC
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the DAVA Consulting, LLC nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE DAVA CONSULTING, LLC AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL DAVA CONSULTING, LLC BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Revision History:
        * Created by Vitaliy Borodovsky 
=====================================================================================*/
#include "Render/GPUFamily.h"
#include "FileSystem/Logger.h"
#include "Debug/DVAssert.h"
#include "FileSystem/FilePath.h"
#include "Utils/StringFormat.h"
#include "Render/TextureDescriptor.h"

namespace DAVA
{
    
Vector<GPUFamily::GPUData> GPUFamily::gpuData;

void GPUFamily::SetupGPUParameters()
{
    gpuData.resize(GPU_FAMILY_COUNT);

    SetupGPUFormats();
    SetupGPUPostfixes();
}

void GPUFamily::SetupGPUFormats()
{
    //pvr ios
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_RGBA8888] = ".pvr";
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_RGBA5551] = ".pvr";
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_RGBA4444] = ".pvr";
//    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_RGB888] = ".pvr";
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_RGB565] = ".pvr";
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_A8] = ".pvr";
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_PVR4] = ".pvr";
    gpuData[GPU_POVERVR_IOS].availableFormats[FORMAT_PVR2] = ".pvr";
    

    //pvr android
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_RGBA8888] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_RGBA5551] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_RGBA4444] = ".pvr";
    //    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_RGB888] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_RGB565] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_A8] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_PVR4] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_PVR2] = ".pvr";
    gpuData[GPU_POVERVR_ANDROID].availableFormats[FORMAT_ETC1] = ".pvr";

    
    gpuData[GPU_TEGRA].availableFormats[FORMAT_RGBA8888] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_DXT1] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_DXT1A] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_DXT1NM] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_DXT3] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_DXT5] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_DXT5NM] = ".dds";
    gpuData[GPU_TEGRA].availableFormats[FORMAT_ETC1] = ".pvr";

    
    gpuData[GPU_MALI].availableFormats[FORMAT_ETC1] = ".pvr";
    

    gpuData[GPU_ADRENO].availableFormats[FORMAT_ATC_RGB] = ".dds";
    gpuData[GPU_ADRENO].availableFormats[FORMAT_ATC_RGBA_EXPLICIT_ALPHA] = ".dds";
    gpuData[GPU_ADRENO].availableFormats[FORMAT_ATC_RGBA_INTERPOLATED_ALPHA] = ".dds";
    gpuData[GPU_ADRENO].availableFormats[FORMAT_ETC1] = ".pvr";
}

void GPUFamily::SetupGPUPostfixes()
{
    gpuData[GPU_POVERVR_IOS].name = "PoverVR_iOS";
    gpuData[GPU_POVERVR_ANDROID].name = "PoverVR_Android";
    gpuData[GPU_TEGRA].name = "tegra";
    gpuData[GPU_MALI].name = "mali";
    gpuData[GPU_ADRENO].name = "adreno";
}

    
const Map<PixelFormat, String> & GPUFamily::GetAvailableFormatsForGpu(eGPUFamily gpuFamily)
{
    DVASSERT(0 <= gpuFamily && gpuFamily < GPU_FAMILY_COUNT);
    
    return gpuData[gpuFamily].availableFormats;
}

eGPUFamily GPUFamily::GetGPUForPathname(const FilePath &pathname)
{
    const String filename = pathname.GetFilename();
    
    for(int32 i = 0; GPU_FAMILY_COUNT; ++i)
    {
        eGPUFamily gpu = (eGPUFamily)i;
        
        String strForFind(Format(".%s.", gpuData[gpu].name.c_str()));
        if(String::npos != filename.rfind(strForFind))
        {
            return gpu;
        }
    }

    
    return GPU_UNKNOWN;
}
 
FilePath GPUFamily::CreatePathnameForGPU(const TextureDescriptor *descriptor, eGPUFamily gpuFamily)
{
    DVASSERT(descriptor);
    
    eGPUFamily requestedGPU = gpuFamily;
    PixelFormat requestedFormat = descriptor->compression[gpuFamily].format;
    
    if(descriptor->IsCompressedFile())
    {
        requestedGPU = (eGPUFamily)descriptor->exportedAsGpuFamily;
        requestedFormat = (PixelFormat)descriptor->exportedAsPixelFormat;
    }
    
    return FilePath::CreateWithNewExtension(descriptor->pathname, GetFilenamePostfix(requestedGPU, requestedFormat));
}

    
    
const String & GPUFamily::GetGPUName(const eGPUFamily gpuFamily)
{
    DVASSERT(0 <= gpuFamily && gpuFamily < GPU_FAMILY_COUNT);

    return gpuData[gpuFamily].name;
}

    
const String & GPUFamily::GetCompressedFileExtension(const eGPUFamily gpuFamily, const PixelFormat pixelFormat)
{
    DVASSERT(0 <= gpuFamily && gpuFamily < GPU_FAMILY_COUNT);

    Map<PixelFormat, String>::const_iterator format = gpuData[gpuFamily].availableFormats.find(pixelFormat);
    DVASSERT(format != gpuData[gpuFamily].availableFormats.end());
    
    return format->second;
}

    
String GPUFamily::GetFilenamePostfix(const eGPUFamily gpuFamily, const PixelFormat pixelFormat)
{
    DVASSERT(0 <= gpuFamily && gpuFamily < GPU_FAMILY_COUNT);
    
    Map<PixelFormat, String>::const_iterator format = gpuData[gpuFamily].availableFormats.find(pixelFormat);
    DVASSERT(format != gpuData[gpuFamily].availableFormats.end());
    
    String postfix(Format(".%s%s", gpuData[gpuFamily].name.c_str(), format->second.c_str()));
    return postfix;
}
    
    
};
