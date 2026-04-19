//*******************************************************
// **** ROUTINES FOR FAT32 IMPLEMATATION OF SD CARD ****
//**********************************************************
//Controller: ATmega32 (8 Mhz internal)
//Compiler	: AVR-GCC
//Version	: 2.2
//Author	: CC Dharmani, Chennai (India)
// 			  www.dharmanitech.com
//Date		: 15 July 2009
//********************************************************
//**************************************************
// ***** SOURCE FILE : FAT32.c ******
//**************************************************

#define F_CPU 32000000UL

#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "FAT32.h"
#include "SD_routines.h"
#include "lcd_driver.h"
#include "colors.h"

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
volatile unsigned int  bytesPerSector, sectorPerCluster, reservedSectorCount;
unsigned long unusedSectors, appendFileSector, appendFileLocation, fileSize, appendStartCluster;

// global flag to keep track of free cluster count updating in FSinfo sector
unsigned char freeClusterCountUpdated;


//***************************************************************************
//Function: to read data from boot sector of SD catransfer
//return: none
//***************************************************************************
void console_write(uint8_t *text)
{
  lcd_write_debug(text);
  _delay_ms(1000);
}

unsigned char getBootSectorData (void)
{
struct BS_Structure *bpb; //mapping the buffer onto the structure
struct MBRinfo_Structure *mbr;
struct partitionInfo_Structure *partition;
unsigned long dataSectors;

unusedSectors = 0;

SD_readSingleBlock(0);
bpb = (struct BS_Structure *)buffer;

if(bpb->jumpBoot[0]!=0xE9 && bpb->jumpBoot[0]!=0xEB)   //check if it is boot sector
{
  mbr = (struct MBRinfo_Structure *) buffer;       //if it is not boot sector, it must be MBR
  
  if(mbr->signature != 0xaa55) return 1;       //if it is not even MBR then it's not FAT32
  	
  partition = (struct partitionInfo_Structure *)(mbr->partitionData);//first partition
  unusedSectors = partition->firstSector; //the unused sectors, hidden to the FAT
  
  SD_readSingleBlock(partition->firstSector);//read the bpb sector
  bpb = (struct BS_Structure *)buffer;
  if(bpb->jumpBoot[0]!=0xE9 && bpb->jumpBoot[0]!=0xEB) return 1; 
}

bytesPerSector = bpb->bytesPerSector;
sectorPerCluster = bpb->sectorPerCluster;
reservedSectorCount = bpb->reservedSectorCount;
rootCluster = bpb->rootCluster;// + (sector / sectorPerCluster) +1;
firstDataSector = bpb->hiddenSectors + reservedSectorCount + (bpb->numberofFATs * bpb->FATsize_F32);

dataSectors = bpb->totalSectors_F32
              - bpb->reservedSectorCount
              - ( bpb->numberofFATs * bpb->FATsize_F32);
totalClusters = dataSectors / sectorPerCluster;

if((getSetFreeCluster (TOTAL_FREE, GET, 0)) > totalClusters)  //check if FSinfo free clusters count is valid
     freeClusterCountUpdated = 0;
else
	 freeClusterCountUpdated = 1;
return 0;
}

//***************************************************************************
//Function: to calculate first sector address of any given cluster
//Arguments: cluster number for which first sector is to be found
//return: first sector address
//***************************************************************************
unsigned long getFirstSector(unsigned long clusterNumber)
{
  return (((clusterNumber - 2) * sectorPerCluster) + firstDataSector);
}

//***************************************************************************
//Function: get cluster entry value from FAT to find out the next cluster in the chain
//or set new cluster entry in FAT
//Arguments: 1. current cluster number, 2. get_set (=GET, if next cluster is to be found or = SET,
//if next cluster is to be set 3. next cluster number, if argument#2 = SET, else 0
//return: next cluster number, if if argument#2 = GET, else 0
//****************************************************************************
unsigned long getSetNextCluster (unsigned long clusterNumber,
                                 unsigned char get_set,
                                 unsigned long clusterEntry)
{
unsigned int FATEntryOffset;
unsigned long *FATEntryValue;
unsigned long FATEntrySector;
unsigned char retry = 0;

//get sector number of the cluster entry in the FAT
FATEntrySector = unusedSectors + reservedSectorCount + ((clusterNumber * 4) / bytesPerSector) ;

//get the offset address in that sector number
FATEntryOffset = (unsigned int) ((clusterNumber * 4) % bytesPerSector);

//read the sector into a buffer
while(retry <10)
{ if(!SD_readSingleBlock(FATEntrySector)) break; retry++;}

//get the cluster address from the buffer
FATEntryValue = (unsigned long *) &buffer[FATEntryOffset];

if(get_set == GET)
  return ((*FATEntryValue) & 0x0fffffff);


*FATEntryValue = clusterEntry;   //for setting new value in cluster entry in FAT

SD_writeSingleBlock(FATEntrySector);

return (0);
}

