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
#include "FAT32.h"
//#include "UART_routines.h"
#include "SD_routines.h"
#include "x128_ili9488_driver.h"
#include "programs.h"

uint32_t file_position_index = 0;

//***************************************************************************
//Function: to read data from boot sector of SD card, to determine important
//parameters like bytesPerSector, sectorsPerCluster etc.
//Arguments: none
//return: none
//***************************************************************************
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

//***************************************************************************
//Function: to get DIR/FILE list or a single file address (cluster number) or to delete a specified file
//Arguments: #1 - flag: GET_LIST, GET_FILE or DELETE #2 - pointer to file name (0 if arg#1 is GET_LIST)
//return: first cluster of the file, if flag = GET_FILE
//        print file/dir list of the root directory, if flag = GET_LIST
//		  Delete the file mentioned in arg#2, if flag = DELETE
//****************************************************************************
struct dir_Structure* findFiles (unsigned char flag, unsigned char *fileName)
{
unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
struct dir_Structure *dir;
unsigned int i;
unsigned char j;

cluster = rootCluster; //root cluster

while(1)
{
   firstSector = getFirstSector (cluster);

   for(sector = 0; sector < sectorPerCluster; sector++)
   {
     SD_readSingleBlock (firstSector + sector);
	
     for(i=0; i<bytesPerSector; i+=32)
     {
	    dir = (struct dir_Structure *) &buffer[i];

        if(dir->name[0] == EMPTY) //indicates end of the file list of the directory
		{
		  if((flag == GET_FILE) || (flag == DELETE))
          {console_write("File ");
            console_write(fileName);
            console_write(" does not exist!\n\0");
		  return 0; }
		}
		if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
        {
          if((flag == GET_FILE) || (flag == DELETE))
          {
            for(j=0; j<11; j++)
            if(dir->name[j] != fileName[j]) break;
            if(j == 11)
			{
			  if(flag == GET_FILE)
              {
			    appendFileSector = firstSector + sector;
				appendFileLocation = i;
				appendStartCluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
				fileSize = dir->fileSize;
			    return (dir);
			  }	
			  else    //when flag = DELETE
			  {
                 console_write("Deleting...\n\0");
				 firstCluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
                
				 //mark file as 'deleted' in FAT table
				 dir->name[0] = DELETED;    
				 SD_writeSingleBlock (firstSector+sector);
				 			 
				 freeMemoryUpdate (ADD, dir->fileSize);

				 //update next free cluster entry in FSinfo sector
				 cluster = getSetFreeCluster (NEXT_FREE, GET, 0); 
				 if(firstCluster < cluster)
				     getSetFreeCluster (NEXT_FREE, SET, firstCluster);

				 //mark all the clusters allocated to the file as 'free'
			     while(1)  
			     {
			        nextCluster = getSetNextCluster (firstCluster, GET, 0);
					getSetNextCluster (firstCluster, SET, 0);
					if(nextCluster > 0x0ffffff6) 
					   {
                         //transmitString_F(PSTR("File deleted!"));
                         console_write("File deleted!\n\0");
                         return 0;}
					firstCluster = nextCluster;
			  	 } 
			  }
            }
          }
          else  //when flag = GET_LIST
                    {
                        uint8_t file_name[32];
                        for (j = 0; j < 11; j++)
                        {
                            if(j < 8) file_name[j] = dir->name[j];
                            if(j == 8) file_name[j] = '.';
                            if(j > 7) file_name[j + 1] = dir->name[j];
                        }
                        file_name[12] = ' ';
                        file_name[13] = ' ';
                        if ((dir->attrib != 0x10) && (dir->attrib != 0x08))
                        {
                            file_name[14] = 'F';
                            file_name[15] = 'I';
                            file_name[16] = 'L';
                            file_name[17] = 'E';
                            file_name[18] = ' ';
                            file_name[19] = '\0';
                            console_write(file_name);
                            displayMemory(dir->fileSize);
                            console_write(" \n\0");
                        }
                        else if (dir->attrib == 0x10)
                        {
                            file_name[14] = 'D';
                            file_name[15] = 'I';
                            file_name[16] = 'R';
                            file_name[17] = '\n';
                            file_name[18] = '\0';
                            file_name[8] = ' ';
                            console_write(file_name);
                        }
                        else if (dir->attrib == 0x08)
                        {
                            file_name[14] = 'R';
                            file_name[15] = 'O';
                            file_name[16] = 'O';
                            file_name[17] = 'T';
                            file_name[18] = '\n';
                            file_name[19] = '\0';
                            file_name[8] = ' ';
                            console_write(file_name);
                        }
                    }
       }
     }
   }

   cluster = (getSetNextCluster (cluster, GET, 0));

   if(cluster > 0x0ffffff6)
   	 return 0;
   if(cluster == 0) 
   {
     //transmitString_F(PSTR("Error in getting cluster"));
     console_write("Error in getting the cluster\n\0");
     return 0;}
 }
return 0;
}

