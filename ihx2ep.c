#include <stdio.h>

static unsigned char fileBuf[65536];

int main(int argc, char **argv)
{
  FILE    *ihxFile, *ldrFile, *outFile;
  size_t  i, lastAddr;
  int     isEOF = 0;
  unsigned int  lineHdr = 0U, ihxByte = 0U, ihxAddr = 0U, ldrEnd = 0U;
  int     lineHdrBits = 0, ihxByteBits = 0, lineBytesLeft = 0;
  if (argc != 4) {
    fprintf(stderr, "Usage: %s INFILE.IHX LOADER.BIN OUTFILE.COM\n", argv[0]);
    return -1;
  }
  for (i = 0; i < 65536; i++)
    fileBuf[i] = 0x00;
  ldrFile = fopen(argv[2], "rb");
  if (!ldrFile) {
    fprintf(stderr, " *** %s: error opening loader file\n", argv[0]);
    return -1;
  }
  lastAddr = 0xFFFF;
  for (i = 0x00F0; i < 65536; i++) {
    int     c = fgetc(ldrFile);
    if (c == EOF) {
      lastAddr = i - 1;
      ldrEnd = (unsigned int) i;
      break;
    }
    if (i < 0x00F2 || i >= 0x0100)
      fileBuf[i] = (unsigned char) (c & 0xFF);
  }
  fclose(ldrFile);
  if (lastAddr < 0x0120 || fileBuf[0x00F0] != 0x00 || fileBuf[0x00F1] != 0x05) {
    fprintf(stderr, " *** %s: invalid loader file\n", argv[0]);
    return -1;
  }
  ihxFile = fopen(argv[1], "rb");
  if (!ihxFile) {
    fprintf(stderr, " *** %s: error opening .ihx file\n", argv[0]);
    return -1;
  }
  while (!isEOF) {
    int     c = fgetc(ihxFile);
    if (c == EOF) {
      isEOF = 1;
      c = '\n';
    }
    else {
      c = c & 0xFF;
      if (c == '\r' || c == '\0')
        c = '\n';
    }
    if (c <= ' ') {
      if (c == '\n') {
        if (lineHdrBits > 0 && (lineHdrBits < 32 || lineBytesLeft > 0)) {
          fprintf(stderr, " *** %s: error in .ihx file data\n", argv[0]);
          return -1;
        }
        lineHdrBits = 0;
        ihxByteBits = 0;
        lineBytesLeft = 0;
      }
      continue;
    }
    if (c == ':') {
      if (lineHdrBits > 0) {
        fprintf(stderr, " *** %s: error in .ihx file data\n", argv[0]);
        return -1;
      }
      continue;
    }
    if (!((c >= '0' && c <= '9') ||
          (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      fprintf(stderr, " *** %s: error in .ihx file data\n", argv[0]);
      return -1;
    }
    c = (c <= '9' ? c : (c + 9)) & 0x0F;
    if (lineHdrBits < 32) {
      lineHdrBits = lineHdrBits + 4;
      lineHdr = ((lineHdr & 0x0FFFFFFFU) << 4) | (unsigned int) c;
      if (lineHdrBits >= 32) {
        lineBytesLeft = (int) ((lineHdr >> 24) & 0xFFU);
        ihxAddr = (lineHdr >> 8) & 0xFFFFU;
      }
    }
    else {
      ihxByte = ((ihxByte & 0x0FU) << 4) | (unsigned int) c;
      ihxByteBits = ihxByteBits + 4;
      if (ihxByteBits >= 8) {
        ihxByteBits = 0;
        if (lineBytesLeft > 0) {
          lineBytesLeft--;
          if (ihxAddr >= 0x0100U) {
            if (ihxAddr < (ldrEnd - 0x0020U)) {
              if (ihxAddr >= 0x0120U) {
                fprintf(stderr, " *** %s: error in .ihx file data\n", argv[0]);
                return -1;
              }
              ihxAddr = ihxAddr + (ldrEnd - 0x0120U);
            }
            fileBuf[ihxAddr] = ihxByte;
            if (ihxAddr > lastAddr)
              lastAddr = ihxAddr;
          }
          ihxAddr = (ihxAddr + 1U) & 0xFFFFU;
        }
      }
    }
  }
  fclose(ihxFile);
  if (lastAddr > 0xBFFF) {
    fprintf(stderr, " *** %s: invalid output file size\n", argv[0]);
    return -1;
  }
  fileBuf[0x00F2] = (unsigned char) ((lastAddr + 1) & 0xFF);
  fileBuf[0x00F3] = (unsigned char) (((lastAddr + 1) >> 8) - 1);
  fileBuf[0x0102] = (unsigned char) (lastAddr & 0xFF);
  fileBuf[0x0103] = (unsigned char) (lastAddr >> 8);
  outFile = fopen(argv[3], "wb");
  if (!outFile) {
    fprintf(stderr, " *** %s: error opening output file\n", argv[0]);
    return -1;
  }
  if (fwrite(&(fileBuf[0x00F0]),
             sizeof(unsigned char), (lastAddr + 1) - 0x00F0, outFile)
      != ((lastAddr + 1) - 0x00F0)) {
    fprintf(stderr, " *** %s: error writing output file\n", argv[0]);
    return -1;
  }
  if (fflush(outFile) != 0 || fclose(outFile) != 0) {
    fprintf(stderr, " *** %s: error writing output file\n", argv[0]);
    return -1;
  }
  return 0;
}
