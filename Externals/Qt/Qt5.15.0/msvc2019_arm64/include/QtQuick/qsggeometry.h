/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt scene graph research project.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSGGEOMETRY_H
#define QSGGEOMETRY_H

#include <QtQuick/qtquickglobal.h>
#include <QtGui/qopengl.h>
#include <QtCore/QRectF>

QT_BEGIN_NAMESPACE

class QSGGeometryData;

class Q_QUICK_EXPORT QSGGeometry
{
public:
    enum AttributeType {
        UnknownAttribute,
        PositionAttribute,
        ColorAttribute,
        TexCoordAttribute,
        TexCoord1Attribute,
        TexCoord2Attribute
    };

    enum DataPattern {
        AlwaysUploadPattern = 0,
        StreamPattern       = 1,
        DynamicPattern      = 2,
        StaticPattern       = 3
    };

    // Equivalents to GL_* drawing modes.
    // Keep in sync with GL headers.
    enum DrawingMode {
        DrawPoints = 0x0000,
        DrawLines = 0x0001,
        DrawLineLoop = 0x0002,
        DrawLineStrip = 0x0003,
        DrawTriangles = 0x0004,
        DrawTriangleStrip = 0x0005,
        DrawTriangleFan = 0x0006
    };

    // Equivalents to GL_BYTE and similar type constants.
    // Keep in sync with GL headers.
    enum Type {
        ByteType = 0x1400,
        UnsignedByteType = 0x1401,
        ShortType = 0x1402,
        UnsignedShortType = 0x1403,
        IntType = 0x1404,
        UnsignedIntType = 0x1405,
        FloatType = 0x1406,
        Bytes2Type = 0x1407,
        Bytes3Type = 0x1408,
        Bytes4Type = 0x1409,
        DoubleType = 0x140A
    };

    struct Q_QUICK_EXPORT Attribute
    {
        int position;
        int tupleSize;
        int type;

        uint isVertexCoordinate : 1;

        AttributeType attributeType : 4;

        uint reserved : 27;

        static Attribute create(int pos, int tupleSize, int primitiveType, bool isPosition = false);
        static Attribute createWithAttributeType(int pos, int tupleSize, int primitiveType, AttributeType attributeType);
    };

    struct AttributeSet {
        int count;
        int stride;
        const Attribute *attributes;
    };

    struct Point2D {
        float x, y;
        void set(float nx, float ny) {
            x = nx; y = ny;
        }
    };
    struct TexturedPoint2D {
        float x, y;
        float tx, ty;
        void set(float nx, float ny, float ntx, float nty) {
            x = nx; y = ny; tx = ntx; ty = nty;
        }
    };
    struct ColoredPoint2D {
        float x, y;
        unsigned char r, g, b, a;
        void set(float nx, float ny, uchar nr, uchar ng, uchar nb, uchar na) {
            x = nx; y = ny;
            r = nr; g = ng, b = nb; a = na;
        }
    };

    static const AttributeSet &defaultAttributes_Point2D();
    static const AttributeSet &defaultAttributes_TexturedPoint2D();
    static const AttributeSet &defaultAttributes_ColoredPoint2D();

    QSGGeometry(const QSGGeometry::AttributeSet &attribs,
                int vertexCount,
                int indexCount = 0,
                int indexType = UnsignedShortType);
    virtual ~QSGGeometry();

    // must use unsigned int to be compatible with the old GLenum to keep BC
    void setDrawingMode(unsigned int mode);
    inline unsigned int drawingMode() const { return m_drawing_mode; }

    void allocate(int vertexCount, int indexCount = 0);

    int vertexCount() const { return m_vertex_count; }

    void *vertexData() { return m_data; }
    inline Point2D *vertexDataAsPoint2D();
    inline TexturedPoint2D *vertexDataAsTexturedPoint2D();
    inline ColoredPoint2D *vertexDataAsColoredPoint2D();

    inline const void *vertexData() const { return m_data; }
    inline const Point2D *vertexDataAsPoint2D() const;
    inline const TexturedPoint2D *vertexDataAsTexturedPoint2D() const;
    inline const ColoredPoint2D *vertexDataAsColoredPoint2D() const;

    inline int indexType() const { return m_index_type; }

    int indexCount() const { return m_index_count; }

    void *indexData();
    inline uint *indexDataAsUInt();
    inline quint16 *indexDataAsUShort();

    inline int sizeOfIndex() const;

    const void *indexData() const;
    inline const uint *indexDataAsUInt() const;
    inline const quint16 *indexDataAsUShort() const;

    inline int attributeCount() const { return m_attributes.count; }
    inline const Attribute *attributes() const { return m_attributes.attributes; }
    inline int sizeOfVertex() const { return m_attributes.stride; }

    static void updateRectGeometry(QSGGeometry *g, const QRectF &rect);
    static void updateTexturedRectGeometry(QSGGeometry *g, const QRectF &rect, const QRectF &sourceRect);
    static void updateColoredRectGeometry(QSGGeometry *g, const QRectF &rect);

    void setIndexDataPattern(DataPattern p);
    DataPattern indexDataPattern() const { return DataPattern(m_index_usage_pattern); }

    void setVertexDataPattern(DataPattern p);
    DataPattern vertexDataPattern() const { return DataPattern(m_vertex_usage_pattern); }

    void markIndexDataDirty();
    void markVertexDataDirty();

