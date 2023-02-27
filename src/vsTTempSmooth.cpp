#include <algorithm>
#include <array>
#include <vector>

#include "avisynth.h"

template<bool pfclip>
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

    template<typename T, bool useDiff, bool fp>
    void filterI(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane, int src_stride) noexcept;
    template<bool useDiff, bool fp>
    void filterF(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane, int src_stride) noexcept;

    void(TTempSmooth::* filter)(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane, int src_stride) noexcept;

public:
    TTempSmooth(PClip _child, int maxr, int ythresh, int uthresh, int vthresh, int ymdiff, int umdiff, int vmdiff, int strength, float scthresh, bool fp, int y, int u, int v, PClip pfclip_, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
    int __stdcall SetCacheHints(int cachehints, int frame_range) override
    {
        return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }
};

template <bool pfclip>
template <typename T, bool useDiff, bool fp>
void TTempSmooth<pfclip>::filterI(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane, int src_stride) noexcept
{
    src_stride /= sizeof(T);
    const size_t stride{ dst->GetPitch(plane) / sizeof(T) };
    const size_t width{ dst->GetRowSize(plane) / sizeof(T) };
    const int height{ dst->GetHeight(plane) };
    const T* srcp[15] = {}, * pfp[15] = {};
    for (int i{ 0 }; i < _diameter; ++i)
    {
        srcp[i] = reinterpret_cast<const T*>(src[i]->GetReadPtr(plane));
        pfp[i] = reinterpret_cast<const T*>(pf[i]->GetReadPtr(plane));
    }

    T* __restrict dstp{ reinterpret_cast<T*>(dst->GetWritePtr(plane)) };

    const int l{ plane >> 1 };
    const int thresh{ _thresh[l] << _shift };
    const float* const weightSaved{ _weight[l].data() };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; ++x)
        {
            const int c{ static_cast<int>(pfp[_maxr][x]) };
            float weights{ _cw };
            float sum{ srcp[_maxr][x] * _cw };

            int frameIndex{ _maxr - 1 };

            if (frameIndex > fromFrame)
            {
                int t1{ static_cast<int>(pfp[frameIndex][x]) };
                int diff{ std::abs(c - t1) };

                if (diff < thresh)
                {
                    float weight{ weightSaved[useDiff ? diff >> _shift : frameIndex] };
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    --frameIndex;
                    int v{ 256 };

                    while (frameIndex > fromFrame)
                    {
                        const int t2{ t1 };
                        t1 = pfp[frameIndex][x];
                        diff = std::abs(c - t1);

                        if (diff < thresh && std::abs(t1 - t2) < thresh)
                        {
                            weight = weightSaved[useDiff ? (diff >> _shift) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            --frameIndex;
                            v += 256;
                        }
                        else
                            break;
                    }
                }
            }

            frameIndex = _maxr + 1;

            if (frameIndex < toFrame)
            {
                int t1{ static_cast<int>(pfp[frameIndex][x]) };
                int diff{ std::abs(c - t1) };

                if (diff < thresh)
                {
                    float weight{ weightSaved[useDiff ? diff >> _shift : frameIndex] };
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    ++frameIndex;
                    int v{ 256 };

                    while (frameIndex < toFrame)
                    {
                        const int t2{ t1 };
                        t1 = pfp[frameIndex][x];
                        diff = std::abs(c - t1);

                        if (diff < thresh && std::abs(t1 - t2) < thresh)
                        {
                            weight = weightSaved[useDiff ? (diff >> _shift) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            ++frameIndex;
                            v += 256;
                        }
                        else
                            break;
                    }
                }
            }

            if constexpr (fp)
                dstp[x] = static_cast<T>(srcp[_maxr][x] * (1.f - weights) + sum + 0.5f);
            else
                dstp[x] = static_cast<T>(sum / weights + 0.5f);
        }

        for (int i{ 0 }; i < _diameter; ++i)
        {
            srcp[i] += src_stride;
            pfp[i] += src_stride;
        }
        dstp += stride;
    }
}

