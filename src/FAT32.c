//*******************************************************
// **** ROUTINES FOR FAT32 IMPLEMATATION OF SD CARD ****
//**********************************************************
// Controller: ATxmega128A3U (32 Mhz internal)
// Compiler	: AVR-GCC
// Version	: 2.2
// Date		: 20 APR 2026
//********************************************************
//**************************************************
// ***** SOURCE FILE : FAT32.c ******
//**************************************************

#define F_CPU 32000000UL

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdint.h>
#include "mcu_driver.h"
#include "FAT32.h"
#include "SD_routines.h"
#include "console.h"


uint32_t FAT_file_position_index = 0;
uint32_t FAT_file_position_cluster = 0;
uint32_t FAT_file_position_sector = 0;
uint32_t FAT_file_size = 0;
uint32_t FAT_working_dir_cluster = 0;
uint32_t FAT_working_dir_cluster_old = 0;
uint16_t index_inside_sector = 0;
uint8_t sector_number = 0;
uint8_t FAT_error_log = 0;

// Global FAT32 state (single definition; declared as extern in FAT32.h)
volatile unsigned long firstDataSector, rootCluster, totalClusters;
volatile unsigned int bytesPerSector, sectorPerCluster, reservedSectorCount;
unsigned long unusedSectors, appendFileSector, appendFileLocation, fileSize,
    appendStartCluster;

// global flag to keep track of free cluster count updating in FSinfo sector
unsigned char freeClusterCountUpdated;

static uint8_t _convert_dir_name(uint8_t *input, uint8_t *output)
{
  uint8_t i = 0;

  // Fill with spaces
  for (i = 0; i < 11; i++)
    output[i] = ' ';

  // Copy name (max 8 chars)
  for (i = 0; i < 8 && input[i] != '\0'; i++) 
  {
    if (input[i] == '.')
      { FAT_error_log = FAT_ERR_NAME_CONVERSION; return 0; }// directories shouldn't have '.'

    // uppercase
    if (input[i] >= 'a' && input[i] <= 'z')
      output[i] = input[i] - 32;
    else
      output[i] = input[i];
  }

  return 1;
}

static uint8_t _convert_file_name(uint8_t *fileName, uint8_t *fileNameFAT) 
{
  uint8_t j, k;

  for (j = 0; j < 12; j++)
    if (fileName[j] == '.')
      break;

  if (j > 8) {
    FAT_error_log = FAT_ERR_FILE_NAME_FORMAT;
    return 0;
  }

  for (k = 0; k < j; k++) // setting file name
    fileNameFAT[k] = fileName[k];

  for (k = j; k < 8; k++) // filling file name trail with blanks
    fileNameFAT[k] = ' ';

  j++;
  for (k = 8; k < 11; k++) // setting file extention
  {
    if (fileName[j] != 0)
      fileNameFAT[k] = fileName[j++];
    else // filling extension trail with blanks
      fileNameFAT[k] = ' ';
  }

  for (j = 0; j < 11; j++) // converting small letters to caps
    if ((fileNameFAT[j] >= 0x61) && (fileNameFAT[j] <= 0x7a))
      fileNameFAT[j] -= 0x20;

  return 1;
}

//***************************************************************************
// Function: to calculate first sector address of any given cluster
// Arguments: cluster number for which first sector is to be found
// return: first sector address
//***************************************************************************
static uint32_t _get_first_sector(uint32_t cluster_number)
{
  return (((cluster_number - 2) * sectorPerCluster) + firstDataSector);
}

static uint32_t _get_next_cluster(uint32_t cluster_number)
{
  uint32_t entry_sector = unusedSectors + reservedSectorCount +
                          ((cluster_number * 4) / bytesPerSector);
  uint16_t entry_offset = (uint16_t)((cluster_number * 4) % bytesPerSector);

  SD_readSingleBlock(entry_sector);

  uint32_t *entry_value = (uint32_t *)&buffer[entry_offset];

  return ((*entry_value) & 0x0fffffff);
}

static uint8_t _set_next_cluster(uint32_t cluster_number, uint32_t new_cluster_entry)
{
  uint32_t entry_sector = unusedSectors + reservedSectorCount +
                          ((cluster_number * 4) / bytesPerSector);
  uint16_t entry_offset = (uint16_t)((cluster_number * 4) % bytesPerSector);

  SD_readSingleBlock(entry_sector);

  uint32_t *entry_value = (uint32_t *)&buffer[entry_offset];

  *entry_value = new_cluster_entry; // for setting new value in cluster entry in FAT

  SD_writeSingleBlock(entry_sector);

  return 0;
}

static uint32_t _get_next_free_cluster()
{
  struct FSInfo_Structure *FS = (struct FSInfo_Structure *)&buffer;

  SD_readSingleBlock(unusedSectors + 1);

  if ((FS->leadSignature != 0x41615252) ||
      (FS->structureSignature != 0x61417272) ||
      (FS->trailSignature != 0xaa550000))
    return 0xffffffff;

  //if (totOrNext == TOTAL_FREE) return (FS->freeClusterCount);
  
  return (FS->nextFreeCluster);
}

static uint32_t _get_total_free_cluster()
{
  struct FSInfo_Structure *FS = (struct FSInfo_Structure *)&buffer;

  SD_readSingleBlock(unusedSectors + 1);

  if ((FS->leadSignature != 0x41615252) ||
      (FS->structureSignature != 0x61417272) ||
      (FS->trailSignature != 0xaa550000))
    return 0xffffffff;

  return (FS->freeClusterCount);
  
  //return (FS->nextFreeCluster);
}

static uint8_t _set_next_free_cluster(uint32_t new_cluster_entry)
{
  struct FSInfo_Structure *FS = (struct FSInfo_Structure *)&buffer;

  SD_readSingleBlock(unusedSectors + 1);

  if ((FS->leadSignature != 0x41615252) ||
      (FS->structureSignature != 0x61417272) ||
      (FS->trailSignature != 0xaa550000))
    return 0;

  //FS->freeClusterCount = new_cluster_entry;

  FS->nextFreeCluster = new_cluster_entry;

  return SD_writeSingleBlock(unusedSectors + 1);
}

