#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define VERSION "000.000.001"
#define VERSION_LEN 12
#define ID_CHARS "0123456789ABCDEFGHIJKLMNOPQRSATUVWXYZ"
#define ID_LEN 36

#define WRITE_AND_CHECK(stream, member, out)                                   \
  do {                                                                         \
    errno = 0;                                                                 \
    if (fwrite(&(stream)->member, sizeof((stream)->member), 1, (out)) < 1 ||   \
        errno)                                                                 \
      return false;                                                            \
  } while (0)

#define READ_AND_CHECK(stream, member, in)                                     \
  do {                                                                         \
    errno = 0;                                                                 \
    if (fread(&(stream)->member, sizeof((stream)->member), 1, (in)) < 1 ||     \
        errno)                                                                 \
      return false;                                                            \
  } while (0)


// utils.c
char *generate_id() {
  char *id = (char *)calloc(ID_LEN, sizeof(char));
  if (!id)
    return NULL;

  for (int i = 0; i < ID_LEN - 1; i++)
    id[i] = ID_CHARS[rand() % strlen(ID_CHARS)];

  return id;
}

uint32_t hash(unsigned char *str) {
  uint32_t h = 5381;
  int c;

  while ((c = *str++))
    h = ((h << 5) + h) + c;

  return h;
}


// stream.c
enum stream_type {
  METADATA_STREAM = 0,
  XREF_STREAM,
  DATA_STREAM,
};

enum encoding {
  UTF8 = 0,
};

enum mime_type {
  TEXT = 0,
  BINARY,
};

enum compression { NO_COMPRESSION = 0, ZIP, TAR };

typedef struct {
  uint8_t stream_type;
  char version[VERSION_LEN]; // 105.512.001
  char id[ID_LEN];           // ab44a3f5-a8f7-4b4b-a354-4f313404630b
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

void print_stream(spdf_stream_t *stream) {
  puts("\n=== STREAM ===");
  printf("  Type %d\n", stream->stream_type);
  printf("  Version %s\n", stream->version);
  printf("  ID %s\n", stream->id);
  printf("  created %ld\n", stream->created);
  printf("  updated %ld\n", stream->updated);
  printf("  Position (%.2f, %.2f)\n", stream->position[0], stream->position[1]);
  printf("  Encoding %d\n", stream->encoding);
  printf("  Format %d\n", stream->mime_type);
  printf("  Compression %d\n", stream->compression);
  printf("  Offset %zu\n", stream->offset);
  printf("  Reading Index %zu\n", stream->reading_idx);
  printf("  Data Size %zu\n", stream->data_size);
  printf("  Data %p\n", stream->data);
}

spdf_stream_t *create_default_metadata_stream() {
  spdf_stream_t *stream = (spdf_stream_t *)calloc(1, sizeof(spdf_stream_t));
  if (!stream)
    return NULL;

  stream->created = time(NULL);
  stream->stream_type = METADATA_STREAM;
  strncpy(stream->version, VERSION, VERSION_LEN);
  stream->id[0] = '0';

  stream->updated = time(NULL);
  return stream;
}

/*
 * create_stream duplicates the caller provided buffer. The caller retains
 * ownership of the original memory.
 */
spdf_stream_t *create_stream(void *data, size_t size) {
  spdf_stream_t *stream = (spdf_stream_t *)calloc(1, sizeof(spdf_stream_t));
  if (!stream)
    return NULL;

  stream->created = time(NULL);
  stream->stream_type = DATA_STREAM;
  strncpy(stream->version, VERSION, VERSION_LEN);
  stream->data = (void*)calloc(size, sizeof(char));
  if (stream->data && data)
    memcpy(stream->data, data, size);
  stream->data_size = size;

  stream->updated = time(NULL);
  return stream;
}

spdf_stream_t *create_default_footer_stream() {
  spdf_stream_t *stream = (spdf_stream_t *)calloc(1, sizeof(spdf_stream_t));
  if (!stream)
    return NULL;

  stream->created = time(NULL);
  stream->stream_type = XREF_STREAM;
  strncpy(stream->version, VERSION, VERSION_LEN);
  stream->id[0] = '1';

  stream->updated = time(NULL);
  return stream;
}

bool serialize_spdf_stream_t(const spdf_stream_t *stream, FILE *out) {
  WRITE_AND_CHECK(stream, stream_type, out);
  WRITE_AND_CHECK(stream, version, out);
  WRITE_AND_CHECK(stream, id, out);
  WRITE_AND_CHECK(stream, created, out);
  WRITE_AND_CHECK(stream, updated, out);
  WRITE_AND_CHECK(stream, position, out);
  WRITE_AND_CHECK(stream, encoding, out);
  WRITE_AND_CHECK(stream, mime_type, out);
  WRITE_AND_CHECK(stream, compression, out);
  WRITE_AND_CHECK(stream, offset, out);
  WRITE_AND_CHECK(stream, reading_idx, out);
  WRITE_AND_CHECK(stream, data_size, out);

  // Write variable-length data
  if (stream->data_size > 0 && stream->data != NULL) {
    WRITE_AND_CHECK(stream, data, out);
  }

  return true;
}

bool deserialize_spdf_stream_t(spdf_stream_t *stream, FILE *in) {
  READ_AND_CHECK(stream, stream_type, in);
  READ_AND_CHECK(stream, version, in);
  READ_AND_CHECK(stream, id, in);
  READ_AND_CHECK(stream, created, in);
  READ_AND_CHECK(stream, updated, in);
  READ_AND_CHECK(stream, position, in);
  READ_AND_CHECK(stream, encoding, in);
  READ_AND_CHECK(stream, mime_type, in);
  READ_AND_CHECK(stream, compression, in);
  READ_AND_CHECK(stream, offset, in);
  READ_AND_CHECK(stream, reading_idx, in);
  READ_AND_CHECK(stream, data_size, in);

  return true;
}

// spdf.c
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

void print_spdf(spdf_t *doc) {
  puts("\n=== SPDF ===");
  printf("  🆚 %s\n", doc->version);
  printf("  📓 %s\n", doc->id);
  printf("  🕰️ %ld\n", doc->created);
  printf("  ⏲️ %ld\n", doc->updated);
  printf("  🔗 %zu\n", doc->xref_offset);
  printf("  💦 %zu\n", doc->n_streams - 2);
  for (size_t i = 2; i < doc->max_streams; i++) {
    if (*doc->streams[i]->id) {
      printf("    %03zu: %s\n", i - 1, doc->streams[i]->data);
    }
  }

  puts("");
}

bool add_stream(spdf_stream_t *stream, spdf_t *doc) {
  if (stream->stream_type == DATA_STREAM)
    printf("+ 💧 %p", stream);

  if (doc->n_streams >= doc->max_streams) {
    puts(" ❌ full");
    return false;
  }

  pthread_mutex_lock(doc->lock);
  printf(" 🔒");

  if (stream->stream_type == DATA_STREAM)
    strncpy(stream->id, generate_id(), ID_LEN);

  doc->xref_offset += sizeof(spdf_stream_t) + stream->data_size;

  size_t i = 0;
  for (; i < doc->max_streams; i++)
    if (!doc->streams[i])
      break;

  doc->streams[i] = stream;
  doc->n_streams++;
  doc->updated = time(NULL);
  pthread_mutex_unlock(doc->lock);
  printf(" 🔓 ✔️ %s\n", (char*)stream->data);
  return true;
}

bool remove_stream(spdf_stream_t *stream, spdf_t *doc) {
  printf("- 💧 %p", stream);
  if (doc->n_streams <= 2) {
    puts(" ❌ empty");
    return false;
  }

  if (stream->stream_type != DATA_STREAM) {
    puts(" ❌ must be data stream");
    return false;
  }

  pthread_mutex_lock(doc->lock);
  printf(" 🔒");

  char tmp[ID_LEN];
  strncpy(tmp, stream->id, ID_LEN);

  for (size_t i = 2; i < doc->max_streams; i++) {
    if (!*doc->streams[i]->id)
      continue;

    if (strcmp(doc->streams[i]->id, stream->id))
      continue;

    doc->xref_offset -= sizeof(spdf_stream_t) + stream->data_size;
    memset(doc->streams[i], 0, sizeof(spdf_stream_t));
    doc->streams[i]->stream_type = METADATA_STREAM;
    strncpy(doc->streams[i]->version, VERSION, VERSION_LEN);
    doc->n_streams--;
    doc->updated = time(NULL);
    pthread_mutex_unlock(doc->lock);
    printf(" 🔓 ✔️ %s\n", tmp);
    return true;
  }

  pthread_mutex_unlock(doc->lock);
  printf(" 🔓 ❌ failed to find '%s' in doc\n", stream->id);
  return false;
}

spdf_t *create_spdf(size_t max_elements) {
  printf("\nCreating spdf...");

  spdf_t *doc = (spdf_t *)calloc(1, sizeof(spdf_t));
  if (!doc)
    return NULL;

  doc->created = time(NULL);
  strncpy(doc->version, VERSION, VERSION_LEN);
  doc->max_streams = max_elements + 2;

  doc->lock = (pthread_mutex_t *)calloc(1, sizeof(*doc->lock));
  if (!doc->lock)
    return NULL;

  pthread_mutex_init(doc->lock, NULL);
  printf(" 🔓\n");

  strncpy(doc->id, generate_id(), ID_LEN);

  doc->streams =
      (spdf_stream_t **)calloc(max_elements + 2, sizeof(spdf_stream_t *));
  if (!doc->streams) {
    free(doc);
    return NULL;
  }
  printf("+ 🗂 💧");
  add_stream(create_default_metadata_stream(), doc);
  printf("+ 🔗 💧");
  add_stream(create_default_footer_stream(), doc);

  puts("✔️\n");
  doc->updated = time(NULL);
  return doc;
}

bool destroy_spdf(spdf_t *doc) {
  for (size_t i = 0; i < doc->max_streams; i++)
    if (doc->streams[i])
      free(doc->streams[i]);

  if (doc->lock)
    pthread_mutex_destroy(doc->lock);

  if (doc)
    free(doc);

  return true;
}

bool save_spdf(const spdf_t *document, FILE *out) { 
  // write magic number 
  fwrite("%%SPDF", 6, 1, out);

  // write metadata stream
  WRITE_AND_CHECK(document, version, out);
  WRITE_AND_CHECK(document, id, out);
  WRITE_AND_CHECK(document, created, out);
  WRITE_AND_CHECK(document, updated, out);
  WRITE_AND_CHECK(document, xref_offset, out);
  WRITE_AND_CHECK(document, n_streams, out);

  // write data streams
  for (size_t i = 0; i < document->n_streams; ++i)    
    serialize_spdf_stream_t(document->streams[i], out);  

  // write xref stream

  // write xref offset
  WRITE_AND_CHECK(document, xref_offset, out);

  // write eof
  fwrite("EOF%%", 5, 1, out);

  return true;
}

bool load_spdf(spdf_t *document, FILE *in) {
  READ_AND_CHECK(document, version, in);
  READ_AND_CHECK(document, id, in);
  READ_AND_CHECK(document, created, in);
  READ_AND_CHECK(document, updated, in);
  READ_AND_CHECK(document, xref_offset, in);
  READ_AND_CHECK(document, n_streams, in);

  document->streams =
      (spdf_stream_t **)calloc(document->n_streams, sizeof(spdf_stream_t *));
  if (document->streams == NULL)
    return false;

  // Read streams
  for (size_t i = 0; i < document->n_streams; ++i) {
    document->streams[i] = (spdf_stream_t *)calloc(1, sizeof(spdf_stream_t));
    if (deserialize_spdf_stream_t(document->streams[i], in) == false)
      return false;
  }

  return true;
}

// main.c
int main(int argc, char *argv[]) {
  srand(time(NULL));

  spdf_t *doc = create_spdf(32);
  if (!doc) {
    fprintf(stderr, "Failed to create spdf");
    return EXIT_FAILURE;
  }
  sleep(1);

  for (size_t i = 0; i < doc->max_streams - 2; i++) {
    if (!add_stream(create_stream((void *)i, 1), doc)) {
      destroy_spdf(doc);
      fprintf(stderr, "failed to add stream");
      return EXIT_FAILURE;
    }
  }

  add_stream(create_stream((void *)0xdeadbeef, 1), doc);
  print_spdf(doc);

  sleep(1);
  for (size_t i = 2; i < doc->max_streams; i++) {
    if (!remove_stream(doc->streams[i], doc)) {
      destroy_spdf(doc);
      fprintf(stderr, "failed to remove stream");
      return EXIT_FAILURE;
    }
  }

  remove_stream(create_stream((void *)0xdeadbeef, 1), doc);
  print_spdf(doc);

  if (!destroy_spdf(doc)) {
    fprintf(stderr, "failed to destroy spdf");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
