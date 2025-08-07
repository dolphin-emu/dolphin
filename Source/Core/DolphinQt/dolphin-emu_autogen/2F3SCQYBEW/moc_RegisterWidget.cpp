/****************************************************************************
** Meta object code from reading C++ file 'RegisterWidget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/Debugger/RegisterWidget.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RegisterWidget.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_RegisterWidget_t {
    uint offsetsAndSizes[30];
    char stringdata0[15];
    char stringdata1[19];
    char stringdata2[1];
    char stringdata3[18];
    char stringdata4[4];
    char stringdata5[5];
    char stringdata6[20];
    char stringdata7[24];
    char stringdata8[13];
    char stringdata9[5];
    char stringdata10[12];
    char stringdata11[12];
    char stringdata12[18];
    char stringdata13[5];
    char stringdata14[16];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_RegisterWidget_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_RegisterWidget_t qt_meta_stringdata_RegisterWidget = {
    {
        QT_MOC_LITERAL(0, 14),  // "RegisterWidget"
        QT_MOC_LITERAL(15, 18),  // "RequestTableUpdate"
        QT_MOC_LITERAL(34, 0),  // ""
        QT_MOC_LITERAL(35, 17),  // "RequestViewInCode"
        QT_MOC_LITERAL(53, 3),  // "u32"
        QT_MOC_LITERAL(57, 4),  // "addr"
        QT_MOC_LITERAL(62, 19),  // "RequestViewInMemory"
        QT_MOC_LITERAL(82, 23),  // "RequestMemoryBreakpoint"
        QT_MOC_LITERAL(106, 12),  // "RequestWatch"
        QT_MOC_LITERAL(119, 4),  // "name"
        QT_MOC_LITERAL(124, 11),  // "UpdateTable"
        QT_MOC_LITERAL(136, 11),  // "UpdateValue"
        QT_MOC_LITERAL(148, 17),  // "QTableWidgetItem*"
        QT_MOC_LITERAL(166, 4),  // "item"
        QT_MOC_LITERAL(171, 15)   // "UpdateValueType"
    },
    "RegisterWidget",
    "RequestTableUpdate",
    "",
    "RequestViewInCode",
    "u32",
    "addr",
    "RequestViewInMemory",
    "RequestMemoryBreakpoint",
    "RequestWatch",
    "name",
    "UpdateTable",
    "UpdateValue",
    "QTableWidgetItem*",
    "item",
    "UpdateValueType"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_RegisterWidget[] = {

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
       1,    0,   62,    2, 0x06,    1 /* Public */,
       3,    1,   63,    2, 0x06,    2 /* Public */,
       6,    1,   66,    2, 0x06,    4 /* Public */,
       7,    1,   69,    2, 0x06,    6 /* Public */,
       8,    2,   72,    2, 0x06,    8 /* Public */,
      10,    0,   77,    2, 0x06,   11 /* Public */,
      11,    1,   78,    2, 0x06,   12 /* Public */,
      14,    1,   81,    2, 0x06,   14 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, QMetaType::QString, 0x80000000 | 4,    9,    5,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, 0x80000000 | 12,   13,

       0        // eod
};

Q_CONSTINIT const QMetaObject RegisterWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QDockWidget::staticMetaObject>(),
    qt_meta_stringdata_RegisterWidget.offsetsAndSizes,
    qt_meta_data_RegisterWidget,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_RegisterWidget_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<RegisterWidget, std::true_type>,
        // method 'RequestTableUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'RequestViewInCode'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<u32, std::false_type>,
        // method 'RequestViewInMemory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<u32, std::false_type>,
        // method 'RequestMemoryBreakpoint'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<u32, std::false_type>,
        // method 'RequestWatch'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<u32, std::false_type>,
        // method 'UpdateTable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'UpdateValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTableWidgetItem *, std::false_type>,
        // method 'UpdateValueType'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTableWidgetItem *, std::false_type>
    >,
    nullptr
} };

void RegisterWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<RegisterWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->RequestTableUpdate(); break;
        case 1: _t->RequestViewInCode((*reinterpret_cast< std::add_pointer_t<u32>>(_a[1]))); break;
        case 2: _t->RequestViewInMemory((*reinterpret_cast< std::add_pointer_t<u32>>(_a[1]))); break;
        case 3: _t->RequestMemoryBreakpoint((*reinterpret_cast< std::add_pointer_t<u32>>(_a[1]))); break;
        case 4: _t->RequestWatch((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<u32>>(_a[2]))); break;
        case 5: _t->UpdateTable(); break;
        case 6: _t->UpdateValue((*reinterpret_cast< std::add_pointer_t<QTableWidgetItem*>>(_a[1]))); break;
        case 7: _t->UpdateValueType((*reinterpret_cast< std::add_pointer_t<QTableWidgetItem*>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (RegisterWidget::*)();
            if (_t _q_method = &RegisterWidget::RequestTableUpdate; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)(u32 );
            if (_t _q_method = &RegisterWidget::RequestViewInCode; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)(u32 );
            if (_t _q_method = &RegisterWidget::RequestViewInMemory; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)(u32 );
            if (_t _q_method = &RegisterWidget::RequestMemoryBreakpoint; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)(QString , u32 );
            if (_t _q_method = &RegisterWidget::RequestWatch; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)();
            if (_t _q_method = &RegisterWidget::UpdateTable; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)(QTableWidgetItem * );
            if (_t _q_method = &RegisterWidget::UpdateValue; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (RegisterWidget::*)(QTableWidgetItem * );
            if (_t _q_method = &RegisterWidget::UpdateValueType; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
    }
}

const QMetaObject *RegisterWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RegisterWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_RegisterWidget.stringdata0))
        return static_cast<void*>(this);
    return QDockWidget::qt_metacast(_clname);
}

int RegisterWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDockWidget::qt_metacall(_c, _id, _a);
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
void RegisterWidget::RequestTableUpdate()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void RegisterWidget::RequestViewInCode(u32 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void RegisterWidget::RequestViewInMemory(u32 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void RegisterWidget::RequestMemoryBreakpoint(u32 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void RegisterWidget::RequestWatch(QString _t1, u32 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void RegisterWidget::UpdateTable()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void RegisterWidget::UpdateValue(QTableWidgetItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void RegisterWidget::UpdateValueType(QTableWidgetItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
