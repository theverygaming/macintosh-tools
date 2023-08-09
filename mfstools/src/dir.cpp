#include <cinttypes>
#include <common.h>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [MFS image filename]\n", argv[0]);
        exit(1);
    }
    auto infile = std::make_shared<std::fstream>(argv[1], std::ios::in | std::ios::binary);
    if (!infile.get()->is_open()) {
        fprintf(stderr, "Failed to open input file (%s)\n", std::strerror(errno));
        exit(1);
    }
    if (maybe_diskcopy42(*infile.get())) {
        fprintf(stderr, "This may be a Apple DiskCopy 4.2 image! Extract it before using it with this tool.\n");
    }

    try {
        mfs mfs(infile);
        if (!mfs.init_readonly()) {
            fprintf(stderr, "Failed to initialize MFS file system\n");
        }

        printf("fsize    rsize    ctime              mtime             name\n");
        for (auto e : mfs.readdir()) {
            printf("%08zu %08zu", e.fsize, e.rsize);
            time_t t = (time_t)e.ctime;
            char buf[18];
            strftime(buf, sizeof(buf), "%b %d %Y %R", localtime(&t));
            printf(" %s ", buf);
            t = (time_t)e.mtime;
            strftime(buf, sizeof(buf), "%b %d %Y %R", localtime(&t));
            printf(" %s ", buf);
            printf("%s\n", e.name.c_str());
        }

    } catch (const std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    return 0;
}
