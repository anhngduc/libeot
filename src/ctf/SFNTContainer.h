/* Copyright (c) 2012 Brennan T. Vincent <brennanv@email.arizona.edu>
 * This file is a part of libeot, which is license under the MIT license.
 * For full details, see the file LICENSE
 */

#ifndef __LIBEOT_SFNT_CONTAINER_H__
#define __LIBEOT_SFNT_CONTAINER_H__

#include <stdint.h>

#include "../EOTError.h"
#include "../util/stream.h"

struct SFNTTable
{
  char tag[4];
  uint8_t *buf;
  unsigned bufSize;
  unsigned offset;
  unsigned checksum;
};

struct SFNTContainer
{
  unsigned numTables;
  unsigned _numTablesReserved;
  struct SFNTTable *tables;
};

enum EOTError constructContainer(struct SFNTContainer **out);
enum EOTError reserveTables(struct SFNTContainer *ctr, unsigned num);
void freeContainer(struct SFNTContainer *ctr);
enum EOTError addTable(struct SFNTContainer *ctr, const char *tag, struct SFNTTable **newTableOut);
enum EOTError loadTableFromStream(struct SFNTTable *tbl, struct Stream *s);
enum EOTError dumpContainer(struct SFNTContainer *ctr, uint8_t **outBuf);

#endif /* #define __LIBEOT_SFNT_CONTAINER_H__ */

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */