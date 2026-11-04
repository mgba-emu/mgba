#pragma stdheader

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cstdint>
#include <zlib.h>
#include <minizip/zip.h>
#include <minizip/ioapi.h>
#include <__fwd/string.h>

namespace ZipUtils {

    struct FdStream {
        int fd;
    };

    static voidpf ZCALLBACK fd_open_file_func(voidpf opaque, const char * /*filename*/, int /*mode*/) {
        int orig_fd = static_cast<int>(reinterpret_cast<intptr_t>(opaque));

        int dup_fd = dup(orig_fd);
        if (dup_fd < 0) return nullptr;

        lseek(dup_fd, 0, SEEK_SET);
        return new FdStream{dup_fd};
    }

    static uLong ZCALLBACK fd_read_file_func(voidpf /*opaque*/, voidpf stream, void *buf, uLong size) {
        auto *fds = static_cast<FdStream *>(stream);
        ssize_t res = read(fds->fd, buf, size);
        return (res < 0) ? 0 : static_cast<uLong>(res);
    }

    static uLong ZCALLBACK fd_write_file_func(voidpf /*opaque*/, voidpf stream, const void *buf, uLong size) {
        auto *fds = static_cast<FdStream *>(stream);
        ssize_t res = write(fds->fd, buf, size);
        return (res < 0) ? 0 : static_cast<uLong>(res);
    }

    static long ZCALLBACK fd_tell_file_func(voidpf /*opaque*/, voidpf stream) {
        auto *fds = static_cast<FdStream *>(stream);
        return static_cast<long>(lseek(fds->fd, 0, SEEK_CUR));
    }

    static long ZCALLBACK fd_seek_file_func(voidpf /*opaque*/, voidpf stream, uLong offset, int origin) {
        auto *fds = static_cast<FdStream *>(stream);
        int optype = SEEK_SET;
        switch (origin) {
            case ZLIB_FILEFUNC_SEEK_CUR:
                optype = SEEK_CUR;
                break;
            case ZLIB_FILEFUNC_SEEK_END:
                optype = SEEK_END;
                break;
            case ZLIB_FILEFUNC_SEEK_SET:
                optype = SEEK_SET;
                break;
            default:
                return -1;
        }
        return (lseek(fds->fd, static_cast<off_t>(offset), optype) < 0) ? -1 : 0;
    }

    static int ZCALLBACK fd_close_file_func(voidpf /*opaque*/, voidpf stream) {
        auto *fds = static_cast<FdStream *>(stream);
        if (fds) {
            close(fds->fd);
            delete fds;
        }
        return 0;
    }

    static int ZCALLBACK fd_testerror_file_func(voidpf /*opaque*/, voidpf /*stream*/) {
        return 0;
    }

    inline void fill_fd_filefunc(zlib_filefunc_def *pfilefunc, int fd) {
        pfilefunc->zopen_file = fd_open_file_func;
        pfilefunc->zread_file = fd_read_file_func;
        pfilefunc->zwrite_file = fd_write_file_func;
        pfilefunc->ztell_file = fd_tell_file_func;
        pfilefunc->zseek_file = fd_seek_file_func;
        pfilefunc->zclose_file = fd_close_file_func;
        pfilefunc->zerror_file = fd_testerror_file_func;
        pfilefunc->opaque = reinterpret_cast<voidpf>(static_cast<intptr_t>(fd));
    }

    bool exportUserData(const std::string& folderPath, int outFd, const std::string& appId);
    bool importUserData(int inFd, const std::string& targetExtractDir, const std::string& expectedAppId);

} // namespace ZipUtils