

//**************************************************
// ***** HEADER FILE : FAT32.h ******
//**************************************************
#ifndef _FAT32_H_
#define _FAT32_H_

#define FAT_ERR_GET_CLUSTER             1
#define FAT_ERR_INITIALIZE              2
#define FAT_ERR_NO_SD_CARD              3
#define FAT_ERR_LAST_CLUSTER            4
#define FAT_ERR_FILE_NAME_FORMAT        5
#define FAT_ERR_END_OF_CLUSTER          6
#define FAT_ERR_NO_MATCH                7
#define FAT_ERR_NAME_TOO_LONG           8
#define FAT_ERR_FILE_NOT_FOUND          9
#define FAT_ERR_LINE_TOO_LONG           10
#define FAT_ERR_LAST_LINE               11
#define FAT_ERR_END_OF_FILE             12
#define FAT_ERR_UNKNOWN                 13
#define FAT_ERR_OVERRUN                 14
#define FAT_ERR_ALREADY_EXISTS          15
#define FAT_ERR_NO_FREE_CLUSTER         16
#define FAT_ERR_DIR_FULL                17   // No empty directory slot and cluster expansion failed
#define FAT_ERR_WRITE_FAILED            18   // SD_writeSingleBlock returned an error
#define FAT_ERR_CLUSTER_CHAIN           19   // Unexpected end or corruption in cluster chain
#define FAT_ERR_NAME_CONVERSION         20
#define FAT_ERR_SD_WRITE                21
#define FAT_ERR_DIR_NOT_EMPTY           22
#define FAT_ERR_INVALID_NAME            23
#define FAT_ERR_DENIED                  24
#define FAT_ERR_DIR_NOT_FOUND           25
#define FAT_ERR_SD_READ                 26
#define FAT_DIRECTORY_CLUSTER_OVERWRITE 27
#define FAT_OK                          0xaa
#define FAT_EOF                         0xbb

extern uint32_t FAT_working_dir_cluster;
extern uint32_t FAT_working_dir_cluster_old;
extern uint32_t FAT_file_position_index;
extern uint32_t FAT_file_position_cluster;
extern uint32_t FAT_file_position_sector;
extern uint32_t FAT_file_size;
extern uint8_t FAT_error_log;

//Structure to access Master Boot Record for getting info about partioions
struct MBRinfo_Structure{
unsigned char	nothing[446];		//ignore, placed here to fill the gap in the structure
unsigned char	partitionData[64];	//partition records (16x4)
unsigned int	signature;		//0xaa55
};

//Structure to access info of the first partioion of the disk 
struct partitionInfo_Structure{ 				
unsigned char	status;				//0x80 - active partition
unsigned char 	headStart;			//starting head
unsigned int	cylSectStart;		//starting cylinder and sector
unsigned char	type;				//partition type 
unsigned char	headEnd;			//ending head of the partition
unsigned int	cylSectEnd;			//ending cylinder and sector
unsigned long	firstSector;		//total sectors between MBR & the first sector of the partition
unsigned long	sectorsTotal;		//size of this partition in sectors
};

//Structure to access boot sector data
struct BS_Structure{
unsigned char jumpBoot[3]; //default: 0x009000EB
unsigned char OEMName[8];
unsigned int bytesPerSector; //deafault: 512
unsigned char sectorPerCluster;
unsigned int reservedSectorCount;
unsigned char numberofFATs;
unsigned int rootEntryCount;
unsigned int totalSectors_F16; //must be 0 for FAT32
unsigned char mediaType;
unsigned int FATsize_F16; //must be 0 for FAT32
unsigned int sectorsPerTrack;
unsigned int numberofHeads;
unsigned long hiddenSectors;
unsigned long totalSectors_F32;
unsigned long FATsize_F32; //count of sectors occupied by one FAT
unsigned int extFlags;
unsigned int FSversion; //0x0000 (defines version 0.0)
unsigned long rootCluster; //first cluster of root directory (=2)
unsigned int FSinfo; //sector number of FSinfo structure (=1)
unsigned int BackupBootSector;
unsigned char reserved[12];
unsigned char driveNumber;
unsigned char reserved1;
unsigned char bootSignature;
unsigned long volumeID;
unsigned char volumeLabel[11]; //"NO NAME "
unsigned char fileSystemType[8]; //"FAT32"
unsigned char bootData[420];
unsigned int bootEndSignature; //0xaa55
};


