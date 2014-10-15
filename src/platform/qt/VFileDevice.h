#ifndef QGBA_VFILE_DEVICE
#define QGBA_VFILE_DEVICE

#include <QFileDevice>

struct VFile;

namespace QGBA {

class VFileDevice : public QIODevice {
Q_OBJECT

public:
	VFileDevice(VFile* vf, QObject* parent = nullptr);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;
	virtual qint64 size() const override;

private:
	mutable VFile* m_vf;
};

}

#endif
