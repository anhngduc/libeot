/* Copyright (c) 2013 Brennan T. Vincent <brennanv@email.arizona.edu>
 * This file is a part of libeot, which is licensed under the MPL license,
 * version 2.0. For full details, see the file LICENSE
 */

#include <libeot/libeot.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/stream.h"
#include "AHUFF.H"
#include "BITIO.H"
#include "ERRCODES.H"
#include "LZCOMP.H"
#include "MTXMEM.H"

unsigned be24ToCpu(const uint8_t *buf)
{
  return ((unsigned)buf[2]) | (((unsigned)buf[1]) << 8) |
         (((unsigned)buf[0]) << 16);
}
enum EOTError unpackMtx(struct Stream *buf, unsigned size, uint8_t **bufsOut,
                        unsigned *bufSizesOut)
{
  for (unsigned i = 0; i < 3; ++i) {
    bufsOut[i] = NULL;
  }
  enum StreamResult sResult;
  enum EOTError returnedStatus = EOT_SUCCESS;
  LZCOMP *lzcomp = NULL;
  MTX_MemHandler *mem = MTX_mem_Create(&malloc, &realloc, &free);
  if (!mem) {
    goto CLEANUP;
  }
  lzcomp = MTX_LZCOMP_Create1(mem);
  if (!lzcomp) {
    goto CLEANUP;
  }
  uint8_t versionMagic;
  uint32_t offsets[3];
  offsets[0] = 10;
  uint32_t copyLimit;
  sResult = BEReadU8(buf, &versionMagic);
  printf("magic: %X\n", versionMagic);
  CHK_CN(sResult, EOT_MTX_ERROR);
  sResult = BEReadU24(buf, &copyLimit);
  CHK_CN(sResult, EOT_MTX_ERROR);
  for (unsigned i = 1 /* sic */; i < 3; ++i) {
    sResult = BEReadU24(buf, &offsets[i]);
    printf("offset!\n");
    CHK_CN(sResult, EOT_MTX_ERROR);
  }
  unsigned sizes[] = {offsets[1] - offsets[0], offsets[2] - offsets[1],
                      buf->size - offsets[2]};
  for (unsigned i = 0; i < 3; ++i) {
    if (offsets[i] + sizes[i] > buf->size) {
      returnedStatus = EOT_MTX_ERROR;
      goto CLEANUP;
    }
    long sizeOut;
    bufsOut[i] = (uint8_t *)MTX_LZCOMP_UnPackMemory(
        lzcomp, buf->buf + offsets[i], sizes[i], &sizeOut, versionMagic);
    bufSizesOut[i] = sizeOut;
    if (!bufsOut[i]) {
      returnedStatus = EOT_MTX_ERROR;
      goto CLEANUP;
    }
  }
CLEANUP:
  if (lzcomp)
    MTX_LZCOMP_Destroy(lzcomp);
  free(mem);
  return returnedStatus;
}

enum EOTError packMtx(struct Stream *buf, unsigned size, uint8_t *bufsIn[3],
                      long bufSizesIn[3], uint8_t **output,
                      unsigned *outputSize)
{
  enum EOTError returnedStatus = EOT_SUCCESS;
  LZCOMP *lzcomp = NULL;
  MTX_MemHandler *mem = NULL;
  uint8_t versionMagic = 3; // Assuming versionMagic is 1
  uint32_t offsets[3];
  uint32_t totalSize = 10; // Initial offset

  // Initialize memory handler
  mem = MTX_mem_Create(&malloc, &realloc, &free);
  if (!mem) {
    return EOT_MTX_ERROR;
  }

  // Initialize lzcomp
  lzcomp = MTX_LZCOMP_Create1(mem);
  if (!lzcomp) {
    goto CLEANUP;
  }

  // Compress each buffer
  uint8_t *compressedBufs[3] = {NULL, NULL, NULL};
  unsigned compressedSizes[3] = {0, 0, 0};
  for (unsigned i = 0; i < 3; ++i) {
    long compSize;
    compressedBufs[i] = (uint8_t *)MTX_LZCOMP_PackMemory(
        lzcomp, bufsIn[i], bufSizesIn[i], &compSize);
    if (!compressedBufs[i]) {
      returnedStatus = EOT_MTX_ERROR;
      goto CLEANUP;
    }
    compressedSizes[i] = compSize;
    offsets[i] = totalSize;
    printf("offset[%d] = %d\n", i, offsets[i]);
    totalSize += compressedSizes[i];
  }

  // Allocate the output buffer
  *output = calloc(totalSize, sizeof(uint8_t));
  if (!*output) {
    returnedStatus = EOT_MTX_ERROR;
    goto CLEANUP;
  }

  // Write header
  (*output)[0] = (uint8_t)0x03;
  (*output)[1] = (uint8_t)0xff;
  (*output)[2] = (uint8_t)0xff;
  (*output)[3] = (uint8_t)0xff;
  int i = 0;
  for (i = 0; i < 2; ++i) {
    (*output)[4 + i * 3] = (uint8_t)(offsets[i + 1] >> 16);
    (*output)[5 + i * 3] = (uint8_t)(offsets[i + 1] >> 8);
    (*output)[6 + i * 3] = (uint8_t)(offsets[i + 1]);
  }
  for (i = 0; i < 10; i++) {
    printf("%X ", (*output)[i]);
  }
  printf("\n");

  // Write compressed data
  uint32_t offset = 10;
  for (unsigned i = 0; i < 3; ++i) {
    memcpy(*output + offset, compressedBufs[i], compressedSizes[i]);
    offset += compressedSizes[i];
  }
  *outputSize = totalSize;

CLEANUP:
  if (lzcomp) {
    MTX_LZCOMP_Destroy(lzcomp);
  }
  if (mem) {
    free(mem);
  }
  for (unsigned i = 0; i < 3; ++i) {
    free(compressedBufs[i]);
  }
  return returnedStatus;
}

