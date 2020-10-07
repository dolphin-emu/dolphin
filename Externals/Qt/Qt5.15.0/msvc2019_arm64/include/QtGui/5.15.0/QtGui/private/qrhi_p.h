/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Gui module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QRHI_H
#define QRHI_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/qtguiglobal.h>
#include <QSize>
#include <QMatrix4x4>
#include <QVector>
#include <QVarLengthArray>
#include <QThread>
#include <QColor>
#include <QImage>
#include <functional>
#include <array>
#include <private/qshader_p.h>

QT_BEGIN_NAMESPACE

class QWindow;
class QRhiImplementation;
class QRhiBuffer;
class QRhiRenderBuffer;
class QRhiTexture;
class QRhiSampler;
class QRhiCommandBuffer;
class QRhiResourceUpdateBatch;
class QRhiResourceUpdateBatchPrivate;
class QRhiProfiler;

class Q_GUI_EXPORT QRhiDepthStencilClearValue
{
public:
    QRhiDepthStencilClearValue() = default;
    QRhiDepthStencilClearValue(float d, quint32 s);

    float depthClearValue() const { return m_d; }
    void setDepthClearValue(float d) { m_d = d; }

    quint32 stencilClearValue() const { return m_s; }
    void setStencilClearValue(quint32 s) { m_s = s; }

private:
    float m_d = 1.0f;
    quint32 m_s = 0;
};

Q_DECLARE_TYPEINFO(QRhiDepthStencilClearValue, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiDepthStencilClearValue &a, const QRhiDepthStencilClearValue &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiDepthStencilClearValue &a, const QRhiDepthStencilClearValue &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiDepthStencilClearValue &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiDepthStencilClearValue &);
#endif

class Q_GUI_EXPORT QRhiViewport
{
public:
    QRhiViewport() = default;
    QRhiViewport(float x, float y, float w, float h, float minDepth = 0.0f, float maxDepth = 1.0f);

    std::array<float, 4> viewport() const { return m_rect; }
    void setViewport(float x, float y, float w, float h) {
        m_rect[0] = x; m_rect[1] = y; m_rect[2] = w; m_rect[3] = h;
    }

    float minDepth() const { return m_minDepth; }
    void setMinDepth(float minDepth) { m_minDepth = minDepth; }

    float maxDepth() const { return m_maxDepth; }
    void setMaxDepth(float maxDepth) { m_maxDepth = maxDepth; }

private:
    std::array<float, 4> m_rect { { 0.0f, 0.0f, 0.0f, 0.0f } };
    float m_minDepth = 0.0f;
    float m_maxDepth = 1.0f;
};

Q_DECLARE_TYPEINFO(QRhiViewport, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiViewport &a, const QRhiViewport &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiViewport &a, const QRhiViewport &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiViewport &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiViewport &);
#endif

class Q_GUI_EXPORT QRhiScissor
{
public:
    QRhiScissor() = default;
    QRhiScissor(int x, int y, int w, int h);

    std::array<int, 4> scissor() const { return m_rect; }
    void setScissor(int x, int y, int w, int h) {
        m_rect[0] = x; m_rect[1] = y; m_rect[2] = w; m_rect[3] = h;
    }

private:
    std::array<int, 4> m_rect { { 0, 0, 0, 0 } };
};

Q_DECLARE_TYPEINFO(QRhiScissor, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiScissor &a, const QRhiScissor &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiScissor &a, const QRhiScissor &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiScissor &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiScissor &);
#endif

class Q_GUI_EXPORT QRhiVertexInputBinding
{
public:
    enum Classification {
        PerVertex,
        PerInstance
    };

    QRhiVertexInputBinding() = default;
    QRhiVertexInputBinding(quint32 stride, Classification cls = PerVertex, int stepRate = 1);

    quint32 stride() const { return m_stride; }
    void setStride(quint32 s) { m_stride = s; }

    Classification classification() const { return m_classification; }
    void setClassification(Classification c) { m_classification = c; }

    int instanceStepRate() const { return m_instanceStepRate; }
    void setInstanceStepRate(int rate) { m_instanceStepRate = rate; }

private:
    quint32 m_stride = 0;
    Classification m_classification = PerVertex;
    int m_instanceStepRate = 1;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputBinding, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiVertexInputBinding &a, const QRhiVertexInputBinding &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiVertexInputBinding &a, const QRhiVertexInputBinding &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiVertexInputBinding &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputBinding &);
#endif

class Q_GUI_EXPORT QRhiVertexInputAttribute
{
public:
    enum Format {
        Float4,
        Float3,
        Float2,
        Float,
        UNormByte4,
        UNormByte2,
        UNormByte
    };

    QRhiVertexInputAttribute() = default;
    QRhiVertexInputAttribute(int binding, int location, Format format, quint32 offset);

    int binding() const { return m_binding; }
    void setBinding(int b) { m_binding = b; }

    int location() const { return m_location; }
    void setLocation(int loc) { m_location = loc; }

    Format format() const { return m_format; }
    void setFormt(Format f) { m_format = f; }

    quint32 offset() const { return m_offset; }
    void setOffset(quint32 ofs) { m_offset = ofs; }

private:
    int m_binding = 0;
    int m_location = 0;
    Format m_format = Float4;
    quint32 m_offset = 0;
};

Q_DECLARE_TYPEINFO(QRhiVertexInputAttribute, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiVertexInputAttribute &a, const QRhiVertexInputAttribute &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiVertexInputAttribute &a, const QRhiVertexInputAttribute &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiVertexInputAttribute &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputAttribute &);
#endif

class Q_GUI_EXPORT QRhiVertexInputLayout
{
public:
    QRhiVertexInputLayout() = default;

