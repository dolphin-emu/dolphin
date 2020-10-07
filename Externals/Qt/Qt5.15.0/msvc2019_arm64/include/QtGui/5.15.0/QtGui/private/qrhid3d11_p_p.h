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

#ifndef QRHID3D11_P_H
#define QRHID3D11_P_H

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

#include "qrhid3d11_p.h"
#include "qrhi_p_p.h"
#include "qshaderdescription_p.h"
#include <QWindow>

#include <d3d11_1.h>
#include <dxgi1_3.h>

QT_BEGIN_NAMESPACE

struct QD3D11Buffer : public QRhiBuffer
{
    QD3D11Buffer(QRhiImplementation *rhi, Type type, UsageFlags usage, int size);
    ~QD3D11Buffer();
    void release() override;
    bool build() override;
    QRhiBuffer::NativeBuffer nativeBuffer() override;

    ID3D11UnorderedAccessView *unorderedAccessView();

    ID3D11Buffer *buffer = nullptr;
    QByteArray dynBuf;
    bool hasPendingDynamicUpdates = false;
    ID3D11UnorderedAccessView *uav = nullptr;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11RenderBuffer : public QRhiRenderBuffer
{
    QD3D11RenderBuffer(QRhiImplementation *rhi, Type type, const QSize &pixelSize,
                       int sampleCount, QRhiRenderBuffer::Flags flags);
    ~QD3D11RenderBuffer();
    void release() override;
    bool build() override;
    QRhiTexture::Format backingFormat() const override;

    ID3D11Texture2D *tex = nullptr;
    ID3D11DepthStencilView *dsv = nullptr;
    ID3D11RenderTargetView *rtv = nullptr;
    DXGI_FORMAT dxgiFormat;
    DXGI_SAMPLE_DESC sampleDesc;
    friend class QRhiD3D11;
};

struct QD3D11Texture : public QRhiTexture
{
    QD3D11Texture(QRhiImplementation *rhi, Format format, const QSize &pixelSize,
                  int sampleCount, Flags flags);
    ~QD3D11Texture();
    void release() override;
    bool build() override;
    bool buildFrom(NativeTexture src) override;
    NativeTexture nativeTexture() override;

    bool prepareBuild(QSize *adjustedSize = nullptr);
    bool finishBuild();
    ID3D11UnorderedAccessView *unorderedAccessViewForLevel(int level);

