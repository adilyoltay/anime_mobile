#include "utils/no_op_factory.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/refcnt.hpp"
#include "utils/lite_rtti.hpp"

#include <cstdint>
#include <utility>
#include <vector>

using namespace rive;

namespace
{
class NoOpRenderBuffer :
    public LITE_RTTI_OVERRIDE(RenderBuffer, NoOpRenderBuffer)
{
public:
    NoOpRenderBuffer(RenderBufferType type,
                     RenderBufferFlags flags,
                     size_t sizeInBytes) :
        LITE_RTTI_OVERRIDE(RenderBuffer, NoOpRenderBuffer)(type,
                                                           flags,
                                                           sizeInBytes),
        m_storage(sizeInBytes)
    {}

private:
    void* onMap() override { return m_storage.data(); }
    void onUnmap() override {}

    std::vector<uint8_t> m_storage;
};

class NoOpRenderShader :
    public LITE_RTTI_OVERRIDE(RenderShader, NoOpRenderShader)
{
public:
    enum class Kind
    {
        linear,
        radial,
    };

    NoOpRenderShader(Kind kind,
                     std::vector<ColorInt> colors,
                     std::vector<float> stops,
                     float p0,
                     float p1,
                     float p2,
                     float p3) :
        m_kind(kind),
        m_colors(std::move(colors)),
        m_stops(std::move(stops)),
        m_p0(p0),
        m_p1(p1),
        m_p2(p2),
        m_p3(p3)
    {}

private:
    Kind m_kind;
    std::vector<ColorInt> m_colors;
    std::vector<float> m_stops;
    float m_p0 = 0.0f;
    float m_p1 = 0.0f;
    float m_p2 = 0.0f;
    float m_p3 = 0.0f;
};

class NoOpRenderPaint :
    public LITE_RTTI_OVERRIDE(RenderPaint, NoOpRenderPaint)
{
public:
    void color(unsigned int value) override { m_color = value; }
    void style(RenderPaintStyle value) override { m_style = value; }
    void thickness(float value) override { m_thickness = value; }
    void join(StrokeJoin value) override { m_join = value; }
    void cap(StrokeCap value) override { m_cap = value; }
    void blendMode(BlendMode value) override { m_blendMode = value; }
    void shader(rcp<RenderShader> shader) override { m_shader = std::move(shader); }
    void invalidateStroke() override { m_strokeRevision++; }
    void feather(float value) override { m_feather = value; }

private:
    RenderPaintStyle m_style = RenderPaintStyle::fill;
    unsigned int m_color = 0xff000000;
    float m_thickness = 1.0f;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    BlendMode m_blendMode = BlendMode::srcOver;
    rcp<RenderShader> m_shader;
    uint32_t m_strokeRevision = 0;
    float m_feather = 0.0f;
};

class NoOpRenderPath :
    public LITE_RTTI_OVERRIDE(RenderPath, NoOpRenderPath)
{
public:
    void rewind() override { m_path.rewind(); }

    void fillRule(FillRule value) override { m_fillRule = value; }

    void addRenderPath(RenderPath* path, const Mat2D& transform) override
    {
        if (path == nullptr)
        {
            return;
        }
        if (auto other = lite_rtti_cast<NoOpRenderPath*>(path))
        {
            m_path.addPath(other->m_path, &transform);
        }
    }

    void addRenderPathBackwards(RenderPath* path,
                                const Mat2D& transform) override
    {
        if (auto other = lite_rtti_cast<NoOpRenderPath*>(path))
        {
            m_path.addPathBackwards(other->m_path, &transform);
        }
    }

    void addRawPath(const RawPath& path) override { m_path.addPath(path); }

    void moveTo(float x, float y) override { m_path.moveTo(x, y); }
    void lineTo(float x, float y) override { m_path.lineTo(x, y); }
    void cubicTo(float ox,
                 float oy,
                 float ix,
                 float iy,
                 float x,
                 float y) override
    {
        m_path.cubicTo(ox, oy, ix, iy, x, y);
    }
    void close() override { m_path.close(); }

private:
    RawPath m_path;
    FillRule m_fillRule = FillRule::nonZero;
};

class NoOpRenderImage :
    public LITE_RTTI_OVERRIDE(RenderImage, NoOpRenderImage)
{
public:
    NoOpRenderImage(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes))
    {
        m_Width = 0;
        m_Height = 0;
    }

private:
    std::vector<uint8_t> m_bytes;
};
} // namespace

rcp<RenderBuffer> NoOpFactory::makeRenderBuffer(RenderBufferType type,
                                                RenderBufferFlags flags,
                                                size_t sizeInBytes)
{
    return make_rcp<NoOpRenderBuffer>(type, flags, sizeInBytes);
}

rcp<RenderShader> NoOpFactory::makeLinearGradient(
    float sx,
    float sy,
    float ex,
    float ey,
    const ColorInt colors[],
    const float stops[],
    size_t count)
{
    std::vector<ColorInt> colorVec(colors, colors + count);
    std::vector<float> stopVec(stops, stops + count);
    return make_rcp<NoOpRenderShader>(NoOpRenderShader::Kind::linear,
                                      std::move(colorVec),
                                      std::move(stopVec),
                                      sx,
                                      sy,
                                      ex,
                                      ey);
}

rcp<RenderShader> NoOpFactory::makeRadialGradient(
    float cx,
    float cy,
    float radius,
    const ColorInt colors[],
    const float stops[],
    size_t count)
{
    std::vector<ColorInt> colorVec(colors, colors + count);
    std::vector<float> stopVec(stops, stops + count);
    return make_rcp<NoOpRenderShader>(NoOpRenderShader::Kind::radial,
                                      std::move(colorVec),
                                      std::move(stopVec),
                                      cx,
                                      cy,
                                      radius,
                                      0.0f);
}

rcp<RenderPath> NoOpFactory::makeRenderPath(RawPath& path, FillRule rule)
{
    auto renderPath = make_rcp<NoOpRenderPath>();
    renderPath->fillRule(rule);
    renderPath->addRawPath(path);
    return renderPath;
}

rcp<RenderPath> NoOpFactory::makeEmptyRenderPath()
{
    return make_rcp<NoOpRenderPath>();
}

rcp<RenderPaint> NoOpFactory::makeRenderPaint()
{
    return make_rcp<NoOpRenderPaint>();
}

rcp<RenderImage> NoOpFactory::decodeImage(Span<const uint8_t> bytes)
{
    return make_rcp<NoOpRenderImage>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()));
}
