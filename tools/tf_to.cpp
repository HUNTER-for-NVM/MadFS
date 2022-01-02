#include <iostream>

#include "file.h"
#include "lib.h"
#include "posix.h"
#include "transform.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
    return 1;
  }

  const char *filename = argv[1];

  int fd = ulayfs::posix::open(filename, O_RDONLY);
  if (fd < 0) {
    std::cerr << "Failed to open " << filename << ": " << strerror(errno)
              << std::endl;
    return 1;
  }

  ulayfs::dram::File *file = ulayfs::utility::Transformer::transform_to(fd);
  delete file;

  return 0;
}
