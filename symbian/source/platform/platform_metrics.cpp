#include "platform/platform_metrics.h"

#include <QtCore/QtGlobal>

#if defined(Q_OS_SYMBIAN)
#include <e32std.h>
#endif

namespace wiliwili {

MemorySample PlatformMetrics::sampleMemory()
{
    MemorySample sample;

#if defined(Q_OS_SYMBIAN)
    TInt totalAllocated = 0;
    User::AllocSize(totalAllocated);

    TInt largestBlock = 0;
    const TInt totalFree = User::Available(largestBlock);

    sample.allocatedBytes = totalAllocated;
    sample.allocationCells = User::CountAllocCells();
    sample.freeBytes = totalFree;
    sample.largestFreeBlock = largestBlock;
#endif

    return sample;
}

} // namespace wiliwili