static uint8_t _set_total_free_cluster(uint32_t new_cluster_entry)
{
  struct FSInfo_Structure *FS = (struct FSInfo_Structure *)&buffer;

  SD_readSingleBlock(unusedSectors + 1);

  if ((FS->leadSignature != 0x41615252) ||
      (FS->structureSignature != 0x61417272) ||
      (FS->trailSignature != 0xaa550000))
    return 0;

  FS->freeClusterCount = new_cluster_entry;

  //FS->nextFreeCluster = new_cluster_entry;

  return SD_writeSingleBlock(unusedSectors + 1);
}

static uint32_t _search_next_free_cluster(uint32_t startCluster)
{
  uint32_t cluster, *value, sector;
  uint8_t i;

  startCluster -= (startCluster % 128); // to start with the first file in a FAT sector

  for (cluster = startCluster; cluster < totalClusters; cluster += 128)
  {
    sector = unusedSectors + reservedSectorCount + ((cluster * 4) / bytesPerSector);
    
    SD_readSingleBlock(sector);
    
    for (i = 0; i < 128; i++)
    {
      value = (uint32_t *)&buffer[i * 4];
      if (((*value) & 0x0fffffff) == 0) return (cluster + i);
    }
  }

  return 0;
}

static void _update_free_mem(uint8_t flag, uint32_t size)
{
  uint32_t freeClusters;
  // convert file size into number of clusters occupied
  if ((size % 512) == 0)
    size = size / 512;
  else
    size = (size / 512) + 1;
  if ((size % 8) == 0)
    size = size / 8;
  else
    size = (size / 8) + 1;

  if (freeClusterCountUpdated)
  {
    freeClusters = _get_total_free_cluster();

    if (flag == ADD)
      freeClusters = freeClusters + size;
    else // when flag = REMOVE
      freeClusters = freeClusters - size;

    _set_total_free_cluster(freeClusters);
  }
}

unsigned char getBootSectorData(void) {
  struct BS_Structure *bpb; // mapping the buffer onto the structure
  struct MBRinfo_Structure *mbr;
  struct partitionInfo_Structure *partition;
  unsigned long dataSectors;

  unusedSectors = 0;

  SD_readSingleBlock(0);
  bpb = (struct BS_Structure *)buffer;

  if (bpb->jumpBoot[0] != 0xE9 &&
      bpb->jumpBoot[0] != 0xEB) // check if it is boot sector
  {
    mbr = (struct MBRinfo_Structure *)
        buffer; // if it is not boot sector, it must be MBR

    if (mbr->signature != 0xaa55)
      return 1; // if it is not even MBR then it's not FAT32

    partition = (struct partitionInfo_Structure
                     *)(mbr->partitionData); // first partition
    unusedSectors =
        partition->firstSector; // the unused sectors, hidden to the FAT

    SD_readSingleBlock(partition->firstSector); // read the bpb sector
    bpb = (struct BS_Structure *)buffer;
    if (bpb->jumpBoot[0] != 0xE9 && bpb->jumpBoot[0] != 0xEB)
      return 1;
  }

  bytesPerSector = bpb->bytesPerSector;
  sectorPerCluster = bpb->sectorPerCluster;
  reservedSectorCount = bpb->reservedSectorCount;
  rootCluster = bpb->rootCluster; // + (sector / sectorPerCluster) +1;
  firstDataSector = bpb->hiddenSectors + reservedSectorCount +
                    (bpb->numberofFATs * bpb->FATsize_F32);

  dataSectors = bpb->totalSectors_F32 - bpb->reservedSectorCount -
                (bpb->numberofFATs * bpb->FATsize_F32);
  totalClusters = dataSectors / sectorPerCluster;

  if ((_get_total_free_cluster()) >
      totalClusters) // check if FSinfo free clusters count is valid
    freeClusterCountUpdated = 0;
  else
    freeClusterCountUpdated = 1;
  return 0;
}

uint32_t _get_working_cluster()
{
  SD_check_alive();

  if (!(SD_status & SD_STATUS_INIT_OK)) {
    FAT_error_log = FAT_ERR_NO_SD_CARD;
    return 0;
  } // SD card not loaded

  uint32_t cluster = FAT_working_dir_cluster;
  if (!cluster)
    cluster = rootCluster;

  if (!cluster) {
    getBootSectorData();
    cluster = rootCluster;
    if (cluster)
      FAT_working_dir_cluster = cluster;
  }

  if (!cluster) {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

  return cluster;
}

uint8_t FAT_unload_directory(void)
{
  SD_check_alive();
  if (!(SD_status & SD_STATUS_INIT_OK)) {
    FAT_working_dir_cluster = 0;
    FAT_error_log = FAT_ERR_NO_SD_CARD;
    return 0;
  } // SD card not loaded
  FAT_working_dir_cluster = FAT_working_dir_cluster_old;
  return 1;
}

uint8_t FAT_load_directory(uint8_t *dirname) // One at a time
{
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;
  uint8_t j;

  cluster = _get_working_cluster();
  if(!cluster) return 0;

  uint8_t formattedName[11];
  if(!_convert_dir_name(dirname, formattedName)) return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] ==
            EMPTY) // Indicates end of the file list of the directory
        {
          FAT_error_log = FAT_ERR_END_OF_CLUSTER;
          return 0;
        }

        if ((dir->name[0] != DELETED) &&
            (dir->attrib == ATTR_DIRECTORY || dir->attrib == ATTR_ARCHIVE)) {
          uint8_t result = 1;
          for (j = 0; j < 11; j++) {
            if (dir->name[j] != formattedName[j]) {
              result = 0;
              break;
            }
          }
          if (result) {
            FAT_working_dir_cluster_old = FAT_working_dir_cluster;
            FAT_working_dir_cluster =
                (((uint32_t)dir->firstClusterHI) << 16) | dir->firstClusterLO;
            return 1;
          } // Loaded
        }
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }

  FAT_error_log = FAT_ERR_NO_MATCH;
  return 0;
}

