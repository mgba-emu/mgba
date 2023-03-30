/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "scripting/ScriptingController.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWidget>

#include "CoreController.h"
#include "Display.h"
#include "input/Gamepad.h"
#include "input/GamepadButtonEvent.h"
#include "input/GamepadHatEvent.h"
#include "InputController.h"
#include "scripting/ScriptingTextBuffer.h"
#include "scripting/ScriptingTextBufferModel.h"

#include <mgba/script.h>
#include <mgba-util/math.h>
#include <mgba-util/string.h>

using namespace QGBA;

ScriptingController::ScriptingController(QObject* parent)
	: QObject(parent)
{
	m_logger.p = this;
	m_logger.log = [](mLogger* log, int, enum mLogLevel level, const char* format, va_list args) {
		Logger* logger = static_cast<Logger*>(log);
		va_list argc;
		va_copy(argc, args);
		QString message = QString::vasprintf(format, argc);
		va_end(argc);
		switch (level) {
		case mLOG_WARN:
			emit logger->p->warn(message);
			break;
		case mLOG_ERROR:
			emit logger->p->error(message);
			break;
		default:
			emit logger->p->log(message);
			break;
		}
	};

	m_bufferModel = new ScriptingTextBufferModel(this);
	QObject::connect(m_bufferModel, &ScriptingTextBufferModel::textBufferCreated, this, &ScriptingController::textBufferCreated);

	connect(&m_storageFlush, &QTimer::timeout, this, &ScriptingController::flushStorage);
	m_storageFlush.setInterval(5);

	mScriptGamepadInit(&m_gamepad);

	init();
}

ScriptingController::~ScriptingController() {
	clearController();
	mScriptContextDeinit(&m_scriptContext);
	mScriptGamepadDeinit(&m_gamepad);
}

void ScriptingController::setController(std::shared_ptr<CoreController> controller) {
	if (controller == m_controller) {
		return;
	}
	clearController();
	m_controller = controller;
	CoreController::Interrupter interrupter(m_controller);
	m_controller->thread()->scriptContext = &m_scriptContext;
	if (m_controller->hasStarted()) {
		mScriptContextAttachCore(&m_scriptContext, m_controller->thread()->core);
	}
	connect(m_controller.get(), &CoreController::stopping, this, &ScriptingController::clearController);
}

void ScriptingController::setInputController(InputController* input) {
	if (m_inputController) {
		m_inputController->disconnect(this);
	}
	m_inputController = input;
	connect(m_inputController, &InputController::updated, this, &ScriptingController::updateGamepad);
}

bool ScriptingController::loadFile(const QString& path) {
	VFileDevice vf(path, QIODevice::ReadOnly);
	if (!vf.isOpen()) {
		return false;
	}
	return load(vf, path);
}

bool ScriptingController::load(VFileDevice& vf, const QString& name) {
	if (!m_activeEngine) {
		return false;
	}
	QByteArray utf8 = name.toUtf8();
	CoreController::Interrupter interrupter(m_controller);
	if (m_controller) {
		m_controller->setSync(false);
		m_controller->unpaused();
	}
	bool ok = true;
	if (!m_activeEngine->load(m_activeEngine, utf8.constData(), vf) || !m_activeEngine->run(m_activeEngine)) {
		emit error(QString::fromUtf8(m_activeEngine->getError(m_activeEngine)));
		ok = false;
	}
	if (m_controller) {
		m_controller->setSync(true);
		if (m_controller->isPaused()) {
			m_controller->paused();
		}
	}
	return ok;
}

void ScriptingController::clearController() {
	if (!m_controller) {
		return;
	}
	{
		CoreController::Interrupter interrupter(m_controller);
		mScriptContextDetachCore(&m_scriptContext);
		m_controller->thread()->scriptContext = nullptr;
	}
	m_controller.reset();
}

void ScriptingController::reset() {
	CoreController::Interrupter interrupter(m_controller);
	m_bufferModel->reset();
	mScriptContextDetachCore(&m_scriptContext);
	mScriptContextDeinit(&m_scriptContext);
	m_engines.clear();
	m_activeEngine = nullptr;
	init();
	if (m_controller && m_controller->hasStarted()) {
		mScriptContextAttachCore(&m_scriptContext, m_controller->thread()->core);
	}
}

void ScriptingController::runCode(const QString& code) {
	VFileDevice vf(code.toUtf8());
	load(vf, "*prompt");
}

void ScriptingController::flushStorage() {
#ifdef USE_JSON_C
	mScriptStorageFlushAll(&m_scriptContext);
#endif
}

bool ScriptingController::eventFilter(QObject* obj, QEvent* ev) {
	event(obj, ev);
	return false;
}

