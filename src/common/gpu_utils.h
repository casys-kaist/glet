#ifndef __GPU_UTILS_H__
#define __GPU_UTILS_H__
#include <cstdint>

class GPUUtil{
    public:
        GPUUtil();
        ~GPUUtil();
        uint64_t GetTotalMemory(int gpu_id);
        uint64_t GetUsedMemory(int gpu_id);
};
#else
#endif