uint8_t FAT_count_files_in_directory(uint16_t *count) // NULL for ROOT
{
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;

  count[0] = 0;

  // Find the cluster of the specified directory
  cluster = _get_working_cluster();
  if(!cluster) return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] == EMPTY) {
          FAT_error_log = FAT_OK;
          return 1;
        } // Last file
        if ((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME) &&
            (dir->attrib != ATTR_VOLUME_ID))
          count[0]++;
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }
  return 0;
}

uint8_t FAT_get_file_name(uint16_t index, uint8_t *file_data)
{
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;

  if (index == 0)
    return 0;

  // Find the cluster of the specified directory
  cluster = _get_working_cluster();
  if(!cluster) return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] == EMPTY)
          return 1; // Last file
        if ((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME) &&
            (dir->attrib != ATTR_VOLUME_ID))
          index--;
        if (!index) {
          memcpy(file_data, dir, 32);
          // file_data = (uint8_t *)dir;
          return 1;
        }
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_GET_CLUSTER;
  return 0;
}

uint8_t FAT_get_volume_id(uint8_t *volume_id) {
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;

  if (!(SD_status & SD_STATUS_INIT_OK)) {
    FAT_error_log = FAT_ERR_NO_SD_CARD;
    return 0;
  } // SD card not loaded

  // Find the cluster of the specified directory
  cluster = rootCluster; // No loaded directory yet
  if (!cluster)          // Still 0? then FAT not initialized
  {
    getBootSectorData();
    cluster = rootCluster;
  }
  if (!cluster) // Still 0? Then probably no SD card in or FAT broken
  {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] == EMPTY) {
          FAT_error_log = FAT_ERR_NO_MATCH;
          return 0;
        } // Last file
        if (dir->attrib == ATTR_VOLUME_ID) {
          memcpy(volume_id, dir, 11);
          return 1;
        }
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_GET_CLUSTER;
  return 0;
}

uint8_t FAT_find_file(uint8_t *fileNameDirty, uint8_t *file_data) {
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint8_t fileName[11];
  uint16_t i;

  // Find the cluster of the specified directory
  cluster = _get_working_cluster();
  if(!cluster) return 0;

  if (!_convert_file_name(fileNameDirty, fileName))
    return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];
        if (dir->name[0] == EMPTY) {
          FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
          return 0;
        } // Last file
        if ((dir->attrib != ATTR_LONG_NAME) &&
            (dir->attrib != ATTR_VOLUME_ID) &&
            (dir->attrib != ATTR_DIRECTORY) && (dir->name[0] == fileName[0]) &&
            (dir->name[1] == fileName[1]) && (dir->name[2] == fileName[2]) &&
            (dir->name[3] == fileName[3]) && (dir->name[4] == fileName[4]) &&
            (dir->name[5] == fileName[5]) && (dir->name[6] == fileName[6]) &&
            (dir->name[7] == fileName[7]) && (dir->name[8] == fileName[8]) &&
            (dir->name[9] == fileName[9]) && (dir->name[10] == fileName[10])) {
          memcpy(file_data, dir, 32);
          FAT_error_log = FAT_OK;
          return 1;
        }
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_GET_CLUSTER;
  return 0;
}

uint8_t FAT_open_file(uint8_t *fileName) {
  uint8_t data[32];
  struct dir_Structure *dir = (struct dir_Structure *)data;

  FAT_file_position_cluster = 0;
  FAT_file_position_sector = 0;
  FAT_file_size = 0;
  FAT_file_position_index = 0;

  if (!FAT_find_file(fileName, data))
    return 0;

  FAT_file_position_cluster =
      (((unsigned long)dir->firstClusterHI) << 16) | dir->firstClusterLO;
  FAT_file_position_sector = 0;
  FAT_file_size = dir->fileSize;
  FAT_file_position_index = 0;
  index_inside_sector = 0;
  sector_number = 0;

  return 1;
}

void FAT_close_file() {
  FAT_file_position_cluster = 0;
  FAT_file_position_sector = 0;
  FAT_file_size = 0;
  FAT_file_position_index = 0;
  index_inside_sector = 0;
  sector_number = 0;
}

