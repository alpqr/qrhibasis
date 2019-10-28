/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifdef _MSC_VER
#if defined(_DEBUG) || defined(DEBUG)
#ifndef _ITERATOR_DEBUG_LEVEL
#define _ITERATOR_DEBUG_LEVEL 1
#endif
#ifndef _SECURE_SCL
#define _SECURE_SCL 1
#endif
#else // defined(_DEBUG) || defined(DEBUG)
#ifndef _SECURE_SCL
#define _SECURE_SCL 0
#endif
#ifndef _ITERATOR_DEBUG_LEVEL
#define _ITERATOR_DEBUG_LEVEL 0
#endif
#endif // defined(_DEBUG) || defined(DEBUG)
#endif // _MSC_VER

#define EXAMPLEFW_PREINIT
#include "examplefw.h"

#include "transcoder/basisu_transcoder.h"

// Set this to 1 to use a plain QImage loaded from a .png and skip all
// the basis stuff.
#define USE_QIMAGE 0

struct {
    QScopedPointer<basist::etc1_global_selector_codebook> codebook;
    QVector<QRhiResource *> releasePool;
    QRhiResourceUpdateBatch *initialUpdates = nullptr;
    QRhiBuffer *vbuf = nullptr;
    QRhiBuffer *ubuf = nullptr;
    QRhiTexture *tex = nullptr;
    QRhiSampler *sampler = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiGraphicsPipeline *ps = nullptr;
    QMatrix4x4 winProj;
} d;

static float vertexData[] =
{ // Y up, CCW
  -0.375f, 0.5f,   0.0f, 0.0f,
  -0.375f, -0.5f,  0.0f, 1.0f,
  0.375f, -0.5f,   1.0f, 1.0f,

  0.375f, -0.5f,   1.0f, 1.0f,
  0.375f, 0.5f,    1.0f, 0.0f,
  -0.375f, 0.5f,   0.0f, 0.0f
};

void preInit()
{
    basist::basisu_transcoder_init();
    d.codebook.reset(new basist::etc1_global_selector_codebook(basist::g_global_selector_cb_size, basist::g_global_selector_cb));
}

void Window::customInit()
{
    d.vbuf = m_r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData));
    d.vbuf->build();
    d.releasePool << d.vbuf;

    d.ubuf = m_r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 68);
    d.ubuf->build();
    d.releasePool << d.ubuf;

    QRhiTexture::Format texFormat;
    QSize texSize;
    QRhiTexture::Flags texFlags = QRhiTexture::MipMapped;

#if USE_QIMAGE
    QImage image;
    image.load(QLatin1String(":/honey.png"));
    if (image.isNull())
        qFatal("Failed to load image");
    image = std::move(image).convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    qDebug("Loading PNG into QImage and autogenerating %d mipmaps", m_r->mipLevelsForSize(image.size()));
    texFormat = QRhiTexture::RGBA8;
    texSize = image.size();
    texFlags |= QRhiTexture::UsedWithGenerateMips;
