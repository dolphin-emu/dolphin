/****************************************************************************
** Meta object code from reading C++ file 'GameTracker.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/GameList/GameTracker.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'GameTracker.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_GameTracker_t {
    uint offsetsAndSizes[18];
    char stringdata0[12];
    char stringdata1[11];
    char stringdata2[1];
    char stringdata3[42];
    char stringdata4[5];
    char stringdata5[12];
    char stringdata6[12];
    char stringdata7[12];
    char stringdata8[5];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_GameTracker_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_GameTracker_t qt_meta_stringdata_GameTracker = {
    {
        QT_MOC_LITERAL(0, 11),  // "GameTracker"
        QT_MOC_LITERAL(12, 10),  // "GameLoaded"
        QT_MOC_LITERAL(23, 0),  // ""
        QT_MOC_LITERAL(24, 41),  // "std::shared_ptr<const UICommo..."
        QT_MOC_LITERAL(66, 4),  // "game"
        QT_MOC_LITERAL(71, 11),  // "GameUpdated"
        QT_MOC_LITERAL(83, 11),  // "GameRemoved"
        QT_MOC_LITERAL(95, 11),  // "std::string"
        QT_MOC_LITERAL(107, 4)   // "path"
    },
    "GameTracker",
    "GameLoaded",
    "",
    "std::shared_ptr<const UICommon::GameFile>",
    "game",
    "GameUpdated",
    "GameRemoved",
    "std::string",
    "path"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_GameTracker[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   32,    2, 0x06,    1 /* Public */,
       5,    1,   35,    2, 0x06,    3 /* Public */,
       6,    1,   38,    2, 0x06,    5 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 7,    8,

       0        // eod
};

Q_CONSTINIT const QMetaObject GameTracker::staticMetaObject = { {
    QMetaObject::SuperData::link<QFileSystemWatcher::staticMetaObject>(),
    qt_meta_stringdata_GameTracker.offsetsAndSizes,
    qt_meta_data_GameTracker,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_GameTracker_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<GameTracker, std::true_type>,
        // method 'GameLoaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::shared_ptr<const UICommon::GameFile> &, std::false_type>,
        // method 'GameUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::shared_ptr<const UICommon::GameFile> &, std::false_type>,
        // method 'GameRemoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const std::string &, std::false_type>
    >,
    nullptr
} };

void GameTracker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<GameTracker *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->GameLoaded((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const UICommon::GameFile>>>(_a[1]))); break;
        case 1: _t->GameUpdated((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const UICommon::GameFile>>>(_a[1]))); break;
        case 2: _t->GameRemoved((*reinterpret_cast< std::add_pointer_t<std::string>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< std::shared_ptr<const UICommon::GameFile> >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< std::shared_ptr<const UICommon::GameFile> >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< std::string >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (GameTracker::*)(const std::shared_ptr<const UICommon::GameFile> & );
            if (_t _q_method = &GameTracker::GameLoaded; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (GameTracker::*)(const std::shared_ptr<const UICommon::GameFile> & );
            if (_t _q_method = &GameTracker::GameUpdated; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (GameTracker::*)(const std::string & );
            if (_t _q_method = &GameTracker::GameRemoved; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *GameTracker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GameTracker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_GameTracker.stringdata0))
        return static_cast<void*>(this);
    return QFileSystemWatcher::qt_metacast(_clname);
}

int GameTracker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QFileSystemWatcher::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void GameTracker::GameLoaded(const std::shared_ptr<const UICommon::GameFile> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void GameTracker::GameUpdated(const std::shared_ptr<const UICommon::GameFile> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void GameTracker::GameRemoved(const std::string & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