//********************************************************************************************
//Function: to get or set next free cluster or total free clusters in FSinfo sector of SD card
//Arguments: 1.flag:TOTAL_FREE or NEXT_FREE, 
//			 2.flag: GET or SET 
//			 3.new FS entry, when argument2 is SET; or 0, when argument2 is GET
//return: next free cluster, if arg1 is NEXT_FREE & arg2 is GET
//        total number of free clusters, if arg1 is TOTAL_FREE & arg2 is GET
//		  0xffffffff, if any error or if arg2 is SET
//********************************************************************************************
unsigned long getSetFreeCluster(unsigned char totOrNext, unsigned char get_set, unsigned long FSEntry)
{
struct FSInfo_Structure *FS = (struct FSInfo_Structure *) &buffer;
unsigned char error;

SD_readSingleBlock(unusedSectors + 1);

if((FS->leadSignature != 0x41615252) || (FS->structureSignature != 0x61417272) || (FS->trailSignature !=0xaa550000))
  return 0xffffffff;

 if(get_set == GET)
 {
   if(totOrNext == TOTAL_FREE)
      return(FS->freeClusterCount);
   else // when totOrNext = NEXT_FREE
      return(FS->nextFreeCluster);
 }
 else
 {
   if(totOrNext == TOTAL_FREE)
      FS->freeClusterCount = FSEntry;
   else // when totOrNext = NEXT_FREE
	  FS->nextFreeCluster = FSEntry;
 
   error = SD_writeSingleBlock(unusedSectors + 1);	//update FSinfo
 }
 return 0xffffffff;
}

uint8_t FAT_unload_directory(void)
{
  SD_check_alive();
  if(!(SD_status & SD_STATUS_INIT_OK))
  {
    FAT_working_dir_cluster = 0;
    FAT_error_log = FAT_ERR_NO_SD_CARD;
    return 0;
  }// SD card not loaded
  FAT_working_dir_cluster = FAT_working_dir_cluster_old;
  return 1;
}

uint8_t FAT_load_directory(uint8_t *dirname) // One at a time
{
  uint32_t cluster, sector, firstSector;
  struct dir_Structure *dir;
  uint16_t i;
  uint8_t j;

  if(!(SD_status & SD_STATUS_INIT_OK)) { FAT_error_log = FAT_ERR_NO_SD_CARD; return 0; }// SD card not loaded

  cluster = FAT_working_dir_cluster;
  if(!cluster) cluster = rootCluster; // No loaded directory yet
  if(!cluster) // Still 0? then FAT not initialized
  {
    getBootSectorData();
    cluster = rootCluster;
    //if(dirname) cluster = findCluster(dirname);
  }
  if(!cluster) // Still 0? Then probably no SD card in or FAT broken
  {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

  uint8_t formattedName[11];
  uint8_t k = 0, l = 0;

  for(k = 0; k < 11; k++) formattedName[k] = ' ';
  k = 0;

  while(dirname[k] != '\0' && dirname[k] != '\n')
  {
      if (dirname[k] == '\n' || dirname[k] == '\0')
          break;
      if((dirname[k] >= 33 && dirname[k] <= 126 && dirname[k] != '/' && dirname[k] != '.') || (dirname[k] == ' '))
        if(dirname[k] >= 'a' && dirname[k] <= 'z') { formattedName[l++] = dirname[k] - 'a' + 'A'; }// Convert to uppercase
        else { formattedName[l++] = dirname[k]; }
      k++;
      if(l == 11) { FAT_error_log = FAT_ERR_NAME_TOO_LONG; }
  }

  while (1)
  {
      firstSector = getFirstSector(cluster);

      for (sector = 0; sector < sectorPerCluster; sector++)
      {
          SD_readSingleBlock(firstSector + sector);

          for (i = 0; i < bytesPerSector; i += 32)
          {
              dir = (struct dir_Structure*)&buffer[i];

              if (dir->name[0] == EMPTY) // Indicates end of the file list of the directory
                  { FAT_error_log = FAT_ERR_END_OF_CLUSTER; return 0; }

              if ((dir->name[0] != DELETED) && (dir->attrib == ATTR_DIRECTORY || dir->attrib == ATTR_ARCHIVE))
              {
                uint8_t result = 1;
                  for (j = 0; j < 11; j++)
                  {
                      if (dir->name[j] != formattedName[j]) { result = 0; break; }
                  }
                  if(result)
                  {
                    FAT_working_dir_cluster_old = FAT_working_dir_cluster;
                    FAT_working_dir_cluster = (((uint32_t)dir->firstClusterHI) << 16) | dir->firstClusterLO;
                    return 1; 
                  } // Loaded
              }
          }
      }

      cluster = getSetNextCluster(cluster, GET, 0);

      if (cluster > 0x0ffffff6)
      {
        FAT_error_log = FAT_ERR_LAST_CLUSTER;
        return 0;
      }
      if (cluster == 0)
      {
        FAT_error_log = FAT_ERR_GET_CLUSTER;
        return 0;
      }
  }
  
  FAT_error_log = FAT_ERR_NO_MATCH;
  return 0;
}

uint8_t FAT_count_files_in_directory(uint8_t *dirname, uint16_t *count) // NULL for ROOT
{
  uint32_t cluster, sector, firstSector, firstCluster, nextCluster;
  struct dir_Structure *dir;
  uint16_t i;
  uint8_t j;

  count[0] = 0;

  if(!(SD_status & SD_STATUS_INIT_OK)) { FAT_error_log = FAT_ERR_NO_SD_CARD; return 0; }// SD card not loaded

  // Find the cluster of the specified directory
  cluster = FAT_working_dir_cluster;
  if(!cluster) cluster = rootCluster; // No loaded directory yet
  if(!cluster) // Still 0? then FAT not initialized
  {
    getBootSectorData();
    cluster = rootCluster;
    if(dirname) cluster = findCluster(dirname);
    if(cluster) FAT_working_dir_cluster = cluster; // Directory loaded
  }
  if(!cluster) // Still 0? Then probably no SD card in or FAT broken
  {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

  while (1)
  {
      firstSector = getFirstSector(cluster);

      for (sector = 0; sector < sectorPerCluster; sector++)
      {
          SD_readSingleBlock(firstSector + sector);

          for (i = 0; i < bytesPerSector; i += 32)
          {
              dir = (struct dir_Structure*)&buffer[i];

              if(dir->name[0] == EMPTY) { FAT_error_log = FAT_OK; return 1; } // Last file
              if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME) && (dir->attrib != ATTR_VOLUME_ID)) count[0]++;
          }
      }

      cluster = getSetNextCluster(cluster, GET, 0);

      if (cluster > 0x0ffffff6)
      {
        FAT_error_log = FAT_ERR_LAST_CLUSTER;
        return 0;
      }
      if (cluster == 0)
      {
        FAT_error_log = FAT_ERR_GET_CLUSTER;
        return 0;
      }
  }
  return 0;
}