template <bool pfclip>
template <bool useDiff, bool fp>
void TTempSmooth<pfclip>::filterF(PVideoFrame src[15], PVideoFrame pf[15], PVideoFrame& dst, const int fromFrame, const int toFrame, const int plane, int src_stride) noexcept
{
    src_stride /= sizeof(T);
    const size_t stride{ dst->GetPitch(plane) / sizeof(T) };
    const size_t width{ dst->GetRowSize(plane) / sizeof(T) };
    const int height{ dst->GetHeight(plane) };
    const float* srcp[15] = {}, * pfp[15] = {};
    for (int i{ 0 }; i < _diameter; ++i)
    {
        srcp[i] = reinterpret_cast<const float*>(src[i]->GetReadPtr(plane));
        pfp[i] = reinterpret_cast<const float*>(pf[i]->GetReadPtr(plane));
    }

    float* __restrict dstp{ reinterpret_cast<float*>(dst->GetWritePtr(plane)) };

    const int l{ plane >> 1 };
    const float thresh{ _threshF[l] };
    const float* const weightSaved{ _weight[l].data() };

    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; ++x)
        {
            const float c{ pfp[_maxr][x] };
            float weights{ _cw };
            float sum{ srcp[_maxr][x] * _cw };

            int frameIndex{ _maxr - 1 };

            if (frameIndex > fromFrame)
            {
                float t1{ pfp[frameIndex][x] };
                float diff{ std::min(std::abs(c - t1), 1.f) };

                if (diff < thresh)
                {
                    float weight{ weightSaved[useDiff ? static_cast<int>(diff * 255.f) : frameIndex] };
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    --frameIndex;
                    int v{ 256 };

                    while (frameIndex > fromFrame)
                    {
                        const float t2{ t1 };
                        t1 = pfp[frameIndex][x];
                        diff = std::min(std::abs(c - t1), 1.f);

                        if (diff < thresh && std::min(std::abs(t1 - t2), 1.f) < thresh)
                        {
                            weight = weightSaved[useDiff ? static_cast<int>(diff * 255.f) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            --frameIndex;
                            v += 256;
                        }
                        else
                            break;
                    }
                }
            }

            frameIndex = _maxr + 1;

            if (frameIndex < toFrame)
            {
                float t1{ pfp[frameIndex][x] };
                float diff{ std::min(std::abs(c - t1), 1.f) };

                if (diff < thresh)
                {
                    float weight{ weightSaved[useDiff ? static_cast<int>(diff * 255.f) : frameIndex] };
                    weights += weight;
                    sum += srcp[frameIndex][x] * weight;

                    ++frameIndex;
                    int v{ 256 };

                    while (frameIndex < toFrame)
                    {
                        const float t2{ t1 };
                        t1 = pfp[frameIndex][x];
                        diff = std::min(std::abs(c - t1), 1.f);

                        if (diff < thresh && std::min(std::abs(t1 - t2), 1.f) < thresh)
                        {
                            weight = weightSaved[useDiff ? static_cast<int>(diff * 255.f) + v : frameIndex];
                            weights += weight;
                            sum += srcp[frameIndex][x] * weight;

                            ++frameIndex;
                            v += 256;
                        }
                        else
                            break;
                    }
                }
            }

            if constexpr (fp)
                dstp[x] = srcp[_maxr][x] * (1.f - weights) + sum;
            else
                dstp[x] = sum / weights;
        }

        for (int i{ 0 }; i < _diameter; ++i)
        {
            srcp[i] += src_stride;
            pfp[i] += src_stride;
        }
        dstp += stride;
    }
}

template <typename pixel_t>
AVS_FORCEINLINE static double get_sad_c(const uint8_t* c_plane8, const uint8_t* t_plane8, size_t height, size_t width, size_t c_pitch, size_t t_pitch) noexcept
{
    c_pitch /= sizeof(pixel_t);
    t_pitch /= sizeof(pixel_t);
    const pixel_t* c_plane{ reinterpret_cast<const pixel_t*>(c_plane8) };
    const pixel_t* t_plane{ reinterpret_cast<const pixel_t*>(t_plane8) };

    double accum{ 0 };

    for (size_t y{ 0 }; y < height; ++y)
    {
        for (size_t x{ 0 }; x < width; ++x)
            accum += std::abs(t_plane[x] - c_plane[x]);

        c_plane += c_pitch;
        t_plane += t_pitch;
    }

    return accum;
}

