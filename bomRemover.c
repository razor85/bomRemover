#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>

int isUTF8(uint32_t bytes) {
  return (((bytes << 8) & 0xFFFFFFFF) == 0xBFBBEF00);
}

int isUTF16(uint32_t bytes) {
  return (((bytes >> 16) & 0xFFFF) == 0xFEFF)
    || (((bytes >> 16) & 0xFFFF) == 0xFFFE);
}

int isUTF32(uint32_t bytes) {
  return bytes == 0x0000FEFF
    || bytes == 0xFFFE0000;
}

int isUTF7(uint32_t bytes) {
  return bytes == 0x38762F2B
    || bytes == 0x39762F2B
    || bytes == 0x2B762F2B
    || bytes == 0x2F762F2B;
}

int isUTF1(uint32_t bytes) {
  return (((bytes << 8) & 0xFFFFFFFF) == 0x4C64F700);
}

int isUTF_EBCDIC(uint32_t bytes) {
  return bytes == 0x736673DD;
}

int isSCSU(uint32_t bytes) {
  return (((bytes << 8) & 0xFFFFFFFF) == 0xFFFE0E00);
}

int isBOCU(uint32_t bytes) {
  return (((bytes << 8) & 0xFFFFFFFF) == 0x28EEFB00);
}

int isGB18030(uint32_t bytes) {
  return bytes == 0x33953184;
}

char* getNewFilename(const char *filename) {
  const char *nobom = "_nobom";

  size_t originalSize = strlen(filename);
  int dotPosition = -1;

  for (int i = originalSize - 1; i >= 0; --i) {
    if (filename[i] == '.') {
      dotPosition = i;
      break;
    }
  }
    
  const size_t sizeWithoutExtension = (dotPosition == -1) ? 
    originalSize 
    : 
    (originalSize - (originalSize - dotPosition));

  size_t newSize = (dotPosition == -1) ? 
    originalSize 
    : 
    originalSize + strlen(nobom);

  char* newFilename = (char*) malloc(newSize + 1);
  memset(newFilename, 0, newSize + 1);

  if (dotPosition == -1) {
    memcpy(newFilename, filename, originalSize);

  } else {
    
    // Add _nobom before the extension.
    char* newFilenamePtr = newFilename;

    memcpy(newFilenamePtr, filename, sizeWithoutExtension);
    newFilenamePtr += sizeWithoutExtension;

    memcpy(newFilenamePtr, nobom, strlen(nobom));
    newFilenamePtr += strlen(nobom);

    memcpy(newFilenamePtr, filename + sizeWithoutExtension, originalSize - sizeWithoutExtension);
  }

  return newFilename;
}

void removeBytes(size_t numBytes, FILE *filePtr, const char *filename) {
  if (fseek(filePtr, 0, SEEK_END) != 0) {
    printf("Error seeking into file!\n");
    exit(-1);
  }

  int64_t fileSize = ftell(filePtr) - numBytes;
  assert(fileSize >= 0);

  if (fseek(filePtr, numBytes, SEEK_SET) != 0) {
    printf("Error seeking into file!\n");
    exit(-1);
  }

  printf("Allocating %lld bytes..", fileSize);
  fflush(stdout);

  char *fileBuffer = (char*) malloc(fileSize);
  if (fileBuffer == NULL) {
    printf("Failed to allocate data for new file!\n");
    exit(-1);
  }

  printf("done\n");
  
  memset(fileBuffer, 0, fileSize);

  char *dstBuffer = fileBuffer;
  uint64_t readBytes = 0;

  while (readBytes < fileSize) {
    const size_t readNow = fread(dstBuffer, 1, fileSize, filePtr);
    if (readNow == 0) {
      if (feof(filePtr))
        printf("Error reading file: unexpected end of file.\n");
      else if (ferror(filePtr))
        printf("Unknown error reading file.");

      exit(-1);

    } else {
      readBytes += readNow;
      dstBuffer += readNow;
    }
  }

  // New filename
  char *newFilename = getNewFilename(filename);
  assert(newFilename != NULL);

  printf("Writing %s..", newFilename);
  fflush(stdout);

  FILE *newFile = fopen(newFilename, "wb");
  if (newFile == NULL) {
    printf("Failed to open file %s for writing, check your permissions.", newFilename);
    exit(-1);
  }
  
  uint64_t writeBytes = 0;
  char *srcBuffer = fileBuffer;

  while (writeBytes < fileSize) {
    const size_t writtenNow = fwrite(srcBuffer, 1, fileSize, newFile);
    if (writtenNow == 0) {
      if (feof(filePtr))
        printf("Error writing file: unexpected end of file.\n");
      else if (ferror(filePtr))
        printf("Unknown error writing file.");

      exit(-1);

    } else {

      srcBuffer += writtenNow;
      writeBytes += writtenNow;
    }
  }


  fclose(newFile);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Invalid syntax, please pass file name as argument\n");
    return 0;
  }

  FILE *bomFile = fopen(argv[1], "rb");
  if (bomFile == NULL) {
    printf("Failed to open %s\n", argv[1]);
    return -1;
  }

  uint32_t bytes;
  if (fread(&bytes, sizeof(uint32_t), 1, bomFile) != 1) {
    printf("Failed to read %s\n", argv[1]);
    return -1;
  }
  
  printf("Read possible BOM bytes: %x\n", bytes);
  if (isUTF8(bytes)
    || isUTF16(bytes)
    || isUTF32(bytes)
    || isUTF7(bytes)
    || isUTF1(bytes)
    || isUTF_EBCDIC(bytes)
    || isSCSU(bytes)
    || isBOCU(bytes)
    || isGB18030(bytes)) {
  
    printf("BOM detected! Replacing...\n");

    if (isUTF8(bytes) || isUTF1(bytes) || isSCSU(bytes) || isBOCU(bytes))
      removeBytes(3, bomFile, argv[1]);
    else if (isUTF16(bytes))
      removeBytes(2, bomFile, argv[1]);
    else if (isUTF32(bytes) || isUTF7(bytes) || isUTF_EBCDIC(bytes) || isGB18030(bytes))
      removeBytes(4, bomFile, argv[1]);
    else {
      assert(0);
    }
    
    printf("Done!\n");

  } else {
    printf("BOM not detected..nothing to do. Bye!\n");
  }

  return 0;
}