    ID3D11Texture2D *tex = nullptr;
    bool owns = true;
    ID3D11ShaderResourceView *srv = nullptr;
    DXGI_FORMAT dxgiFormat;
    uint mipLevelCount = 0;
    DXGI_SAMPLE_DESC sampleDesc;
    ID3D11UnorderedAccessView *perLevelViews[QRhi::MAX_LEVELS];
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11Sampler : public QRhiSampler
{
    QD3D11Sampler(QRhiImplementation *rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                  AddressMode u, AddressMode v, AddressMode w);
    ~QD3D11Sampler();
    void release() override;
    bool build() override;

    ID3D11SamplerState *samplerState = nullptr;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11RenderPassDescriptor : public QRhiRenderPassDescriptor
{
    QD3D11RenderPassDescriptor(QRhiImplementation *rhi);
    ~QD3D11RenderPassDescriptor();
    void release() override;
    bool isCompatible(const QRhiRenderPassDescriptor *other) const override;
};

struct QD3D11RenderTargetData
{
    QD3D11RenderTargetData(QRhiImplementation *)
    {
        for (int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
            rtv[i] = nullptr;
    }

    QD3D11RenderPassDescriptor *rp = nullptr;
    QSize pixelSize;
    float dpr = 1;
    int sampleCount = 1;
    int colorAttCount = 0;
    int dsAttCount = 0;

    static const int MAX_COLOR_ATTACHMENTS = 8;
    ID3D11RenderTargetView *rtv[MAX_COLOR_ATTACHMENTS];
    ID3D11DepthStencilView *dsv = nullptr;
};

struct QD3D11ReferenceRenderTarget : public QRhiRenderTarget
{
    QD3D11ReferenceRenderTarget(QRhiImplementation *rhi);
    ~QD3D11ReferenceRenderTarget();
    void release() override;

    QSize pixelSize() const override;
    float devicePixelRatio() const override;
    int sampleCount() const override;

    QD3D11RenderTargetData d;
};

struct QD3D11TextureRenderTarget : public QRhiTextureRenderTarget
{
    QD3D11TextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags);
    ~QD3D11TextureRenderTarget();
    void release() override;

    QSize pixelSize() const override;
    float devicePixelRatio() const override;
    int sampleCount() const override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool build() override;

    QD3D11RenderTargetData d;
    bool ownsRtv[QD3D11RenderTargetData::MAX_COLOR_ATTACHMENTS];
    ID3D11RenderTargetView *rtv[QD3D11RenderTargetData::MAX_COLOR_ATTACHMENTS];
    bool ownsDsv = false;
    ID3D11DepthStencilView *dsv = nullptr;
    friend class QRhiD3D11;
};

struct QD3D11ShaderResourceBindings : public QRhiShaderResourceBindings
{
    QD3D11ShaderResourceBindings(QRhiImplementation *rhi);
    ~QD3D11ShaderResourceBindings();
    void release() override;
    bool build() override;

    QVarLengthArray<QRhiShaderResourceBinding, 8> sortedBindings;
    uint generation = 0;

    // Keep track of the generation number of each referenced QRhi* to be able
    // to detect that the batched bindings are out of date.
    struct BoundUniformBufferData {
        quint64 id;
        uint generation;
    };
    struct BoundSampledTextureData {
        int count;
        struct {
            quint64 texId;
            uint texGeneration;
            quint64 samplerId;
            uint samplerGeneration;
        } d[QRhiShaderResourceBinding::Data::MAX_TEX_SAMPLER_ARRAY_SIZE];
    };
    struct BoundStorageImageData {
        quint64 id;
        uint generation;
    };
    struct BoundStorageBufferData {
        quint64 id;
        uint generation;
    };
    struct BoundResourceData {
        union {
            BoundUniformBufferData ubuf;
            BoundSampledTextureData stex;
            BoundStorageImageData simage;
            BoundStorageBufferData sbuf;
        };
    };
    QVarLengthArray<BoundResourceData, 8> boundResourceData;

    QRhiBatchedBindings<ID3D11Buffer *> vsubufs;
    QRhiBatchedBindings<UINT> vsubufoffsets;
    QRhiBatchedBindings<UINT> vsubufsizes;

    QRhiBatchedBindings<ID3D11Buffer *> fsubufs;
    QRhiBatchedBindings<UINT> fsubufoffsets;
    QRhiBatchedBindings<UINT> fsubufsizes;

    QRhiBatchedBindings<ID3D11Buffer *> csubufs;
    QRhiBatchedBindings<UINT> csubufoffsets;
    QRhiBatchedBindings<UINT> csubufsizes;

    QRhiBatchedBindings<ID3D11SamplerState *> vssamplers;
    QRhiBatchedBindings<ID3D11ShaderResourceView *> vsshaderresources;

    QRhiBatchedBindings<ID3D11SamplerState *> fssamplers;
    QRhiBatchedBindings<ID3D11ShaderResourceView *> fsshaderresources;

    QRhiBatchedBindings<ID3D11SamplerState *> cssamplers;
    QRhiBatchedBindings<ID3D11ShaderResourceView *> csshaderresources;

    QRhiBatchedBindings<ID3D11UnorderedAccessView *> csUAVs;

    friend class QRhiD3D11;
};

Q_DECLARE_TYPEINFO(QD3D11ShaderResourceBindings::BoundResourceData, Q_MOVABLE_TYPE);

struct QD3D11GraphicsPipeline : public QRhiGraphicsPipeline
{
    QD3D11GraphicsPipeline(QRhiImplementation *rhi);
    ~QD3D11GraphicsPipeline();
    void release() override;
    bool build() override;

    ID3D11DepthStencilState *dsState = nullptr;
    ID3D11BlendState *blendState = nullptr;
    struct {
        ID3D11VertexShader *shader = nullptr;
        QShader::NativeResourceBindingMap nativeResourceBindingMap;
    } vs;
    struct {
        ID3D11PixelShader *shader = nullptr;
        QShader::NativeResourceBindingMap nativeResourceBindingMap;
    } fs;
    ID3D11InputLayout *inputLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ID3D11RasterizerState *rastState = nullptr;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11ComputePipeline : public QRhiComputePipeline
{
    QD3D11ComputePipeline(QRhiImplementation *rhi);
    ~QD3D11ComputePipeline();
    void release() override;
    bool build() override;

    struct {
        ID3D11ComputeShader *shader = nullptr;
        QShader::NativeResourceBindingMap nativeResourceBindingMap;
    } cs;
    uint generation = 0;
    friend class QRhiD3D11;
};

struct QD3D11SwapChain;

struct QD3D11CommandBuffer : public QRhiCommandBuffer
{
    QD3D11CommandBuffer(QRhiImplementation *rhi);
    ~QD3D11CommandBuffer();
    void release() override;

    struct Command {
        enum Cmd {
            ResetShaderResources,
            SetRenderTarget,
            Clear,
            Viewport,
            Scissor,
            BindVertexBuffers,
            BindIndexBuffer,
            BindGraphicsPipeline,
            BindShaderResources,
            StencilRef,
            BlendConstants,
            Draw,
            DrawIndexed,
            UpdateSubRes,
            CopySubRes,
            ResolveSubRes,
            GenMip,
            DebugMarkBegin,
            DebugMarkEnd,
            DebugMarkMsg,
            BindComputePipeline,
            Dispatch
        };
        enum ClearFlag { Color = 1, Depth = 2, Stencil = 4 };
        Cmd cmd;

        static const int MAX_UBUF_BINDINGS = 32; // should be D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT but 128 is a waste of space for our purposes

        // QRhi*/QD3D11* references should be kept at minimum (so no
        // QRhiTexture/Buffer/etc. pointers).
        union {
            struct {
                QRhiRenderTarget *rt;
            } setRenderTarget;
            struct {
                QRhiRenderTarget *rt;
                int mask;
                float c[4];
                float d;
                quint32 s;
            } clear;
            struct {
                float x, y, w, h;
                float d0, d1;
            } viewport;
            struct {
                int x, y, w, h;
            } scissor;
            struct {
                int startSlot;
                int slotCount;
                ID3D11Buffer *buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
                UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
                UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
            } bindVertexBuffers;
            struct {
                ID3D11Buffer *buffer;
                quint32 offset;
                DXGI_FORMAT format;
            } bindIndexBuffer;
            struct {
                QD3D11GraphicsPipeline *ps;
            } bindGraphicsPipeline;
            struct {
                QD3D11ShaderResourceBindings *srb;
                bool offsetOnlyChange;
                int dynamicOffsetCount;
                uint dynamicOffsetPairs[MAX_UBUF_BINDINGS * 2]; // binding, offsetInConstants
            } bindShaderResources;
            struct {
                QD3D11GraphicsPipeline *ps;
                quint32 ref;
            } stencilRef;
            struct {
                QD3D11GraphicsPipeline *ps;
                float c[4];
            } blendConstants;
            struct {
                QD3D11GraphicsPipeline *ps;
                quint32 vertexCount;
                quint32 instanceCount;
                quint32 firstVertex;
                quint32 firstInstance;
            } draw;
            struct {
                QD3D11GraphicsPipeline *ps;
                quint32 indexCount;
                quint32 instanceCount;
                quint32 firstIndex;
                qint32 vertexOffset;
                quint32 firstInstance;
            } drawIndexed;
            struct {
                ID3D11Resource *dst;
                UINT dstSubRes;
                bool hasDstBox;
                D3D11_BOX dstBox;
                const void *src; // must come from retain*()
                UINT srcRowPitch;
            } updateSubRes;
            struct {
                ID3D11Resource *dst;
                UINT dstSubRes;
                UINT dstX;
                UINT dstY;
                ID3D11Resource *src;
                UINT srcSubRes;
                bool hasSrcBox;
                D3D11_BOX srcBox;
            } copySubRes;
            struct {
                ID3D11Resource *dst;
                UINT dstSubRes;
                ID3D11Resource *src;
                UINT srcSubRes;
                DXGI_FORMAT format;
            } resolveSubRes;
            struct {
                ID3D11ShaderResourceView *srv;
            } genMip;
            struct {
                char s[64];
            } debugMark;
            struct {
                QD3D11ComputePipeline *ps;
            } bindComputePipeline;
            struct {
                UINT x;
                UINT y;
                UINT z;
            } dispatch;
        } args;
    };

    enum PassType {
        NoPass,
        RenderPass,
        ComputePass
    };

    QVector<Command> commands;
    PassType recordingPass;
    QRhiRenderTarget *currentTarget;
    QRhiGraphicsPipeline *currentGraphicsPipeline;
    QRhiComputePipeline *currentComputePipeline;
    uint currentPipelineGeneration;
    QRhiShaderResourceBindings *currentGraphicsSrb;
    QRhiShaderResourceBindings *currentComputeSrb;
    uint currentSrbGeneration;
    ID3D11Buffer *currentIndexBuffer;
    quint32 currentIndexOffset;
    DXGI_FORMAT currentIndexFormat;
    ID3D11Buffer *currentVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    quint32 currentVertexOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

    QVector<QByteArray> dataRetainPool;
    QVector<QImage> imageRetainPool;

    // relies heavily on implicit sharing (no copies of the actual data will be made)
    const uchar *retainData(const QByteArray &data) {
        dataRetainPool.append(data);
        return reinterpret_cast<const uchar *>(dataRetainPool.constLast().constData());
    }
    const uchar *retainImage(const QImage &image) {
        imageRetainPool.append(image);
        return imageRetainPool.constLast().constBits();
    }
    void resetCommands() {
        commands.clear();
        dataRetainPool.clear();
        imageRetainPool.clear();
    }
    void resetState() {
        recordingPass = NoPass;
        currentTarget = nullptr;
        resetCommands();
        resetCachedState();
    }
    void resetCachedState() {
        currentGraphicsPipeline = nullptr;
        currentComputePipeline = nullptr;
        currentPipelineGeneration = 0;
        resetCachedShaderResourceState();
    }
    void resetCachedShaderResourceState() {
        currentGraphicsSrb = nullptr;
        currentComputeSrb = nullptr;
        currentSrbGeneration = 0;
        currentIndexBuffer = nullptr;
        currentIndexOffset = 0;
        currentIndexFormat = DXGI_FORMAT_R16_UINT;
        memset(currentVertexBuffers, 0, sizeof(currentVertexBuffers));
        memset(currentVertexOffsets, 0, sizeof(currentVertexOffsets));
    }
};

Q_DECLARE_TYPEINFO(QD3D11CommandBuffer::Command, Q_MOVABLE_TYPE);

struct QD3D11SwapChain : public QRhiSwapChain
{
    QD3D11SwapChain(QRhiImplementation *rhi);
    ~QD3D11SwapChain();
    void release() override;

    QRhiCommandBuffer *currentFrameCommandBuffer() override;
    QRhiRenderTarget *currentFrameRenderTarget() override;

    QSize surfacePixelSize() override;

    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool buildOrResize() override;

    void releaseBuffers();
    bool newColorBuffer(const QSize &size, DXGI_FORMAT format, DXGI_SAMPLE_DESC sampleDesc,
                        ID3D11Texture2D **tex, ID3D11RenderTargetView **rtv) const;

    QWindow *window = nullptr;
    QSize pixelSize;
    QD3D11ReferenceRenderTarget rt;
    QD3D11CommandBuffer cb;
    DXGI_FORMAT colorFormat;
    IDXGISwapChain *swapChain = nullptr;
    static const int BUFFER_COUNT = 2;
    ID3D11Texture2D *backBufferTex;
    ID3D11RenderTargetView *backBufferRtv;
    ID3D11Texture2D *msaaTex[BUFFER_COUNT];
    ID3D11RenderTargetView *msaaRtv[BUFFER_COUNT];
    DXGI_SAMPLE_DESC sampleDesc;
    int currentFrameSlot = 0;
    int frameCount = 0;
    QD3D11RenderBuffer *ds = nullptr;
    bool timestampActive[BUFFER_COUNT];
    ID3D11Query *timestampDisjointQuery[BUFFER_COUNT];
    ID3D11Query *timestampQuery[BUFFER_COUNT * 2];
    UINT swapInterval = 1;
};

class QRhiD3D11 : public QRhiImplementation
{
public:
    QRhiD3D11(QRhiD3D11InitParams *params, QRhiD3D11NativeHandles *importDevice = nullptr);

    bool create(QRhi::Flags flags) override;
    void destroy() override;

    QRhiGraphicsPipeline *createGraphicsPipeline() override;
    QRhiComputePipeline *createComputePipeline() override;
    QRhiShaderResourceBindings *createShaderResourceBindings() override;
    QRhiBuffer *createBuffer(QRhiBuffer::Type type,
                             QRhiBuffer::UsageFlags usage,
                             int size) override;
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type,
                                         const QSize &pixelSize,
                                         int sampleCount,
                                         QRhiRenderBuffer::Flags flags) override;
    QRhiTexture *createTexture(QRhiTexture::Format format,
                               const QSize &pixelSize,
                               int sampleCount,
                               QRhiTexture::Flags flags) override;
    QRhiSampler *createSampler(QRhiSampler::Filter magFilter,
                               QRhiSampler::Filter minFilter,
                               QRhiSampler::Filter mipmapMode,
                               QRhiSampler:: AddressMode u,
                               QRhiSampler::AddressMode v,
                               QRhiSampler::AddressMode w) override;

    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags flags) override;

    QRhiSwapChain *createSwapChain() override;
    QRhi::FrameOpResult beginFrame(QRhiSwapChain *swapChain, QRhi::BeginFrameFlags flags) override;
    QRhi::FrameOpResult endFrame(QRhiSwapChain *swapChain, QRhi::EndFrameFlags flags) override;
    QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb, QRhi::BeginFrameFlags flags) override;
    QRhi::FrameOpResult endOffscreenFrame(QRhi::EndFrameFlags flags) override;
    QRhi::FrameOpResult finish() override;