static float ComparePlane(PVideoFrame& src, PVideoFrame& src1, const int pixelsize, const int bits_per_pixel, IScriptEnvironment* env)
{
    int pitch{ src->GetPitch(PLANAR_Y) };
    int pitch2{ src1->GetPitch(PLANAR_Y) };
    int rowsize{ src->GetRowSize(PLANAR_Y) };
    int width{ rowsize / pixelsize };
    int height{ src->GetHeight(PLANAR_Y) };
    const uint8_t* srcp{ src->GetReadPtr(PLANAR_Y) };
    const uint8_t* srcp2{ src1->GetReadPtr(PLANAR_Y) };

    double sad{ 0 };
    // for c: width, for sse: rowsize
    if (pixelsize == 1)
        sad = get_sad_c<uint8_t>(srcp, srcp2, height, width, pitch, pitch2);
    else if (pixelsize == 2)
        sad = get_sad_c<uint16_t>(srcp, srcp2, height, width, pitch, pitch2);
    else // pixelsize==4
        sad = get_sad_c<float>(srcp, srcp2, height, width, pitch, pitch2);

    float f{ static_cast<float>((sad / (static_cast<double>(height) * width))) };

    if (bits_per_pixel != 32)
        f /= static_cast<float>((static_cast<int64_t>(1) << bits_per_pixel) - 1);

    return f;
}

template <bool pfclip>
TTempSmooth<pfclip>::TTempSmooth(PClip _child, int maxr, int ythresh, int uthresh, int vthresh, int ymdiff, int umdiff, int vmdiff, int strength, float scthresh, bool fp, int y, int u, int v, PClip pfclip_, IScriptEnvironment* env)
    : GenericVideoFilter(_child), _maxr(maxr), _scthresh(scthresh), _diameter(maxr * 2 + 1), _thresh{ ythresh, uthresh, vthresh }, _mdiff{ ymdiff, umdiff, vmdiff }, _shift(vi.BitsPerComponent() - 8), _threshF{ 0.0f, 0.0f, 0.0f },
    _cw(0.0f), _pfclip(pfclip_)
{
    has_at_least_v8 = env->FunctionExists("propShow");

    if (vi.IsRGB() || !vi.IsPlanar())
        env->ThrowError("vsTTempSmooth: clip must be Y/YUV(A) 8..32-bit planar format.");
    if (_maxr < 1 || _maxr > 7)
        env->ThrowError("vsTTempSmooth: maxr must be between 1..7.");
    if (ythresh < 1 || ythresh > 256)
        env->ThrowError("vsTTempSmooth: ythresh must be between 1..256.");
    if (uthresh < 1 || uthresh > 256)
        env->ThrowError("vsTTempSmooth: uthresh must be between 1..256.");
    if (vthresh < 1 || vthresh > 256)
        env->ThrowError("vsTTempSmooth: vthresh must be between 1..256.");
    if (ymdiff < 0 || ymdiff > 255)
        env->ThrowError("vsTTempSmooth: ymdiff must be between 0..255.");
    if (umdiff < 0 || umdiff > 255)
        env->ThrowError("vsTTempSmooth: umdiff must be between 0..255.");
    if (vmdiff < 0 || vmdiff > 255)
        env->ThrowError("vsTTempSmooth: vmdiff must be between 0..255.");
    if (strength < 1 || strength > 8)
        env->ThrowError("vsTTempSmooth: strength must be between 1..8.");
    if (_scthresh < 0.f || _scthresh > 100.f)
        env->ThrowError("vsTTempSmooth: scthresh must be between 0.0..100.0.");

    if constexpr (pfclip)
    {
        const VideoInfo& vi1 = pfclip_->GetVideoInfo();
        if (!vi.IsSameColorspace(vi1) || vi.width != vi1.width || vi.height != vi1.height)
            env->ThrowError("vsTTempSmooth: pfclip must have the same dimension as the main clip and be the same format.");
        if (vi.num_frames != vi1.num_frames)
            env->ThrowError("vsTTempSmooth: pfclip's number of frames doesn't match.");
    }

    const int planes[3] = { y, u, v };

    for (int i{ 0 }; i < std::min(vi.NumComponents(), 3); ++i)
    {
        switch (planes[i])
        {
            case 3: proccesplanes[i] = 3; break;
            case 2: proccesplanes[i] = 2; break;
            case 1: proccesplanes[i] = 1; break;
            default: env->ThrowError("vsTTempSmooth: y / u / v must be between 1..3.");
        }

        if (proccesplanes[i] == 3)
        {
            if (_thresh[i] > _mdiff[i] + 1)
            {
                _weight[i].resize(256 * _maxr);
                float dt[15] = {}, rt[256] = {}, sum = 0.f;

                for (int i{ 0 }; i < strength && i <= _maxr; ++i)
                    dt[i] = 1.f;
                for (int i{ strength }; i <= _maxr; ++i)
                    dt[i] = 1.f / (i - strength + 2);

                const float step{ 256.f / (_thresh[i] - std::min(_mdiff[i], _thresh[i] - 1)) };
                float base{ 256.f };
                for (int j{ 0 }; j < _thresh[i]; ++j)
                {
                    if (_mdiff[i] > j)
                    {
                        rt[j] = 256.f;
                    }
                    else
                    {
                        if (base > 0.f)
                            rt[j] = base;
                        else
                            break;
                        base -= step;
                    }
                }

                sum += dt[0];
                for (int j{ 1 }; j <= _maxr; ++j)
                {
                    sum += dt[j] * 2.f;
                    for (int v{ 0 }; v < 256; ++v)
                        _weight[i][256 * (j - 1) + v] = dt[j] * rt[v] / 256.f;
                }

                for (int j{ 0 }; j < 256 * _maxr; ++j)
                    _weight[i][j] /= sum;

                _cw = dt[0] / sum;
            }
            else
            {
                _weight[i].reserve(_diameter);
                float dt[15] = {}, sum = 0.f;

                for (int i{ 0 }; i < strength && i <= _maxr; ++i)
                    dt[_maxr - i] = dt[_maxr + i] = 1.f;
                for (int i{ strength }; i <= _maxr; ++i)
                    dt[_maxr - i] = dt[_maxr + i] = 1.f / (i - strength + 2);

                for (int j{ 0 }; j < _diameter; ++j)
                {
                    sum += dt[j];
                    _weight[i][j] = dt[j];
                }

                for (int j{ 0 }; j < _diameter; ++j)
                    _weight[i][j] /= sum;

                _cw = _weight[i][_maxr];
            }

            if (vi.ComponentSize() == 4)
                _threshF[i] = _thresh[i] / 256.f;

            if (_thresh[i] > _mdiff[i] + 1)
            {
                switch (vi.ComponentSize())
                {
                    case 1: filter = (fp) ? &TTempSmooth::filterI<uint8_t, true, true> : &TTempSmooth::filterI<uint8_t, true, false>; break;
                    case 2: filter = (fp) ? &TTempSmooth::filterI<uint16_t, true, true> : &TTempSmooth::filterI<uint16_t, true, false>; break;
                    default: filter = (fp) ? &TTempSmooth::filterI<float, true, true> : &TTempSmooth::filterI<float, true, false>;
                }
            }
            else
            {
                switch (vi.ComponentSize())
                {
                    case 1: filter = (fp) ? &TTempSmooth::filterI<uint8_t, false, true> : &TTempSmooth::filterI<uint8_t, false, false>; break;
                    case 2: filter = (fp) ? &TTempSmooth::filterI<uint16_t, false, true> : &TTempSmooth::filterI<uint16_t, false, false>; break;
                    default: filter = (fp) ? &TTempSmooth::filterI<float, false, true> : &TTempSmooth::filterI<float, false, false>;
                }
            }
        }
    }
}

