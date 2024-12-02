#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <libeot/libeot.h>

#include "../flags.h"
#include "../writeFontFile.h"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {

  const char *outFileName = "out.ttf";
  FILE *outFile = fopen(outFileName, "wb");
  if (outFile == NULL) {
    fprintf(stderr, "The file %s could not be opened for writing.\n",
            outFileName);
    return 1;
  }

  struct EOTMetadata out;
  enum EOTError result = EOT2ttf_file(Data, Size, &out, outFile);
  EOTfreeMetadata(&out);
  fclose(outFile);
  return 0;
}

// TO-DO: implement CUSTOM_MUTATOR current cov: 2
#ifdef CUSTOM_MUTATOR
// refer https://github.com/google/oss-fuzz/blob/master/projects/libpng-proto/libpng_transforms_fuzzer.cc
#endif  // CUSTOM_MUTATOR