uint8_t FAT_read_file_line(uint8_t *fileName, uint8_t *data)
{
  uint16_t bytes_read = 0;

  if (!FAT_file_size) {
    if (!FAT_open_file(fileName))
      return 0;
  }

  while (1) {
    if (!FAT_file_position_sector)
      FAT_file_position_sector = _get_first_sector(FAT_file_position_cluster);

    for (uint8_t j = sector_number; j < sectorPerCluster; j++) {
      SD_readSingleBlock(FAT_file_position_sector + j);

      for (uint16_t k = index_inside_sector; k < 512; k++) {

        uint8_t c = buffer[k];

        /* ── advance file position first ── */
        FAT_file_position_index++;

        /* ── newline / null terminator: end of this line ── */
        if (c == '\n' || c == '\r' || c == '\0') {
          data[bytes_read] = '\n';
          data[bytes_read + 1] = '\0';

          /*
           * Save the position of the NEXT byte so the following call
           * starts on the first char of the next line, not the
           * control character we just consumed.
           */
          uint16_t next_k = k + 1;
          uint8_t  next_j = j;

          if (next_k >= 512) {
            next_k = 0;
            next_j = j + 1;
          }

          if (next_j >= sectorPerCluster) {
            FAT_file_position_cluster =
                _get_next_cluster(FAT_file_position_cluster);
            FAT_file_position_sector = 0;
            next_j = 0;
          }

          index_inside_sector = next_k;
          sector_number       = next_j;

          /* Skip consecutive \r\n pairs (Windows line endings) */
          if (FAT_file_position_index < FAT_file_size) {
            FAT_error_log = FAT_OK;
            return 1;
          } else {
            /*
             * The newline was the very last byte in the file.
             * We already have a complete line in data[] (possibly empty).
             * Return it if it has content, otherwise signal EOF.
             */
            if (bytes_read > 0) {
              FAT_error_log = FAT_EOF;
              FAT_close_file();
              return 1;
            }
            FAT_error_log = FAT_ERR_END_OF_FILE;
            FAT_close_file();
            return 0;
          }
        }

        /* ── normal character: store it ── */
        data[bytes_read++] = c;

        /* ── last byte of file and it was NOT a newline ── */
        if (FAT_file_position_index >= FAT_file_size) {
          /*
           * Bug that was here before: the check happened before storing
           * the char, so the last char of the last line was never stored
           * and the line was returned as 0 (error/EOF) instead of 1.
           *
           * Fix: we store the char first (above), null-terminate, then
           * close and return 1 so the caller gets the final line.
           */
          data[bytes_read] = '\0';
          FAT_error_log = FAT_OK;
          FAT_close_file();
          return 1;   /* last line, no trailing newline — still valid */
        }
      }
      index_inside_sector = 0;
    }

    FAT_file_position_cluster = _get_next_cluster(FAT_file_position_cluster);
    FAT_file_position_sector  = 0;
    index_inside_sector       = 0;
    sector_number             = 0;

    if (FAT_file_position_cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      FAT_close_file();
      return 0;
    }
  }

  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

/*uint8_t FAT_read_file_line(uint8_t *fileName, uint8_t *data) {
  uint16_t bytes_read = 0;
  uint8_t find_nl = 0;

  if (!FAT_file_size) {
    if (!FAT_open_file(fileName))
      return 0;
  }

  while (1) {
    if (!FAT_file_position_sector)
      FAT_file_position_sector = _get_first_sector(FAT_file_position_cluster);

    for (uint8_t j = sector_number; j < sectorPerCluster; j++) {
      SD_readSingleBlock(FAT_file_position_sector + j);
      for (uint16_t k = index_inside_sector; k < 512; k++) {
        if (!find_nl)
          data[bytes_read++] = buffer[k]; // If new line not found, store char
        if (((buffer[k] == '\n') || (buffer[k] == '\r') ||
             (buffer[k] == '\0')) &&
            (!find_nl)) // If new line found, set flag
        {
          data[bytes_read] = '\0';
          FAT_error_log = FAT_OK;
          find_nl = 1;
        }
        if ((buffer[k] != '\n') && (buffer[k] != '\r') && (buffer[k] != '\0') &&
            (find_nl)) // If nl found, but no control char, ->nex line start
        {
          if (k + 1 < 512) {
            index_inside_sector = k + 1;
            sector_number = j;
          } else {
            index_inside_sector = 0;
            sector_number = j + 1;
          }

          if (sector_number >= sectorPerCluster) {
            FAT_file_position_cluster =
                _get_next_cluster(FAT_file_position_cluster);

            FAT_file_position_sector = 0;
            sector_number = 0;
          }

          FAT_error_log = FAT_OK;
          return 1;
        }
        FAT_file_position_index++;
        if (FAT_file_position_index >= FAT_file_size) {
          FAT_error_log = FAT_ERR_END_OF_FILE;
          FAT_close_file();
          return 0;
        }
      }
      index_inside_sector = 0;
    }
    FAT_file_position_cluster = _get_next_cluster(FAT_file_position_cluster);
    FAT_file_position_sector = 0;
    index_inside_sector = 0;
    sector_number = 0;
    if (FAT_file_position_cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      FAT_close_file();
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  FAT_close_file();
  return 0;
}*/

uint8_t FAT_read_specific_file_line(uint8_t *fileName, uint8_t *data,
                                    uint16_t line_index) {
  uint8_t data_structure[32];
  struct dir_Structure *dir = (struct dir_Structure *)data_structure;
  uint32_t cluster, sector, filesize, bytes_read = 0;
  uint8_t find_nl = 0;

  if (!FAT_find_file(fileName, data_structure)) {
    return 0;
  }

  cluster = (((unsigned long)dir->firstClusterHI) << 16) | dir->firstClusterLO;
  filesize = dir->fileSize;

  while (1) {
    sector = _get_first_sector(cluster);

    for (uint8_t j = 0; j < sectorPerCluster; j++) {
      SD_readSingleBlock(sector + j);
      for (uint16_t k = 0; k < 512; k++) {
        // write_file_in_lcd(buffer[k]);
        if (!line_index) {
          data[k] = buffer[k];
          if ((buffer[k] == '\n') || (buffer[k] == '\0') ||
              (buffer[k] == '\r')) {
            data[k] = '\0';
            FAT_error_log = FAT_OK;
            return 1; // Line end found
          }
        }
        if (line_index) {
          if ((buffer[k] == '\n') || (buffer[k] == '\0') || (buffer[k] == '\r'))
            find_nl = 1;
          if ((buffer[k] != '\n') && (buffer[k] != '\r') &&
              (buffer[k] != '\0') && (find_nl)) {
            line_index--; // New line start detected
            find_nl = 0;
          }
        }
        if (bytes_read++ >= filesize) {
          FAT_error_log = FAT_ERR_END_OF_FILE;
          return 0;
        }
      }
    }
    cluster = _get_next_cluster(cluster);
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

uint8_t FAT_read_bytes(uint8_t *fileName, uint8_t *data, uint16_t bytes) {
  uint16_t bytes_read = 0;

  if (!FAT_file_size) {
    if (!FAT_open_file(fileName))
      return 0;
  }

  while (1) {
    if (!FAT_file_position_sector)
      FAT_file_position_sector = _get_first_sector(FAT_file_position_cluster);

    for (uint8_t j = sector_number; j < sectorPerCluster; j++) {
      SD_readSingleBlock(FAT_file_position_sector + j);
      uint16_t start_k = (j == sector_number) ? index_inside_sector : 0;

      for (uint16_t k = start_k; k < 512; k++) {
        data[bytes_read++] = buffer[k];
        FAT_file_position_index++;

        if (bytes_read == bytes) {
          if (k + 1 < 512) {
            index_inside_sector = k + 1;
            sector_number = j;
          } else {
            index_inside_sector = 0;
            sector_number = j + 1;
          }

          if (sector_number >= sectorPerCluster) {
            FAT_file_position_cluster =
                _get_next_cluster(FAT_file_position_cluster);

            FAT_file_position_sector = 0;
            sector_number = 0;
          }
          FAT_error_log = FAT_OK;
          return 1;
        }

        if (FAT_file_position_index >= FAT_file_size) {
          FAT_error_log = FAT_EOF;
          FAT_close_file();
          return 0;
        }
      }
      // index_inside_sector = 0;
    }
    FAT_file_position_cluster = _get_next_cluster(FAT_file_position_cluster);
    FAT_file_position_sector = 0;
    index_inside_sector = 0;
    sector_number = 0;
    if (FAT_file_position_cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      FAT_close_file();
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

uint8_t FAT_create_file(uint8_t *fileName) {
  uint8_t j, fileCreatedFlag = 0, sector;
  uint16_t i, firstClusterHigh, firstClusterLow;
  uint32_t cluster, prevCluster, nextCluster, firstSector;
  struct dir_Structure *dir;

  /* Reject if file already exists */
  if (FAT_open_file(fileName)) {
    FAT_close_file();
    FAT_error_log = FAT_ERR_ALREADY_EXISTS;
    return 0;
  }

  /* Find a free cluster to use as the file's first (and initially only) cluster
   */
  cluster = _get_next_free_cluster();
  if (cluster > totalClusters)
    cluster = 2;

  cluster = _search_next_free_cluster(cluster);
  if (cluster == 0) {
    FAT_error_log = FAT_ERR_NO_FREE_CLUSTER;
    return 0;
  }

  /* Mark it as end-of-chain — file is empty for now */
  _set_next_cluster(cluster, EOF1);
  _set_next_free_cluster(cluster);

  firstClusterHigh = (uint16_t)((cluster & 0xFFFF0000) >> 16);
  firstClusterLow = (uint16_t)(cluster & 0x0000FFFF);

  /* Walk the root directory to find a free slot for the directory entry */
  prevCluster = FAT_working_dir_cluster;

  while (1) {
    firstSector = _get_first_sector(prevCluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        /* After writing our entry, mark the next slot as the new
         * end-of-directory */
        if (fileCreatedFlag) {
          dir->name[0] = 0x00;
          FAT_error_log = FAT_OK;
          return 1;
        }

        if ((dir->name[0] == EMPTY) || (dir->name[0] == DELETED)) {
          uint8_t fileNameFAT[11];
          _convert_file_name(fileName, fileNameFAT);
          for (j = 0; j < 11; j++)
            dir->name[j] = fileNameFAT[j];

          dir->attrib = ATTR_ARCHIVE;
          dir->NTreserved = 0;
          dir->timeTenth = 0;
          dir->createTime = system_time;
          dir->createDate = system_date;
          dir->lastAccessDate = system_date;
          dir->writeTime = 0x9684;
          dir->writeDate = 0x3A37;
          dir->firstClusterHI = firstClusterHigh;
          dir->firstClusterLO = firstClusterLow;
          dir->fileSize = 0; /* no data yet */

          SD_writeSingleBlock(firstSector + sector);
          fileCreatedFlag = 1;
          /* continue scanning to stamp the end-of-directory marker */
        }
      }
    }

    /* Advance to the next cluster of the root directory */
    nextCluster = _get_next_cluster(prevCluster);

    if (nextCluster > 0x0FFFFFF6) {
      if (nextCluster == EOF1) {
        /* Root directory is full — allocate a new cluster for it */
        nextCluster = _search_next_free_cluster(prevCluster);
        if (nextCluster == 0) {
          FAT_error_log = FAT_ERR_DIR_FULL;
          return 0;
        }
        _set_next_cluster(prevCluster, nextCluster);
        _set_next_cluster(nextCluster, EOF1);
      } else {
        FAT_error_log = FAT_ERR_CLUSTER_CHAIN;
        return 0;
      }
    }

    if (nextCluster == 0) {
      FAT_error_log = FAT_ERR_CLUSTER_CHAIN;
      return 0;
    }

    prevCluster = nextCluster;
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

uint8_t FAT_append_data(uint8_t *fileNameDirty, uint8_t *data,
                        uint16_t byte_count) {
  uint8_t j, sector, error;
  uint16_t i;
  uint32_t cluster, prevCluster, nextCluster, firstSector;
  uint32_t existingSize, totalWritten;

  /* FAT_open_file does one SD read internally — it will clobber `buffer`.
   * That is fine here because we capture everything we need from the
   * globals it populates, before we touch the SD card again. */
  if (!FAT_open_file(fileNameDirty)) {
    FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
    return 0;
  }

  /* Snapshot globals before any further SD I/O clobbers them */
  existingSize = FAT_file_size;
  cluster = FAT_file_position_cluster;

  /* ----------------------------------------------------------------
   * Walk the cluster chain to the last cluster.
   * ---------------------------------------------------------------- */
  while (1) {
    nextCluster = _get_next_cluster(cluster);
    if (nextCluster == EOF1 || nextCluster >= 0x0FFFFFF6)
      break;
    if (nextCluster == 0) {
      FAT_close_file();
      FAT_error_log = FAT_ERR_CLUSTER_CHAIN;
      return 0;
    }
    cluster = nextCluster;
  }

  /* ----------------------------------------------------------------
   * Calculate write position inside the last cluster.
   *
   * Edge case: if existingSize is an exact multiple of cluster size
   * the last cluster is already full — allocate a new one immediately.
   * ---------------------------------------------------------------- */
  uint32_t clusterSize = (uint32_t)sectorPerCluster * bytesPerSector;
  uint32_t bytesInLastCluster = existingSize % clusterSize;

  if (existingSize > 0 && bytesInLastCluster == 0) {
    prevCluster = cluster;
    cluster = _search_next_free_cluster(prevCluster);
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_NO_FREE_CLUSTER;
      return 0;
    }
    _set_next_cluster(prevCluster, cluster);
    _set_next_cluster(cluster, EOF1);
    bytesInLastCluster = 0;
  }

  sector = (uint8_t)(bytesInLastCluster / bytesPerSector);
  i = (uint16_t)(bytesInLastCluster % bytesPerSector);
  firstSector = _get_first_sector(cluster);
  uint32_t startBlock = firstSector + sector;

  /* Resume mid-sector: must read existing content before overwriting */
  if (i != 0)
    SD_readSingleBlock(startBlock);

  /* ----------------------------------------------------------------
   * Write loop
   * ---------------------------------------------------------------- */
  totalWritten = 0;
  while (totalWritten < byte_count) {
    buffer[i++] = data[totalWritten++];

    if (i == bytesPerSector) {
      error = SD_writeSingleBlock(startBlock);
      if (error) {
        FAT_error_log = FAT_ERR_WRITE_FAILED;
        return 0;
      }

      i = 0;
      sector++;

      if (sector == sectorPerCluster) {
        sector = 0;
        prevCluster = cluster;
        cluster = _search_next_free_cluster(prevCluster);
        if (cluster == 0) {
          FAT_error_log = FAT_ERR_NO_FREE_CLUSTER;
          return 0;
        }
        _set_next_cluster(prevCluster, cluster);
        _set_next_cluster(cluster, EOF1);
      }

      firstSector = _get_first_sector(cluster);
      startBlock = firstSector + sector;
    }
  }

  /* Flush the final partial sector, zero-padding to the sector boundary */
  if (i != 0) {
    for (; i < bytesPerSector; i++)
      buffer[i] = 0x00;

    error = SD_writeSingleBlock(startBlock);
    if (error) {
      FAT_close_file();
      FAT_error_log = FAT_ERR_WRITE_FAILED;
      return 0;
    }
  }

  /* Update FSinfo free-cluster hint */
  _set_next_free_cluster(cluster);

  /* ----------------------------------------------------------------
   * Update directory entry.
   *
   * Scan from FAT_working_dir_cluster — already loaded by the cd
   * function, no need to re-derive it. This is the ONLY directory
   * scan in the function and it happens after all data writes are
   * done, so there is no buffer aliasing risk.
   * ---------------------------------------------------------------- */
  uint8_t fileName[11];
  _convert_file_name(fileNameDirty, fileName);

  uint8_t found = 0;
  uint32_t scanCluster = FAT_working_dir_cluster;

  while (!found) {
    firstSector = _get_first_sector(scanCluster);

    for (sector = 0; sector < sectorPerCluster && !found; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector && !found; i += 32) {
        struct dir_Structure *dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] == 0x00)
          goto update_done; /* end of directory */
        if (dir->name[0] == DELETED)
          continue;

        uint8_t match = 1;
        for (j = 0; j < 11; j++)
          if (dir->name[j] != fileName[j]) {
            match = 0;
            break;
          }

        if (match) {
          dir->fileSize = existingSize + byte_count;
          dir->writeTime = 0x9684;
          dir->writeDate = 0x3A37;
          dir->lastAccessDate = 0x3A37;

          SD_writeSingleBlock(firstSector + sector);
          found = 1;
        }
      }
    }

    nextCluster = _get_next_cluster(scanCluster);
    if (nextCluster >= 0x0FFFFFF6 || nextCluster == 0)
      break;
    scanCluster = nextCluster;
  }
update_done:;

  _update_free_mem(REMOVE, byte_count);
  FAT_close_file();
  FAT_error_log = FAT_OK;
  return 1;
}

uint8_t FAT_delete_file(uint8_t *fileNameDirty) {
  uint32_t cluster, sector, firstSector;
  uint32_t firstCluster, nextCluster;
  struct dir_Structure *dir;
  uint8_t fileName[11];
  uint16_t i;

  // Find the cluster of the specified directory
  cluster = _get_working_cluster();
  if(!cluster) return 0;

  if (!_convert_file_name(fileNameDirty, fileName)) return 0;

  // =========================
  // SEARCH FILE
  // =========================
  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] == EMPTY) {
          FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
          return 0;
        }

        if ((dir->attrib != ATTR_LONG_NAME) &&
            (dir->attrib != ATTR_VOLUME_ID) &&
            (dir->attrib != ATTR_DIRECTORY) &&
            (memcmp(dir->name, fileName, 11) == 0)) {
          // =========================
          // 1. SAVE CLUSTER
          // =========================
          firstCluster =
              (((uint32_t)dir->firstClusterHI) << 16) | dir->firstClusterLO;

          // =========================
          // 2. MARK AS DELETED
          // =========================
          dir->name[0] = DELETED;

          if (SD_writeSingleBlock(firstSector + sector)) {
            FAT_error_log = FAT_ERR_SD_WRITE;
            return 0;
          }

          // =========================
          // 3. FREE CLUSTER CHAIN
          // =========================
          uint32_t currentCluster = firstCluster;

          while (currentCluster >= 2 && currentCluster < 0x0FFFFFF8) {
            nextCluster = _get_next_cluster(currentCluster);
            _set_next_cluster(currentCluster, 0);
            currentCluster = nextCluster;
          }

          // =========================
          // 4. UPDATE FSINFO
          // =========================
          uint32_t nextFree = _get_next_free_cluster();

          if (firstCluster < nextFree)
            _set_next_free_cluster(firstCluster);

          // =========================
          // 5. UPDATE FREE MEMORY
          // =========================
          _update_free_mem(ADD, dir->fileSize);

          FAT_error_log = FAT_OK;
          return 1;
        }
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
      return 0;
    }

    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }

  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

