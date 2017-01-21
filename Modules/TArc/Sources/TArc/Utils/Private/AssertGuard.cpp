#include "TArc/Utils/AssertGuard.h"
#include "TArc/Utils/ScopedValueGuard.h"

#include "Debug/DVAssert.h"
#include "Debug/DVAssertDefaultHandlers.h"
#include "Concurrency/LockGuard.h"
#include "Concurrency/Thread.h"

#if defined(__DAVAENGINE_MACOS__)
#include "TArc/Utils/AssertGuardMacOSHack.h"
#endif

#include "Base/StaticSingleton.h"

#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QWidget>

namespace DAVA
{
namespace TArc
{
class EventFilter final : public QObject
{
public:
    EventFilter()
    {
        QWidgetList lst = qApp->allWidgets();
        for (int i = 0; i < lst.size(); ++i)
        {
            QWidget* widget = lst[i];
            widget->installEventFilter(this);
        }

        QAbstractEventDispatcher* dispatcher = qApp->eventDispatcher();
        qApp->installEventFilter(this);
        if (dispatcher != nullptr)
        {
            dispatcher->installEventFilter(this);
        }
    }

    bool eventFilter(QObject* obj, QEvent* e) override
    {
        if (e->spontaneous())
        {
            return true;
        }

        QEvent::Type type = e->type();
        switch (type)
        {
        case QEvent::Timer:
        case QEvent::Expose:
        case QEvent::Paint:
            return true;
        default:
            break;
        }

        return false;
    }
};

class AssertGuard : public StaticSingleton<AssertGuard>
{
public:
    void SetMode(eApplicationMode mode_)
    {
        mode = mode_;
    }

    Assert::FailBehaviour HandleAssert(const Assert::AssertInfo& assertInfo)
    {
        LockGuard<Mutex> mutexGuard(mutex);
        DAVA::TArc::ScopedValueGuard<bool> valueGuard(isInAssert, true);

        std::unique_ptr<EventFilter> filter;
        if (Thread::IsMainThread())
        {
            filter.reset(new EventFilter());
        }

#if defined(__DAVAENGINE_MACOS__)
        MacOSRunLoopGuard macOSGuard;
#endif

        switch (mode)
        {
        case eApplicationMode::CONSOLE_MODE:
            return Assert::DefaultLoggerHandler(assertInfo);
        case eApplicationMode::GUI_MODE:
            Assert::DefaultLoggerHandler(assertInfo);
            return Assert::DefaultDialogBoxHandler(assertInfo);
        case eApplicationMode::TEST_MODE:
            return Assert::DefaultLoggerHandler(assertInfo);
        default:
            break;
        }

        return Assert::FailBehaviour::Default;
    }

    bool IsInsideAssert() const
    {
        return isInAssert;
    }

private:
    Mutex mutex;
    bool isInAssert = false;
    eApplicationMode mode;
};

Assert::FailBehaviour AssertHandler(const Assert::AssertInfo& assertInfo)
{
    return AssertGuard::Instance()->HandleAssert(assertInfo);
}

void SetupToolsAssertHandlers(eApplicationMode mode)
{
    AssertGuard::Instance()->SetMode(mode);
    Assert::AddHandler(&AssertHandler);
}

bool IsInsideAssertHandler()
{
    return AssertGuard::Instance()->IsInsideAssert();
}

} // namespace TArc
} // namespace DAVA
