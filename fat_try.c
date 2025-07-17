 
uint8_t FAT_try()
{
	unsigned char option, error, data, FAT32_active;
unsigned int i;
unsigned char fileName[13];

	uint8_t buffer2[2];

FAT32_active = 1;
error = getBootSectorData (); //read boot sector and keep necessary data in global variables
if(error)
{
  console_write("FAT32 not found!\n\0");  //FAT32 incompatible drive
  FAT32_active = 0;
}

while(1)
{

console_write("Press any key...\n\0");
console_update(0, 0);

option = ui_get_key(console_keypad_size, 1, 0);

console_write("> 0 : Erase Blocks\n\0");

console_write("> 1 : Write single Block\n\0");

console_write("> 2 : Read single Block\n\0");

#ifndef FAT_TESTING_ONLY

console_write("> 3 : Write multiple Blocks\n\0");

console_write("> 4 : Read multiple Blocks\n\0");
#endif



console_write("> 5 : Get file list\n\0");

console_write("> 6 : Read File\n\0");

console_write("> 7 : Create File\n\0");

console_write("> 8 : Delete File\n\0");

console_write("> 9 : Read SD Memory Capacity (Total/Free)\n\0");



console_write("> Select Option (0-9): \n\0");
console_update(0, 0);


/*WARNING: If option 0, 1 or 3 is selected, the card may not be detected by PC/Laptop again,
as it disturbs the FAT format, and you may have to format it again with FAT32.
This options are given for learning the raw data transfer to & from the SD Card*/

option = ui_get_key(console_keypad_size, 1, 0);

if(option >= 0x35 && option <= 0x39)  //options 5 to 9 disabled if FAT32 not found
{
  if(!FAT32_active)
  {


      console_write("FAT32 options disabled!\n\0");
	  console_update(0, 0);
      continue;
  }
}


if((option >= 0x30) && (option <=0x34)) //get starting block address for options 0 to 4
{


console_write("Enter the Block number (0000-9999):\n\0");
console_update(0, 0);
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
startBlock = (data & 0x0f) * 1000;
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
startBlock += (data & 0x0f) * 100;
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
startBlock += (data & 0x0f) * 10;
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
startBlock += (data & 0x0f);

}

totalBlocks = 1;

#ifndef FAT_TESTING_ONLY

if((option == 0x30) || (option == 0x33) || (option == 0x34)) //get total number of blocks for options 0, 3 or 4
{


console_write("How many blocks? (000-999):\n\0");
console_update(0, 0);
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
totalBlocks = (data & 0x0f) * 100;
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
totalBlocks += (data & 0x0f) * 10;
while(data > '9' || data < '0') data = ui_get_key(console_keypad_size, 1, 0);
totalBlocks += (data & 0x0f);

}
#endif

switch (option)
{
case '0': //error = SD_erase (block, totalBlocks);
          error = SD_erase (startBlock, totalBlocks);

          if(error)
              console_write("Erase failed..\n\0");
          else
              console_write("Erased!\n\0");
			console_update(0, 0);
          break;

case '1':
          console_write(" Enter text (End with ~):\n\0");
		  console_update(0, 0);
          i=0;
            do
            {
				buffer2[1] = '\0';
                data = ui_get_key(console_keypad_size, 1, 0);
                buffer2[0] = data;
				console_write(buffer2);
				console_update(0, 0);
                buffer[i++] = data;
                if(data == '\n')    //append 'newline' character whenevr 'carriage return' is received
                {
                    buffer2[0] = '\n';
					console_write(buffer2);
                    buffer[i++] = '\n';
                }
                if(i == 512) break;
            }while (data != '~');

            error = SD_writeSingleBlock (startBlock);


            if(error)
                console_write("Write failed..\n\0");
            else
                console_write("Write successful!\n\0");
	console_update(0, 0);
            break;

case '2': error = SD_readSingleBlock (startBlock);

          if(error)
            console_write("Read failed..\n\0");
          else
          {
            for(i=0;i<512;i++)
            {
                if(buffer[i] == '~') break;
                //transmitByte(buffer[i]);
            }

            console_write((uint8_t *)buffer);
            console_write("Read successful!\n\0");
          }
			console_update(0, 0);
          break;
//next two options will work only if following macro is cleared from SD_routines.h
#ifndef FAT_TESTING_ONLY

case '3':
          error = SD_writeMultipleBlock (startBlock, totalBlocks);

          if(error)
            console_write("Write failed..\n\0");
          else
            console_write("Write successful!\n\0");
	console_update(0, 0);
          break;

case '4': error = SD_readMultipleBlock (startBlock, totalBlocks);

          if(error)
            console_write("Read failed..\n\0");
          else
            console_write("Read successful!\n\0");
	console_update(0, 0);
          break;
#endif

case '5':
              findFiles(GET_LIST,0);
          break;

case '6':
case '7': return 0; break;
case '8':

          console_write("Enter file name: \n\0");
		  console_update(0, 0);
          for(i=0; i<13; i++)
                  fileName[i] = 0x00;   //clearing any previously stored file name
          i=0;
          while(1)
          {
            data = ui_get_key(console_keypad_size, 1, 0);
            if(data == '\n') break;  //'ENTER' key pressed
            if(data == 0x08)  //'Back Space' key pressed
            {
              if(i != 0)
              {
                i--;
              }
              continue;
            }
            if(data <0x20 || data > 0x7e) continue;  //check for valid English text character
            fileName[i++] = data;
            if(i==13){console_write(" file name too long..\n\0"); console_update(0, 0); break;}
          }
          if(i>12) break;


          if(option == '6')
            readFile( READ, fileName);
          if(option == '7')
            //createFile(fileName);
			console_write("Create file function not available...\n\0");
	console_update(0, 0);
          if(option == '8')
            deleteFile(fileName);
          break;

case '9': memoryStatistics();
          break;


default:

         console_write(" Invalid option!\n\0");
		 console_update(0, 0);

}


}
return 0;
}
