/*
 * SPDX license identifier: MPL-2.0
 *
 * Copyright (C) 2026, MyCo
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dlt_config_file_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[] = "/tmp/dlt-fuzz-conf-XXXXXX";
    int fd = -1;
    DltConfigFile *cfg = NULL;

    if ((data == NULL) || (size == 0U) || (size > 4096U))
        return 0;

    fd = mkstemp(path);

    if (fd < 0)
        return 0;

    (void)write(fd, data, size);
    (void)close(fd);

    cfg = dlt_config_file_init(path);

    if (cfg != NULL) {
        int section_count = 0;
        char section_name[DLT_CONFIG_FILE_ENTRY_MAX_LEN + 1] = { 0 };

        (void)dlt_config_file_get_num_sections(cfg, &section_count);
        if (section_count > 0)
            (void)dlt_config_file_get_section_name(cfg, 0, section_name);
        dlt_config_file_release(cfg);
    }

    (void)unlink(path);
    return 0;
}

#ifdef DLT_FUZZ_STANDALONE
int main(void)
{
    static const uint8_t seed[] = "RingbufferMaxSize=10000000\n";
    (void)LLVMFuzzerTestOneInput(seed, sizeof(seed) - 1U);
    return 0;
}
#endif
