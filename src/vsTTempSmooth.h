#pragma once

#include <algorithm>
#include <array>
#include <vector>

#include "include\avisynth.h"

#define MAX_TEMP_RAD 7

template<bool pfclip, bool fp>
class TTempSmooth : public GenericVideoFilter
{
    int _maxr;
    float _scthresh;
    int _diameter;
    int _thresh[3];
    int _mdiff[3];
    int _shift;
    float _threshF[3];
    std::array<std::vector<float>, 3> _weight;
    float _cw;
    int proccesplanes[3];
    PClip _pfclip;
    bool has_at_least_v8;
    int _opt;

    int _pmode;

    template<typename T, bool useDiff>
    void filterI(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;
    template<bool useDiff>
    void filterF(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;

    template<typename T, bool useDiff>
    void filterI_sse2(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;
    template<bool useDiff>
    void filterF_sse2(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;

    template<typename T, bool useDiff>
    void filterI_avx2(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;
    template<bool useDiff>
    void filterF_avx2(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;

    template<typename T, bool useDiff>
    void filterI_avx512(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;
    template<bool useDiff>
    void filterF_avx512(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane) noexcept;

    float (*compare)(PVideoFrame& src, PVideoFrame& src1, const int bits_per_pixel) noexcept;

    void filterI_mode2(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane);


#ifdef _DEBUG
    //MEL debug stat
    int iMEL_non_current_samples;
    int iMEL_mem_hits;
    int iMEL_mem_updates;
    int iDM_cache_hits;
#endif

public:
    TTempSmooth(PClip _child, int maxr, int ythresh, int uthresh, int vthresh, int ymdiff, int umdiff, int vmdiff, int strength, float scthresh, int y, int u, int v, PClip pfclip_, int opt, int pmode, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }
};

template <typename T>
float ComparePlane_sse2(PVideoFrame& src, PVideoFrame& src1, const int bits_per_pixel) noexcept;
template <typename T>
float ComparePlane_avx2(PVideoFrame& src, PVideoFrame& src1, const int bits_per_pixel) noexcept;
template <typename T>
float ComparePlane_avx512(PVideoFrame& src, PVideoFrame& src1, const int bits_per_pixel) noexcept;
