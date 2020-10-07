/****************************************************************************
**
** Copyright (C) 2014 Jeremy Lainé <jeremy.laine@m4x.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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


#ifndef QASN1ELEMENT_P_H
#define QASN1ELEMENT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtNetwork/private/qtnetworkglobal_p.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmap.h>

QT_BEGIN_NAMESPACE

// General
#define RSADSI_OID "1.2.840.113549."

#define RSA_ENCRYPTION_OID QByteArrayLiteral(RSADSI_OID "1.1.1")
#define DSA_ENCRYPTION_OID QByteArrayLiteral("1.2.840.10040.4.1")
#define EC_ENCRYPTION_OID QByteArrayLiteral("1.2.840.10045.2.1")
#define DH_ENCRYPTION_OID QByteArrayLiteral(RSADSI_OID "1.3.1")

// These are mostly from the RFC for PKCS#5
// PKCS#5: https://tools.ietf.org/html/rfc8018#appendix-B
#define PKCS5_OID RSADSI_OID "1.5."
// PKCS#12: https://tools.ietf.org/html/rfc7292#appendix-D)
#define PKCS12_OID RSADSI_OID "1.12."

// -PBES1
#define PKCS5_MD2_DES_CBC_OID QByteArrayLiteral(PKCS5_OID "1") // Not (yet) implemented
#define PKCS5_MD2_RC2_CBC_OID QByteArrayLiteral(PKCS5_OID "4") // Not (yet) implemented
#define PKCS5_MD5_DES_CBC_OID QByteArrayLiteral(PKCS5_OID "3")
#define PKCS5_MD5_RC2_CBC_OID QByteArrayLiteral(PKCS5_OID "6")
#define PKCS5_SHA1_DES_CBC_OID QByteArrayLiteral(PKCS5_OID "10")
#define PKCS5_SHA1_RC2_CBC_OID QByteArrayLiteral(PKCS5_OID "11")
#define PKCS12_SHA1_RC4_128_OID QByteArrayLiteral(PKCS12_OID "1.1") // Not (yet) implemented
#define PKCS12_SHA1_RC4_40_OID QByteArrayLiteral(PKCS12_OID "1.2") // Not (yet) implemented
#define PKCS12_SHA1_3KEY_3DES_CBC_OID QByteArrayLiteral(PKCS12_OID "1.3")
#define PKCS12_SHA1_2KEY_3DES_CBC_OID QByteArrayLiteral(PKCS12_OID "1.4")
#define PKCS12_SHA1_RC2_128_CBC_OID QByteArrayLiteral(PKCS12_OID "1.5")
#define PKCS12_SHA1_RC2_40_CBC_OID QByteArrayLiteral(PKCS12_OID "1.6")

// -PBKDF2
#define PKCS5_PBKDF2_ENCRYPTION_OID QByteArrayLiteral(PKCS5_OID "12")

// -PBES2
#define PKCS5_PBES2_ENCRYPTION_OID QByteArrayLiteral(PKCS5_OID "13")

// Digest
#define DIGEST_ALGORITHM_OID RSADSI_OID "2."
// -HMAC-SHA-1
#define HMAC_WITH_SHA1 QByteArrayLiteral(DIGEST_ALGORITHM_OID "7")
// -HMAC-SHA-2
#define HMAC_WITH_SHA224 QByteArrayLiteral(DIGEST_ALGORITHM_OID "8")
#define HMAC_WITH_SHA256 QByteArrayLiteral(DIGEST_ALGORITHM_OID "9")
#define HMAC_WITH_SHA384 QByteArrayLiteral(DIGEST_ALGORITHM_OID "10")
#define HMAC_WITH_SHA512 QByteArrayLiteral(DIGEST_ALGORITHM_OID "11")
#define HMAC_WITH_SHA512_224 QByteArrayLiteral(DIGEST_ALGORITHM_OID "12")
#define HMAC_WITH_SHA512_256 QByteArrayLiteral(DIGEST_ALGORITHM_OID "13")

// Encryption algorithms
#define ENCRYPTION_ALGORITHM_OID RSADSI_OID "3."
#define DES_CBC_ENCRYPTION_OID QByteArrayLiteral("1.3.14.3.2.7")
#define DES_EDE3_CBC_ENCRYPTION_OID QByteArrayLiteral(ENCRYPTION_ALGORITHM_OID "7")
#define RC2_CBC_ENCRYPTION_OID QByteArrayLiteral(ENCRYPTION_ALGORITHM_OID "2")
#define RC5_CBC_ENCRYPTION_OID QByteArrayLiteral(ENCRYPTION_ALGORITHM_OID "9") // Not (yet) implemented
#define AES_OID "2.16.840.1.101.3.4.1."
#define AES128_CBC_ENCRYPTION_OID QByteArrayLiteral(AES_OID "2")
#define AES192_CBC_ENCRYPTION_OID QByteArrayLiteral(AES_OID "22") // Not (yet) implemented
#define AES256_CBC_ENCRYPTION_OID QByteArrayLiteral(AES_OID "42") // Not (yet) implemented

class Q_AUTOTEST_EXPORT QAsn1Element
{
public:
    enum ElementType {
        // universal
        BooleanType = 0x01,
        IntegerType  = 0x02,
        BitStringType  = 0x03,
        OctetStringType = 0x04,
        NullType = 0x05,
        ObjectIdentifierType = 0x06,
        Utf8StringType = 0x0c,
        PrintableStringType = 0x13,
        TeletexStringType = 0x14,
        UtcTimeType = 0x17,
        GeneralizedTimeType = 0x18,
        SequenceType = 0x30,
        SetType = 0x31,

        // GeneralNameTypes
        Rfc822NameType = 0x81,
        DnsNameType = 0x82,
        UniformResourceIdentifierType = 0x86,
        IpAddressType = 0x87,

        // context specific
        Context0Type = 0xA0,
        Context1Type = 0xA1,
        Context3Type = 0xA3
    };

    explicit QAsn1Element(quint8 type = 0, const QByteArray &value = QByteArray());
    bool read(QDataStream &data);
    bool read(const QByteArray &data);
    void write(QDataStream &data) const;

    static QAsn1Element fromBool(bool val);
    static QAsn1Element fromInteger(unsigned int val);
    static QAsn1Element fromVector(const QVector<QAsn1Element> &items);
    static QAsn1Element fromObjectId(const QByteArray &id);

    bool toBool(bool *ok = nullptr) const;
    QDateTime toDateTime() const;
    QMultiMap<QByteArray, QString> toInfo() const;
    qint64 toInteger(bool *ok = nullptr) const;
    QVector<QAsn1Element> toVector() const;
    QByteArray toObjectId() const;
    QByteArray toObjectName() const;
    QString toString() const;

    quint8 type() const { return mType; }
    QByteArray value() const { return mValue; }

    friend inline bool operator==(const QAsn1Element &, const QAsn1Element &);
    friend inline bool operator!=(const QAsn1Element &, const QAsn1Element &);

private:
    quint8 mType;
    QByteArray mValue;
};
Q_DECLARE_TYPEINFO(QAsn1Element, Q_MOVABLE_TYPE);

inline bool operator==(const QAsn1Element &e1, const QAsn1Element &e2)
{ return e1.mType == e2.mType && e1.mValue == e2.mValue; }

inline bool operator!=(const QAsn1Element &e1, const QAsn1Element &e2)
{ return e1.mType != e2.mType || e1.mValue != e2.mValue; }

QT_END_NAMESPACE

#endif
