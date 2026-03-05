#pragma once

void init_logging(const char *log_filepath = "log.txt");
void shutdown_logging();

void logprintf(const char *fmt, ...);
