/****************************************************************************
** Meta object code from reading C++ file 'Host.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/Host.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Host.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_Host_t {
    uint offsetsAndSizes[26];
    char stringdata0[5];
    char stringdata1[13];
    char stringdata2[1];
    char stringdata3[6];
    char stringdata4[12];
    char stringdata5[18];
    char stringdata6[2];
    char stringdata7[2];
    char stringdata8[19];
    char stringdata9[21];
    char stringdata10[20];
    char stringdata11[18];
    char stringdata12[22];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_Host_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_Host_t qt_meta_stringdata_Host = {
    {
        QT_MOC_LITERAL(0, 4),  // "Host"
        QT_MOC_LITERAL(5, 12),  // "RequestTitle"
        QT_MOC_LITERAL(18, 0),  // ""
        QT_MOC_LITERAL(19, 5),  // "title"
        QT_MOC_LITERAL(25, 11),  // "RequestStop"
        QT_MOC_LITERAL(37, 17),  // "RequestRenderSize"
        QT_MOC_LITERAL(55, 1),  // "w"
        QT_MOC_LITERAL(57, 1),  // "h"
        QT_MOC_LITERAL(59, 18),  // "UpdateDisasmDialog"
        QT_MOC_LITERAL(78, 20),  // "JitCacheInvalidation"
        QT_MOC_LITERAL(99, 19),  // "JitProfileDataWiped"
        QT_MOC_LITERAL(119, 17),  // "PPCSymbolsChanged"
        QT_MOC_LITERAL(137, 21)   // "PPCBreakpointsChanged"
    },
    "Host",
    "RequestTitle",
    "",
    "title",
    "RequestStop",
    "RequestRenderSize",
    "w",
    "h",
    "UpdateDisasmDialog",
    "JitCacheInvalidation",
    "JitProfileDataWiped",
    "PPCSymbolsChanged",
    "PPCBreakpointsChanged"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_Host[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       8,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   62,    2, 0x06,    1 /* Public */,
       4,    0,   65,    2, 0x06,    3 /* Public */,
       5,    2,   66,    2, 0x06,    4 /* Public */,
       8,    0,   71,    2, 0x06,    7 /* Public */,
       9,    0,   72,    2, 0x06,    8 /* Public */,
      10,    0,   73,    2, 0x06,    9 /* Public */,
      11,    0,   74,    2, 0x06,   10 /* Public */,
      12,    0,   75,    2, 0x06,   11 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    6,    7,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject Host::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_Host.offsetsAndSizes,
    qt_meta_data_Host,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_Host_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Host, std::true_type>,
        // method 'RequestTitle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'RequestStop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'RequestRenderSize'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'UpdateDisasmDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'JitCacheInvalidation'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'JitProfileDataWiped'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'PPCSymbolsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'PPCBreakpointsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void Host::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Host *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->RequestTitle((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->RequestStop(); break;
        case 2: _t->RequestRenderSize((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->UpdateDisasmDialog(); break;
        case 4: _t->JitCacheInvalidation(); break;
        case 5: _t->JitProfileDataWiped(); break;
        case 6: _t->PPCSymbolsChanged(); break;
        case 7: _t->PPCBreakpointsChanged(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Host::*)(const QString & );
            if (_t _q_method = &Host::RequestTitle; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Host::*)();
            if (_t _q_method = &Host::RequestStop; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (Host::*)(int , int );
            if (_t _q_method = &Host::RequestRenderSize; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (Host::*)();
            if (_t _q_method = &Host::UpdateDisasmDialog; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (Host::*)();
            if (_t _q_method = &Host::JitCacheInvalidation; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (Host::*)();
            if (_t _q_method = &Host::JitProfileDataWiped; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (Host::*)();
            if (_t _q_method = &Host::PPCSymbolsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (Host::*)();
            if (_t _q_method = &Host::PPCBreakpointsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
    }
}

const QMetaObject *Host::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Host::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Host.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Host::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void Host::RequestTitle(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void Host::RequestStop()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Host::RequestRenderSize(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void Host::UpdateDisasmDialog()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Host::JitCacheInvalidation()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Host::JitProfileDataWiped()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void Host::PPCSymbolsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void Host::PPCBreakpointsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
