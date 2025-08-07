/****************************************************************************
** Meta object code from reading C++ file 'GameList.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../../Source/Core/DolphinQt/GameList/GameList.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'GameList.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_GameList_t {
    uint offsetsAndSizes[26];
    char stringdata0[9];
    char stringdata1[13];
    char stringdata2[1];
    char stringdata3[23];
    char stringdata4[19];
    char stringdata5[5];
    char stringdata6[12];
    char stringdata7[17];
    char stringdata8[42];
    char stringdata9[10];
    char stringdata10[20];
    char stringdata11[21];
    char stringdata12[24];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_GameList_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_GameList_t qt_meta_stringdata_GameList = {
    {
        QT_MOC_LITERAL(0, 8),  // "GameList"
        QT_MOC_LITERAL(9, 12),  // "GameSelected"
        QT_MOC_LITERAL(22, 0),  // ""
        QT_MOC_LITERAL(23, 22),  // "OnStartWithRiivolution"
        QT_MOC_LITERAL(46, 18),  // "UICommon::GameFile"
        QT_MOC_LITERAL(65, 4),  // "game"
        QT_MOC_LITERAL(70, 11),  // "NetPlayHost"
        QT_MOC_LITERAL(82, 16),  // "SelectionChanged"
        QT_MOC_LITERAL(99, 41),  // "std::shared_ptr<const UICommo..."
        QT_MOC_LITERAL(141, 9),  // "game_file"
        QT_MOC_LITERAL(151, 19),  // "OpenGeneralSettings"
        QT_MOC_LITERAL(171, 20),  // "OpenGraphicsSettings"
        QT_MOC_LITERAL(192, 23)   // "OpenAchievementSettings"
    },
    "GameList",
    "GameSelected",
    "",
    "OnStartWithRiivolution",
    "UICommon::GameFile",
    "game",
    "NetPlayHost",
    "SelectionChanged",
    "std::shared_ptr<const UICommon::GameFile>",
    "game_file",
    "OpenGeneralSettings",
    "OpenGraphicsSettings",
    "OpenAchievementSettings"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_GameList[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   56,    2, 0x06,    1 /* Public */,
       3,    1,   57,    2, 0x06,    2 /* Public */,
       6,    1,   60,    2, 0x06,    4 /* Public */,
       7,    1,   63,    2, 0x06,    6 /* Public */,
      10,    0,   66,    2, 0x06,    8 /* Public */,
      11,    0,   67,    2, 0x06,    9 /* Public */,
      12,    0,   68,    2, 0x06,   10 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, 0x80000000 | 4,    5,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject GameList::staticMetaObject = { {
    QMetaObject::SuperData::link<QStackedWidget::staticMetaObject>(),
    qt_meta_stringdata_GameList.offsetsAndSizes,
    qt_meta_data_GameList,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_GameList_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<GameList, std::true_type>,
        // method 'GameSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'OnStartWithRiivolution'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const UICommon::GameFile &, std::false_type>,
        // method 'NetPlayHost'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const UICommon::GameFile &, std::false_type>,
        // method 'SelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const UICommon::GameFile>, std::false_type>,
        // method 'OpenGeneralSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'OpenGraphicsSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'OpenAchievementSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void GameList::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<GameList *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->GameSelected(); break;
        case 1: _t->OnStartWithRiivolution((*reinterpret_cast< std::add_pointer_t<UICommon::GameFile>>(_a[1]))); break;
        case 2: _t->NetPlayHost((*reinterpret_cast< std::add_pointer_t<UICommon::GameFile>>(_a[1]))); break;
        case 3: _t->SelectionChanged((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const UICommon::GameFile>>>(_a[1]))); break;
        case 4: _t->OpenGeneralSettings(); break;
        case 5: _t->OpenGraphicsSettings(); break;
        case 6: _t->OpenAchievementSettings(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< std::shared_ptr<const UICommon::GameFile> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (GameList::*)();
            if (_t _q_method = &GameList::GameSelected; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (GameList::*)(const UICommon::GameFile & );
            if (_t _q_method = &GameList::OnStartWithRiivolution; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (GameList::*)(const UICommon::GameFile & );
            if (_t _q_method = &GameList::NetPlayHost; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (GameList::*)(std::shared_ptr<const UICommon::GameFile> );
            if (_t _q_method = &GameList::SelectionChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (GameList::*)();
            if (_t _q_method = &GameList::OpenGeneralSettings; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (GameList::*)();
            if (_t _q_method = &GameList::OpenGraphicsSettings; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (GameList::*)();
            if (_t _q_method = &GameList::OpenAchievementSettings; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }
}

const QMetaObject *GameList::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GameList::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_GameList.stringdata0))
        return static_cast<void*>(this);
    return QStackedWidget::qt_metacast(_clname);
}

int GameList::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QStackedWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void GameList::GameSelected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void GameList::OnStartWithRiivolution(const UICommon::GameFile & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void GameList::NetPlayHost(const UICommon::GameFile & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void GameList::SelectionChanged(std::shared_ptr<const UICommon::GameFile> _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void GameList::OpenGeneralSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void GameList::OpenGraphicsSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void GameList::OpenAchievementSettings()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
