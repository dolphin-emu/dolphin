/****************************************************************************
** Meta object code from reading C++ file 'DiscordHandler.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/DiscordHandler.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DiscordHandler.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_DiscordHandler_t {
    uint offsetsAndSizes[16];
    char stringdata0[15];
    char stringdata1[5];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[12];
    char stringdata5[3];
    char stringdata6[12];
    char stringdata7[7];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_DiscordHandler_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_DiscordHandler_t qt_meta_stringdata_DiscordHandler = {
    {
        QT_MOC_LITERAL(0, 14),  // "DiscordHandler"
        QT_MOC_LITERAL(15, 4),  // "Join"
        QT_MOC_LITERAL(20, 0),  // ""
        QT_MOC_LITERAL(21, 11),  // "JoinRequest"
        QT_MOC_LITERAL(33, 11),  // "std::string"
        QT_MOC_LITERAL(45, 2),  // "id"
        QT_MOC_LITERAL(48, 11),  // "discord_tag"
        QT_MOC_LITERAL(60, 6)   // "avatar"
    },
    "DiscordHandler",
    "Join",
    "",
    "JoinRequest",
    "std::string",
    "id",
    "discord_tag",
    "avatar"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_DiscordHandler[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   26,    2, 0x06,    1 /* Public */,
       3,    3,   27,    2, 0x06,    2 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4, 0x80000000 | 4, 0x80000000 | 4,    5,    6,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject DiscordHandler::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_DiscordHandler.offsetsAndSizes,
    qt_meta_data_DiscordHandler,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_DiscordHandler_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<DiscordHandler, std::true_type>,
        // method 'Join'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'JoinRequest'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::string, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::string, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::string, std::false_type>
    >,
    nullptr
} };

void DiscordHandler::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DiscordHandler *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->Join(); break;
        case 1: _t->JoinRequest((*reinterpret_cast< std::add_pointer_t<std::string>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<std::string>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<std::string>>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (DiscordHandler::*)();
            if (_t _q_method = &DiscordHandler::Join; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (DiscordHandler::*)(const std::string , const std::string , const std::string );
            if (_t _q_method = &DiscordHandler::JoinRequest; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *DiscordHandler::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DiscordHandler::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DiscordHandler.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "Discord::Handler"))
        return static_cast< Discord::Handler*>(this);
    return QObject::qt_metacast(_clname);
}

int DiscordHandler::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void DiscordHandler::Join()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DiscordHandler::JoinRequest(const std::string _t1, const std::string _t2, const std::string _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
