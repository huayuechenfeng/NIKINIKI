#ifndef WILIWILI_SYMBIAN_PLATFORM_METRICS_H
#define WILIWILI_SYMBIAN_PLATFORM_METRICS_H

namespace wiliwili {

struct MemorySample
{
    int allocatedBytes;
    int allocationCells;
    int freeBytes;
    int largestFreeBlock;

    MemorySample()
        : allocatedBytes(0),
          allocationCells(0),
          freeBytes(0),
          largestFreeBlock(0)
    {
    }
};

class PlatformMetrics
{
public:
    static MemorySample sampleMemory();
};

} // namespace wiliwili

#endif
