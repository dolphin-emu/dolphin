/****************************************************************************
** Meta object code from reading C++ file 'CheatsManager.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/CheatsManager.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CheatsManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_CheatsManager_t {
    uint offsetsAndSizes[18];
    char stringdata0[14];
    char stringdata1[20];
    char stringdata2[1];
    char stringdata3[24];
    char stringdata4[11];
    char stringdata5[4];
    char stringdata6[8];
    char stringdata7[13];
    char stringdata8[5];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CheatsManager_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CheatsManager_t qt_meta_stringdata_CheatsManager = {
    {
        QT_MOC_LITERAL(0, 13),  // "CheatsManager"
        QT_MOC_LITERAL(14, 19),  // "OpenGeneralSettings"
        QT_MOC_LITERAL(34, 0),  // ""
        QT_MOC_LITERAL(35, 23),  // "OpenAchievementSettings"
        QT_MOC_LITERAL(59, 10),  // "ShowMemory"
        QT_MOC_LITERAL(70, 3),  // "u32"
        QT_MOC_LITERAL(74, 7),  // "address"
        QT_MOC_LITERAL(82, 12),  // "RequestWatch"
        QT_MOC_LITERAL(95, 4)   // "name"
    },
    "CheatsManager",
    "OpenGeneralSettings",
    "",
    "OpenAchievementSettings",
    "ShowMemory",
    "u32",
    "address",
    "RequestWatch",
    "name"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CheatsManager[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   38,    2, 0x06,    1 /* Public */,
       3,    0,   39,    2, 0x06,    2 /* Public */,
       4,    1,   40,    2, 0x06,    3 /* Public */,
       7,    2,   43,    2, 0x06,    5 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 5,    8,    6,

       0        // eod
};

Q_CONSTINIT const QMetaObject CheatsManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_CheatsManager.offsetsAndSizes,
    qt_meta_data_CheatsManager,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CheatsManager_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CheatsManager, std::true_type>,
        // method 'OpenGeneralSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'OpenAchievementSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'ShowMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<u32, std::false_type>,
        // method 'RequestWatch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<u32, std::false_type>
    >,
    nullptr
} };

void CheatsManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CheatsManager *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->OpenGeneralSettings(); break;
        case 1: _t->OpenAchievementSettings(); break;
        case 2: _t->ShowMemory((*reinterpret_cast< std::add_pointer_t<u32>>(_a[1]))); break;
        case 3: _t->RequestWatch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<u32>>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CheatsManager::*)();
            if (_t _q_method = &CheatsManager::OpenGeneralSettings; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (CheatsManager::*)();
            if (_t _q_method = &CheatsManager::OpenAchievementSettings; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (CheatsManager::*)(u32 );
            if (_t _q_method = &CheatsManager::ShowMemory; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (CheatsManager::*)(QString , u32 );
            if (_t _q_method = &CheatsManager::RequestWatch; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject *CheatsManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CheatsManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CheatsManager.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int CheatsManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void CheatsManager::OpenGeneralSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void CheatsManager::OpenAchievementSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void CheatsManager::ShowMemory(u32 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void CheatsManager::RequestWatch(QString _t1, u32 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