    void setBindings(std::initializer_list<QRhiVertexInputBinding> list) { m_bindings = list; }
    template<typename InputIterator>
    void setBindings(InputIterator first, InputIterator last)
    {
        m_bindings.clear();
        std::copy(first, last, std::back_inserter(m_bindings));
    }
    const QRhiVertexInputBinding *cbeginBindings() const { return m_bindings.cbegin(); }
    const QRhiVertexInputBinding *cendBindings() const { return m_bindings.cend(); }
    const QRhiVertexInputBinding *bindingAt(int index) const { return &m_bindings.at(index); }

    void setAttributes(std::initializer_list<QRhiVertexInputAttribute> list) { m_attributes = list; }
    template<typename InputIterator>
    void setAttributes(InputIterator first, InputIterator last)
    {
        m_attributes.clear();
        std::copy(first, last, std::back_inserter(m_attributes));
    }
    const QRhiVertexInputAttribute *cbeginAttributes() const { return m_attributes.cbegin(); }
    const QRhiVertexInputAttribute *cendAttributes() const { return m_attributes.cend(); }

private:
    QVarLengthArray<QRhiVertexInputBinding, 8> m_bindings;
    QVarLengthArray<QRhiVertexInputAttribute, 8> m_attributes;

    friend Q_GUI_EXPORT bool operator==(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW;
    friend Q_GUI_EXPORT uint qHash(const QRhiVertexInputLayout &v, uint seed) Q_DECL_NOTHROW;
    friend Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputLayout &);
};

Q_DECLARE_TYPEINFO(QRhiVertexInputLayout, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiVertexInputLayout &a, const QRhiVertexInputLayout &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiVertexInputLayout &v, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiVertexInputLayout &);
#endif

class Q_GUI_EXPORT QRhiShaderStage
{
public:
    enum Type {
        Vertex,
        Fragment,
        Compute
    };

    QRhiShaderStage() = default;
    QRhiShaderStage(Type type, const QShader &shader,
                    QShader::Variant v = QShader::StandardShader);

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    QShader shader() const { return m_shader; }
    void setShader(const QShader &s) { m_shader = s; }

    QShader::Variant shaderVariant() const { return m_shaderVariant; }
    void setShaderVariant(QShader::Variant v) { m_shaderVariant = v; }

private:
    Type m_type = Vertex;
    QShader m_shader;
    QShader::Variant m_shaderVariant = QShader::StandardShader;
};

Q_DECLARE_TYPEINFO(QRhiShaderStage, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiShaderStage &a, const QRhiShaderStage &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiShaderStage &a, const QRhiShaderStage &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiShaderStage &s, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiShaderStage &);
#endif

using QRhiGraphicsShaderStage = QRhiShaderStage;

class Q_GUI_EXPORT QRhiShaderResourceBinding
{
public:
    enum Type {
        UniformBuffer,
        SampledTexture,
        ImageLoad,
        ImageStore,
        ImageLoadStore,
        BufferLoad,
        BufferStore,
        BufferLoadStore
    };

    enum StageFlag {
        VertexStage = 1 << 0,
        FragmentStage = 1 << 1,
        ComputeStage = 1 << 2
    };
    Q_DECLARE_FLAGS(StageFlags, StageFlag)

    QRhiShaderResourceBinding();

    bool isLayoutCompatible(const QRhiShaderResourceBinding &other) const;

    static QRhiShaderResourceBinding uniformBuffer(int binding, StageFlags stage, QRhiBuffer *buf);
    static QRhiShaderResourceBinding uniformBuffer(int binding, StageFlags stage, QRhiBuffer *buf, int offset, int size);
    static QRhiShaderResourceBinding uniformBufferWithDynamicOffset(int binding, StageFlags stage, QRhiBuffer *buf, int size);

    static QRhiShaderResourceBinding sampledTexture(int binding, StageFlags stage, QRhiTexture *tex, QRhiSampler *sampler);

    struct TextureAndSampler {
        QRhiTexture *tex;
        QRhiSampler *sampler;
    };
    static QRhiShaderResourceBinding sampledTextures(int binding, StageFlags stage, int count, const TextureAndSampler *texSamplers);

    static QRhiShaderResourceBinding imageLoad(int binding, StageFlags stage, QRhiTexture *tex, int level);
    static QRhiShaderResourceBinding imageStore(int binding, StageFlags stage, QRhiTexture *tex, int level);
    static QRhiShaderResourceBinding imageLoadStore(int binding, StageFlags stage, QRhiTexture *tex, int level);

    static QRhiShaderResourceBinding bufferLoad(int binding, StageFlags stage, QRhiBuffer *buf);
    static QRhiShaderResourceBinding bufferLoad(int binding, StageFlags stage, QRhiBuffer *buf, int offset, int size);
    static QRhiShaderResourceBinding bufferStore(int binding, StageFlags stage, QRhiBuffer *buf);
    static QRhiShaderResourceBinding bufferStore(int binding, StageFlags stage, QRhiBuffer *buf, int offset, int size);
    static QRhiShaderResourceBinding bufferLoadStore(int binding, StageFlags stage, QRhiBuffer *buf);
    static QRhiShaderResourceBinding bufferLoadStore(int binding, StageFlags stage, QRhiBuffer *buf, int offset, int size);

    struct Data
    {
        int binding;
        QRhiShaderResourceBinding::StageFlags stage;
        QRhiShaderResourceBinding::Type type;
        struct UniformBufferData {
            QRhiBuffer *buf;
            int offset;
            int maybeSize;
            bool hasDynamicOffset;
        };
        static const int MAX_TEX_SAMPLER_ARRAY_SIZE = 16;
        struct SampledTextureData {
            int count;
            TextureAndSampler texSamplers[MAX_TEX_SAMPLER_ARRAY_SIZE];
        };
        struct StorageImageData {
            QRhiTexture *tex;
            int level;
        };
        struct StorageBufferData {
            QRhiBuffer *buf;
            int offset;
            int maybeSize;
        };
        union {
            UniformBufferData ubuf;
            SampledTextureData stex;
            StorageImageData simage;
            StorageBufferData sbuf;
        } u;
    };