void _debug_dump_sector(uint32_t sector, const char *label)
{
    uint8_t name[12];
    name[11] = '\0';
    SD_readSingleBlock(sector);
    _print_f("\r\n=== %s (sector %lu) ===\r\n", label, sector);
    for (uint16_t i = 0; i < bytesPerSector; i += 32)
    {
        struct dir_Structure *d = (struct dir_Structure*)&buffer[i];
        if (d->name[0] == 0x00) { _print_f("  [%lu] EMPTY\n", (uint32_t)i); break; }
        if (d->name[0] == 0xE5) { _print_f("  [%lu] DELETED\n", (uint32_t)i); continue; }
        memcpy(name, d->name, 11);
        _print_f("  [%lu] name='%s' attr=%lx cluHI=%lu cluLO=%lu size=%lu\n",
               (uint32_t)i,
               name,
               (uint32_t)d->attrib,
               (uint32_t)d->firstClusterHI,
               (uint32_t)d->firstClusterLO,
               (uint32_t)d->fileSize);
    }
}

uint8_t FAT_create_dir(uint8_t *dirname)
{
  uint8_t name[11];
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;

  if (!_convert_dir_name(dirname, name)) return 0;

  // Find the cluster of the specified directory
  cluster = _get_working_cluster();
  if(!cluster) return 0;

  //console_write_d(dirname);
  //console_write_d(name);

    //_print_f("\r\n>>> FAT_create_dir: '%s'\n", dirname);
    //_print_f("    working_dir_cluster=%lu  rootCluster=%lu\n", FAT_working_dir_cluster, rootCluster);

    //uint32_t workingDirSector = _get_first_sector(cluster);
    //_print_f("    working dir first sector=%lu\n", workingDirSector);

    // Dump working dir BEFORE we touch anything
    //_debug_dump_sector(workingDirSector, "WORKING DIR - BEFORE");

    while(1)
    {
        firstSector = _get_first_sector(cluster);

        for(sector = 0; sector < sectorPerCluster; sector++)
        {
            SD_readSingleBlock(firstSector + sector);

            for(i = 0; i < bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure*)&buffer[i];

                if(dir->name[0] == EMPTY || dir->name[0] == DELETED)
                {
                    // --- ALLOCATION ---
                    uint32_t newCluster = _get_next_free_cluster();
                    //_print_f("    FSInfo hint = %lu\r\n", newCluster);

                    if (newCluster > totalClusters) newCluster = 2;
                    newCluster = _search_next_free_cluster(newCluster);
                    //_print_f("    actual free cluster found = %lu\n", newCluster);
                    //_print_f("    working dir cluster = %lu\n", cluster);
                    //_print_f("    newCluster first sector = %lu\n", _get_first_sector(newCluster));
                    //_print_f("    working dir first sector = %lu\n", firstSector);

                    // *** CRITICAL CHECK ***
                    if (newCluster == cluster || newCluster == FAT_working_dir_cluster)
                    {
                        FAT_error_log = FAT_DIRECTORY_CLUSTER_OVERWRITE;
                        //_print_f("!!! DANGER: newCluster overlaps working directory!\n");
                        return 0;
                    }

                    // Dump working dir BEFORE write
                    //_debug_dump_sector(firstSector, "WORKING DIR - BEFORE ENTRY WRITE");

                    _set_next_cluster(newCluster, 0x0FFFFFFF);

                    // Dump working dir AFTER FAT write — did _set_next_cluster trash buffer?
                    //_debug_dump_sector(firstSector, "WORKING DIR - AFTER SET_NEXT_CLUSTER");

                    // Re-read because _set_next_cluster clobbered buffer
                    SD_readSingleBlock(firstSector + sector);
                    dir = (struct dir_Structure*)&buffer[i];

                    memcpy(dir->name, name, 11);
                    dir->attrib = ATTR_DIRECTORY;
                    dir->firstClusterHI = (newCluster >> 16) & 0xFFFF;
                    dir->firstClusterLO = newCluster & 0xFFFF;
                    dir->fileSize = 0;

                    SD_writeSingleBlock(firstSector + sector);
                    //_debug_dump_sector(firstSector + sector, "WORKING DIR - AFTER ENTRY WRITE");

                    // Zero new cluster
                    uint32_t dirFirstSector = _get_first_sector(newCluster);
                    //_print_f("    zeroing sectors %lu to %lu\n", dirFirstSector, dirFirstSector + sectorPerCluster - 1);

                    for(uint8_t s = 0; s < sectorPerCluster; s++)
                    {
                        memset(buffer, 0, bytesPerSector);
                        SD_writeSingleBlock(dirFirstSector + s);
                    }

                    // Dump working dir AFTER zeroing — did we zero the wrong sector?
                    //_debug_dump_sector(firstSector, "WORKING DIR - AFTER ZERO FILL");

                    // Write . and ..
                    SD_readSingleBlock(dirFirstSector);
                    
                    struct dir_Structure *d;

                    // "."
                    d = (struct dir_Structure *)&buffer[0];
                    memcpy(d->name, ".          ", 11);
                    d->attrib = ATTR_DIRECTORY;
                    d->firstClusterHI = (newCluster >> 16) & 0xFFFF;
                    d->firstClusterLO = newCluster & 0xFFFF;
          
                    // ".."
                    d = (struct dir_Structure *)&buffer[32];
                    memcpy(d->name, "..         ", 11);
                    d->attrib = ATTR_DIRECTORY;
                    d->firstClusterHI = (cluster >> 16) & 0xFFFF;
                    d->firstClusterLO = cluster & 0xFFFF;

                    SD_writeSingleBlock(dirFirstSector);

                    //_debug_dump_sector(firstSector, "WORKING DIR - FINAL STATE");
                    //_debug_dump_sector(dirFirstSector, "NEW DIR - FINAL STATE");

                    //_print_f(">>> FAT_create_dir DONE, newCluster=%lu\n", newCluster);
                    FAT_error_log = FAT_OK;
                    return 1;
                }
            }
        }
        cluster = _get_next_cluster(cluster);

        if (cluster > 0x0FFFFFF6) {
          FAT_error_log = FAT_ERR_NO_FREE_CLUSTER;
          return 0;
        }
    }

    FAT_error_log = FAT_ERR_UNKNOWN;
    return 0;
}

uint8_t FAT_delete_dir(uint8_t *dirname) {
  uint8_t name[11];
  uint32_t cluster, sector, firstSector;
  uint32_t dirCluster, nextCluster;
  struct dir_Structure *dir;
  uint16_t i;

  if (!_convert_dir_name(dirname, name)) return 0;

  cluster = _get_working_cluster();
  if(!cluster) return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];

        if (dir->name[0] == EMPTY) {
          FAT_error_log = FAT_ERR_DIR_NOT_FOUND;
          return 0;
        }

        if ((dir->attrib == ATTR_DIRECTORY) && (memcmp(dir->name, name, 11) == 0)) 
        {
          dirCluster = (((uint32_t)dir->firstClusterHI) << 16) | dir->firstClusterLO;

          // =========================
          // SAFETY CHECKS
          // =========================
          // Prevent ".", ".."
          if (name[0] == '.') {
            FAT_error_log = FAT_ERR_INVALID_NAME;
            return 0;
          }

          // Prevent deleting root
          if (dirCluster == rootCluster) {
            FAT_error_log = FAT_ERR_DENIED;
            return 0;
          }

          // Prevent deleting current directory
          if (dirCluster == FAT_working_dir_cluster) {
            FAT_error_log = FAT_ERR_DENIED;
            return 0;
          }

          // =========================
          // CHECK EMPTY
          // =========================
          uint32_t s = _get_first_sector(dirCluster);

          for (uint8_t sec = 0; sec < sectorPerCluster; sec++)
          {
            SD_readSingleBlock(s + sec);

            for(uint16_t j = 0; j < bytesPerSector; j += 32)
            {
              struct dir_Structure *d2 = (struct dir_Structure *)&buffer[j];

              if (d2->name[0] == EMPTY) break;

              // skip "." and ".."
              if (d2->name[0] == '.' && (d2->name[1] == ' ' || d2->name[1] == '.')) continue;
                
              // found something → not empty
              FAT_error_log = FAT_ERR_DIR_NOT_EMPTY;
              return 0;
            }
          }

          // =========================
          // DELETE ENTRY
          // =========================
          SD_readSingleBlock(firstSector + sector);
          dir->name[0] = DELETED;
          SD_writeSingleBlock(firstSector + sector);

          // =========================
          // FREE CLUSTER
          // =========================
          uint32_t current = dirCluster;

          while (current >= 2 && current < 0x0FFFFFF8) {
            nextCluster = _get_next_cluster(current);
            _set_next_cluster(current, 0);
            current = nextCluster;
          }

          FAT_error_log = FAT_OK;
          return 1;
        }
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0FFFFFF6)
      return 0;
  }

  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