uint8_t FAT_get_file_name(uint16_t index, uint8_t *file_data)
{
    uint32_t cluster, sector, firstSector, firstCluster, nextCluster;
    struct dir_Structure *dir;
    uint16_t i;
    uint8_t j;

    if(!(SD_status & SD_STATUS_INIT_OK)) { FAT_error_log = FAT_ERR_NO_SD_CARD; return 0; }// SD card not loaded
    if(index == 0) return 0;

  // Find the cluster of the specified directory
  cluster = FAT_working_dir_cluster;
  if(!cluster) cluster = rootCluster; // No loaded directory yet
  if(!cluster) // Still 0? then FAT not initialized
  {
    getBootSectorData();
    cluster = rootCluster;
    if(cluster) FAT_working_dir_cluster = cluster; // Directory loaded
  }
  if(!cluster) // Still 0? Then probably no SD card in or FAT broken
  {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

    while (1)
    {
        firstSector = getFirstSector(cluster);

        for (sector = 0; sector < sectorPerCluster; sector++)
        {
            SD_readSingleBlock(firstSector + sector);

            for (i = 0; i < bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure*)&buffer[i];

                if (dir->name[0] == EMPTY) return 1; // Last file
                if ((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME) && (dir->attrib != ATTR_VOLUME_ID)) index--;
                if(!index)
                {
                    memcpy(file_data, dir, 32);
                    //file_data = (uint8_t *)dir;
                    return 1;
                }
            }
        }

        cluster = getSetNextCluster(cluster, GET, 0);

        if (cluster > 0x0ffffff6)
            {
              FAT_error_log = FAT_ERR_LAST_CLUSTER;
              return 0;
            }
        if (cluster == 0)
        {
          FAT_error_log = FAT_ERR_GET_CLUSTER;
            return 0;
        }
    }
    FAT_error_log = FAT_ERR_GET_CLUSTER;
    return 0;
}

uint8_t FAT_get_volume_id(uint8_t *volume_id)
{
    uint32_t cluster, sector, firstSector, firstCluster, nextCluster;
    struct dir_Structure *dir;
    uint16_t i;
    uint8_t j;

    if(!(SD_status & SD_STATUS_INIT_OK)) { FAT_error_log = FAT_ERR_NO_SD_CARD; return 0; }// SD card not loaded

  // Find the cluster of the specified directory
  cluster = rootCluster; // No loaded directory yet
  if(!cluster) // Still 0? then FAT not initialized
  {
    getBootSectorData();
    cluster = rootCluster;
  }
  if(!cluster) // Still 0? Then probably no SD card in or FAT broken
  {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

    while (1)
    {
        firstSector = getFirstSector(cluster);

        for (sector = 0; sector < sectorPerCluster; sector++)
        {
            SD_readSingleBlock(firstSector + sector);

            for (i = 0; i < bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure*)&buffer[i];

                if (dir->name[0] == EMPTY) { FAT_error_log = FAT_ERR_NO_MATCH; return 0; }// Last file
                if (dir->attrib == ATTR_VOLUME_ID)
                {
                    memcpy(volume_id, dir, 11);
                    return 1;
                }
            }
        }

        cluster = getSetNextCluster(cluster, GET, 0);

        if (cluster > 0x0ffffff6)
            {
              FAT_error_log = FAT_ERR_LAST_CLUSTER;
              return 0;
            }
        if (cluster == 0)
        {
          FAT_error_log = FAT_ERR_GET_CLUSTER;
            return 0;
        }
    }
    FAT_error_log = FAT_ERR_GET_CLUSTER;
    return 0;
}