    float lineWidth() const;
    void setLineWidth(float w);

private:
    friend class QSGGeometryData;

    int m_drawing_mode;
    int m_vertex_count;
    int m_index_count;
    int m_index_type;
    const AttributeSet &m_attributes;
    void *m_data;
    int m_index_data_offset;

    QSGGeometryData *m_server_data;

    uint m_owns_data : 1;
    uint m_index_usage_pattern : 2;
    uint m_vertex_usage_pattern : 2;
    uint m_dirty_index_data : 1;
    uint m_dirty_vertex_data : 1;
    uint m_reserved_bits : 25;

    float m_prealloc[16];

    float m_line_width;
};

inline uint *QSGGeometry::indexDataAsUInt()
{
    Q_ASSERT(m_index_type == UnsignedIntType);
    return static_cast<uint *>(indexData());
}

inline quint16 *QSGGeometry::indexDataAsUShort()
{
    Q_ASSERT(m_index_type == UnsignedShortType);
    return static_cast<quint16 *>(indexData());
}

inline const uint *QSGGeometry::indexDataAsUInt() const
{
    Q_ASSERT(m_index_type == UnsignedIntType);
    return static_cast<const uint *>(indexData());
}

inline const quint16 *QSGGeometry::indexDataAsUShort() const
{
    Q_ASSERT(m_index_type == UnsignedShortType);
    return static_cast<const quint16 *>(indexData());
}

inline QSGGeometry::Point2D *QSGGeometry::vertexDataAsPoint2D()
{
    Q_ASSERT(m_attributes.count == 1);
    Q_ASSERT(m_attributes.stride == 2 * sizeof(float));
    Q_ASSERT(m_attributes.attributes[0].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[0].type == FloatType);
    Q_ASSERT(m_attributes.attributes[0].position == 0);
    return static_cast<Point2D *>(m_data);
}

inline QSGGeometry::TexturedPoint2D *QSGGeometry::vertexDataAsTexturedPoint2D()
{
    Q_ASSERT(m_attributes.count == 2);
    Q_ASSERT(m_attributes.stride == 4 * sizeof(float));
    Q_ASSERT(m_attributes.attributes[0].position == 0);
    Q_ASSERT(m_attributes.attributes[0].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[0].type == FloatType);
    Q_ASSERT(m_attributes.attributes[1].position == 1);
    Q_ASSERT(m_attributes.attributes[1].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[1].type == FloatType);
    return static_cast<TexturedPoint2D *>(m_data);
}

inline QSGGeometry::ColoredPoint2D *QSGGeometry::vertexDataAsColoredPoint2D()
{
    Q_ASSERT(m_attributes.count == 2);
    Q_ASSERT(m_attributes.stride == 2 * sizeof(float) + 4 * sizeof(char));
    Q_ASSERT(m_attributes.attributes[0].position == 0);
    Q_ASSERT(m_attributes.attributes[0].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[0].type == FloatType);
    Q_ASSERT(m_attributes.attributes[1].position == 1);
    Q_ASSERT(m_attributes.attributes[1].tupleSize == 4);
    Q_ASSERT(m_attributes.attributes[1].type == UnsignedByteType);
    return static_cast<ColoredPoint2D *>(m_data);
}

inline const QSGGeometry::Point2D *QSGGeometry::vertexDataAsPoint2D() const
{
    Q_ASSERT(m_attributes.count == 1);
    Q_ASSERT(m_attributes.stride == 2 * sizeof(float));
    Q_ASSERT(m_attributes.attributes[0].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[0].type == FloatType);
    Q_ASSERT(m_attributes.attributes[0].position == 0);
    return static_cast<const Point2D *>(m_data);
}

inline const QSGGeometry::TexturedPoint2D *QSGGeometry::vertexDataAsTexturedPoint2D() const
{
    Q_ASSERT(m_attributes.count == 2);
    Q_ASSERT(m_attributes.stride == 4 * sizeof(float));
    Q_ASSERT(m_attributes.attributes[0].position == 0);
    Q_ASSERT(m_attributes.attributes[0].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[0].type == FloatType);
    Q_ASSERT(m_attributes.attributes[1].position == 1);
    Q_ASSERT(m_attributes.attributes[1].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[1].type == FloatType);
    return static_cast<const TexturedPoint2D *>(m_data);
}

inline const QSGGeometry::ColoredPoint2D *QSGGeometry::vertexDataAsColoredPoint2D() const
{
    Q_ASSERT(m_attributes.count == 2);
    Q_ASSERT(m_attributes.stride == 2 * sizeof(float) + 4 * sizeof(char));
    Q_ASSERT(m_attributes.attributes[0].position == 0);
    Q_ASSERT(m_attributes.attributes[0].tupleSize == 2);
    Q_ASSERT(m_attributes.attributes[0].type == FloatType);
    Q_ASSERT(m_attributes.attributes[1].position == 1);
    Q_ASSERT(m_attributes.attributes[1].tupleSize == 4);
    Q_ASSERT(m_attributes.attributes[1].type == UnsignedByteType);
    return static_cast<const ColoredPoint2D *>(m_data);
}

int QSGGeometry::sizeOfIndex() const
{
    if (m_index_type == UnsignedShortType) return 2;
    else if (m_index_type == UnsignedByteType) return 1;
    else if (m_index_type == UnsignedIntType) return 4;
    return 0;
}

QT_END_NAMESPACE

#endif // QSGGEOMETRY_H