    Data *data() { return &d; }
    const Data *data() const { return &d; }

private:
    Data d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiShaderResourceBinding::StageFlags)

Q_DECLARE_TYPEINFO(QRhiShaderResourceBinding, Q_MOVABLE_TYPE);

Q_GUI_EXPORT bool operator==(const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT bool operator!=(const QRhiShaderResourceBinding &a, const QRhiShaderResourceBinding &b) Q_DECL_NOTHROW;
Q_GUI_EXPORT uint qHash(const QRhiShaderResourceBinding &b, uint seed = 0) Q_DECL_NOTHROW;
#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBinding &);
#endif

class Q_GUI_EXPORT QRhiColorAttachment
{
public:
    QRhiColorAttachment() = default;
    QRhiColorAttachment(QRhiTexture *texture);
    QRhiColorAttachment(QRhiRenderBuffer *renderBuffer);

    QRhiTexture *texture() const { return m_texture; }
    void setTexture(QRhiTexture *tex) { m_texture = tex; }

    QRhiRenderBuffer *renderBuffer() const { return m_renderBuffer; }
    void setRenderBuffer(QRhiRenderBuffer *rb) { m_renderBuffer = rb; }

    int layer() const { return m_layer; }
    void setLayer(int layer) { m_layer = layer; }

    int level() const { return m_level; }
    void setLevel(int level) { m_level = level; }

    QRhiTexture *resolveTexture() const { return m_resolveTexture; }
    void setResolveTexture(QRhiTexture *tex) { m_resolveTexture = tex; }

    int resolveLayer() const { return m_resolveLayer; }
    void setResolveLayer(int layer) { m_resolveLayer = layer; }

    int resolveLevel() const { return m_resolveLevel; }
    void setResolveLevel(int level) { m_resolveLevel = level; }

private:
    QRhiTexture *m_texture = nullptr;
    QRhiRenderBuffer *m_renderBuffer = nullptr;
    int m_layer = 0;
    int m_level = 0;
    QRhiTexture *m_resolveTexture = nullptr;
    int m_resolveLayer = 0;
    int m_resolveLevel = 0;
};

Q_DECLARE_TYPEINFO(QRhiColorAttachment, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiTextureRenderTargetDescription
{
public:
    QRhiTextureRenderTargetDescription() = default;
    QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment);
    QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment, QRhiRenderBuffer *depthStencilBuffer);
    QRhiTextureRenderTargetDescription(const QRhiColorAttachment &colorAttachment, QRhiTexture *depthTexture);

    void setColorAttachments(std::initializer_list<QRhiColorAttachment> list) { m_colorAttachments = list; }
    template<typename InputIterator>
    void setColorAttachments(InputIterator first, InputIterator last)
    {
        m_colorAttachments.clear();
        std::copy(first, last, std::back_inserter(m_colorAttachments));
    }
    const QRhiColorAttachment *cbeginColorAttachments() const { return m_colorAttachments.cbegin(); }
    const QRhiColorAttachment *cendColorAttachments() const { return m_colorAttachments.cend(); }
    const QRhiColorAttachment *colorAttachmentAt(int index) const { return &m_colorAttachments.at(index); }

    QRhiRenderBuffer *depthStencilBuffer() const { return m_depthStencilBuffer; }
    void setDepthStencilBuffer(QRhiRenderBuffer *renderBuffer) { m_depthStencilBuffer = renderBuffer; }

    QRhiTexture *depthTexture() const { return m_depthTexture; }
    void setDepthTexture(QRhiTexture *texture) { m_depthTexture = texture; }

private:
    QVarLengthArray<QRhiColorAttachment, 8> m_colorAttachments;
    QRhiRenderBuffer *m_depthStencilBuffer = nullptr;
    QRhiTexture *m_depthTexture = nullptr;
};

