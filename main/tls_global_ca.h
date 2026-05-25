#pragma once

#include <mbedtls/x509_crt.h>

int               global_tls_ca_init(void);
mbedtls_x509_crt* global_tls_ca_get(void);
void              global_tls_ca_dispose(void);
