#include <getopt.h>  /* getopt */
#include <stdio.h>  /* fprintf */
#include <stdlib.h>  /* NULL */
#include <string.h>  /* memset */

#include "openssl/err.h"
#include "openssl/ssl.h"
#include "parson.h"

#include "config.h"
#include "common.h"
#include "version.h"

static int bud_config_init(bud_config_t* config);
static void bud_config_set_defaults(bud_config_t* config);
static void bud_print_help(int argc, char** argv);
static void bud_print_version();
static void bud_config_print_default();


bud_config_t* bud_config_cli_load(int argc, char** argv) {
  int index;
  struct option long_options[] = {
    { "version", 0, NULL, 'v' },
    { "config", 1, NULL, 'c' },
    { "default-config", 0, NULL, 1001 },
    { NULL, 0, NULL, 0 }
  };

  index = 0;
  switch (getopt_long(argc, argv, "vc:", long_options, &index)) {
    case 'v':
      bud_print_version();
      return NULL;
    case 'c':
      return bud_config_load(optarg);
    case 1001:
      bud_config_print_default();
      return NULL;
    default:
      bud_print_help(argc, argv);
      return NULL;
  }
}


bud_config_t* bud_config_load(const char* path) {
  int i;
  int context_count;
  JSON_Value* json;
  JSON_Object* obj;
  JSON_Array* contexts;
  bud_config_t* config;
  bud_context_t* ctx;

  json = json_parse_file(path);
  if (json == NULL) {
    fprintf(stderr, "Failed to load or parse: %s\n", path);
    goto end;
  }

  obj = json_value_get_object(json);
  if (obj == NULL) {
    fprintf(stderr, "Invalid json, root should be an object\n");
    goto failed_get_object;
  }
  contexts = json_object_get_array(obj, "contexts");
  context_count = contexts == NULL ? 0 : json_array_get_count(contexts);

  config = calloc(1,
                  sizeof(*config) +
                      (context_count - 1) * sizeof(*config->contexts));
  ASSERT(config != NULL, "Failed to allocate config");

  config->port = (uint16_t) json_object_get_number(obj, "port");
  config->host = json_object_get_string(obj, "host");

  for (i = 0; i < context_count; i++) {
    ctx = &config->contexts[i];
    obj = json_array_get_object(contexts, i);
    if (obj == NULL) {
      fprintf(stderr, "Invalid json, each context should be an object\n");
      goto failed_get_index;
    }

    ctx->hostname = json_object_get_string(obj, "hostname");
    ctx->cert_file = json_object_get_string(obj, "cert");
    ctx->key_file = json_object_get_string(obj, "key");
  }

  bud_config_set_defaults(config);
  if (bud_config_init(config) != 0) {
    bud_config_free(config);
    return NULL;
  }

  return config;

failed_get_index:
  free(config);

failed_get_object:
  json_value_free(json);

end:
  return NULL;
}


void bud_config_free(bud_config_t* config) {
  int i;

  for (i = 0; i < config->context_count; i++)
    SSL_CTX_free(config->contexts[i].ctx);
  json_value_free(config->json);
  config->json = NULL;
  free(config);
}


void bud_print_help(int argc, char** argv) {
  ASSERT(argc >= 1, "Not enough arguments");
  fprintf(stdout, "Usage: %s [options]\n\n", argv[0]);
  fprintf(stdout, "options:\n");
  fprintf(stdout, "  --version, -v              Print bud version\n");
  fprintf(stdout, "  --config PATH, -c PATH     Load JSON configuration\n");
  fprintf(stdout, "  --default-config           Print default JSON config\n");
  fprintf(stdout, "\n");
}


void bud_print_version() {
  fprintf(stdout, "bud %d.%d\n", BUD_VERSION_MAJOR, BUD_VERSION_MINOR);
}


void bud_config_print_default() {
  int i;
  bud_config_t config;
  bud_context_t* ctx;

  memset(&config, 0, sizeof(config));
  bud_config_set_defaults(&config);

  fprintf(stdout, "{\n");
  fprintf(stdout, "  \"port\": %d,\n", config.port);
  fprintf(stdout, "  \"host\": \"%s\",\n", config.host);
  fprintf(stdout, "  \"contexts\": [");
  for (i = 0; i < config.context_count; i++) {
    ctx = &config.contexts[i];

    fprintf(stdout, i == 0 ? "{\n" : "  }, {\n");
    if (ctx->hostname != NULL)
      fprintf(stdout, "    \"hostname\": \"%s\",\n", ctx->hostname);
    else
      fprintf(stdout, "    \"hostname\": null,\n");
    fprintf(stdout, "    \"cert\": \"%s\",\n", ctx->cert_file);
    fprintf(stdout, "    \"key\": \"%s\",\n", ctx->key_file);

    if (i == config.context_count - 1)
      fprintf(stdout, "  }");
  }
  fprintf(stdout, "]\n");
  fprintf(stdout, "}\n");
}


#define DEFAULT(param, null, value)                                           \
    do {                                                                      \
      if ((param) == (null))                                                  \
        (param) = (value);                                                    \
    } while (0)

void bud_config_set_defaults(bud_config_t* config) {
  int i;

  DEFAULT(config->port, 0, 1443);
  DEFAULT(config->host, NULL, "0.0.0.0");
  DEFAULT(config->context_count, 0, 1);

  for (i = 0; i < config->context_count; i++) {
    DEFAULT(config->contexts[i].cert_file, NULL, "keys/cert.pem");
    DEFAULT(config->contexts[i].key_file, NULL, "keys/key.pem");
  }
}

#undef DEFAULT


int bud_config_init(bud_config_t* config) {
  int i;
  bud_context_t* ctx;

  /* Load all contexts */
  for (i = 0; i < config->context_count; i++) {
    ctx = &config->contexts[i];

    ctx->ctx = SSL_CTX_new(SSLv23_server_method());
    ASSERT(ctx->ctx != NULL, "Failed to allocate context");

    if (!SSL_CTX_use_certificate_chain_file(ctx->ctx, ctx->cert_file)) {
      fprintf(stderr, "Failed to load/parse cert %s:\n", ctx->cert_file);
      ERR_print_errors_fp(stderr);
      goto fatal;
    }

    if (!SSL_CTX_use_PrivateKey_file(ctx->ctx,
                                     ctx->key_file,
                                     SSL_FILETYPE_PEM)) {
      fprintf(stderr, "Failed to load/parse key %s:\n", ctx->key_file);
      ERR_print_errors_fp(stderr);
      goto fatal;
    }
  }

  return 0;

fatal:
  /* Free all allocated contexts */
  do {
    SSL_CTX_free(config->contexts[i].ctx);
    config->contexts[i].ctx = NULL;

    i--;
  } while (i >= 0);

  return -1;
}
