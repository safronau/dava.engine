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



#include "DAVAEngine.h"
#include "UI/UIEvent.h"
#include "UI/UIControlSystem.h"

#include "Platform/Qt5/QtLayer.h"

#if defined (__DAVAENGINE_MACOS__)
    #include "Platform/Qt5/MacOS/CoreMacOSPlatformQt.h"
#elif defined (__DAVAENGINE_WIN32__)
    #include "Platform/Qt5/Win32/CorePlatformWin32Qt.h"
#endif //#if defined (__DAVAENGINE_MACOS__)

#include "davaglwidget.h"

#include <QMessageBox>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>


#if defined(__DAVAENGINE_WIN32__) || defined(__DAVAENGINE_MACOS__)
#include "Network/NetCore.h"
#endif

#include "Main/mainwindow.h"
#include "ui_mainwindow.h"

#include <QOpenGLContext>
#include <QPainter>
#include <QOpenGLPaintDevice>

#include <QBoxLayout>



OpenGLWindow::OpenGLWindow() : QWindow()
{
    context = nullptr;
    paintDevice = nullptr;
    setSurfaceType(QWindow::OpenGLSurface);
}

OpenGLWindow::~OpenGLWindow()
{
    
}


void OpenGLWindow::render()
{
    if (!paintDevice)
    {
        paintDevice = new QOpenGLPaintDevice;
    }
    
    paintDevice->setSize(size());

//    DAVA::QtLayer::Instance()->ProcessFrame();
    
    QPainter painter(paintDevice);
    render(&painter);
}

void OpenGLWindow::render(QPainter *painter)
{
    Q_UNUSED(painter);
}

void OpenGLWindow::renderNow()
{
    if (!isExposed())
        return;
    
    bool needsInitialize = false;
    
    if (!context)
    {
        context = new QOpenGLContext(this);
        context->setFormat(requestedFormat());
        context->create();
        
        needsInitialize = true;
    }
    
    context->makeCurrent(this);
    
    if (needsInitialize)
    {
        initializeOpenGLFunctions();
    }
    
    render();
    
    context->swapBuffers(this);
}

void OpenGLWindow::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);
    
    if (isExposed())
    {
        renderNow();
        
        emit Exposed();
    }
}

bool OpenGLWindow::event(QEvent *event)
{
    if(event->type() == QEvent::UpdateRequest)
    {
        renderNow();
        return true;
    }
    
    return QWindow::event(event);
}


///=======================
DavaGLWidget::DavaGLWidget(QWidget *parent)
    : QWidget(parent)
    , renderTimer(nullptr)
    , fps(60)
    , isInitialized(false)
    , currentDPR(1)
    , currentWidth(0)
    , currentHeight(0)
{
    setAcceptDrops(true);
    setMouseTracking(true);

    setFocusPolicy( Qt::StrongFocus );
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    setMinimumSize(100, 100);
    
    DAVA::QtLayer::Instance()->SetDelegate(this);
    
    SetFPS(60);
    renderTimer = new QTimer(this);
    
    openGlWindow = new OpenGLWindow();
    connect(openGlWindow, SIGNAL(Exposed()), this, SLOT(OnWindowExposed()));
    
    QWidget *w = createWindowContainer(openGlWindow);
    
    QBoxLayout *lay = new QBoxLayout(QBoxLayout::TopToBottom, this);
    setLayout(lay);
    
    layout()->addWidget(w);
}

DavaGLWidget::~DavaGLWidget()
{
}

void DavaGLWidget::SetFPS(int _fps)
{
    DVASSERT(0 != fps);
    
    DAVA::RenderManager::Instance()->SetFPS(fps);
    fps = _fps;
}


void DavaGLWidget::OnWindowExposed()
{
    currentDPR = devicePixelRatio();
    
    DAVA::QtLayer::Instance()->InitializeGlWindow();
    PerformSizeChange();

    isInitialized = true;
    
    emit Initialized();
    
    renderTimer->singleShot(16, this, SLOT(OnRenderTimer()));
}

