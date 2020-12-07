#pragma once
#include "serialcon/serialcon.h"

char *is_conn_libvirt(const char *serial_dev);
int libvirt_console_open(serialcon_connection *conn);
int libvirt_console_run(serialcon_connection *conn, const char *cmd);
void libvirt_console_close(serialcon_connection *conn);
