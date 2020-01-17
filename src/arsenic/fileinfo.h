#ifndef FILEINFO_H
#define FILEINFO_H

#include <memory>

#include <QString>
#include <QIcon>

#include "dirthread.h"

class FileInfo;
typedef std::unique_ptr<FileInfo> FileInfoPtr;

namespace FileInfoPublic
{
    enum FileInfoType : int {MFIT_EMPTY, MFIT_DIR, MFIT_FILE};
}


/*******************************************************************************



*******************************************************************************/


class FileInfo : public QObject
{
	Q_OBJECT

public:

    FileInfo(FileSystemModel* arg_model);
    FileInfo(FileSystemModel* arg_model, const QString &arg_path);
	//MyFileInfo(const MyFileInfo &rhs);
	//MyFileInfo &operator=(MyFileInfo rhs);
	//MyFileInfo &operator=(const MyFileInfo &rhs);
    ~FileInfo();

	QString getPath() const;
	QString getType() const;
	QString getName() const;
	QString getSize() const;
	QString getDate() const;
	QString getFullPath() const;
	QString getSuffix() const;
	QIcon getIcon() const;
	Qt::CheckState getChecked() const;
	void setChecked(Qt::CheckState arg_checked);
	void startDirThread();
	void stopDirThread();
    bool equals(const FileInfo &rhs) const;
    bool operator==(const FileInfo &rhs) const;
    bool contains(const FileInfo &rhs) const;
    bool isRedundant(const FileInfo &rhs) const;
    bool makesRedundant(const FileInfo &rhs) const;

    static QString typeToString(FileInfoPublic::FileInfoType arg_type);
	void setEmpty();
	void setItem(const QString &arg_path);

private:

	// member functions
	void deleteDirThread();
	void setItemHelper(const QString &arg_path);

	// member variables
	QString name;
	QString path;
	QString date;
	QString full_path;
    FileInfoPublic::FileInfoType type;
	qint64 size;
	Qt::CheckState checked;
	//MyFileSystemModel *ptr_model;
	QIcon icon;

	// used for potentially calculating directory size, so as to not block the main event loop
    FileInfoPublicThread::DirThread *dir_thread;

private slots:

	void updateSize(qint64 arg_size);

};	// class FileInfo

#endif // FILEINFO_H