void ScriptingController::event(QObject* obj, QEvent* event) {
	if (!m_controller) {
		return;
	}

	switch (event->type()) {
	case QEvent::FocusOut:
	case QEvent::WindowDeactivate:
		mScriptContextClearKeys(&m_scriptContext);
		return;
	case QEvent::KeyPress:
	case QEvent::KeyRelease: {
		struct mScriptKeyEvent ev{mSCRIPT_EV_TYPE_KEY};
		auto keyEvent = static_cast<QKeyEvent*>(event);
		ev.state = event->type() == QEvent::KeyRelease ? mSCRIPT_INPUT_STATE_UP :
			static_cast<QKeyEvent*>(event)->isAutoRepeat() ? mSCRIPT_INPUT_STATE_HELD : mSCRIPT_INPUT_STATE_DOWN;
		ev.key = qtToScriptingKey(keyEvent);
		ev.modifiers = qtToScriptingModifiers(keyEvent->modifiers());
		mScriptContextFireEvent(&m_scriptContext, &ev.d);
		return;
	}
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease: {
		struct mScriptMouseButtonEvent ev{mSCRIPT_EV_TYPE_MOUSE_BUTTON};
		auto mouseEvent = static_cast<QMouseEvent*>(event);
		ev.mouse = 0;
		ev.state = event->type() == QEvent::MouseButtonPress ? mSCRIPT_INPUT_STATE_DOWN : mSCRIPT_INPUT_STATE_UP;
		ev.button = 31 - clz32(mouseEvent->button());
		mScriptContextFireEvent(&m_scriptContext, &ev.d);
		return;
	}
	case QEvent::MouseMove: {
		struct mScriptMouseMoveEvent ev{mSCRIPT_EV_TYPE_MOUSE_MOVE};
		auto mouseEvent = static_cast<QMouseEvent*>(event);
		QPoint pos = mouseEvent->pos();
		pos = static_cast<Display*>(obj)->normalizedPoint(m_controller.get(), pos);
		ev.mouse = 0;
		ev.x = pos.x();
		ev.y = pos.y();
		mScriptContextFireEvent(&m_scriptContext, &ev.d);
		return;
	}
	case QEvent::Wheel: {
		struct mScriptMouseWheelEvent ev{mSCRIPT_EV_TYPE_MOUSE_WHEEL};
		auto wheelEvent = static_cast<QWheelEvent*>(event);
		QPoint adelta = wheelEvent->angleDelta();
		QPoint pdelta = wheelEvent->pixelDelta();
		ev.mouse = 0;
		if (!pdelta.isNull()) {
			ev.x = pdelta.x();
			ev.y = pdelta.y();
		} else {
			ev.x = adelta.x();
			ev.y = adelta.y();
		}
		mScriptContextFireEvent(&m_scriptContext, &ev.d);
		return;
	}
	default:
		break;
	}

	auto type = event->type();
	if (type == GamepadButtonEvent::Down() || type == GamepadButtonEvent::Up()) {
		struct mScriptGamepadButtonEvent ev{mSCRIPT_EV_TYPE_GAMEPAD_BUTTON};
		auto gamepadEvent = static_cast<GamepadButtonEvent*>(event);
		ev.pad = 0;
		ev.state = event->type() == GamepadButtonEvent::Down() ? mSCRIPT_INPUT_STATE_DOWN : mSCRIPT_INPUT_STATE_UP;
		ev.button = gamepadEvent->value();
		mScriptContextFireEvent(&m_scriptContext, &ev.d);
	}
	if (type == GamepadHatEvent::Type()) {
		struct mScriptGamepadHatEvent ev{mSCRIPT_EV_TYPE_GAMEPAD_HAT};
		updateGamepad();
		auto gamepadEvent = static_cast<GamepadHatEvent*>(event);
		ev.pad = 0;
		ev.hat = gamepadEvent->hatId();
		ev.direction = gamepadEvent->direction();
		mScriptContextFireEvent(&m_scriptContext, &ev.d);
	}
}