#else
    QFile f(QLatin1String(":/honey.basis"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Failed to load .basis file");
    const QByteArray bdata = f.readAll();
    f.close();

    qDebug("Loading .basis file and transcoding to compressed texture");
    basist::basisu_transcoder transcoder(d.codebook.data());
    if (!transcoder.validate_header(bdata.constData(), bdata.size()))
        qFatal(".basis header validation failed");

    basist::basisu_image_info imageInfo;
    if (!transcoder.get_image_info(bdata.constData(), bdata.size(), imageInfo, 0))
        qFatal("Failed to get .basis image info for image 0");

    basist::transcoder_texture_format outFormat;
    if (m_r->isTextureFormatSupported(QRhiTexture::ETC2_RGBA8)) {
        qDebug("Transcoding to ETC2_RGBA8");
        texFormat = QRhiTexture::ETC2_RGBA8;
        outFormat = basist::transcoder_texture_format::cTFETC2_RGBA;
        texSize = QSize(imageInfo.m_width, imageInfo.m_height);
    } else if (m_r->isTextureFormatSupported(QRhiTexture::BC1)) {
        qDebug("Transcoding to BC1");
        texFormat = QRhiTexture::BC1;
        outFormat = basist::transcoder_texture_format::cTFBC1;
        texSize = QSize(imageInfo.m_width, imageInfo.m_height);
    } else {
        qDebug("No ETC2/BC1 support. Transcoding to plain RGBA8");
        texFormat = QRhiTexture::RGBA8;
        outFormat = basist::transcoder_texture_format::cTFRGBA32;
        texSize = QSize(imageInfo.m_orig_width, imageInfo.m_orig_height);
    }

    qDebug("Texture size is %dx%d, original size is %ux%u",
           texSize.width(), texSize.height(), imageInfo.m_orig_width, imageInfo.m_orig_height);

    transcoder.start_transcoding(bdata.constData(), bdata.size());

    qDebug("%u mip levels", imageInfo.m_total_levels);
    QVector<QByteArray> outMips;
    outMips.resize(imageInfo.m_total_levels);

    for (uint32_t level = 0; level < imageInfo.m_total_levels; ++level) {
        basist::basisu_image_level_info levelInfo;
        if (!transcoder.get_image_level_info(bdata.constData(), bdata.size(), levelInfo, 0, level))
            qFatal("Failed to get level %u info", level);

        qDebug("level %u: %ux%u (orig. %ux%u) %u blocks", level, levelInfo.m_width, levelInfo.m_height,
               levelInfo.m_orig_width, levelInfo.m_orig_height, levelInfo.m_total_blocks);
        uint32_t blocks;
        if (outFormat == basist::transcoder_texture_format::cTFRGBA32) {
            blocks = levelInfo.m_orig_width * levelInfo.m_orig_height;
            outMips[level].resize(blocks * 4);
        } else {
            blocks = levelInfo.m_total_blocks;
            const uint32_t bytesPerBlock = basist::basis_get_bytes_per_block(outFormat);
            outMips[level].resize(blocks * bytesPerBlock);
        }
        if (!transcoder.transcode_image_level(bdata.constData(), bdata.size(), 0, level,
                                              outMips[level].data(), blocks, outFormat))
        {
            qFatal("Failed to transcode image level %u", level);
        }
    }
#endif

    d.tex = m_r->newTexture(texFormat, texSize, 1, texFlags);
    d.releasePool << d.tex;
    d.tex->build();

    d.sampler = m_r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    d.releasePool << d.sampler;
    d.sampler->build();

    d.srb = m_r->newShaderResourceBindings();
    d.releasePool << d.srb;
    d.srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, d.ubuf),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, d.tex, d.sampler)
    });
    d.srb->build();

    d.ps = m_r->newGraphicsPipeline();
    d.releasePool << d.ps;
    d.ps->setShaderStages({
        { QRhiShaderStage::Vertex, getShader(QLatin1String(":/texture.vert.qsb")) },
        { QRhiShaderStage::Fragment, getShader(QLatin1String(":/texture.frag.qsb")) }
    });
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        { 4 * sizeof(float) }
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
    });
    d.ps->setVertexInputLayout(inputLayout);
    d.ps->setShaderResourceBindings(d.srb);
    d.ps->setRenderPassDescriptor(m_rp);
    d.ps->build();

    d.initialUpdates = m_r->nextResourceUpdateBatch();
    d.initialUpdates->uploadStaticBuffer(d.vbuf, vertexData);

    qint32 flip = 0;
    d.initialUpdates->updateDynamicBuffer(d.ubuf, 64, 4, &flip);

#if USE_QIMAGE
    d.initialUpdates->uploadTexture(d.tex, image);
    d.initialUpdates->generateMips(d.tex);
#else
    const int mipCount = m_r->mipLevelsForSize(d.tex->pixelSize());
    if (outMips.count() != mipCount)
        qFatal("Mip level count mismatch (%d vs %d), this should not happen.", outMips.count(), mipCount);
    QVarLengthArray<QRhiTextureUploadEntry, 16> descEntries;
    for (int i = 0; i < outMips.count(); ++i)
        descEntries.append({ 0, i, QRhiTextureSubresourceUploadDescription(outMips[i].constData(), outMips[i].size()) });
    QRhiTextureUploadDescription desc;
    desc.setEntries(descEntries.cbegin(), descEntries.cend());
    d.initialUpdates->uploadTexture(d.tex, desc);
#endif
}

void Window::customRelease()
{
    qDeleteAll(d.releasePool);
    d.releasePool.clear();
}

void Window::customRender()
{
    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    QRhiResourceUpdateBatch *u = m_r->nextResourceUpdateBatch();
    if (d.initialUpdates) {
        u->merge(d.initialUpdates);
        d.initialUpdates->release();
        d.initialUpdates = nullptr;
    }

    if (d.winProj != m_proj) {
        d.winProj = m_proj;
        QMatrix4x4 mvp = m_proj;
        mvp.scale(2.5f);
        u->updateDynamicBuffer(d.ubuf, 0, 64, mvp.constData());
    }

    const QSize outputSizeInPixels = m_sc->currentPixelSize();
    cb->beginPass(m_sc->currentFrameRenderTarget(), m_clearColor, { 1.0f, 0 }, u);
    cb->setGraphicsPipeline(d.ps);
    cb->setViewport({ 0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height()) });
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vbufBinding(d.vbuf, 0);
    cb->setVertexInput(0, 1, &vbufBinding);
    cb->draw(6);
    cb->endPass();
}