uint8_t FAT_find_file(uint8_t *fileNameDirty, uint8_t *file_data)
{
  uint32_t cluster, sector, firstSector, firstCluster, nextCluster;
  struct dir_Structure *dir;
  uint8_t fileName[11];
  uint16_t i;
  uint8_t j;

  if(!(SD_status & SD_STATUS_INIT_OK)) { FAT_error_log = FAT_ERR_NO_SD_CARD; return 0; }// SD card not loaded

  // Find the cluster of the specified directory
  cluster = FAT_working_dir_cluster;
  if(!cluster) cluster = rootCluster; // No loaded directory yet
  if(!cluster) // Still 0? then FAT not initialized
  {
    getBootSectorData();
    cluster = rootCluster;
    if(cluster) FAT_working_dir_cluster = cluster; // Directory loaded
  }
  if(!cluster) // Still 0? Then probably no SD card in or FAT broken
  {
    FAT_error_log = FAT_ERR_INITIALIZE;
    return 0;
  }

  if(!convertFileName(fileNameDirty, fileName)) return 0;

    while (1)
    {
        firstSector = getFirstSector(cluster);

        for (sector = 0; sector < sectorPerCluster; sector++)
        {
            SD_readSingleBlock(firstSector + sector);

            for (i = 0; i < bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure*)&buffer[i];
                if(dir->name[0] == EMPTY) { FAT_error_log = FAT_ERR_FILE_NOT_FOUND; return 0; } // Last file
                if((dir->attrib != ATTR_LONG_NAME) && (dir->attrib != ATTR_VOLUME_ID) && (dir->attrib != ATTR_DIRECTORY) &&
                (dir->name[0] == fileName[0]) && (dir->name[1] == fileName[1]) && (dir->name[2] == fileName[2]) && (dir->name[3] == fileName[3]) && (dir->name[4] == fileName[4]) && (dir->name[5] == fileName[5]) && (dir->name[6] == fileName[6]) && (dir->name[7] == fileName[7]) && (dir->name[8] == fileName[8]) && (dir->name[9] == fileName[9]) && (dir->name[10] == fileName[10]))
                {
                    memcpy(file_data, dir, 32);
                    FAT_error_log = FAT_OK;
                    return 1;
                }
            }
        }

        cluster = getSetNextCluster(cluster, GET, 0);

        if (cluster > 0x0ffffff6)
            {
              FAT_error_log = FAT_ERR_LAST_CLUSTER;
              return 0;
            }
        if (cluster == 0)
        {
          FAT_error_log = FAT_ERR_GET_CLUSTER;
            return 0;
        }
    }
    FAT_error_log = FAT_ERR_GET_CLUSTER;
    return 0;
}

unsigned long findCluster(const char *directoryName)
{
    unsigned long cluster, sector, firstSector;
    struct dir_Structure *dir;
    unsigned int i;
    unsigned char j;

    cluster = rootCluster; // root cluster

    char formattedName[8];
    uint8_t k, l = 0;
    for(k = 0; k < 8; k++) formattedName[k] = ' ';
    k = 0;
    while(directoryName[k] != '\0' && directoryName[k] != '\n')
    {
        if (directoryName[k] == '\n' || directoryName[k] == '\0')
            break;
        if(directoryName[k] >= 33 && directoryName[k] <= 126 && directoryName[k] != '/' && directoryName[k] != '.')
          if(directoryName[k] >= 'a' && directoryName[k] <= 'z') { formattedName[l++] = directoryName[k] - 'a' + 'A'; }// Convert to uppercase
          else { formattedName[l++] = directoryName[k]; }
        k++;
    }

    while (1)
    {
        firstSector = getFirstSector(cluster);

        for (sector = 0; sector < sectorPerCluster; sector++)
        {
            SD_readSingleBlock(firstSector + sector);

            for (i = 0; i < bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure*)&buffer[i];

                if (dir->name[0] == EMPTY) // Indicates end of the file list of the directory
                    return 0;

                if ((dir->name[0] != DELETED) && (dir->attrib == ATTR_DIRECTORY || dir->attrib == ATTR_ARCHIVE))
                {
                  uint8_t result = 1;
                    for (j = 0; j < 8; j++)
                    {
                        if (dir->name[j] != formattedName[j]) { result = 0; break; }
                    }
                    if (result) return (((unsigned long)dir->firstClusterHI) << 16) | dir->firstClusterLO;
                }
            }
        }

        cluster = getSetNextCluster(cluster, GET, 0);

        if (cluster > 0x0ffffff6)
            return 0;
        if (cluster == 0)
            return 0;
    }
    return 0;
}

uint8_t FAT_open_file(uint8_t *fileName)
{
  uint8_t data[32];
  struct dir_Structure *dir = (struct dir_Structure*)data;

  FAT_file_position_cluster = 0;
  FAT_file_position_sector = 0;
  FAT_file_size = 0;
  FAT_file_position_index = 0;
  
  if(!FAT_find_file(fileName, data)) return 0;

  FAT_file_position_cluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
  FAT_file_position_sector = 0;
  FAT_file_size = dir->fileSize;
  FAT_file_position_index = 0;
  index_inside_sector = 0;
  sector_number = 0;
  
  return 1;
}

void FAT_close_file()
{
  FAT_file_position_cluster = 0;
  FAT_file_position_sector = 0;
  FAT_file_size = 0;
  FAT_file_position_index = 0;
  index_inside_sector = 0;
  sector_number = 0;
}

uint16_t x = 0, y = 0;
void write_file_in_lcd(uint8_t data)
{
  lcd_set_position(x, y);
  lcd_draw_char(data, GREEN, 1);
  x += 6;
  if(x > 473) { x = 0; y += 8; }
  if(y > 310) { x = 0; y = 0; }
  _delay_ms(50);
}