void ScriptingController::updateGamepad() {
	InputDriver* driver = m_inputController->gamepadDriver();
	if (!driver) {
		detachGamepad();
		return;
	}
	Gamepad* gamepad = driver->activeGamepad();
	if (!gamepad) {
		detachGamepad();
		return;
	}

	QString name = gamepad->name();
	strlcpy(m_gamepad.internalName, name.toUtf8().constData(), sizeof(m_gamepad.internalName));
	name = gamepad->visibleName();
	strlcpy(m_gamepad.visibleName, name.toUtf8().constData(), sizeof(m_gamepad.visibleName));
	attachGamepad();

	QList<bool> buttons = gamepad->currentButtons();
	int nButtons = gamepad->buttonCount();
	mScriptGamepadSetButtonCount(&m_gamepad, nButtons);
	for (int i = 0; i < nButtons; ++i) {
		mScriptGamepadSetButton(&m_gamepad, i, buttons.at(i));
	}

	QList<int16_t> axes = gamepad->currentAxes();
	int nAxes = gamepad->axisCount();
	mScriptGamepadSetAxisCount(&m_gamepad, nAxes);
	for (int i = 0; i < nAxes; ++i) {
		mScriptGamepadSetAxis(&m_gamepad, i, axes.at(i));
	}

	QList<GamepadHatEvent::Direction> hats = gamepad->currentHats();
	int nHats = gamepad->hatCount();
	mScriptGamepadSetHatCount(&m_gamepad, nHats);
	for (int i = 0; i < nHats; ++i) {
		mScriptGamepadSetHat(&m_gamepad, i, hats.at(i));
	}
}

void ScriptingController::attachGamepad() {
	mScriptGamepad* pad = mScriptContextGamepadLookup(&m_scriptContext, 0);
	if (pad == &m_gamepad) {
		return;
	}
	if (pad) {
		mScriptContextGamepadDetach(&m_scriptContext, 0);
	}
	mScriptContextGamepadAttach(&m_scriptContext, &m_gamepad);
}

void ScriptingController::detachGamepad() {
	mScriptGamepad* pad = mScriptContextGamepadLookup(&m_scriptContext, 0);
	if (pad != &m_gamepad) {
		return;
	}
	mScriptContextGamepadDetach(&m_scriptContext, 0);
}

void ScriptingController::init() {
	mScriptContextInit(&m_scriptContext);
	mScriptContextAttachStdlib(&m_scriptContext);
#ifdef USE_JSON_C
	mScriptContextAttachStorage(&m_scriptContext);
#endif
	mScriptContextAttachSocket(&m_scriptContext);
	mScriptContextAttachInput(&m_scriptContext);
	mScriptContextRegisterEngines(&m_scriptContext);

	mScriptContextAttachLogger(&m_scriptContext, &m_logger);
	m_bufferModel->attachToContext(&m_scriptContext);

	HashTableEnumerate(&m_scriptContext.engines, [](const char* key, void* engine, void* context) {
	ScriptingController* self = static_cast<ScriptingController*>(context);
		self->m_engines[QString::fromUtf8(key)] = static_cast<mScriptEngineContext*>(engine);
	}, this);

	if (m_engines.count() == 1) {
		m_activeEngine = *m_engines.begin();
	}

#ifdef USE_JSON_C
	m_storageFlush.start();
#endif
}

