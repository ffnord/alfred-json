#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define CHUNK 0x4000
#define windowBits 15
#define ENABLE_ZLIB_GZIP 32

size_t zcat(unsigned char **output_buffer, const unsigned char *data, const size_t data_len) {
  size_t output_bytes = 0;

  z_stream strm = {0};

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.next_in = (unsigned char *)data;
  strm.avail_in = data_len;

  if (inflateInit2(& strm, windowBits | ENABLE_ZLIB_GZIP) < 0) {
    fprintf(stderr, "inflateInit2() failed\n");
    return 0;
  }

  int ret;

  do {
    size_t have;

    unsigned char *old_buffer = *output_buffer;

    // ensure there's enough space for one chunk
    *output_buffer = realloc(*output_buffer, sizeof(unsigned char) * (output_bytes + CHUNK));

    strm.next_out = *output_buffer + output_bytes;

    if (*output_buffer == NULL) {
      free(old_buffer);

      return 0;
    }

    strm.avail_out = CHUNK;

    ret = inflate(&strm, Z_NO_FLUSH);

    if (ret < 0) {
      inflateEnd(&strm);
      free(*output_buffer);

      return 0;
    }

    have = CHUNK - strm.avail_out;

    output_bytes += have;
  } while (ret != Z_STREAM_END);

  inflateEnd(&strm);

  if (output_bytes == 0) {
    free(*output_buffer);
    return 0;
  }

  *output_buffer = realloc(*output_buffer, sizeof(unsigned char) * output_bytes);

  if (*output_buffer == NULL)
    return 0;

  return output_bytes;
}