uint32_t FAT_get_total_memory() {
  return (totalClusters * sectorPerCluster * bytesPerSector);
}

uint32_t FAT_get_free_memory() {
  uint32_t freeClusters, totalClusterCount, cluster;
  uint32_t sector, *value;
  uint16_t i;

  freeClusters = _get_total_free_cluster();

  if (freeClusters > totalClusters) {
    freeClusterCountUpdated = 0;
    freeClusters = 0;
    totalClusterCount = 0;
    cluster = rootCluster;
    while (1) {
      sector = unusedSectors + reservedSectorCount +
               ((cluster * 4) / bytesPerSector);
      SD_readSingleBlock(sector);
      for (i = 0; i < 128; i++) {
        value = (unsigned long *)&buffer[i * 4];
        if (((*value) & 0x0fffffff) == 0)
          freeClusters++;
        ;

        totalClusterCount++;
        if (totalClusterCount == (totalClusters + 2))
          break;
      }
      if (i < 128)
        break;
      cluster += 128;
    }
  }

  if (!freeClusterCountUpdated)
    _set_total_free_cluster(freeClusters); // update FSinfo next free cluster entry
  freeClusterCountUpdated = 1;       // set flag
  return (freeClusters * sectorPerCluster * bytesPerSector);
}

uint8_t FAT_rapair_get_entry(uint16_t entry, uint8_t *file_data) {
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;
  uint16_t j = 0;

  cluster = _get_working_cluster();
  if(!cluster) return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        dir = (struct dir_Structure *)&buffer[i];
        if (dir->name[0] == EMPTY) {
          FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
          return 0;
        } // Last file
        if (j == entry) {
          memcpy(file_data, dir, 32);
          FAT_error_log = FAT_OK;
          return 1;
        }
        j++;
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }

  FAT_error_log = FAT_ERR_GET_CLUSTER;
  return 0;
}