uint32_t ScriptingController::qtToScriptingKey(const QKeyEvent* event) {
	QString text(event->text());
	int key = event->key();

	static const QHash<int, uint32_t> keypadMap{
		{'0', mSCRIPT_KEY_KP_0},
		{'1', mSCRIPT_KEY_KP_1},
		{'2', mSCRIPT_KEY_KP_2},
		{'3', mSCRIPT_KEY_KP_3},
		{'4', mSCRIPT_KEY_KP_4},
		{'5', mSCRIPT_KEY_KP_5},
		{'6', mSCRIPT_KEY_KP_6},
		{'7', mSCRIPT_KEY_KP_7},
		{'8', mSCRIPT_KEY_KP_8},
		{'9', mSCRIPT_KEY_KP_9},
		{'+', mSCRIPT_KEY_KP_PLUS},
		{'-', mSCRIPT_KEY_KP_MINUS},
		{'*', mSCRIPT_KEY_KP_MULTIPLY},
		{'/', mSCRIPT_KEY_KP_DIVIDE},
		{',', mSCRIPT_KEY_KP_COMMA},
		{'.', mSCRIPT_KEY_KP_POINT},
		{Qt::Key_Enter, mSCRIPT_KEY_KP_ENTER},
	};
	static const QHash<int, uint32_t> extraKeyMap{
		{Qt::Key_Escape, mSCRIPT_KEY_ESCAPE},
		{Qt::Key_Tab, mSCRIPT_KEY_TAB},
		{Qt::Key_Backtab, mSCRIPT_KEY_BACKSPACE},
		{Qt::Key_Backspace, mSCRIPT_KEY_BACKSPACE},
		{Qt::Key_Return, mSCRIPT_KEY_ENTER},
		{Qt::Key_Enter, mSCRIPT_KEY_ENTER},
		{Qt::Key_Insert, mSCRIPT_KEY_INSERT},
		{Qt::Key_Delete, mSCRIPT_KEY_DELETE},
		{Qt::Key_Pause, mSCRIPT_KEY_BREAK},
		{Qt::Key_Print, mSCRIPT_KEY_PRINT_SCREEN},
		{Qt::Key_SysReq, mSCRIPT_KEY_SYSRQ},
		{Qt::Key_Clear, mSCRIPT_KEY_CLEAR},
		{Qt::Key_Home, mSCRIPT_KEY_HOME},
		{Qt::Key_End, mSCRIPT_KEY_END},
		{Qt::Key_Left, mSCRIPT_KEY_LEFT},
		{Qt::Key_Up, mSCRIPT_KEY_UP},
		{Qt::Key_Right, mSCRIPT_KEY_RIGHT},
		{Qt::Key_Down, mSCRIPT_KEY_DOWN},
		{Qt::Key_PageUp, mSCRIPT_KEY_PAGE_UP},
		{Qt::Key_PageDown, mSCRIPT_KEY_DOWN},
		{Qt::Key_Shift, mSCRIPT_KEY_SHIFT},
		{Qt::Key_Control, mSCRIPT_KEY_CONTROL},
		{Qt::Key_Meta, mSCRIPT_KEY_SUPER},
		{Qt::Key_Alt, mSCRIPT_KEY_ALT},
		{Qt::Key_CapsLock, mSCRIPT_KEY_CAPS_LOCK},
		{Qt::Key_NumLock, mSCRIPT_KEY_NUM_LOCK},
		{Qt::Key_ScrollLock, mSCRIPT_KEY_SCROLL_LOCK},
		{Qt::Key_F1, mSCRIPT_KEY_F1},
		{Qt::Key_F2, mSCRIPT_KEY_F2},
		{Qt::Key_F3, mSCRIPT_KEY_F3},
		{Qt::Key_F4, mSCRIPT_KEY_F4},
		{Qt::Key_F5, mSCRIPT_KEY_F5},
		{Qt::Key_F6, mSCRIPT_KEY_F6},
		{Qt::Key_F7, mSCRIPT_KEY_F7},
		{Qt::Key_F8, mSCRIPT_KEY_F8},
		{Qt::Key_F9, mSCRIPT_KEY_F9},
		{Qt::Key_F10, mSCRIPT_KEY_F10},
		{Qt::Key_F11, mSCRIPT_KEY_F11},
		{Qt::Key_F12, mSCRIPT_KEY_F12},
		{Qt::Key_F13, mSCRIPT_KEY_F13},
		{Qt::Key_F14, mSCRIPT_KEY_F14},
		{Qt::Key_F15, mSCRIPT_KEY_F15},
		{Qt::Key_F16, mSCRIPT_KEY_F16},
		{Qt::Key_F17, mSCRIPT_KEY_F17},
		{Qt::Key_F18, mSCRIPT_KEY_F18},
		{Qt::Key_F19, mSCRIPT_KEY_F19},
		{Qt::Key_F20, mSCRIPT_KEY_F20},
		{Qt::Key_F21, mSCRIPT_KEY_F21},
		{Qt::Key_F22, mSCRIPT_KEY_F22},
		{Qt::Key_F23, mSCRIPT_KEY_F23},
		{Qt::Key_F24, mSCRIPT_KEY_F24},
		{Qt::Key_Menu, mSCRIPT_KEY_MENU},
		{Qt::Key_Super_L, mSCRIPT_KEY_SUPER},
		{Qt::Key_Super_R, mSCRIPT_KEY_SUPER},
		{Qt::Key_Help, mSCRIPT_KEY_HELP},
		{Qt::Key_Hyper_L, mSCRIPT_KEY_SUPER},
		{Qt::Key_Hyper_R, mSCRIPT_KEY_SUPER},
	};

	if (event->modifiers() & Qt::KeypadModifier && keypadMap.contains(key)) {
		return keypadMap[key];
	}
	if (key >= 0 && key < 0x100) {
		return key;
	}
	if (key < 0x01000000) {
		if (text.isEmpty()) {
			return 0;
		}
		QChar high = text[0];
		if (!high.isSurrogate()) {
			return high.unicode();
		}
		if (text.size() < 2) {
			return 0;
		}
		return QChar::surrogateToUcs4(high, text[1]);
	}

	if (extraKeyMap.contains(key)) {
		return extraKeyMap[key];
	}
	return 0;
}


uint16_t ScriptingController::qtToScriptingModifiers(Qt::KeyboardModifiers modifiers) {
	uint16_t mod = 0;
	if (modifiers & Qt::ShiftModifier) {
		mod |= mSCRIPT_KMOD_SHIFT;
	}
	if (modifiers & Qt::ControlModifier) {
		mod |= mSCRIPT_KMOD_CONTROL;
	}
	if (modifiers & Qt::AltModifier) {
		mod |= mSCRIPT_KMOD_ALT;
	}
	if (modifiers & Qt::MetaModifier) {
		mod |= mSCRIPT_KMOD_SUPER;
	}
	return mod;
}
