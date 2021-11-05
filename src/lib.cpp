#include "lib.h"

#include <cstdarg>
#include <cstdio>

#include "config.h"
#include "layout.h"
#include "posix.h"

namespace ulayfs {
dram::File* get_file(int fd) {
  auto it = files.find(fd);
  if (it != files.end()) return it->second;
  return nullptr;
}

extern "C" {
int open(const char* pathname, int flags, ...) {
  mode_t mode = 0;

  if (__OPEN_NEEDS_MODE(flags)) {
    va_list arg;
    va_start(arg, flags);
    mode = va_arg(arg, mode_t);
    va_end(arg);
  }

  auto file = new dram::File(pathname, flags, mode);
  auto fd = file->get_fd();
  if (file->is_valid()) {
    INFO("ulayfs::open(%s, %x, %x) = %d", pathname, flags, mode, fd);
    files[fd] = file;
  } else {
    DEBUG("posix::open(%s, %x, %x) = %d", pathname, flags, mode, fd);
    delete file;
  }

  return fd;
}

int close(int fd) {
  if (files.erase(fd) == 1) {
    INFO("ulayfs::close(%d)", fd);
    return 0;
  } else {
    DEBUG("posix::close(%d)", fd);
    return posix::close(fd);
  }
}

ssize_t write(int fd, const void* buf, size_t count) {
  if (auto file = get_file(fd)) {
    INFO("ulayfs::write(%d, buf, %zu)", fd, count);
    return file->write(buf, count);
  } else {
    DEBUG("posix::write(%d, buf, %zu)", fd, count);
    return posix::write(fd, buf, count);
  }
}

ssize_t read(int fd, void* buf, size_t count) {
  if (auto file = get_file(fd)) {
    INFO("ulayfs::read(%d, buf, %zu)", fd, count);
    return file->read(buf, count);
  } else {
    DEBUG("posix::read(%d, buf, %zu)", fd, count);
    return posix::read(fd, buf, count);
  }
}

off_t lseek(int fd, off_t offset, int whence) {
  if (auto file = get_file(fd)) {
    INFO("ulayfs::lseek(%d, %zu, %d)", fd, offset, whence);
    return file->lseek(offset, whence);
  } else {
    DEBUG("posix::lseek(%d, %zu, %d)", fd, offset, whence);
    return posix::lseek(fd, offset, whence);
  }
}

ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
  if (auto file = get_file(fd)) {
    INFO("ulayfs::pwrite(%d, buf, %zu, %ld)", fd, count, offset);
    return file->pwrite(buf, count, offset);
  } else {
    DEBUG("posix::pwrite(%d, buf, %zu, %ld)", fd, count, offset);
    return posix::pwrite(fd, buf, count, offset);
  }
}

ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
  if (auto file = get_file(fd)) {
    INFO("ulayfs::pread(%d, buf, %zu, %ld)", fd, count, offset);
    return file->pread(buf, count, offset);
  } else {
    DEBUG("posix::pread(%d, buf, %zu, %ld)", fd, count, offset);
    return posix::pread(fd, buf, count, offset);
  }
}

int fstat(int fd, struct stat* buf) {
  if (auto file = get_file(fd)) {
    INFO("ulayfs::fstat(%d)", fd);
    // TODO: implement this
    return posix::fstat(fd, buf);
  } else {
    DEBUG("posix::fstat(%d)", fd);
    return posix::fstat(fd, buf);
  }
}

/**
 * Called when the shared library is first loaded
 *
 * Note that the global variables may not be initialized at this point
 * e.g., all the functions in the ulayfs::posix namespace
 */
void __attribute__((constructor)) ulayfs_ctor() {
  if (runtime_options.show_config) {
    std::cerr << build_options << std::endl;
    std::cerr << runtime_options << std::endl;
  }
  if (runtime_options.log_file) {
    log_file = fopen(runtime_options.log_file, "a");
  }
}

/**
 * Called when the shared library is unloaded
 */
void __attribute__((destructor)) ulayfs_dtor() {}
}  // extern "C"
}  // namespace ulayfs