#ifdef LZCOMP_MAIN
void usage(char *arg)
{
  fprintf(stderr,
          "Usage: %s [pack] input1.ctf input2.ctf input3.ctf output.mtx\n %s "
          "unpack input.mtx outputprefix\n",
          arg, arg);
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "pack") == 0) {
    uint8_t *bufsIn[3] = {NULL, NULL, NULL};
    long bufSizesIn[3] = {0, 0, 0};
    for (unsigned i = 0; i < 3; ++i) {
      FILE *in = fopen(argv[i + 2], "rb");
      if (in == NULL) {
        fprintf(stderr, "Cannot open file: %s\n", argv[i + 2]);
        return 1;
      }
      fseek(in, 0, SEEK_END);
      bufSizesIn[i] = ftell(in);
      fseek(in, 0, SEEK_SET);
      bufsIn[i] = (uint8_t *)malloc(bufSizesIn[i]);
      fread(bufsIn[i], 1, bufSizesIn[i], in);
      fclose(in);
    }

    uint8_t *output = NULL;
    unsigned outputSize = 0;
    enum EOTError status =
        packMtx(NULL, 0, bufsIn, bufSizesIn, &output, &outputSize);
    if (status != EOT_SUCCESS) {
      fprintf(stderr, "Error compressing files\n");
      return 1;
    }

    FILE *out = fopen(argv[5], "wb");
    if (out == NULL) {
      fprintf(stderr, "Cannot open file: %s for writing\n", argv[5]);
      return 1;
    }
    fwrite(output, 1, outputSize, out);
    fclose(out);
    free(output);

    for (unsigned i = 0; i < 3; ++i) {
      free(bufsIn[i]);
    }
  } else if (strcmp(argv[1], "unpack") == 0) {

    if (argc != 4) {
      printf("unpack need 4\n");
      usage(argv[0]);
      return 1;
    }
    MTX_MemHandler *mem = MTX_mem_Create(&malloc, &realloc, &free);
    LZCOMP *lzcomp = MTX_LZCOMP_Create1(mem);
    FILE *in = fopen(argv[2], "rb");
    if (in == NULL) {
      fprintf(stderr, "Cannot open file: %s\n", argv[2]);
      return 1;
    }
    uint8_t versionMagic;
    const unsigned block1Offset = 10;
    unsigned offsets[3];
    offsets[0] = 10;
    unsigned copyLimit;
    unsigned block2Offset, block3Offset;
    uint8_t buf24[3];
    fread(&versionMagic, 1, 1, in);
    fread(buf24, 1, 3, in);
    copyLimit = be24ToCpu(buf24);
    fread(buf24, 1, 3, in);
    offsets[1] = be24ToCpu(buf24);
    fread(buf24, 1, 3, in);
    offsets[2] = be24ToCpu(buf24);
    fseek(in, 0, SEEK_END);
    unsigned totalFileSize = ftell(in);
    unsigned sizes[] = {offsets[1] - offsets[0], offsets[2] - offsets[1],
                        totalFileSize - offsets[2]};
    for (unsigned i = 0; i < 3; ++i) {
      char *buf = malloc(sizes[i]);
      fseek(in, offsets[i], SEEK_SET);
      fread(buf, 1, sizes[i], in);
      char fnBuf[10];
      sprintf(fnBuf, "%s%d.ctf", argv[3], i + 1);
      FILE *out = fopen(fnBuf, "wb");
      if (out == NULL) {
        fprintf(stderr, "Cannot open file: %s for writing\n", fnBuf);
        return 1;
      }
      long sizeOut;
      char *outBuf = MTX_LZCOMP_UnPackMemory(lzcomp, buf, sizes[i], &sizeOut,
                                             versionMagic);
      fwrite(outBuf, 1, sizeOut, out);
      fclose(out);
      free(buf);
      free(outBuf);
    }
  }

  else {
    usage(argv[0]);
    return 1;
  }

  return 0;
}
#endif
