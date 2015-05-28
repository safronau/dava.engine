/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    =====================================================================================*/


#ifndef __FORMATS_TEST_H__
#define __FORMATS_TEST_H__

#include "DAVAEngine.h"
using namespace DAVA;

#include "TestTemplate.h"

class FormatsTest : public TestTemplate<FormatsTest>
{
public:
    FormatsTest();
    ~FormatsTest() = default;

    void LoadResources() override;
    void UnloadResources() override;

    void TestFunction(PerfFuncData *data);

private:
    void TestJpeg(PerfFuncData *data);
    void TestPng(PerfFuncData *data);
    void TestPvr(PerfFuncData *data);
    void TestDds(PerfFuncData *data);
    void TestWebP(PerfFuncData *data);

    void TestImageInfo(const DAVA::FilePath &fileName, DAVA::PixelFormat &requestedFormat, PerfFuncData *data);

    static const DAVA::float32 MAX_DIFFERENCE; // in percents
};

#endif // __FORMATS_TEST_H__
