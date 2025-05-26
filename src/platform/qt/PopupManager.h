/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QExplicitlySharedDataPointer>
#include <QPointer>
#include <QSharedData>

#include <functional>
#include <memory>
#include <type_traits>

#include "CoreConsumer.h"

namespace QGBA {

class CoreController;

class PopupManagerBase {
public:
	using CorePtr = std::shared_ptr<CoreController>;

	PopupManagerBase(const PopupManagerBase&) = default;
	virtual ~PopupManagerBase() = default;

	QWidget* construct();
	void show();
	inline void operator()() { show(); }

protected:
	class Private : public QSharedData {
	public:
		Private(PopupManagerBase* pub) : pub(pub) {}
		Private(const Private& other) = default;

		PopupManagerBase* pub;
		bool isModal = false;
		bool keepAlive = false;
		CoreConsumer controller;
		QMetaObject::Connection stopConnection;

		void setProvider(CoreProvider* provide);
		void updateConnections();
		virtual QWidget* window() const = 0;
		virtual void notifyWindow() = 0;
	};

	PopupManagerBase(Private* d);

	virtual void constructImpl() = 0;

	virtual Private* d() const { return m_d.data(); }
	virtual Private* d() { return m_d.data(); }

private:
	QExplicitlySharedDataPointer<Private> m_d;
};

template <class WINDOW>
class PopupManager : public PopupManagerBase {
	static_assert(std::is_convertible<WINDOW*, QWidget*>::value, "class must derive from QWidget");

protected:
	class Private;
	Private* d() const override { return static_cast<Private*>(PopupManagerBase::d()); }
	Private* d() override { return static_cast<Private*>(PopupManagerBase::d()); }

	template <typename T>
	struct HasSetController {
		using Pass = char;
		using Fail = int;
		struct Base { bool setController; };
		struct Test : T, Base {};
		template <typename U> static Fail Check(decltype(U::setController)*);
		template <typename U> static Pass Check(U*);
		static constexpr bool value = sizeof(Check<Test>(nullptr)) == sizeof(Pass);
	};

public:
	PopupManager() : PopupManagerBase(new Private(this)) {}
	PopupManager(const PopupManager&) = default;

	bool isNull() const { return d()->ptr.isNull(); }
	WINDOW* operator->() const { return d()->ptr; }
	WINDOW& operator*() const { return &d()->ptr; }
	operator WINDOW*() const { return d()->ptr; }

	PopupManager& withController(CoreProvider& provider) { d()->setProvider(&provider); return *this; }
	PopupManager& setModal(bool modal) { d()->isModal = modal; return *this; }
	PopupManager& setKeepAlive(bool keepAlive) { d()->keepAlive = keepAlive; return *this; }
	PopupManager& constructWith(const std::function<WINDOW*()>& ctor) { d()->construct = ctor; return *this; }

	template <typename... Args>
	PopupManager& constructWith(Args... args) {
		d()->construct = makeConstruct<WINDOW, Args...>(args...);
		return *this;
	}

protected:
	virtual void constructImpl() override {
		Private* d = this->d();
		if (!d->construct) {
			qWarning("No valid constructor specified for popup");
			return;
		}
		WINDOW* w = d->construct();
		if (!w) {
			qWarning("Constructor did not return a window");
			return;
		}
		d->ptr = w;
	}

	template <typename T, typename... Args>
	typename std::enable_if<std::is_constructible<T, CorePtr, Args...>::value, std::function<T*()>>::type makeConstruct(Args... args) {
		return [=]() -> T* { return new T(d()->controller.sharedController(), args...); };
	}

	template <typename T, typename... Args>
	typename std::enable_if<!std::is_constructible<T, CorePtr, Args...>::value, std::function<T*()>>::type makeConstruct(Args... args) {
		return [=]() -> T* { return new T(args...); };
	}

	class Private : public PopupManagerBase::Private {
		template <class T = WINDOW>
		struct Ctors {
			static constexpr bool useController = std::is_constructible<T, CorePtr>::value;
			static constexpr bool useDefault = !useController && std::is_default_constructible<T>::value;
			static constexpr bool noDefault = !useController && !useDefault;
		};

	public:
		template<class T = WINDOW>
		Private(PopupManagerBase* pub, typename std::enable_if<Ctors<T>::useDefault>::type* = 0)
		: PopupManagerBase::Private(pub), construct([]{ return new WINDOW(); }) {}

		template<class T = WINDOW>
		Private(PopupManagerBase* pub, typename std::enable_if<Ctors<T>::useController>::type* = 0)
		: PopupManagerBase::Private(pub), construct([this]{ return new WINDOW(controller.sharedController()); }) {}

		template<class T = WINDOW>
		Private(PopupManagerBase* pub, typename std::enable_if<Ctors<T>::noDefault>::type* = 0)
		: PopupManagerBase::Private(pub) {}

		~Private() {
			if (ptr && !keepAlive) {
				ptr->close();
				ptr->deleteLater();
			}
		}

		virtual QWidget* window() const override { return ptr.data(); }

		virtual void notifyWindow() override { notifyWindow<WINDOW>(); }

		template<class T>
		typename std::enable_if<HasSetController<T>::value>::type notifyWindow() {
			if (ptr) {
				ptr->setController(controller.sharedController());
			}
		}

		template<class T>
		typename std::enable_if<!HasSetController<T>::value>::type notifyWindow() {
			// Nothing to do
		}

		QPointer<WINDOW> ptr;
		std::function<WINDOW*()> construct;
	};
};

}
