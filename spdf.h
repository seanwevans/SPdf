#ifndef SPDF_H
#define SPDF_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define VERSION "000.000.001"
#define VERSION_LEN 12
#define ID_CHARS "0123456789ABCDEFGHIJKLMNOPQRSATUVWXYZ"
#define ID_LEN 36

#define WRITE_AND_CHECK(stream, member, out) \
  do { \
    errno = 0; \
    if (fwrite(&(stream)->member, sizeof((stream)->member), 1, (out)) < 1 || errno) \
      return false; \
  } while (0)

#define READ_AND_CHECK(stream, member, in) \
  do { \
    errno = 0; \
    if (fread(&(stream)->member, sizeof((stream)->member), 1, (in)) < 1 || errno) \
      return false; \
  } while (0)

enum stream_type { METADATA_STREAM = 0, XREF_STREAM, DATA_STREAM };
enum encoding { UTF8 = 0 };
enum mime_type { TEXT = 0, BINARY };
enum compression { NO_COMPRESSION = 0, ZIP, TAR };

typedef struct {
  uint8_t stream_type;
  char version[VERSION_LEN];
  char id[ID_LEN];
  time_t created;
  time_t updated;
  double position[2];
  uint8_t encoding;
  uint8_t mime_type;
  uint8_t compression;
  size_t offset;
  size_t reading_idx;
  size_t data_size;
  void *data;
} spdf_stream_t;

typedef struct {
  pthread_mutex_t *lock;
  char version[VERSION_LEN];
  char id[ID_LEN];
  time_t created;
  time_t updated;
  size_t xref_offset;
  size_t n_streams;
  size_t max_streams;
  spdf_stream_t **streams;
} spdf_t;

char *generate_id(void);
uint32_t hash(unsigned char *str);
void print_stream(spdf_stream_t *stream);
spdf_stream_t *create_default_metadata_stream(void);
spdf_stream_t *create_default_footer_stream(void);
spdf_stream_t *create_stream(void *data, size_t size);
bool serialize_spdf_stream_t(const spdf_stream_t *stream, FILE *out);
bool deserialize_spdf_stream_t(spdf_stream_t *stream, FILE *in);
spdf_t *create_spdf(size_t max_elements);
bool destroy_spdf(spdf_t *doc);
bool add_stream(spdf_stream_t *stream, spdf_t *doc);
bool remove_stream(spdf_stream_t *stream, spdf_t *doc);
bool save_spdf(const spdf_t *document, FILE *out);
bool load_spdf(spdf_t *document, FILE *in);
void print_spdf(spdf_t *doc);

#endif // SPDF_H