//Structure to access FSinfo sector data
struct FSInfo_Structure
{
unsigned long leadSignature; //0x41615252
unsigned char reserved1[480];
unsigned long structureSignature; //0x61417272
unsigned long freeClusterCount; //initial: 0xffffffff
unsigned long nextFreeCluster; //initial: 0xffffffff
unsigned char reserved2[12];
unsigned long trailSignature; //0xaa550000
};

//Structure to access Directory Entry in the FAT
struct dir_Structure{
unsigned char name[11];
unsigned char attrib; //file attributes
unsigned char NTreserved; //always 0
unsigned char timeTenth; //tenths of seconds, set to 0 here
unsigned int createTime; //time file was created
unsigned int createDate; //date file was created
unsigned int lastAccessDate;
unsigned int firstClusterHI; //higher word of the first cluster number
unsigned int writeTime; //time of last write
unsigned int writeDate; //date of last write
unsigned int firstClusterLO; //lower word of the first cluster number
unsigned long fileSize; //size of file in bytes
};

//Attribute definitions for file/directory
#define ATTR_READ_ONLY     0x01
#define ATTR_HIDDEN        0x02
#define ATTR_SYSTEM        0x04
#define ATTR_VOLUME_ID     0x08
#define ATTR_DIRECTORY     0x10
#define ATTR_ARCHIVE       0x20
#define ATTR_LONG_NAME     0x0f


#define DIR_ENTRY_SIZE     0x32
#define EMPTY              0x00
#define DELETED            0xe5
#define GET     0
#define SET     1
#define READ	0
#define VERIFY  1
#define ADD		0
#define REMOVE	1
#define TOTAL_FREE   1
#define NEXT_FREE    2
#define GET_LIST     0
#define GET_FILE     1
#define DELETE		 2
#define EOF1		0x0fffffff


//************* external variables *************
extern volatile unsigned long firstDataSector, rootCluster, totalClusters;
extern volatile unsigned int  bytesPerSector, sectorPerCluster, reservedSectorCount;
extern unsigned long unusedSectors, appendFileSector, appendFileLocation, fileSize, appendStartCluster;

//global flag to keep track of free cluster count updating in FSinfo sector
extern unsigned char freeClusterCountUpdated;



uint8_t FAT_get_volume_id(uint8_t *volume_id);
uint8_t FAT_unload_directory(void);
uint8_t FAT_load_directory(uint8_t *dirname); // One at a time
uint8_t FAT_count_files_in_directory(uint16_t *count); // directory should be loaded
uint8_t FAT_open_file(uint8_t *fileName);
void FAT_close_file();
uint8_t FAT_get_file_name(uint16_t index, uint8_t *file_data); // Within loaded directory
uint8_t FAT_read_specific_file_line(uint8_t *fileName, uint8_t *data, uint16_t line_index);
uint8_t FAT_read_file_line(uint8_t *fileName, uint8_t *data);
uint8_t FAT_read_bytes(uint8_t *fileName, uint8_t *data, uint16_t bytes);
uint8_t FAT_find_file(uint8_t *fileNameDirty, uint8_t *file_data);
uint8_t FAT_create_file(uint8_t *fileName);
uint8_t FAT_append_data(uint8_t *fileName, uint8_t *data, uint16_t byte_count);
uint8_t FAT_delete_file(uint8_t *fileNameDirty);
uint8_t FAT_create_dir(uint8_t *dirname);
uint8_t FAT_delete_dir(uint8_t *dirname);
uint32_t FAT_get_total_memory(void);
uint32_t FAT_get_free_memory(void);

uint8_t FAT_rapair_get_entry(uint16_t entry, uint8_t *file_data);
uint8_t FAT_rapair_write_entry(uint16_t entry, uint8_t *file_data);
uint32_t FAT_repair_get_actual_dir_cluster();
uint32_t FAT_get_parent_dir_cluster();

#endif
