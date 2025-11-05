/****************************************************************************
** Meta object code from reading C++ file 'AppController.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../client-qt/src/AppController.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AppController.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_AppController_t {
    uint offsetsAndSizes[62];
    char stringdata0[14];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[16];
    char stringdata4[20];
    char stringdata5[24];
    char stringdata6[17];
    char stringdata7[27];
    char stringdata8[20];
    char stringdata9[5];
    char stringdata10[5];
    char stringdata11[22];
    char stringdata12[7];
    char stringdata13[13];
    char stringdata14[9];
    char stringdata15[13];
    char stringdata16[13];
    char stringdata17[13];
    char stringdata18[13];
    char stringdata19[9];
    char stringdata20[9];
    char stringdata21[16];
    char stringdata22[21];
    char stringdata23[18];
    char stringdata24[9];
    char stringdata25[9];
    char stringdata26[13];
    char stringdata27[17];
    char stringdata28[10];
    char stringdata29[20];
    char stringdata30[11];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_AppController_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_AppController_t qt_meta_stringdata_AppController = {
    {
        QT_MOC_LITERAL(0, 13),  // "AppController"
        QT_MOC_LITERAL(14, 15),  // "authInfoChanged"
        QT_MOC_LITERAL(30, 0),  // ""
        QT_MOC_LITERAL(31, 15),  // "userListChanged"
        QT_MOC_LITERAL(47, 19),  // "conversationChanged"
        QT_MOC_LITERAL(67, 23),  // "conversationListChanged"
        QT_MOC_LITERAL(91, 16),  // "serverLogChanged"
        QT_MOC_LITERAL(108, 26),  // "currentConversationChanged"
        QT_MOC_LITERAL(135, 19),  // "registrationChanged"
        QT_MOC_LITERAL(155, 4),  // "send"
        QT_MOC_LITERAL(160, 4),  // "text"
        QT_MOC_LITERAL(165, 21),  // "startConversationWith"
        QT_MOC_LITERAL(187, 6),  // "userId"
        QT_MOC_LITERAL(194, 12),  // "rotateDevice"
        QT_MOC_LITERAL(207, 8),  // "deviceId"
        QT_MOC_LITERAL(216, 12),  // "revokeDevice"
        QT_MOC_LITERAL(229, 12),  // "refreshUsers"
        QT_MOC_LITERAL(242, 12),  // "simulatePull"
        QT_MOC_LITERAL(255, 12),  // "authenticate"
        QT_MOC_LITERAL(268, 8),  // "nickname"
        QT_MOC_LITERAL(277, 8),  // "password"
        QT_MOC_LITERAL(286, 15),  // "certificatePath"
        QT_MOC_LITERAL(302, 20),  // "completeRegistration"
        QT_MOC_LITERAL(323, 17),  // "resetRegistration"
        QT_MOC_LITERAL(341, 8),  // "authInfo"
        QT_MOC_LITERAL(350, 8),  // "userList"
        QT_MOC_LITERAL(359, 12),  // "conversation"
        QT_MOC_LITERAL(372, 16),  // "conversationList"
        QT_MOC_LITERAL(389, 9),  // "serverLog"
        QT_MOC_LITERAL(399, 19),  // "currentConversation"
        QT_MOC_LITERAL(419, 10)   // "registered"
    },
    "AppController",
    "authInfoChanged",
    "",
    "userListChanged",
    "conversationChanged",
    "conversationListChanged",
    "serverLogChanged",
    "currentConversationChanged",
    "registrationChanged",
    "send",
    "text",
    "startConversationWith",
    "userId",
    "rotateDevice",
    "deviceId",
    "revokeDevice",
    "refreshUsers",
    "simulatePull",
    "authenticate",
    "nickname",
    "password",
    "certificatePath",
    "completeRegistration",
    "resetRegistration",
    "authInfo",
    "userList",
    "conversation",
    "conversationList",
    "serverLog",
    "currentConversation",
    "registered"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_AppController[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       8,  150, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  110,    2, 0x06,    9 /* Public */,
       3,    0,  111,    2, 0x06,   10 /* Public */,
       4,    0,  112,    2, 0x06,   11 /* Public */,
       5,    0,  113,    2, 0x06,   12 /* Public */,
       6,    0,  114,    2, 0x06,   13 /* Public */,
       7,    0,  115,    2, 0x06,   14 /* Public */,
       8,    0,  116,    2, 0x06,   15 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       9,    1,  117,    2, 0x02,   16 /* Public */,
      11,    1,  120,    2, 0x02,   18 /* Public */,
      13,    2,  123,    2, 0x02,   20 /* Public */,
      15,    2,  128,    2, 0x02,   23 /* Public */,
      16,    0,  133,    2, 0x02,   26 /* Public */,
      17,    0,  134,    2, 0x02,   27 /* Public */,
      18,    3,  135,    2, 0x02,   28 /* Public */,
      22,    3,  142,    2, 0x02,   32 /* Public */,
      23,    0,  149,    2, 0x02,   36 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   12,   14,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   12,   14,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   19,   20,   21,
    QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   19,   20,   21,
    QMetaType::Void,

 // properties: name, type, flags
      24, QMetaType::QVariantMap, 0x00015001, uint(0), 0,
      25, QMetaType::QVariantList, 0x00015001, uint(1), 0,
      26, QMetaType::QVariantList, 0x00015001, uint(2), 0,
      27, QMetaType::QVariantList, 0x00015001, uint(3), 0,
      28, QMetaType::QStringList, 0x00015001, uint(4), 0,
      29, QMetaType::QString, 0x00015103, uint(5), 0,
      30, QMetaType::Bool, 0x00015001, uint(6), 0,
      19, QMetaType::QString, 0x00015001, uint(6), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject AppController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_AppController.offsetsAndSizes,
    qt_meta_data_AppController,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_AppController_t,
        // property 'authInfo'
        QtPrivate::TypeAndForceComplete<QVariantMap, std::true_type>,
        // property 'userList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'conversation'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'conversationList'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'serverLog'
        QtPrivate::TypeAndForceComplete<QStringList, std::true_type>,
        // property 'currentConversation'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'registered'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'nickname'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AppController, std::true_type>,
        // method 'authInfoChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'userListChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'conversationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'conversationListChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'serverLogChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'currentConversationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'registrationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'send'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'startConversationWith'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'rotateDevice'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'revokeDevice'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'refreshUsers'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'simulatePull'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'authenticate'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'completeRegistration'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'resetRegistration'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void AppController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AppController *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->authInfoChanged(); break;
        case 1: _t->userListChanged(); break;
        case 2: _t->conversationChanged(); break;
        case 3: _t->conversationListChanged(); break;
        case 4: _t->serverLogChanged(); break;
        case 5: _t->currentConversationChanged(); break;
        case 6: _t->registrationChanged(); break;
        case 7: _t->send((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->startConversationWith((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->rotateDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 10: _t->revokeDevice((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 11: _t->refreshUsers(); break;
        case 12: _t->simulatePull(); break;
        case 13: { QString _r = _t->authenticate((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 14: { QString _r = _t->completeRegistration((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 15: _t->resetRegistration(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::authInfoChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::userListChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::conversationChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::conversationListChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::serverLogChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::currentConversationChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (AppController::*)();
            if (_t _q_method = &AppController::registrationChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<AppController *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QVariantMap*>(_v) = _t->authInfo(); break;
        case 1: *reinterpret_cast< QVariantList*>(_v) = _t->userList(); break;
        case 2: *reinterpret_cast< QVariantList*>(_v) = _t->conversation(); break;
        case 3: *reinterpret_cast< QVariantList*>(_v) = _t->conversationList(); break;
        case 4: *reinterpret_cast< QStringList*>(_v) = _t->serverLog(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->currentConversation(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->isRegistered(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->nickname(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        auto *_t = static_cast<AppController *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 5: _t->setCurrentConversation(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *AppController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AppController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AppController.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AppController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void AppController::authInfoChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AppController::userListChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void AppController::conversationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void AppController::conversationListChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void AppController::serverLogChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void AppController::currentConversationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void AppController::registrationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