template <bool pfclip>
PVideoFrame __stdcall TTempSmooth<pfclip>::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src[15] = {};
    PVideoFrame pf[15] = {};
    PVideoFrame srcc{ child->GetFrame(n, env) };
    PVideoFrame srcp{ child->GetFrame(n - 1, env) };
    PVideoFrame srcn{ child->GetFrame(n + 1, env) };

    for (int i{ n - _maxr }; i <= n + _maxr; ++i)
    {
        const int frameNumber{ std::clamp(i, 0, vi.num_frames - 1) };

        src[i - n + _maxr] = child->GetFrame(frameNumber, env);

        if constexpr (pfclip)
            pf[i - n + _maxr] = _pfclip->GetFrame(frameNumber, env);
    }

    PVideoFrame dst{ (has_at_least_v8) ? env->NewVideoFrameP(vi, &srcc) : env->NewVideoFrame(vi) };

    int fromFrame{ -1 };
    int toFrame{ _diameter };
    const int pixelsize{ vi.ComponentSize() };
    const int bits_per_pixel{ vi.BitsPerComponent() };

    if (_scthresh)
    {
        for (int i{ _maxr }; i > 0; --i)
        {
            if (ComparePlane(srcc, srcp, pixelsize, bits_per_pixel, env) > _scthresh / 100.f)
                fromFrame = i;
            break;
        }

        for (int i{ _maxr }; i < _diameter - 1; ++i)
        {
            if (ComparePlane(srcc, srcn, pixelsize, bits_per_pixel, env) > _scthresh / 100.f)
                toFrame = i;
            break;
        }
    }

    int planes_y[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    for (int i{ 0 }; i < std::min(vi.NumComponents(), 3); ++i)
    {
        if (proccesplanes[i] == 3)
            (this->*filter)(src, (pfclip) ? pf : src, dst, fromFrame, toFrame, planes_y[i], srcc->GetPitch(planes_y[i]));
        else if (proccesplanes[i] == 2)
            env->BitBlt(dst->GetWritePtr(planes_y[i]), dst->GetPitch(planes_y[i]), srcc->GetReadPtr(planes_y[i]), srcc->GetPitch(planes_y[i]), srcc->GetRowSize(planes_y[i]), srcc->GetHeight(planes_y[i]));
    }

    return dst;
}

AVSValue __cdecl Create_TTempSmooth(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    enum { Clip, Maxr, Ythresh, Uthresh, Vthresh, Ymdiff, Umdiff, Vmdiff, Strength, Scthresh, Fp, Y, U, V, Pfclip };

    PClip pfclip{ (args[Pfclip].Defined() ? args[Pfclip].AsClip() : nullptr) };

    if (pfclip)
        return new TTempSmooth<true>(
            args[Clip].AsClip(),
            args[Maxr].AsInt(3),
            args[Ythresh].AsInt(4),
            args[Uthresh].AsInt(5),
            args[Vthresh].AsInt(5),
            args[Ymdiff].AsInt(2),
            args[Umdiff].AsInt(3),
            args[Vmdiff].AsInt(3),
            args[Strength].AsInt(2),
            args[Scthresh].AsFloatf(12),
            args[Fp].AsBool(true),
            args[Y].AsInt(3),
            args[U].AsInt(3),
            args[V].AsInt(3),
            pfclip,
            env);
    else
        return new TTempSmooth<false>(
            args[Clip].AsClip(),
            args[Maxr].AsInt(3),
            args[Ythresh].AsInt(4),
            args[Uthresh].AsInt(5),
            args[Vthresh].AsInt(5),
            args[Ymdiff].AsInt(2),
            args[Umdiff].AsInt(3),
            args[Vmdiff].AsInt(3),
            args[Strength].AsInt(2),
            args[Scthresh].AsFloatf(12),
            args[Fp].AsBool(true),
            args[Y].AsInt(3),
            args[U].AsInt(3),
            args[V].AsInt(3),
            pfclip,
            env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("vsTTempSmooth", "c[maxr]i[ythresh]i[uthresh]i[vthresh]i[ymdiff]i[umdiff]i[vmdiff]i[strength]i[scthresh]f[fp]b[y]i[u]i[v]i[pfclip]c", Create_TTempSmooth, 0);
    return "vsTTempSmooth";
}
