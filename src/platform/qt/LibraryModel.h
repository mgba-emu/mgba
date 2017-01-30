/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LIBRARY_MODEL
#define QGBA_LIBRARY_MODEL

#include <QAbstractItemModel>
#include <QStringList>
#include <QThread>

#include <mgba/core/library.h>

#include <functional>

struct VDir;
struct VFile;
struct NoIntroDB;

namespace QGBA {

class LibraryLoader;
class LibraryModel : public QAbstractItemModel {
Q_OBJECT

public:
	LibraryModel(const QString& path, QObject* parent = nullptr);
	virtual ~LibraryModel();

	bool entryAt(int row, mLibraryEntry* out) const;
	VFile* openVFile(const QModelIndex& index) const;
	QString filename(const QModelIndex& index) const;
	QString location(const QModelIndex& index) const;

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	void attachGameDB(const NoIntroDB* gameDB);

signals:
	void doneLoading();

public slots:
	void loadDirectory(const QString& path);

	void constrainBase(const QString& path);
	void clearConstraints();

private slots:
	void directoryLoaded(const QString& path);

private:
	struct LibraryColumn {
		LibraryColumn();
		LibraryColumn(const QString&, std::function<QString(const mLibraryEntry&)>, int = Qt::AlignLeft);
		QString name;
		std::function<QString(const mLibraryEntry&)> value;
		int alignment;
	};

	class LibraryHandle {
	public:
		LibraryHandle(mLibrary*, const QString& path = QString());
		~LibraryHandle();

		mLibrary* const library;
		LibraryLoader* const loader;
		const QString path;

		void ref();
		bool deref();

	private:
		QThread m_loaderThread;
		size_t m_ref;
	};

	LibraryHandle* m_library;
	static QMap<QString, LibraryHandle*> s_handles;

	mLibraryEntry m_constraints;
	QStringList m_queue;

	QList<LibraryColumn> m_columns;
	static QMap<QString, LibraryColumn> s_columns;
};

class LibraryLoader : public QObject {
Q_OBJECT

public:
	LibraryLoader(mLibrary* library, QObject* parent = nullptr);

public slots:
	void loadDirectory(const QString& path);

signals:
	void directoryLoaded(const QString& path);

private:
	mLibrary* m_library;
};

}

#endif