    void resourceUpdate(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void beginPass(QRhiCommandBuffer *cb,
                   QRhiRenderTarget *rt,
                   const QColor &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue,
                   QRhiResourceUpdateBatch *resourceUpdates) override;
    void endPass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;

    void setGraphicsPipeline(QRhiCommandBuffer *cb,
                             QRhiGraphicsPipeline *ps) override;

    void setShaderResources(QRhiCommandBuffer *cb,
                            QRhiShaderResourceBindings *srb,
                            int dynamicOffsetCount,
                            const QRhiCommandBuffer::DynamicOffset *dynamicOffsets) override;

    void setVertexInput(QRhiCommandBuffer *cb,
                        int startBinding, int bindingCount, const QRhiCommandBuffer::VertexInput *bindings,
                        QRhiBuffer *indexBuf, quint32 indexOffset,
                        QRhiCommandBuffer::IndexFormat indexFormat) override;

    void setViewport(QRhiCommandBuffer *cb, const QRhiViewport &viewport) override;
    void setScissor(QRhiCommandBuffer *cb, const QRhiScissor &scissor) override;
    void setBlendConstants(QRhiCommandBuffer *cb, const QColor &c) override;
    void setStencilRef(QRhiCommandBuffer *cb, quint32 refValue) override;

    void draw(QRhiCommandBuffer *cb, quint32 vertexCount,
              quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) override;

    void drawIndexed(QRhiCommandBuffer *cb, quint32 indexCount,
                     quint32 instanceCount, quint32 firstIndex,
                     qint32 vertexOffset, quint32 firstInstance) override;

    void debugMarkBegin(QRhiCommandBuffer *cb, const QByteArray &name) override;
    void debugMarkEnd(QRhiCommandBuffer *cb) override;
    void debugMarkMsg(QRhiCommandBuffer *cb, const QByteArray &msg) override;

    void beginComputePass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;
    void endComputePass(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates) override;
    void setComputePipeline(QRhiCommandBuffer *cb, QRhiComputePipeline *ps) override;
    void dispatch(QRhiCommandBuffer *cb, int x, int y, int z) override;

    const QRhiNativeHandles *nativeHandles(QRhiCommandBuffer *cb) override;
    void beginExternal(QRhiCommandBuffer *cb) override;
    void endExternal(QRhiCommandBuffer *cb) override;

    QVector<int> supportedSampleCounts() const override;
    int ubufAlignment() const override;
    bool isYUpInFramebuffer() const override;
    bool isYUpInNDC() const override;
    bool isClipDepthZeroToOne() const override;
    QMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const override;
    bool isFeatureSupported(QRhi::Feature feature) const override;
    int resourceLimit(QRhi::ResourceLimit limit) const override;
    const QRhiNativeHandles *nativeHandles() override;
    void sendVMemStatsToProfiler() override;
    bool makeThreadLocalNativeContextCurrent() override;
    void releaseCachedResources() override;
    bool isDeviceLost() const override;

    void enqueueSubresUpload(QD3D11Texture *texD, QD3D11CommandBuffer *cbD,
                             int layer, int level, const QRhiTextureSubresourceUploadDescription &subresDesc);
    void enqueueResourceUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *resourceUpdates);
    void updateShaderResourceBindings(QD3D11ShaderResourceBindings *srbD,
                                      const QShader::NativeResourceBindingMap *nativeResourceBindingMaps[]);
    void executeBufferHostWrites(QD3D11Buffer *bufD);
    void bindShaderResources(QD3D11ShaderResourceBindings *srbD,
                             const uint *dynOfsPairs, int dynOfsPairCount,
                             bool offsetOnlyChange);
    void resetShaderResources();
    void executeCommandBuffer(QD3D11CommandBuffer *cbD, QD3D11SwapChain *timestampSwapChain = nullptr);
    DXGI_SAMPLE_DESC effectiveSampleCount(int sampleCount) const;
    void finishActiveReadbacks();
    void reportLiveObjects(ID3D11Device *device);
    void clearShaderCache();