void DavaGLWidget::OnRenderTimer()
{
    const int dpr = devicePixelRatio();
    if(dpr != currentDPR)   //TODO: move to event processing section
    {
        currentDPR = dpr;
        PerformSizeChange();
    }                       // END OF TODO
    
    QCoreApplication::postEvent(openGlWindow, new QEvent(QEvent::UpdateRequest));
    DVASSERT(QOpenGLContext::currentContext() == openGlWindow->context);

    DAVA::QtLayer::Instance()->ProcessFrame();

    
    renderTimer->singleShot(16, this, SLOT(OnRenderTimer()));
}

void DavaGLWidget::resizeEvent(QResizeEvent *e)
{
    currentWidth = e->size().width();
    currentHeight = e->size().height();
    
    QWidget::resizeEvent(e);
    
    PerformSizeChange();
}


void DavaGLWidget::keyPressEvent(QKeyEvent *e)
{
    char16 keyChar = e->nativeVirtualKey();
    const Qt::KeyboardModifiers modifiers = e->modifiers();
    
    if(modifiers & Qt::ShiftModifier)
    {
        DAVA::InputSystem::Instance()->GetKeyboard()->OnKeyPressed(DAVA::DVKEY_SHIFT);
    }
    if(modifiers & Qt::ControlModifier)
    {
        DAVA::InputSystem::Instance()->GetKeyboard()->OnKeyPressed(DAVA::DVKEY_CTRL);
    }
    if(modifiers & Qt::AltModifier)
    {
        DAVA::InputSystem::Instance()->GetKeyboard()->OnKeyPressed(DAVA::DVKEY_ALT);
    }
    
    if(keyChar)
    {
        char16 davaKey = DAVA::InputSystem::Instance()->GetKeyboard()->GetDavaKeyForSystemKey(keyChar);
        DAVA::QtLayer::Instance()->KeyPressed(keyChar, davaKey, e->count(), e->timestamp());
    }
}

void DavaGLWidget::keyReleaseEvent(QKeyEvent *e)
{
    int key = e->key();
    char16 keyChar = e->nativeVirtualKey();

    if(Qt::Key_Shift == key)
    {
        DAVA::InputSystem::Instance()->GetKeyboard()->OnKeyUnpressed(DAVA::DVKEY_SHIFT);
    }
    else if(Qt::Key_Control == key)
    {
        DAVA::InputSystem::Instance()->GetKeyboard()->OnKeyUnpressed(DAVA::DVKEY_CTRL);
    }
    else if(Qt::Key_Alt == key)
    {
        DAVA::InputSystem::Instance()->GetKeyboard()->OnKeyUnpressed(DAVA::DVKEY_ALT);
    }
    
    if(keyChar)
    {
        char16 davaKey = DAVA::InputSystem::Instance()->GetKeyboard()->GetDavaKeyForSystemKey(keyChar);
        DAVA::QtLayer::Instance()->KeyReleased(keyChar, davaKey);
    }
}


void DavaGLWidget::mouseMoveEvent(QMouseEvent * event)
{
    const Qt::MouseButtons buttons = event->buttons();
    DAVA::UIEvent davaEvent = MapMouseEventToDAVA(event);
    davaEvent.phase = DAVA::UIEvent::PHASE_DRAG;

    
    bool dragWasApplied = false;
    if(buttons & Qt::LeftButton)
    {
        dragWasApplied = true;
        davaEvent.tid = MapQtButtonToDAVA(Qt::LeftButton);
        DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
    }
    if(buttons & Qt::RightButton)
    {
        dragWasApplied = true;
        davaEvent.tid = MapQtButtonToDAVA(Qt::RightButton);
        DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
    }
    if(buttons & Qt::MiddleButton)
    {
        dragWasApplied = true;
        davaEvent.tid = MapQtButtonToDAVA(Qt::MiddleButton);
        DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
    }

    if(!dragWasApplied)
    {
        davaEvent.phase = DAVA::UIEvent::PHASE_MOVE;
        davaEvent.tid = MapQtButtonToDAVA(Qt::LeftButton);
        DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
    }
}

void DavaGLWidget::mousePressEvent(QMouseEvent * event)
{
    DAVA::UIEvent davaEvent = MapMouseEventToDAVA(event);
    davaEvent.phase = DAVA::UIEvent::PHASE_BEGAN;

    DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
}

void DavaGLWidget::mouseReleaseEvent(QMouseEvent * event)
{
    DAVA::UIEvent davaEvent = MapMouseEventToDAVA(event);
    davaEvent.phase = DAVA::UIEvent::PHASE_ENDED;
    
    DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
}

void DavaGLWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    DAVA::UIEvent davaEvent = MapMouseEventToDAVA(event);
    davaEvent.phase = DAVA::UIEvent::PHASE_ENDED;
    davaEvent.tapCount = 2;
    
    DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
}

#ifndef QT_NO_WHEELEVENT
void DavaGLWidget::wheelEvent(QWheelEvent *event)
{
    DAVA::UIEvent davaEvent;
    
    int numDegrees = event->delta() / 8;
    int numSteps = numDegrees / 15;
    
    davaEvent.point = davaEvent.physPoint = Vector2(numSteps, numSteps);
    davaEvent.tid = DAVA::UIEvent::PHASE_WHEEL;
    davaEvent.phase = DAVA::UIEvent::PHASE_WHEEL;

    davaEvent.timestamp = event->timestamp();
    davaEvent.tapCount = 1;
    
    DAVA::QtLayer::Instance()->MouseEvent(davaEvent);
}
#endif //QT_NO_WHEELEVENT

DAVA::UIEvent DavaGLWidget::MapMouseEventToDAVA(const QMouseEvent *event) const
{
    DAVA::UIEvent davaEvent;
    QPoint pos = event->pos();
    davaEvent.point = davaEvent.physPoint = Vector2(pos.x() * currentDPR, pos.y() * currentDPR);
    davaEvent.tid = MapQtButtonToDAVA(event->button());
    davaEvent.timestamp = event->timestamp();
    davaEvent.tapCount = 1;

    return davaEvent;
}

DAVA::UIEvent::eButtonID DavaGLWidget::MapQtButtonToDAVA(const Qt::MouseButton button) const
{
    switch (button)
    {
        case Qt::LeftButton:
            return DAVA::UIEvent::BUTTON_1;

        case Qt::RightButton:
            return DAVA::UIEvent::BUTTON_2;

        case Qt::MiddleButton:
            return DAVA::UIEvent::BUTTON_3;

        default:
            break;
    }
    
    return DAVA::UIEvent::BUTTON_NONE;
}

void DavaGLWidget::dragEnterEvent(QDragEnterEvent *event)
{
    event->setDropAction(Qt::LinkAction);
	event->accept();
}

void DavaGLWidget::dragMoveEvent(QDragMoveEvent *event)
{
	DAVA::UIEvent davaEvent;
    QPoint pos = event->pos();
    davaEvent.point = davaEvent.physPoint = Vector2(pos.x() * currentDPR, pos.y() * currentDPR);
    davaEvent.tid = MapQtButtonToDAVA(Qt::LeftButton);
    davaEvent.timestamp = 0;
    davaEvent.tapCount = 1;
    davaEvent.phase = DAVA::UIEvent::PHASE_MOVE;

    DAVA::QtLayer::Instance()->MouseEvent(davaEvent);

    event->setDropAction(Qt::LinkAction);
	event->accept();
}

void DavaGLWidget::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();
	emit OnDrop(mimeData);

    event->setDropAction(Qt::LinkAction);
	event->accept();
}


void DavaGLWidget::Quit()
{
    exit(0);
}

void DavaGLWidget::ShowAssertMessage(const char * message)
{
    QMessageBox::critical(this, "", message);
}


void DavaGLWidget::PerformSizeChange()
{
    if(IsInitialized())
        DAVA::QtLayer::Instance()->Resize(currentWidth * currentDPR, currentHeight * currentDPR);
    
    emit Resized(currentWidth, currentHeight, currentDPR);
}