Q_DECLARE_TYPEINFO(QRhiTextureRenderTargetDescription, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiTextureSubresourceUploadDescription
{
public:
    QRhiTextureSubresourceUploadDescription() = default;
    QRhiTextureSubresourceUploadDescription(const QImage &image);
    QRhiTextureSubresourceUploadDescription(const void *data, int size);

    QImage image() const { return m_image; }
    void setImage(const QImage &image) { m_image = image; }

    QByteArray data() const { return m_data; }
    void setData(const QByteArray &data) { m_data = data; }

    QPoint destinationTopLeft() const { return m_destinationTopLeft; }
    void setDestinationTopLeft(const QPoint &p) { m_destinationTopLeft = p; }

    QSize sourceSize() const { return m_sourceSize; }
    void setSourceSize(const QSize &size) { m_sourceSize = size; }

    QPoint sourceTopLeft() const { return m_sourceTopLeft; }
    void setSourceTopLeft(const QPoint &p) { m_sourceTopLeft = p; }

private:
    QImage m_image;
    QByteArray m_data;
    QPoint m_destinationTopLeft;
    QSize m_sourceSize;
    QPoint m_sourceTopLeft;
};

Q_DECLARE_TYPEINFO(QRhiTextureSubresourceUploadDescription, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiTextureUploadEntry
{
public:
    QRhiTextureUploadEntry() = default;
    QRhiTextureUploadEntry(int layer, int level, const QRhiTextureSubresourceUploadDescription &desc);

    int layer() const { return m_layer; }
    void setLayer(int layer) { m_layer = layer; }

    int level() const { return m_level; }
    void setLevel(int level) { m_level = level; }

    QRhiTextureSubresourceUploadDescription description() const { return m_desc; }
    void setDescription(const QRhiTextureSubresourceUploadDescription &desc) { m_desc = desc; }

private:
    int m_layer = 0;
    int m_level = 0;
    QRhiTextureSubresourceUploadDescription m_desc;
};

Q_DECLARE_TYPEINFO(QRhiTextureUploadEntry, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiTextureUploadDescription
{
public:
    QRhiTextureUploadDescription() = default;
    QRhiTextureUploadDescription(const QRhiTextureUploadEntry &entry);
    QRhiTextureUploadDescription(std::initializer_list<QRhiTextureUploadEntry> list);

    void setEntries(std::initializer_list<QRhiTextureUploadEntry> list) { m_entries = list; }
    template<typename InputIterator>
    void setEntries(InputIterator first, InputIterator last)
    {
        m_entries.clear();
        std::copy(first, last, std::back_inserter(m_entries));
    }
    const QRhiTextureUploadEntry *cbeginEntries() const { return m_entries.cbegin(); }
    const QRhiTextureUploadEntry *cendEntries() const { return m_entries.cend(); }

private:
    QVarLengthArray<QRhiTextureUploadEntry, 16> m_entries;
};

Q_DECLARE_TYPEINFO(QRhiTextureUploadDescription, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiTextureCopyDescription
{
public:
    QRhiTextureCopyDescription() = default;

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    int sourceLayer() const { return m_sourceLayer; }
    void setSourceLayer(int layer) { m_sourceLayer = layer; }

    int sourceLevel() const { return m_sourceLevel; }
    void setSourceLevel(int level) { m_sourceLevel = level; }

    QPoint sourceTopLeft() const { return m_sourceTopLeft; }
    void setSourceTopLeft(const QPoint &p) { m_sourceTopLeft = p; }

    int destinationLayer() const { return m_destinationLayer; }
    void setDestinationLayer(int layer) { m_destinationLayer = layer; }

    int destinationLevel() const { return m_destinationLevel; }
    void setDestinationLevel(int level) { m_destinationLevel = level; }

    QPoint destinationTopLeft() const { return m_destinationTopLeft; }
    void setDestinationTopLeft(const QPoint &p) { m_destinationTopLeft = p; }

private:
    QSize m_pixelSize;
    int m_sourceLayer = 0;
    int m_sourceLevel = 0;
    QPoint m_sourceTopLeft;
    int m_destinationLayer = 0;
    int m_destinationLevel = 0;
    QPoint m_destinationTopLeft;
};

Q_DECLARE_TYPEINFO(QRhiTextureCopyDescription, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiReadbackDescription
{
public:
    QRhiReadbackDescription() = default;
    QRhiReadbackDescription(QRhiTexture *texture);

    QRhiTexture *texture() const { return m_texture; }
    void setTexture(QRhiTexture *tex) { m_texture = tex; }

    int layer() const { return m_layer; }
    void setLayer(int layer) { m_layer = layer; }

    int level() const { return m_level; }
    void setLevel(int level) { m_level = level; }

private:
    QRhiTexture *m_texture = nullptr;
    int m_layer = 0;
    int m_level = 0;
};

Q_DECLARE_TYPEINFO(QRhiReadbackDescription, Q_MOVABLE_TYPE);

struct Q_GUI_EXPORT QRhiNativeHandles
{
};

class Q_GUI_EXPORT QRhiResource
{
public:
    enum Type {
        Buffer,
        Texture,
        Sampler,
        RenderBuffer,
        RenderPassDescriptor,
        RenderTarget,
        TextureRenderTarget,
        ShaderResourceBindings,
        GraphicsPipeline,
        SwapChain,
        ComputePipeline,
        CommandBuffer
    };

    virtual ~QRhiResource();

    virtual Type resourceType() const = 0;

    virtual void release() = 0;
    void releaseAndDestroyLater();

    QByteArray name() const;
    void setName(const QByteArray &name);

    quint64 globalResourceId() const;

protected:
    QRhiResource(QRhiImplementation *rhi);
    Q_DISABLE_COPY(QRhiResource)
    friend class QRhiImplementation;
    QRhiImplementation *m_rhi = nullptr;
    quint64 m_id;
    QByteArray m_objectName;
};

class Q_GUI_EXPORT QRhiBuffer : public QRhiResource
{
public:
    enum Type {
        Immutable,
        Static,
        Dynamic
    };

    enum UsageFlag {
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        UniformBuffer = 1 << 2,
        StorageBuffer = 1 << 3
    };
    Q_DECLARE_FLAGS(UsageFlags, UsageFlag)

    struct NativeBuffer {
        const void *objects[3];
        int slotCount;
    };

    QRhiResource::Type resourceType() const override;

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    UsageFlags usage() const { return m_usage; }
    void setUsage(UsageFlags u) { m_usage = u; }

    int size() const { return m_size; }
    void setSize(int sz) { m_size = sz; }

    virtual bool build() = 0;

    virtual NativeBuffer nativeBuffer();

protected:
    QRhiBuffer(QRhiImplementation *rhi, Type type_, UsageFlags usage_, int size_);
    Type m_type;
    UsageFlags m_usage;
    int m_size;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiBuffer::UsageFlags)

class Q_GUI_EXPORT QRhiTexture : public QRhiResource
{
public:
    enum Flag {
        RenderTarget = 1 << 0,
        CubeMap = 1 << 2,
        MipMapped = 1 << 3,
        sRGB = 1 << 4,
        UsedAsTransferSource = 1 << 5,
        UsedWithGenerateMips = 1 << 6,
        UsedWithLoadStore = 1 << 7
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Format {
        UnknownFormat,

        RGBA8,
        BGRA8,
        R8,
        R16,
        RED_OR_ALPHA8,

        RGBA16F,
        RGBA32F,
        R16F,
        R32F,

        D16,
        D32F,

        BC1,
        BC2,
        BC3,
        BC4,
        BC5,
        BC6H,
        BC7,

        ETC2_RGB8,
        ETC2_RGB8A1,
        ETC2_RGBA8,

        ASTC_4x4,
        ASTC_5x4,
        ASTC_5x5,
        ASTC_6x5,
        ASTC_6x6,
        ASTC_8x5,
        ASTC_8x6,
        ASTC_8x8,
        ASTC_10x5,
        ASTC_10x6,
        ASTC_10x8,
        ASTC_10x10,
        ASTC_12x10,
        ASTC_12x12
    };

    struct NativeTexture {
        const void *object;
        int layout;
    };

    QRhiResource::Type resourceType() const override;

    Format format() const { return m_format; }
    void setFormat(Format fmt) { m_format = fmt; }

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    virtual bool build() = 0;
    virtual NativeTexture nativeTexture();
    virtual bool buildFrom(NativeTexture src);
    virtual void setNativeLayout(int layout);

protected:
    QRhiTexture(QRhiImplementation *rhi, Format format_, const QSize &pixelSize_,
                int sampleCount_, Flags flags_);
    Format m_format;
    QSize m_pixelSize;
    int m_sampleCount;
    Flags m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTexture::Flags)

class Q_GUI_EXPORT QRhiSampler : public QRhiResource
{
public:
    enum Filter {
        None,
        Nearest,
        Linear
    };

    enum AddressMode {
        Repeat,
        ClampToEdge,
        Mirror,
    };

    enum CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    QRhiResource::Type resourceType() const override;

    Filter magFilter() const { return m_magFilter; }
    void setMagFilter(Filter f) { m_magFilter = f; }

    Filter minFilter() const { return m_minFilter; }
    void setMinFilter(Filter f) { m_minFilter = f; }

    Filter mipmapMode() const { return m_mipmapMode; }
    void setMipmapMode(Filter f) { m_mipmapMode = f; }

    AddressMode addressU() const { return m_addressU; }
    void setAddressU(AddressMode mode) { m_addressU = mode; }

    AddressMode addressV() const { return m_addressV; }
    void setAddressV(AddressMode mode) { m_addressV = mode; }

    AddressMode addressW() const { return m_addressW; }
    void setAddressW(AddressMode mode) { m_addressW = mode; }

    CompareOp textureCompareOp() const { return m_compareOp; }
    void setTextureCompareOp(CompareOp op) { m_compareOp = op; }

    virtual bool build() = 0;

protected:
    QRhiSampler(QRhiImplementation *rhi,
                Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                AddressMode u_, AddressMode v_, AddressMode w_);
    Filter m_magFilter;
    Filter m_minFilter;
    Filter m_mipmapMode;
    AddressMode m_addressU;
    AddressMode m_addressV;
    AddressMode m_addressW;
    CompareOp m_compareOp;
};

class Q_GUI_EXPORT QRhiRenderBuffer : public QRhiResource
{
public:
    enum Type {
        DepthStencil,
        Color
    };

    enum Flag {
        UsedWithSwapChainOnly = 1 << 0
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QRhiResource::Type resourceType() const override;

    Type type() const { return m_type; }
    void setType(Type t) { m_type = t; }

    QSize pixelSize() const { return m_pixelSize; }
    void setPixelSize(const QSize &sz) { m_pixelSize = sz; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags h) { m_flags = h; }

    virtual bool build() = 0;

    virtual QRhiTexture::Format backingFormat() const = 0;

protected:
    QRhiRenderBuffer(QRhiImplementation *rhi, Type type_, const QSize &pixelSize_,
                     int sampleCount_, Flags flags_);
    Type m_type;
    QSize m_pixelSize;
    int m_sampleCount;
    Flags m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiRenderBuffer::Flags)

class Q_GUI_EXPORT QRhiRenderPassDescriptor : public QRhiResource
{
public:
    QRhiResource::Type resourceType() const override;

    virtual bool isCompatible(const QRhiRenderPassDescriptor *other) const = 0;
    virtual const QRhiNativeHandles *nativeHandles();

protected:
    QRhiRenderPassDescriptor(QRhiImplementation *rhi);
};

class Q_GUI_EXPORT QRhiRenderTarget : public QRhiResource
{
public:
    QRhiResource::Type resourceType() const override;

    virtual QSize pixelSize() const = 0;
    virtual float devicePixelRatio() const = 0;
    virtual int sampleCount() const = 0;

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

protected:
    QRhiRenderTarget(QRhiImplementation *rhi);
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
};

class Q_GUI_EXPORT QRhiTextureRenderTarget : public QRhiRenderTarget
{
public:
    enum Flag {
        PreserveColorContents = 1 << 0,
        PreserveDepthStencilContents = 1 << 1
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QRhiResource::Type resourceType() const override;

    QRhiTextureRenderTargetDescription description() const { return m_desc; }
    void setDescription(const QRhiTextureRenderTargetDescription &desc) { m_desc = desc; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    virtual QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() = 0;

    virtual bool build() = 0;

protected:
    QRhiTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc_, Flags flags_);
    QRhiTextureRenderTargetDescription m_desc;
    Flags m_flags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiTextureRenderTarget::Flags)

class Q_GUI_EXPORT QRhiShaderResourceBindings : public QRhiResource
{
public:
    QRhiResource::Type resourceType() const override;

    void setBindings(std::initializer_list<QRhiShaderResourceBinding> list) { m_bindings = list; }

    template<typename InputIterator>
    void setBindings(InputIterator first, InputIterator last)
    {
        m_bindings.clear();
        std::copy(first, last, std::back_inserter(m_bindings));
    }

    const QRhiShaderResourceBinding *cbeginBindings() const { return m_bindings.cbegin(); }
    const QRhiShaderResourceBinding *cendBindings() const { return m_bindings.cend(); }

    bool isLayoutCompatible(const QRhiShaderResourceBindings *other) const;

    virtual bool build() = 0;

protected:
    QRhiShaderResourceBindings(QRhiImplementation *rhi);
    QVarLengthArray<QRhiShaderResourceBinding, 8> m_bindings;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBindings &);
#endif
};

#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QRhiShaderResourceBindings &);
#endif

class Q_GUI_EXPORT QRhiGraphicsPipeline : public QRhiResource
{
public:
    enum Flag {
        UsesBlendConstants = 1 << 0,
        UsesStencilRef = 1 << 1,
        UsesScissor = 1 << 2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum Topology {
        Triangles,
        TriangleStrip,
        TriangleFan,
        Lines,
        LineStrip,
        Points
    };

    enum CullMode {
        None,
        Front,
        Back
    };

    enum FrontFace {
        CCW,
        CW
    };

    enum ColorMaskComponent {
        R = 1 << 0,
        G = 1 << 1,
        B = 1 << 2,
        A = 1 << 3
    };
    Q_DECLARE_FLAGS(ColorMask, ColorMaskComponent)

    enum BlendFactor {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha
    };

    enum BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    struct TargetBlend {
        ColorMask colorWrite = ColorMask(0xF); // R | G | B | A
        bool enable = false;
        BlendFactor srcColor = One;
        BlendFactor dstColor = OneMinusSrcAlpha;
        BlendOp opColor = Add;
        BlendFactor srcAlpha = One;
        BlendFactor dstAlpha = OneMinusSrcAlpha;
        BlendOp opAlpha = Add;
    };

    enum CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum StencilOp {
        StencilZero,
        Keep,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };

    struct StencilOpState {
        StencilOp failOp = Keep;
        StencilOp depthFailOp = Keep;
        StencilOp passOp = Keep;
        CompareOp compareOp = Always;
    };

    QRhiResource::Type resourceType() const override;

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    Topology topology() const { return m_topology; }
    void setTopology(Topology t) { m_topology = t; }

    CullMode cullMode() const { return m_cullMode; }
    void setCullMode(CullMode mode) { m_cullMode = mode; }

    FrontFace frontFace() const { return m_frontFace; }
    void setFrontFace(FrontFace f) { m_frontFace = f; }

    void setTargetBlends(std::initializer_list<TargetBlend> list) { m_targetBlends = list; }
    template<typename InputIterator>
    void setTargetBlends(InputIterator first, InputIterator last)
    {
        m_targetBlends.clear();
        std::copy(first, last, std::back_inserter(m_targetBlends));
    }
    const TargetBlend *cbeginTargetBlends() const { return m_targetBlends.cbegin(); }
    const TargetBlend *cendTargetBlends() const { return m_targetBlends.cend(); }

    bool hasDepthTest() const { return m_depthTest; }
    void setDepthTest(bool enable) { m_depthTest = enable; }

    bool hasDepthWrite() const { return m_depthWrite; }
    void setDepthWrite(bool enable) { m_depthWrite = enable; }

    CompareOp depthOp() const { return m_depthOp; }
    void setDepthOp(CompareOp op) { m_depthOp = op; }

    bool hasStencilTest() const { return m_stencilTest; }
    void setStencilTest(bool enable) { m_stencilTest = enable; }

    StencilOpState stencilFront() const { return m_stencilFront; }
    void setStencilFront(const StencilOpState &state) { m_stencilFront = state; }

    StencilOpState stencilBack() const { return m_stencilBack; }
    void setStencilBack(const StencilOpState &state) { m_stencilBack = state; }

    quint32 stencilReadMask() const { return m_stencilReadMask; }
    void setStencilReadMask(quint32 mask) { m_stencilReadMask = mask; }

    quint32 stencilWriteMask() const { return m_stencilWriteMask; }
    void setStencilWriteMask(quint32 mask) { m_stencilWriteMask = mask; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int s) { m_sampleCount = s; }

    float lineWidth() const { return m_lineWidth; }
    void setLineWidth(float width) { m_lineWidth = width; }

    int depthBias() const { return m_depthBias; }
    void setDepthBias(int bias) { m_depthBias = bias; }

    float slopeScaledDepthBias() const { return m_slopeScaledDepthBias; }
    void setSlopeScaledDepthBias(float bias) { m_slopeScaledDepthBias = bias; }

    void setShaderStages(std::initializer_list<QRhiShaderStage> list) { m_shaderStages = list; }
    template<typename InputIterator>
    void setShaderStages(InputIterator first, InputIterator last)
    {
        m_shaderStages.clear();
        std::copy(first, last, std::back_inserter(m_shaderStages));
    }
    const QRhiShaderStage *cbeginShaderStages() const { return m_shaderStages.cbegin(); }
    const QRhiShaderStage *cendShaderStages() const { return m_shaderStages.cend(); }

    QRhiVertexInputLayout vertexInputLayout() const { return m_vertexInputLayout; }
    void setVertexInputLayout(const QRhiVertexInputLayout &layout) { m_vertexInputLayout = layout; }

    QRhiShaderResourceBindings *shaderResourceBindings() const { return m_shaderResourceBindings; }
    void setShaderResourceBindings(QRhiShaderResourceBindings *srb) { m_shaderResourceBindings = srb; }

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

    virtual bool build() = 0;

protected:
    QRhiGraphicsPipeline(QRhiImplementation *rhi);
    Flags m_flags;
    Topology m_topology = Triangles;
    CullMode m_cullMode = None;
    FrontFace m_frontFace = CCW;
    QVarLengthArray<TargetBlend, 8> m_targetBlends;
    bool m_depthTest = false;
    bool m_depthWrite = false;
    CompareOp m_depthOp = Less;
    bool m_stencilTest = false;
    StencilOpState m_stencilFront;
    StencilOpState m_stencilBack;
    quint32 m_stencilReadMask = 0xFF;
    quint32 m_stencilWriteMask = 0xFF;
    int m_sampleCount = 1;
    float m_lineWidth = 1.0f;
    int m_depthBias = 0;
    float m_slopeScaledDepthBias = 0.0f;
    QVarLengthArray<QRhiShaderStage, 4> m_shaderStages;
    QRhiVertexInputLayout m_vertexInputLayout;
    QRhiShaderResourceBindings *m_shaderResourceBindings = nullptr;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiGraphicsPipeline::ColorMask)
Q_DECLARE_TYPEINFO(QRhiGraphicsPipeline::TargetBlend, Q_MOVABLE_TYPE);

class Q_GUI_EXPORT QRhiSwapChain : public QRhiResource
{
public:
    enum Flag {
        SurfaceHasPreMulAlpha = 1 << 0,
        SurfaceHasNonPreMulAlpha = 1 << 1,
        sRGB = 1 << 2,
        UsedAsTransferSource = 1 << 3,
        NoVSync = 1 << 4,
        MinimalBufferCount = 1 << 5
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QRhiResource::Type resourceType() const override;

    QWindow *window() const { return m_window; }
    void setWindow(QWindow *window) { m_window = window; }

    Flags flags() const { return m_flags; }
    void setFlags(Flags f) { m_flags = f; }

    QRhiRenderBuffer *depthStencil() const { return m_depthStencil; }
    void setDepthStencil(QRhiRenderBuffer *ds) { m_depthStencil = ds; }

    int sampleCount() const { return m_sampleCount; }
    void setSampleCount(int samples) { m_sampleCount = samples; }

    QRhiRenderPassDescriptor *renderPassDescriptor() const { return m_renderPassDesc; }
    void setRenderPassDescriptor(QRhiRenderPassDescriptor *desc) { m_renderPassDesc = desc; }

    QSize currentPixelSize() const { return m_currentPixelSize; }

    virtual QRhiCommandBuffer *currentFrameCommandBuffer() = 0;
    virtual QRhiRenderTarget *currentFrameRenderTarget() = 0;
    virtual QSize surfacePixelSize() = 0;
    virtual QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() = 0;
    virtual bool buildOrResize() = 0;

protected:
    QRhiSwapChain(QRhiImplementation *rhi);
    QWindow *m_window = nullptr;
    Flags m_flags;
    QRhiRenderBuffer *m_depthStencil = nullptr;
    int m_sampleCount = 1;
    QRhiRenderPassDescriptor *m_renderPassDesc = nullptr;
    QSize m_currentPixelSize;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhiSwapChain::Flags)

class Q_GUI_EXPORT QRhiComputePipeline : public QRhiResource
{
public:
    QRhiResource::Type resourceType() const override;
    virtual bool build() = 0;

    QRhiShaderStage shaderStage() const { return m_shaderStage; }
    void setShaderStage(const QRhiShaderStage &stage) { m_shaderStage = stage; }

    QRhiShaderResourceBindings *shaderResourceBindings() const { return m_shaderResourceBindings; }
    void setShaderResourceBindings(QRhiShaderResourceBindings *srb) { m_shaderResourceBindings = srb; }

protected:
    QRhiComputePipeline(QRhiImplementation *rhi);
    QRhiShaderStage m_shaderStage;
    QRhiShaderResourceBindings *m_shaderResourceBindings = nullptr;
};

class Q_GUI_EXPORT QRhiCommandBuffer : public QRhiResource
{
public:
    enum IndexFormat {
        IndexUInt16,
        IndexUInt32
    };

    QRhiResource::Type resourceType() const override;

    void resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates);

    void beginPass(QRhiRenderTarget *rt,
                   const QColor &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates = nullptr);
    void endPass(QRhiResourceUpdateBatch *resourceUpdates = nullptr);

    void setGraphicsPipeline(QRhiGraphicsPipeline *ps);
    using DynamicOffset = QPair<int, quint32>; // binding, offset
    void setShaderResources(QRhiShaderResourceBindings *srb = nullptr,
                            int dynamicOffsetCount = 0,
                            const DynamicOffset *dynamicOffsets = nullptr);
    using VertexInput = QPair<QRhiBuffer *, quint32>; // buffer, offset
    void setVertexInput(int startBinding, int bindingCount, const VertexInput *bindings,
                        QRhiBuffer *indexBuf = nullptr, quint32 indexOffset = 0,
                        IndexFormat indexFormat = IndexUInt16);

    void setViewport(const QRhiViewport &viewport);
    void setScissor(const QRhiScissor &scissor);
    void setBlendConstants(const QColor &c);
    void setStencilRef(quint32 refValue);

    void draw(quint32 vertexCount,
              quint32 instanceCount = 1,
              quint32 firstVertex = 0,
              quint32 firstInstance = 0);

    void drawIndexed(quint32 indexCount,
                     quint32 instanceCount = 1,
                     quint32 firstIndex = 0,
                     qint32 vertexOffset = 0,
                     quint32 firstInstance = 0);

    void debugMarkBegin(const QByteArray &name);
    void debugMarkEnd();
    void debugMarkMsg(const QByteArray &msg);

    void beginComputePass(QRhiResourceUpdateBatch *resourceUpdates = nullptr);
    void endComputePass(QRhiResourceUpdateBatch *resourceUpdates = nullptr);
    void setComputePipeline(QRhiComputePipeline *ps);
    void dispatch(int x, int y, int z);

    const QRhiNativeHandles *nativeHandles();
    void beginExternal();
    void endExternal();

protected:
    QRhiCommandBuffer(QRhiImplementation *rhi);
};

struct Q_GUI_EXPORT QRhiReadbackResult
{
    std::function<void()> completed = nullptr;
    QRhiTexture::Format format;
    QSize pixelSize;
    QByteArray data;
}; // non-movable due to the std::function

struct Q_GUI_EXPORT QRhiBufferReadbackResult
{
    std::function<void()> completed = nullptr;
    QByteArray data;
};

class Q_GUI_EXPORT QRhiResourceUpdateBatch
{
public:
    ~QRhiResourceUpdateBatch();

    void release();

    void merge(QRhiResourceUpdateBatch *other);

    void updateDynamicBuffer(QRhiBuffer *buf, int offset, int size, const void *data);
    void uploadStaticBuffer(QRhiBuffer *buf, int offset, int size, const void *data);
    void uploadStaticBuffer(QRhiBuffer *buf, const void *data);
    void readBackBuffer(QRhiBuffer *buf, int offset, int size, QRhiBufferReadbackResult *result);
    void uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc);
    void uploadTexture(QRhiTexture *tex, const QImage &image);
    void copyTexture(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc = QRhiTextureCopyDescription());
    void readBackTexture(const QRhiReadbackDescription &rb, QRhiReadbackResult *result);
    void generateMips(QRhiTexture *tex, int layer = 0);

private:
    QRhiResourceUpdateBatch(QRhiImplementation *rhi);
    Q_DISABLE_COPY(QRhiResourceUpdateBatch)
    QRhiResourceUpdateBatchPrivate *d;
    friend class QRhiResourceUpdateBatchPrivate;
    friend class QRhi;
};

struct Q_GUI_EXPORT QRhiInitParams
{
};

class Q_GUI_EXPORT QRhi
{
public:
    enum Implementation {
        Null,
        Vulkan,
        OpenGLES2,
        D3D11,
        Metal
    };

    enum Flag {
        EnableProfiling = 1 << 0,
        EnableDebugMarkers = 1 << 1,
        PreferSoftwareRenderer = 1 << 2
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum FrameOpResult {
        FrameOpSuccess = 0,
        FrameOpError,
        FrameOpSwapChainOutOfDate,
        FrameOpDeviceLost
    };

    enum Feature {
        MultisampleTexture = 1,
        MultisampleRenderBuffer,
        DebugMarkers,
        Timestamps,
        Instancing,
        CustomInstanceStepRate,
        PrimitiveRestart,
        NonDynamicUniformBuffers,
        NonFourAlignedEffectiveIndexBufferOffset,
        NPOTTextureRepeat,
        RedOrAlpha8IsRed,
        ElementIndexUint,
        Compute,
        WideLines,
        VertexShaderPointSize,
        BaseVertex,
        BaseInstance,
        TriangleFanTopology,
        ReadBackNonUniformBuffer,
        ReadBackNonBaseMipLevel,
        TexelFetch
    };

    enum BeginFrameFlag {
        ExternalContentsInPass = 0x01
    };
    Q_DECLARE_FLAGS(BeginFrameFlags, BeginFrameFlag)

    enum EndFrameFlag {
        SkipPresent = 1 << 0
    };
    Q_DECLARE_FLAGS(EndFrameFlags, EndFrameFlag)

    enum ResourceLimit {
        TextureSizeMin = 1,
        TextureSizeMax,
        MaxColorAttachments,
        FramesInFlight,
        MaxAsyncReadbackFrames
    };

    ~QRhi();

    static QRhi *create(Implementation impl,
                        QRhiInitParams *params,
                        Flags flags = Flags(),
                        QRhiNativeHandles *importDevice = nullptr);

    Implementation backend() const;
    QThread *thread() const;

    using CleanupCallback = std::function<void(QRhi *)>;
    void addCleanupCallback(const CleanupCallback &callback);
    void runCleanup();

    QRhiGraphicsPipeline *newGraphicsPipeline();
    QRhiComputePipeline *newComputePipeline();
    QRhiShaderResourceBindings *newShaderResourceBindings();

    QRhiBuffer *newBuffer(QRhiBuffer::Type type,
                          QRhiBuffer::UsageFlags usage,
                          int size);

    QRhiRenderBuffer *newRenderBuffer(QRhiRenderBuffer::Type type,
                                      const QSize &pixelSize,
                                      int sampleCount = 1,
                                      QRhiRenderBuffer::Flags flags = QRhiRenderBuffer::Flags());

    QRhiTexture *newTexture(QRhiTexture::Format format,
                            const QSize &pixelSize,
                            int sampleCount = 1,
                            QRhiTexture::Flags flags = QRhiTexture::Flags());

    QRhiSampler *newSampler(QRhiSampler::Filter magFilter,
                            QRhiSampler::Filter minFilter,
                            QRhiSampler::Filter mipmapMode,
                            QRhiSampler::AddressMode addressU,
                            QRhiSampler::AddressMode addressV,
                            QRhiSampler::AddressMode addressW = QRhiSampler::Repeat);

    QRhiTextureRenderTarget *newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                    QRhiTextureRenderTarget::Flags flags = QRhiTextureRenderTarget::Flags());

    QRhiSwapChain *newSwapChain();
    FrameOpResult beginFrame(QRhiSwapChain *swapChain, BeginFrameFlags flags = BeginFrameFlags());
    FrameOpResult endFrame(QRhiSwapChain *swapChain, EndFrameFlags flags = EndFrameFlags());
    bool isRecordingFrame() const;
    int currentFrameSlot() const;

    FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb, BeginFrameFlags flags = BeginFrameFlags());
    FrameOpResult endOffscreenFrame(EndFrameFlags flags = EndFrameFlags());

    QRhi::FrameOpResult finish();

    QRhiResourceUpdateBatch *nextResourceUpdateBatch();

    QVector<int> supportedSampleCounts() const;

    int ubufAlignment() const;
    int ubufAligned(int v) const;

    int mipLevelsForSize(const QSize &size) const;
    QSize sizeForMipLevel(int mipLevel, const QSize &baseLevelSize) const;

    bool isYUpInFramebuffer() const;
    bool isYUpInNDC() const;
    bool isClipDepthZeroToOne() const;

    QMatrix4x4 clipSpaceCorrMatrix() const;

    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags = QRhiTexture::Flags()) const;
    bool isFeatureSupported(QRhi::Feature feature) const;
    int resourceLimit(ResourceLimit limit) const;

    const QRhiNativeHandles *nativeHandles();
    bool makeThreadLocalNativeContextCurrent();

    QRhiProfiler *profiler();

    static const int MAX_LAYERS = 6; // cubemaps only
    static const int MAX_LEVELS = 16; // a width and/or height of 65536 should be enough for everyone

    void releaseCachedResources();

    bool isDeviceLost() const;

protected:
    QRhi();

private:
    Q_DISABLE_COPY(QRhi)
    QRhiImplementation *d = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRhi::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhi::BeginFrameFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRhi::EndFrameFlags)

QT_END_NAMESPACE

#endif