    bool debugLayer = false;
    bool importedDevice = false;
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext1 *context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    ID3DUserDefinedAnnotation *annotations = nullptr;
    IDXGIFactory1 *dxgiFactory = nullptr;
    bool hasDxgi2 = false;
    bool supportsFlipDiscardSwapchain = false;
    bool deviceLost = false;
    QRhiD3D11NativeHandles nativeHandlesStruct;

    struct {
        int vsHighestActiveVertexBufferBinding = -1;
        bool vsHasIndexBufferBound = false;
        int vsHighestActiveSrvBinding = -1;
        int fsHighestActiveSrvBinding = -1;
        int csHighestActiveSrvBinding = -1;
        int csHighestActiveUavBinding = -1;
        QD3D11SwapChain *currentSwapChain = nullptr;
    } contextState;

    struct OffscreenFrame {
        OffscreenFrame(QRhiImplementation *rhi) : cbWrapper(rhi) { }
        bool active = false;
        QD3D11CommandBuffer cbWrapper;
    } ofr;

    struct TextureReadback {
        QRhiReadbackDescription desc;
        QRhiReadbackResult *result;
        ID3D11Texture2D *stagingTex;
        quint32 byteSize;
        quint32 bpl;
        QSize pixelSize;
        QRhiTexture::Format format;
    };
    QVector<TextureReadback> activeTextureReadbacks;
    struct BufferReadback {
        QRhiBufferReadbackResult *result;
        quint32 byteSize;
        ID3D11Buffer *stagingBuf;
    };
    QVector<BufferReadback> activeBufferReadbacks;

    struct Shader {
        Shader() = default;
        Shader(IUnknown *s, const QByteArray &bytecode, const QShader::NativeResourceBindingMap &rbm)
            : s(s), bytecode(bytecode), nativeResourceBindingMap(rbm) { }
        IUnknown *s;
        QByteArray bytecode;
        QShader::NativeResourceBindingMap nativeResourceBindingMap;
    };
    QHash<QRhiShaderStage, Shader> m_shaderCache;

    struct DeviceCurse {
        DeviceCurse(QRhiD3D11 *impl) : q(impl) { }
        QRhiD3D11 *q;
        int framesToActivate = -1;
        bool permanent = false;
        int framesLeft = 0;
        ID3D11ComputeShader *cs = nullptr;

        void initResources();
        void releaseResources();
        void activate();
    } deviceCurse;
};

Q_DECLARE_TYPEINFO(QRhiD3D11::TextureReadback, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(QRhiD3D11::BufferReadback, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif
