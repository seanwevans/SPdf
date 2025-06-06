#include "spdf.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// utils.c
char *generate_id() {
  char *id = (char *)calloc(ID_LEN, sizeof(char));
  if (!id)
    return NULL;

  for (int i = 0; i < ID_LEN - 1; i++)
    id[i] = ID_CHARS[rand() % strlen(ID_CHARS)];

  return id;
}



// stream.c

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

  stream->data = calloc(size, sizeof(char));
  if (!stream->data) {
    free(stream);
    return NULL;
  }

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
    errno = 0;
    if (fwrite(stream->data, stream->data_size, 1, out) < 1 || errno)
      return false;
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

  if (stream->data_size > 0) {
    stream->data = calloc(stream->data_size, 1);
    if (stream->data == NULL)
      return false;
    errno = 0;
    if (fread(stream->data, stream->data_size, 1, in) < 1 || errno)
      return false;
  }

  return true;
}

// spdf.c
void print_spdf(spdf_t *doc) {
  puts("\n=== SPDF ===");
  printf("  🆚 %s\n", doc->version);
  printf("  📓 %s\n", doc->id);
  printf("  🕰️ %ld\n", doc->created);
  printf("  ⏲️ %ld\n", doc->updated);
  printf("  🔗 %zu\n", doc->xref_offset);
  printf("  💦 %zu\n", doc->n_streams - 2);
  for (size_t i = 2; i < doc->max_streams; i++) {
    if (doc->streams[i] && *doc->streams[i]->id) {
      if (doc->streams[i]->mime_type == TEXT && doc->streams[i]->data) {
        printf("    %03zu: %s\n", i - 1, (char *)doc->streams[i]->data);
      } else {
        printf("    %03zu: %p\n", i - 1, doc->streams[i]->data);
      }
    }
  }

  puts("");
}

bool add_stream(spdf_stream_t *stream, spdf_t *doc) {
  if (stream->stream_type == DATA_STREAM)
    printf("+ 💧 %p", stream);

  if (doc->n_streams >= doc->max_streams)
    return false;

  pthread_mutex_lock(doc->lock);
  printf(" 🔒");

  if (stream->stream_type == DATA_STREAM) {
    char *tmp_id = generate_id();
    if (tmp_id)
      strncpy(stream->id, tmp_id, ID_LEN);
    free(tmp_id);
  }

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
  if (doc->n_streams <= 2)
    return false;

  if (stream->stream_type != DATA_STREAM)
    return false;

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

    free(doc->streams[i]->data);
    doc->streams[i]->data_size = 0;
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
  if (!doc->lock) {
    free(doc);
    return NULL;
  }

  if (pthread_mutex_init(doc->lock, NULL) != 0) {
    free(doc->lock);
    free(doc);
    return NULL;
  }
  printf(" 🔓\n");


  char *tmp_id = generate_id();
  if (tmp_id)
    strncpy(doc->id, tmp_id, ID_LEN);
  free(tmp_id);


  doc->streams =
      (spdf_stream_t **)calloc(max_elements + 2, sizeof(spdf_stream_t *));
  if (!doc->streams) {
    pthread_mutex_destroy(doc->lock);
    free(doc->lock);
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

  for (size_t i = 0; i < doc->max_streams; i++) {
    if (doc->streams[i]) {
      if (doc->streams[i]->data)
        free(doc->streams[i]->data);
      free(doc->streams[i]);
    }
  }

  if (doc->streams)
    free(doc->streams);

  if (doc->lock) {
    pthread_mutex_destroy(doc->lock);
    free(doc->lock);
  }

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
    if (!document->streams[i])
      return false;
    if (deserialize_spdf_stream_t(document->streams[i], in) == false)
      return false;
  }

  return true;
}