uint8_t FAT_read_file_line(uint8_t *fileName, uint8_t *data)
{
  uint16_t bytes_read = 0;
  uint8_t find_nl = 0;

  if(!FAT_file_size)
  {
    if(!FAT_open_file(fileName)) return 0;
  }
  
  while(1)
  {
    if(!FAT_file_position_sector)
      FAT_file_position_sector = getFirstSector (FAT_file_position_cluster);
  
    for(uint8_t j = sector_number; j < sectorPerCluster; j++)
    {
      SD_readSingleBlock(FAT_file_position_sector + j);
      for(uint16_t k = index_inside_sector; k < 512; k++)
      {
        if(!find_nl) data[bytes_read++] = buffer[k]; // If new line not found, store char
        if(((buffer[k] == '\n') || (buffer[k] == '\r') || (buffer[k] == '\0')) && (!find_nl)) // If new line found, set flag
        {
          data[bytes_read] = '\0';
          FAT_error_log = FAT_OK;
          find_nl = 1;
        }
        if((buffer[k] != '\n') && (buffer[k] != '\r') && (buffer[k] != '\0') && (find_nl)) // If nl found, but no control char, ->nex line start
        {
            if (k + 1 < 512)
            {
              index_inside_sector = k + 1;
              sector_number = j;
            }
            else
            {
              index_inside_sector = 0;
              sector_number = j + 1;
            }
      
            if (sector_number >= sectorPerCluster)
            {
              FAT_file_position_cluster =
                  getSetNextCluster(FAT_file_position_cluster, GET, 0);
      
              FAT_file_position_sector = 0;
              sector_number = 0;
            }
    
          FAT_error_log = FAT_OK;
          return 1;
        }
        FAT_file_position_index++;
        if(FAT_file_position_index >= FAT_file_size)
        {
          FAT_error_log = FAT_ERR_END_OF_FILE;
          FAT_close_file();
          return 0;
        }
      }
      index_inside_sector = 0;
    }
    FAT_file_position_cluster = getSetNextCluster (FAT_file_position_cluster, GET, 0);
    FAT_file_position_sector = 0;
    index_inside_sector = 0;
    sector_number = 0;
    if(FAT_file_position_cluster == 0)
    {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      FAT_close_file();
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

uint8_t FAT_read_specific_file_line(uint8_t *fileName, uint8_t *data, uint16_t line_index)
{
  uint8_t data_structure[32];
  struct dir_Structure *dir = (struct dir_Structure*)data_structure;
  uint32_t cluster, sector, filesize, bytes_read = 0;
  uint8_t find_nl = 0;
  
  if(!FAT_find_file(fileName, data_structure)) { return 0; }
  
  cluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
  filesize = dir->fileSize;
  
  while(1)
  {
    sector = getFirstSector (cluster);
  
    for(uint8_t j = 0; j < sectorPerCluster; j++)
    {
      SD_readSingleBlock(sector + j);
      for(uint16_t k = 0; k < 512; k++)
      {
        //write_file_in_lcd(buffer[k]);
        if(!line_index) 
        {
          data[k] = buffer[k];
          if((buffer[k] == '\n') || (buffer[k] == '\0') || (buffer[k] == '\r'))
          {
            data[k] = '\0';
            FAT_error_log = FAT_OK;
            return 1; // Line end found
          }
        }
        if(line_index)
        {
          if((buffer[k] == '\n') || (buffer[k] == '\0') || (buffer[k] == '\r')) find_nl = 1;
          if((buffer[k] != '\n') && (buffer[k] != '\r') && (buffer[k] != '\0') && (find_nl))
          {
            line_index--; // New line start detected
            find_nl = 0;
          }
        }
        if(bytes_read++ >= filesize)
        {
          FAT_error_log = FAT_ERR_END_OF_FILE;
          return 0;
        }
      }
    }
    cluster = getSetNextCluster (cluster, GET, 0);
    if(cluster == 0)
    {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}

uint8_t FAT_read_bytes(uint8_t *fileName, uint8_t *data, uint16_t bytes)
{
  uint16_t bytes_read = 0;
  
  if(!FAT_file_size)
  {
    if(!FAT_open_file(fileName)) return 0;
  }
  
  while(1)
  {
    if(!FAT_file_position_sector)
      FAT_file_position_sector = getFirstSector (FAT_file_position_cluster);
  
      for(uint8_t j = sector_number; j < sectorPerCluster; j++)
      {
        SD_readSingleBlock(FAT_file_position_sector + j);
      
        uint16_t start_k = (j == sector_number) ? index_inside_sector : 0;
      
        for(uint16_t k = start_k; k < 512; k++)
        {
          data[bytes_read++] = buffer[k];
          FAT_file_position_index++;
      
          if(bytes_read == bytes)
          {
            if (k + 1 < 512)
            {
              index_inside_sector = k + 1;
              sector_number = j;
            }
            else
            {
              index_inside_sector = 0;
              sector_number = j + 1;
            }
      
            if (sector_number >= sectorPerCluster)
            {
              FAT_file_position_cluster =
                  getSetNextCluster(FAT_file_position_cluster, GET, 0);
      
              FAT_file_position_sector = 0;
              sector_number = 0;
            }
            FAT_error_log = FAT_OK;
            return 1;
          }
      
          if(FAT_file_position_index >= FAT_file_size)
          {
            FAT_error_log = FAT_EOF;
            FAT_close_file();
            return 0;
          }
        }
        //index_inside_sector = 0;
      }
    FAT_file_position_cluster = getSetNextCluster (FAT_file_position_cluster, GET, 0);
    FAT_file_position_sector = 0;
    index_inside_sector = 0;
    sector_number = 0;
    if(FAT_file_position_cluster == 0)
    {
      FAT_error_log = FAT_ERR_GET_CLUSTER;
      FAT_close_file();
      return 0;
    }
  }
  FAT_error_log = FAT_ERR_UNKNOWN;
  return 0;
}




uint8_t convertFileName(uint8_t *fileName, uint8_t *fileNameFAT)
{
uint8_t j, k;

for(j = 0; j < 12; j++) if(fileName[j] == '.') break; 

if(j > 8)
{
  FAT_error_log = FAT_ERR_FILE_NAME_FORMAT;
  return 0;
}

for(k = 0; k < j; k++) //setting file name
  fileNameFAT[k] = fileName[k];

for(k = j; k < 8; k++) //filling file name trail with blanks
  fileNameFAT[k] = ' ';

j++;
for(k = 8; k < 11; k++) //setting file extention
{
  if(fileName[j] != 0)
    fileNameFAT[k] = fileName[j++];
  else //filling extension trail with blanks
    fileNameFAT[k] = ' ';
}

for(j = 0; j < 11; j++) //converting small letters to caps
  if((fileNameFAT[j] >= 0x61) && (fileNameFAT[j] <= 0x7a))
    fileNameFAT[j] -= 0x20;

return 1;
}

/**
 * @brief Creates an empty file entry in the FAT directory.
 *        Does NOT write any data — use FAT_append_data() afterwards.
 *
 * @param fileName  11-byte FAT8.3 formatted filename
 * @return 1 on success, 0 on failure (FAT_error_log set)
 */
 uint8_t FAT_create_file(uint8_t *fileName)
 {
     uint8_t  j, fileCreatedFlag = 0, sector;
     uint16_t i, firstClusterHigh, firstClusterLow;
     uint32_t cluster, prevCluster, nextCluster, firstSector;
     struct dir_Structure *dir;
 
     /* Reject if file already exists */
     if (FAT_open_file(fileName))
     {
         FAT_close_file();
         FAT_error_log = FAT_ERR_ALREADY_EXISTS;
         return 0;
     }
 
     /* Find a free cluster to use as the file's first (and initially only) cluster */
     cluster = getSetFreeCluster(NEXT_FREE, GET, 0);
     if (cluster > totalClusters)
         cluster = FAT_working_dir_cluster;
 
     cluster = searchNextFreeCluster(cluster);
     if (cluster == 0)
     {
         FAT_error_log = FAT_ERR_NO_FREE_CLUSTER;
         return 0;
     }
 
     /* Mark it as end-of-chain — file is empty for now */
     getSetNextCluster(cluster, SET, EOF1);
     getSetFreeCluster(NEXT_FREE, SET, cluster);
 
     firstClusterHigh = (uint16_t)((cluster & 0xFFFF0000) >> 16);
     firstClusterLow  = (uint16_t) (cluster & 0x0000FFFF);
 
     /* Walk the root directory to find a free slot for the directory entry */
     prevCluster = FAT_working_dir_cluster;
 
     while (1)
     {
         firstSector = getFirstSector(prevCluster);
 
         for (sector = 0; sector < sectorPerCluster; sector++)
         {
             SD_readSingleBlock(firstSector + sector);
 
             for (i = 0; i < bytesPerSector; i += 32)
             {
                 dir = (struct dir_Structure *)&buffer[i];
 
                 /* After writing our entry, mark the next slot as the new end-of-directory */
                 if (fileCreatedFlag)
                 {
                     dir->name[0] = 0x00;
                     FAT_error_log = FAT_OK;
                     return 1;
                 }
 
                 if ((dir->name[0] == EMPTY) || (dir->name[0] == DELETED))
                 {
                     for (j = 0; j < 11; j++)
                         dir->name[j] = fileName[j];
 
                     dir->attrib          = ATTR_ARCHIVE;
                     dir->NTreserved      = 0;
                     dir->timeTenth       = 0;
                     dir->createTime      = 0x9684;
                     dir->createDate      = 0x3A37;
                     dir->lastAccessDate  = 0x3A37;
                     dir->writeTime       = 0x9684;
                     dir->writeDate       = 0x3A37;
                     dir->firstClusterHI  = firstClusterHigh;
                     dir->firstClusterLO  = firstClusterLow;
                     dir->fileSize        = 0;   /* no data yet */
 
                     SD_writeSingleBlock(firstSector + sector);
                     fileCreatedFlag = 1;
                     /* continue scanning to stamp the end-of-directory marker */
                 }
             }
         }
 
         /* Advance to the next cluster of the root directory */
         nextCluster = getSetNextCluster(prevCluster, GET, 0);
 
         if (nextCluster > 0x0FFFFFF6)
         {
             if (nextCluster == EOF1)
             {
                 /* Root directory is full — allocate a new cluster for it */
                 nextCluster = searchNextFreeCluster(prevCluster);
                 if (nextCluster == 0)
                 {
                     FAT_error_log = FAT_ERR_DIR_FULL;
                     return 0;
                 }
                 getSetNextCluster(prevCluster, SET, nextCluster);
                 getSetNextCluster(nextCluster, SET, EOF1);
             }
             else
             {
                 FAT_error_log = FAT_ERR_CLUSTER_CHAIN;
                 return 0;
             }
         }
 
         if (nextCluster == 0)
         {
             FAT_error_log = FAT_ERR_CLUSTER_CHAIN;
             return 0;
         }
 
         prevCluster = nextCluster;
     }
     FAT_error_log = FAT_ERR_UNKNOWN;
     return 0;
 }

 /**
 * @brief Appends exactly byte_count bytes of data to an existing file.
 *        Allocates new clusters as needed. Updates the directory entry's
 *        file size and the FSinfo free-cluster count.
 *
 * @param fileName   11-byte FAT8.3 formatted filename (must already exist)
 * @param data       Pointer to the data buffer
 * @param byte_count Number of bytes to write
 * @return 1 on success, 0 on failure (FAT_error_log set)
 */
uint8_t FAT_append_data(uint8_t *fileName, uint8_t *data, uint16_t byte_count)
{
    uint8_t  j, sector, error;
    uint16_t i;
    uint32_t cluster, prevCluster, nextCluster, firstSector;
    uint32_t appendFileSector, appendFileLocation;
    uint32_t existingSize, totalWritten;
    struct dir_Structure *dir;

    /* File must exist */
    if (!FAT_open_file(fileName))
    {
        FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
        return 0;
    }

    /* ------------------------------------------------------------------
     * Locate the directory entry so we can read the current file size
     * and update it when we're done.
     * ------------------------------------------------------------------ */
    {
        uint8_t  found = 0;
        uint32_t scanCluster = FAT_working_dir_cluster;

        while (!found)
        {
            firstSector = getFirstSector(scanCluster);

            for (sector = 0; sector < sectorPerCluster && !found; sector++)
            {
                SD_readSingleBlock(firstSector + sector);

                for (i = 0; i < bytesPerSector && !found; i += 32)
                {
                    dir = (struct dir_Structure *)&buffer[i];

                    if (dir->name[0] == 0x00) goto dir_scan_done; /* end of list */
                    if (dir->name[0] == DELETED) continue;

                    uint8_t match = 1;
                    for (j = 0; j < 11; j++)
                        if (dir->name[j] != fileName[j]) { match = 0; break; }

                    if (match)
                    {
                        appendFileSector   = firstSector + sector;
                        appendFileLocation = i;
                        found = 1;
                    }
                }
            }

            nextCluster = getSetNextCluster(scanCluster, GET, 0);
            if (nextCluster >= 0x0FFFFFF6) break;
            if (nextCluster == 0)          break;
            scanCluster = nextCluster;
        }
        dir_scan_done:;

        if (!found)
        {
            FAT_error_log = FAT_ERR_FILE_NOT_FOUND;
            return 0;
        }
    }

    /* Re-read the directory sector and grab current file size + first cluster */
    SD_readSingleBlock(appendFileSector);
    dir = (struct dir_Structure *)&buffer[appendFileLocation];

    existingSize = dir->fileSize;
    cluster      = ((uint32_t)dir->firstClusterHI << 16) | dir->firstClusterLO;

    /* ------------------------------------------------------------------
     * Walk to the last cluster of the file so we can continue writing
     * from where it left off.
     * ------------------------------------------------------------------ */
    while (1)
    {
        nextCluster = getSetNextCluster(cluster, GET, 0);
        if (nextCluster == EOF1 || nextCluster >= 0x0FFFFFF6) break;
        if (nextCluster == 0)
        {
            FAT_error_log = FAT_ERR_CLUSTER_CHAIN;
            return 0;
        }
        cluster = nextCluster;
    }

    /* ------------------------------------------------------------------
     * Position within the last cluster:
     *   - which sector within the cluster
     *   - which byte within that sector
     * ------------------------------------------------------------------ */
    uint32_t bytesInLastCluster = existingSize % ((uint32_t)sectorPerCluster * bytesPerSector);
    sector = (uint8_t)(bytesInLastCluster / bytesPerSector);
    i      = (uint16_t)(bytesInLastCluster % bytesPerSector);

    firstSector = getFirstSector(cluster);
    uint32_t startBlock = firstSector + sector;

    /* If we're resuming mid-sector, read the existing contents first */
    if (i != 0)
        SD_readSingleBlock(startBlock);

    totalWritten = 0;

    while (totalWritten < byte_count)
    {
        buffer[i++] = data[totalWritten++];

        if (i == bytesPerSector)
        {
            /* Sector is full — flush it */
            error = SD_writeSingleBlock(startBlock);
            if (error)
            {
                FAT_error_log = FAT_ERR_WRITE_FAILED;
                return 0;
            }
            i = 0;
            sector++;

            if (sector == sectorPerCluster)
            {
                /* Current cluster exhausted — allocate a new one */
                sector = 0;
                prevCluster = cluster;
                cluster = searchNextFreeCluster(prevCluster);

                if (cluster == 0)
                {
                    FAT_error_log = FAT_ERR_NO_FREE_CLUSTER;
                    return 0;
                }

                getSetNextCluster(prevCluster, SET, cluster);
                getSetNextCluster(cluster, SET, EOF1);
            }

            firstSector = getFirstSector(cluster);
            startBlock  = firstSector + sector;
            /* New sector — no need to pre-read, we'll fill it from byte 0 */
        }
    }

    /* Flush the final (possibly partial) sector, zero-padding to sector boundary */
    if (i != 0)
    {
        for (; i < bytesPerSector; i++)
            buffer[i] = 0x00;

        error = SD_writeSingleBlock(startBlock);
        if (error)
        {
            FAT_error_log = FAT_ERR_WRITE_FAILED;
            return 0;
        }
    }

    /* Update FSinfo hint */
    getSetFreeCluster(NEXT_FREE, SET, cluster);

    /* ------------------------------------------------------------------
     * Update directory entry: new file size and last-write timestamp
     * ------------------------------------------------------------------ */
    SD_readSingleBlock(appendFileSector);
    dir = (struct dir_Structure *)&buffer[appendFileLocation];

    uint32_t newSize = existingSize + byte_count;
    freeMemoryUpdate(REMOVE, byte_count);   /* charge only the newly added bytes */

    dir->fileSize       = newSize;
    dir->writeTime      = 0x9684;   /* replace with real RTC values if available */
    dir->writeDate      = 0x3A37;
    dir->lastAccessDate = 0x3A37;

    SD_writeSingleBlock(appendFileSector);

    FAT_error_log = FAT_OK;
    return 1;
}



//***************************************************************************
//Function: to search for the next free cluster in the root directory
//          starting from a specified cluster
//Arguments: Starting cluster
//return: the next free cluster
//****************************************************************
unsigned long searchNextFreeCluster (unsigned long startCluster)
{
  unsigned long cluster, *value, sector;
  unsigned char i;
    
	startCluster -=  (startCluster % 128);   //to start with the first file in a FAT sector
    for(cluster =startCluster; cluster <totalClusters; cluster+=128) 
    {
      sector = unusedSectors + reservedSectorCount + ((cluster * 4) / bytesPerSector);
      SD_readSingleBlock(sector);
      for(i=0; i<128; i++)
      {
       	 value = (unsigned long *) &buffer[i*4];
         if(((*value) & 0x0fffffff) == 0)
            return(cluster+i);
      }  
    } 

 return 0;
}

//***************************************************************************
//Function: to display total memory and free memory of SD card, using UART
//Arguments: none
//return: none
//Note: this routine can take upto 15sec for 1GB card (@1MHz clock)
//it tries to read from SD whether a free cluster count is stored, if it is stored
//then it will return immediately. Otherwise it will count the total number of
//free clusters, which takes time
//****************************************************************************
void memoryStatistics (void)
{
unsigned long totalMemory, freeMemory, freeClusters, totalClusterCount, cluster;
unsigned long sector, *value;
unsigned int i;

totalMemory = totalClusters * sectorPerCluster * bytesPerSector;

//TX_NEWLINE;
//TX_NEWLINE;
//transmitString_F(PSTR("Total Memory: "));
console_write("Total memory : ");
displayMemory (totalMemory);
console_write(" \n\0");

freeClusters = getSetFreeCluster (TOTAL_FREE, GET, 0);
//freeClusters = 0xffffffff;    

if(freeClusters > totalClusters)
{
   freeClusterCountUpdated = 0;
   freeClusters = 0;
   totalClusterCount = 0;
   cluster = rootCluster;    
    while(1)
    {
      sector = unusedSectors + reservedSectorCount + ((cluster * 4) / bytesPerSector) ;
      SD_readSingleBlock(sector);
      for(i=0; i<128; i++)
      {
           value = (unsigned long *) &buffer[i*4];
         if(((*value)& 0x0fffffff) == 0)
            freeClusters++;;
        
         totalClusterCount++;
         if(totalClusterCount == (totalClusters+2)) break;
      }  
      if(i < 128) break;
      cluster+=128;
    } 
}

if(!freeClusterCountUpdated)
  getSetFreeCluster (TOTAL_FREE, SET, freeClusters); //update FSinfo next free cluster entry
freeClusterCountUpdated = 1;  //set flag
freeMemory = freeClusters * sectorPerCluster * bytesPerSector;
//TX_NEWLINE;
//transmitString_F(PSTR(" Free Memory: "));
console_write("Free memory : ");
displayMemory (freeMemory);
console_write(" \n\0");
//TX_NEWLINE;
}

//************************************************************
//Function: To convert the unsigned long value of memory into 
//          text string and send to UART
//Arguments: unsigned long memory value
//return: none
//************************************************************
void displayMemory (unsigned long memory)
{
  unsigned char memoryString[] = "             Bytes"; //18 character long string for memory display
  unsigned char i;
  for(i=12; i>0; i--) //converting freeMemory into ASCII string
  {
    if(i == 5 || i == 9) 
	{
	   memoryString[i-1] = ',';  
	   i--;
	}
    memoryString[i-1] = (memory % 10) | 0x30;
    memory /= 10;
	if(memory == 0) break;
  }

  //transmitString(memoryString);
  console_write(memoryString);
}

//********************************************************************
//Function: update the free memory count in the FSinfo sector. 
//			Whenever a file is deleted or created, this function will be called
//			to ADD or REMOVE clusters occupied by the file
//Arguments: #1.flag ADD or REMOVE #2.file size in Bytes
//return: none
//********************************************************************
void freeMemoryUpdate (unsigned char flag, unsigned long size)
{
  unsigned long freeClusters;
  //convert file size into number of clusters occupied
  if((size % 512) == 0) size = size / 512;
  else size = (size / 512) +1;
  if((size % 8) == 0) size = size / 8;
  else size = (size / 8) +1;

  if(freeClusterCountUpdated)
  {
	freeClusters = getSetFreeCluster (TOTAL_FREE, GET, 0);
	if(flag == ADD)
  	   freeClusters = freeClusters + size;
	else  //when flag = REMOVE
	   freeClusters = freeClusters - size;
	getSetFreeCluster (TOTAL_FREE, SET, freeClusters);
  }
}

//******** END ****** www.dharmanitech.com *****
