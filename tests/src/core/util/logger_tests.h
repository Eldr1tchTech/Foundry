#pragma once

#include <util/logger.h>

bool logger_tests() {
    LOG_FATAL("test");
    LOG_ERROR("test");
    LOG_WARNING("test");
    LOG_DEBUG("test");
    LOG_INFO("test");

    return true;
}