// By chat GTP
void findFilesInDirectory(uint8_t *directoryName)
{
    unsigned long cluster, sector, firstSector, firstCluster, nextCluster;
    struct dir_Structure *dir;
    unsigned int i;
    unsigned char j;

    // Find the cluster of the specified directory
    firstCluster = rootCluster;
    if(directoryName) firstCluster = findCluster(directoryName);

    if (firstCluster == 0)
    {
        console_write("Directory not found: ");
        console_write(directoryName);
        console_write("\n");
        return;
    }

    cluster = firstCluster;

    while (1)
    {
        firstSector = getFirstSector(cluster);

        for (sector = 0; sector < sectorPerCluster; sector++)
        {
            SD_readSingleBlock(firstSector + sector);

            for (i = 0; i < bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure*)&buffer[i];

                if (dir->name[0] == EMPTY) return;
                if ((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
                {
                    uint8_t file_name[32];
                    for (j = 0; j < 11; j++)
                    {
                        if(j < 8) file_name[j] = dir->name[j];
                        if(j == 8) file_name[j] = '.';
                        if(j > 7) file_name[j + 1] = dir->name[j];
                    }
                    file_name[12] = ' ';
                    file_name[13] = ' ';
                    if ((dir->attrib != 0x10) && (dir->attrib != 0x08))
                    {
                        file_name[14] = 'F';
                        file_name[15] = 'I';
                        file_name[16] = 'L';
                        file_name[17] = 'E';
                        file_name[18] = ' ';
                        file_name[19] = '\0';
                        console_write(file_name);
                        displayMemory(dir->fileSize);
                        console_write(" \n\0");
                    }
                    else if (dir->attrib == 0x10)
                    {
                        file_name[14] = 'D';
                        file_name[15] = 'I';
                        file_name[16] = 'R';
                        file_name[17] = '\n';
                        file_name[18] = '\0';
                        file_name[8] = ' ';
                        console_write(file_name);
                    }
                }
            }
        }

        cluster = getSetNextCluster(cluster, GET, 0);

        if (cluster > 0x0ffffff6)
            return;
        if (cluster == 0)
        {
            console_write("Error in getting the cluster\n\0");
            return;
        }
    }
    return;
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



//***************************************************************************
//Function: if flag=READ then to read file from SD card and send contents to UART 
//if flag=VERIFY then functions will verify whether a specified file is already existing
//Arguments: flag (READ or VERIFY) and pointer to the file name
//return: 0, if normal operation or flag is READ
//	      1, if file is already existing and flag = VERIFY
//		  2, if file name is incompatible
//***************************************************************************
unsigned char readFile (unsigned char flag, unsigned char *fileName)
{
struct dir_Structure *dir;
unsigned long cluster, byteCounter = 0, fileSize, firstSector;
unsigned int k;
unsigned char j, error;

error = convertFileName (fileName); //convert fileName into FAT format
if(error) return 2;

dir = findFiles (GET_FILE, fileName); //get the file location
if(dir == 0) 
  return (0);

if(flag == VERIFY) return (1);	//specified file name is already existing

cluster = (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;

fileSize = dir->fileSize;

//TX_NEWLINE;
//TX_NEWLINE;
console_write(" \n\0");

while(1)
{
  firstSector = getFirstSector (cluster);

  for(j=0; j<sectorPerCluster; j++)
  {
    SD_readSingleBlock(firstSector + j);
    
	for(k=0; k<512; k++)
    {
      //transmitByte(buffer[k]);
      lcd_draw_char(buffer[k], RED, X1);
      if ((byteCounter++) >= fileSize ) return 0;
    }
  }
  cluster = getSetNextCluster (cluster, GET, 0);
  if(cluster == 0) {
    //transmitString_F(PSTR("Error in getting cluster"));
    console_write("Error in getting the cluster 2\n\0"); return 0;}
}
return 0;
}

#include <string.h>

// Assume file_position_index is a global variable

uint16_t readFileData(unsigned char *fileName, uint8_t *data, unsigned char flag, uint16_t bytes)
{
    struct dir_Structure *dir;
    unsigned long cluster, byteCounter = 0, fileSize, firstSector;
    unsigned int k, bytesRead = 0;
    unsigned char j, error;

    error = convertFileName(fileName); // convert fileName into FAT format
    if (error)
    {
        console_write("Filename error...\n\0");
        return 0;
    }

    dir = findFiles(GET_FILE, fileName); // get the file location
    if (dir == 0)
    {
        console_write("File not found...\n\0");
        return 0;
    }

    cluster = (((unsigned long)dir->firstClusterHI) << 16) | dir->firstClusterLO;

    fileSize = dir->fileSize;

    if (flag == 1)
        file_position_index = 0; // Start from 0

    while (1)
    {
        firstSector = getFirstSector(cluster);

        for (j = 0; j < sectorPerCluster; j++)
        {
            SD_readSingleBlock(firstSector + j);

            for (k = 0; k < 512; k++)
            {
                if (flag == 0 || flag == 1)
                {
                    // Continue from the last position or read from the beginning
                    if (byteCounter < file_position_index)
                    {
                        byteCounter++;
                        continue;
                    }
                    if (bytesRead < bytes)
                    {
                        data[bytesRead] = buffer[k];
                        bytesRead++;
                    }
                }
                else if (flag == 2)
                {
                    // Read one line starting from file_position_index
                    if (buffer[k] == '\n' || buffer[k] == '\0')
                    {
                        // End of line or end of file
                        data[bytesRead] = '\0'; // Null-terminate the string
                        file_position_index += bytesRead;
                        return bytesRead;
                    }
                    else
                    {
                        data[bytesRead] = buffer[k];
                        bytesRead++;
                    }
                }

                if ((byteCounter++) >= fileSize || bytesRead >= bytes)
                {
                    // End of file or read the requested number of bytes
                    file_position_index += bytesRead;
                    if ((byteCounter++) >= fileSize) file_position_index = 0;
                    return bytesRead;
                }
            }
        }

        cluster = getSetNextCluster(cluster, GET, 0);
        if (cluster == 0)
        {
            console_write("Error in getting the cluster...\n\0");
            file_position_index += bytesRead; // Increment even if an error occurs
            return bytesRead;                 // Return the number of bytes read before encountering an error
        }
    }

    return bytesRead;
}


//***************************************************************************
//Function: to convert normal short file name into FAT format
//Arguments: pointer to the file name
//return: 0, if successful else 1.
//***************************************************************************
unsigned char convertFileName (unsigned char *fileName)
{
unsigned char fileNameFAT[11];
unsigned char j, k;

for(j=0; j<12; j++)
if(fileName[j] == '.') break;

if(j>8) {
  console_write("Your file name is fucked up...\n\0");
  return 1;}

for(k=0; k<j; k++) //setting file name
  fileNameFAT[k] = fileName[k];

for(k=j; k<=7; k++) //filling file name trail with blanks
  fileNameFAT[k] = ' ';

j++;
for(k=8; k<11; k++) //setting file extention
{
  if(fileName[j] != 0)
    fileNameFAT[k] = fileName[j++];
  else //filling extension trail with blanks
    while(k<11)
      fileNameFAT[k++] = ' ';
}

for(j=0; j<11; j++) //converting small letters to caps
  if((fileNameFAT[j] >= 0x61) && (fileNameFAT[j] <= 0x7a))
    fileNameFAT[j] -= 0x20;

for(j=0; j<11; j++)
  fileName[j] = fileNameFAT[j];

return 0;
}

//************************************************************************************
//Function: to create a file in FAT32 format in the root directory if given 
//			file name does not exist; if the file already exists then append the data
//Arguments: pointer to the file name
//return: none
//************************************************************************************
void writeFile (unsigned char *fileName, unsigned char *data)
{
unsigned char j, /*data,*/ error, fileCreatedFlag = 0, start = 0, appendFile = 0, sector;
unsigned int i, firstClusterHigh, firstClusterLow;
struct dir_Structure *dir;
unsigned long cluster, nextCluster, prevCluster, firstSector, clusterCount, extraMemory;
unsigned char a;

j = readFile (VERIFY, fileName);

if(j == 1) 
{
  //transmitString_F(PSTR("  File already existing, appending data.."));
  console_write("Your fucking file already exists, appending data...\n\0");
  appendFile = 1;
  cluster = appendStartCluster;
  clusterCount=0;
  while(1)
  {
    nextCluster = getSetNextCluster (cluster, GET, 0);
    if(nextCluster == EOF1) break;
	cluster = nextCluster;
	clusterCount++;
  }

  sector = (fileSize - (clusterCount * sectorPerCluster * bytesPerSector)) / bytesPerSector; //last sector number of the last cluster of the file
  start = 1;
//  appendFile();
//  return;
}
else if(j == 2) 
   return; //invalid file name
else
{
  //TX_NEWLINE;
  console_write(" Creating File..\n\0");
  //transmitString_F(PSTR(" Creating File.."));

  cluster = getSetFreeCluster (NEXT_FREE, GET, 0);
  if(cluster > totalClusters)
     cluster = rootCluster;

  cluster = searchNextFreeCluster(cluster);
   if(cluster == 0)
   {
      //TX_NEWLINE;
      //transmitString_F(PSTR(" No free cluster!"));
     console_write(" No free cluster, asshole..\n\0");
	  return;
   }
  getSetNextCluster(cluster, SET, EOF1);   //last cluster of the file, marked EOF1
   
  firstClusterHigh = (unsigned int) ((cluster & 0xffff0000) >> 16 );
  firstClusterLow = (unsigned int) ( cluster & 0x0000ffff);
  fileSize = 0;
}



while(1)
{
   if(start)
   {
      start = 0;
	  startBlock = getFirstSector (cluster) + sector;
	  SD_readSingleBlock (startBlock);
	  i = fileSize % bytesPerSector;
	  j = sector;
   }
   else
   {
      startBlock = getFirstSector (cluster);
	  i=0;
	  j=0;
   }
   

   //TX_NEWLINE;
   //transmitString_F(PSTR(" Enter text (end with ~):"));
   console_write(" Enter text (end with ~):\n\0");
   a = 0;
   do
   {
     //data = receiveByte();
	 if(data[a] == 0x08)	//'Back Space' key pressed
	 { 
	   if(i != 0)
	   { 
	     //transmitByte(data);
		 //transmitByte(' ');
	     //transmitByte(data);
	     i--; 
		 fileSize--;
	   } 
	   continue;     
	 }
	 //transmitByte(data);
     buffer[i++] = data[a];
	 fileSize++;
     if(data[a] == '\r')  //'Carriege Return (CR)' character
     {
        //transmitByte ('\n');
        buffer[i++] = '\n'; //appending 'Line Feed (LF)' character
		fileSize++;
     }
	 
     if(i == 512) 
	 {
	   i=0;
	   error = SD_writeSingleBlock (startBlock);
       j++;
	   if(j == sectorPerCluster) {j = 0; break;}
	   startBlock++; 
     }
     a++;
	}while (data[a] != '~');

   if(data[a] == '~')
   {
      fileSize--;	//to remove the last entered '~' character
	  i--;
	  for(;i<512;i++)  //fill the rest of the buffer with 0x00
        buffer[i]= 0x00;
   	  error = SD_writeSingleBlock (startBlock);

      break;
   } 
	  
   prevCluster = cluster;

   cluster = searchNextFreeCluster(prevCluster); //look for a free cluster starting from the current cluster

   if(cluster == 0)
   {
      //TX_NEWLINE;
      //transmitString_F(PSTR(" No free cluster!"));
     console_write(" No free cluster jerk!\n\0");
	  return;
   }

   getSetNextCluster(prevCluster, SET, cluster);
   getSetNextCluster(cluster, SET, EOF1);   //last cluster of the file, marked EOF1
}        

getSetFreeCluster (NEXT_FREE, SET, cluster); //update FSinfo next free cluster entry

if(appendFile)  //executes this loop if file is to be appended
{
  SD_readSingleBlock (appendFileSector);    
  dir = (struct dir_Structure *) &buffer[appendFileLocation]; 
  extraMemory = fileSize - dir->fileSize;
  dir->fileSize = fileSize;
  SD_writeSingleBlock (appendFileSector);
  freeMemoryUpdate (REMOVE, extraMemory); //updating free memory count in FSinfo sector;

  
  //TX_NEWLINE;
  //transmitString_F(PSTR(" File appended!"));
  //TX_NEWLINE;
  console_write(" File appended!\n\0");
  return;
}

//executes following portion when new file is created

prevCluster = rootCluster; //root cluster

while(1)
{
   firstSector = getFirstSector (prevCluster);

   for(sector = 0; sector < sectorPerCluster; sector++)
   {
     SD_readSingleBlock (firstSector + sector);
	

     for(i=0; i<bytesPerSector; i+=32)
     {
	    dir = (struct dir_Structure *) &buffer[i];

		if(fileCreatedFlag)   //to mark last directory entry with 0x00 (empty) mark
		 { 					  //indicating end of the directory file list
		   dir->name[0] = 0x00;
           return;
         }

        if((dir->name[0] == EMPTY) || (dir->name[0] == DELETED))  //looking for an empty slot to enter file info
		{
		  for(j=0; j<11; j++)
  			dir->name[j] = fileName[j];
		  dir->attrib = ATTR_ARCHIVE;	//settting file attribute as 'archive'
		  dir->NTreserved = 0;			//always set to 0
		  dir->timeTenth = 0;			//always set to 0
		  dir->createTime = 0x9684;		//fixed time of creation
		  dir->createDate = 0x3a37;		//fixed date of creation
		  dir->lastAccessDate = 0x3a37;	//fixed date of last access
		  dir->writeTime = 0x9684;		//fixed time of last write
		  dir->writeDate = 0x3a37;		//fixed date of last write
		  dir->firstClusterHI = firstClusterHigh;
		  dir->firstClusterLO = firstClusterLow;
		  dir->fileSize = fileSize;

		  SD_writeSingleBlock (firstSector + sector);
		  fileCreatedFlag = 1;

		  //TX_NEWLINE;
		  //TX_NEWLINE;
		  //transmitString_F(PSTR(" File Created!"));
          console_write("File created\n\0");

		  freeMemoryUpdate (REMOVE, fileSize); //updating free memory count in FSinfo sector
	     
        }
     }
   }

   cluster = getSetNextCluster (prevCluster, GET, 0);

   if(cluster > 0x0ffffff6)
   {
      if(cluster == EOF1)   //this situation will come when total files in root is multiple of (32*sectorPerCluster)
	  {  
		cluster = searchNextFreeCluster(prevCluster); //find next cluster for root directory entries
		getSetNextCluster(prevCluster, SET, cluster); //link the new cluster of root to the previous cluster
		getSetNextCluster(cluster, SET, EOF1);  //set the new cluster as end of the root directory
      } 

      else
      {	
	    //transmitString_F(PSTR("End of Cluster Chain"));
        console_write("End of Cluster Chain\n\0");
	    return;
      }
   }
   if(cluster == 0) {
     //transmitString_F(PSTR("Error in getting cluster"));
     console_write("Eerror of Cluster Chain 23\n\0");
     return;}
   
   prevCluster = cluster;
 }
 
 return;
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
//Function: to delete a specified file from the root directory
//Arguments: pointer to the file name
//return: none
//********************************************************************
void deleteFile (unsigned char *fileName)
{
  unsigned char error;

  error = convertFileName (fileName);
  if(error) return;

  findFiles (DELETE, fileName);
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