uint8_t FAT_rapair_write_entry(uint16_t entry, uint8_t *file_data) {
  uint32_t cluster, sector, firstSector;
  // struct dir_Structure *dir;
  uint16_t i;
  uint16_t j = 0;

    // Find the cluster of the specified directory
  cluster = _get_working_cluster();
  if(!cluster) return 0;

  while (1) {
    firstSector = _get_first_sector(cluster);

    for (sector = 0; sector < sectorPerCluster; sector++) {
      SD_readSingleBlock(firstSector + sector);

      for (i = 0; i < bytesPerSector; i += 32) {
        if (j == entry) {
          memcpy((uint8_t *)&buffer[i], file_data, 32);
          SD_writeSingleBlock(firstSector + sector);
          FAT_error_log = FAT_OK;
          return 1;
        }
        j++;
      }
    }

    cluster = _get_next_cluster(cluster);

    if (cluster > 0x0ffffff6) {
      FAT_error_log = FAT_ERR_LAST_CLUSTER;
      return 0;
    }
    if (cluster == 0) {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }

  FAT_error_log = FAT_ERR_GET_CLUSTER;
  return 0;
}

uint32_t FAT_repair_get_actual_dir_cluster() { return FAT_working_dir_cluster; }

uint32_t FAT_get_parent_dir_cluster() { return FAT_working_dir_cluster_old; }

