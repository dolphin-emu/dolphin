/****************************************************************************
** Meta object code from reading C++ file 'MappingWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/Config/Mapping/MappingWindow.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MappingWindow.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_MappingWindow_t {
    uint offsetsAndSizes[18];
    char stringdata0[14];
    char stringdata1[14];
    char stringdata2[1];
    char stringdata3[7];
    char stringdata4[5];
    char stringdata5[22];
    char stringdata6[15];
    char stringdata7[20];
    char stringdata8[14];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_MappingWindow_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_MappingWindow_t qt_meta_stringdata_MappingWindow = {
    {
        QT_MOC_LITERAL(0, 13),  // "MappingWindow"
        QT_MOC_LITERAL(14, 13),  // "ConfigChanged"
        QT_MOC_LITERAL(28, 0),  // ""
        QT_MOC_LITERAL(29, 6),  // "Update"
        QT_MOC_LITERAL(36, 4),  // "Save"
        QT_MOC_LITERAL(41, 21),  // "UnQueueInputDetection"
        QT_MOC_LITERAL(63, 14),  // "MappingButton*"
        QT_MOC_LITERAL(78, 19),  // "QueueInputDetection"
        QT_MOC_LITERAL(98, 13)   // "CancelMapping"
    },
    "MappingWindow",
    "ConfigChanged",
    "",
    "Update",
    "Save",
    "UnQueueInputDetection",
    "MappingButton*",
    "QueueInputDetection",
    "CancelMapping"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_MappingWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x06,    1 /* Public */,
       3,    0,   51,    2, 0x06,    2 /* Public */,
       4,    0,   52,    2, 0x06,    3 /* Public */,
       5,    1,   53,    2, 0x06,    4 /* Public */,
       7,    1,   56,    2, 0x06,    6 /* Public */,
       8,    0,   59,    2, 0x06,    8 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 6,    2,
    QMetaType::Void, 0x80000000 | 6,    2,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject MappingWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_MappingWindow.offsetsAndSizes,
    qt_meta_data_MappingWindow,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_MappingWindow_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MappingWindow, std::true_type>,
        // method 'ConfigChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'Update'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'Save'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'UnQueueInputDetection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<MappingButton *, std::false_type>,
        // method 'QueueInputDetection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<MappingButton *, std::false_type>,
        // method 'CancelMapping'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void MappingWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MappingWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->ConfigChanged(); break;
        case 1: _t->Update(); break;
        case 2: _t->Save(); break;
        case 3: _t->UnQueueInputDetection((*reinterpret_cast< std::add_pointer_t<MappingButton*>>(_a[1]))); break;
        case 4: _t->QueueInputDetection((*reinterpret_cast< std::add_pointer_t<MappingButton*>>(_a[1]))); break;
        case 5: _t->CancelMapping(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MappingWindow::*)();
            if (_t _q_method = &MappingWindow::ConfigChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (MappingWindow::*)();
            if (_t _q_method = &MappingWindow::Update; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (MappingWindow::*)();
            if (_t _q_method = &MappingWindow::Save; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (MappingWindow::*)(MappingButton * );
            if (_t _q_method = &MappingWindow::UnQueueInputDetection; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (MappingWindow::*)(MappingButton * );
            if (_t _q_method = &MappingWindow::QueueInputDetection; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (MappingWindow::*)();
            if (_t _q_method = &MappingWindow::CancelMapping; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }
}

const QMetaObject *MappingWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MappingWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MappingWindow.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int MappingWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void MappingWindow::ConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void MappingWindow::Update()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void MappingWindow::Save()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void MappingWindow::UnQueueInputDetection(MappingButton * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void MappingWindow::QueueInputDetection(MappingButton * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void MappingWindow::CancelMapping()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
