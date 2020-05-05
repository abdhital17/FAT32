
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>


#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255      // The maximum command-line size
#define MAX_NUM_ARGUMENTS 5      // Mav shell only supports five arguments
#define ROOT_DIR_PARENT -2	    //Return value in case the parent directory of a directory is the
						       //  root directory
#define DIR_NOT_FOUND -1	  //Return value in case the directory name input by user is not found 

static uint16_t BPB_BytesPerSec;
static uint8_t  BPB_SecPerClus;
static uint16_t BPB_RsvdSecCnt;
static uint8_t  BPB_NumFATs;
static uint16_t BPB_RootEntCnt;
static uint32_t BPB_FATz32;


static uint32_t root_Dir ;  //stores the root directory address
int fileOpen = 0;		//global variable whose value is 1 if a fire is currently open and 0 if not


static FILE *file;    //pointer to the image file entered by the user

struct __attribute__ ((__packed__)) DirectoryEntry
{
  char DIR_Name[11];
  uint8_t Dir_Attr;
  uint8_t Unused1[8];
  uint16_t DIR_FirstCLusterHigh;
  uint8_t Unused[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];



uint32_t LBAToOffset(int32_t sector)
{
  return (( sector - 2)* BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + 
          (BPB_NumFATs * BPB_FATz32 * BPB_BytesPerSec);
}


uint16_t NextLB(uint32_t sector)
{
  uint32_t FATAdress = ( BPB_BytesPerSec * BPB_RsvdSecCnt ) + ( sector * 4);
  int16_t val;
  fseek( file, FATAdress, SEEK_SET);
  fread( &val, 2, 1, file);
}

/*
 function that gets the info from the reserved sector 
 of the image file
*/
void getInfo()
{
  fseek(file, 11, SEEK_SET);
  fread(&BPB_BytesPerSec, 2, 1, file);

  fseek(file, 13, SEEK_SET);
  fread(&BPB_SecPerClus, 1, 1, file);

  fseek(file, 14, SEEK_SET);
  fread(&BPB_RsvdSecCnt, 2, 1, file);
  

  fseek(file, 16, SEEK_SET);
  fread(&BPB_NumFATs, 1, 1, file);


  fseek(file, 17, SEEK_SET);
  fread(&BPB_RootEntCnt, 2, 1, file);

  fseek(file, 36, SEEK_SET);
  fread(&BPB_FATz32, 4, 1, file);
}


/* function that loads the dir array with the files and directory 
in the cluster pointed by the parameter "address"
*/
void openDir(uint32_t address)
{
  int i;
  for(i=0; i<16; i++)
  {
    fseek(file, address +(32*i), SEEK_SET);
    fread(&(dir[i].DIR_Name), 11, 1, file);

    fseek(file, address+ (32*i)+ 0x0B, SEEK_SET);
    fread(&(dir[i].Dir_Attr), 1, 1, file);

    fseek(file, address+ (32*i)+ 0x14, SEEK_SET);
    fread(&(dir[i].DIR_FirstCLusterHigh), 2, 1, file);

    fseek(file, address+ (32*i)+ 0x1A, SEEK_SET);
    fread(&(dir[i].DIR_FirstClusterLow), 2, 1, file);

    fseek(file, address+ (32*i)+ 0x1C, SEEK_SET);
    fread(&(dir[i].DIR_FileSize), 11, 1, file);
  }


}

/*
Function to print out the information of the reserved sector of the image file
in base10 and hexadecimal values
*/
void printInfo()
{
  printf("*************** Base10\tHexadecimal\n");
  printf("BPB_BytesPerSec %i\t%x\n", BPB_BytesPerSec, BPB_BytesPerSec);
  printf("BPB_SecPerClus  %i\t%x\n", BPB_SecPerClus, BPB_SecPerClus);
  printf("BPB_RsvdSecCnt  %i\t%x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
  printf("BPB_NumFATs     %i\t%x\n", BPB_NumFATs, BPB_NumFATs);
  printf("BPB_RootEntCnt  %i\t%x\n", BPB_RootEntCnt, BPB_RootEntCnt);
  printf("BPB_FATz32      %i\t%x\n", BPB_FATz32, BPB_FATz32);
}



/*
This function opens the file given by filename in tok[1] 
and sets the current working directory to root
*/
void openFile(char** tok, int count)
{

  char * filename;

  if( count < 3 )
  {
    printf("Error: You must provide a file image as an argument.\n");
    printf("Example: open fat32.img\n");

    return;
  }


  filename = ( char * ) malloc( strlen( tok[1] ) + 1 );

  memset( filename, 0, strlen( tok[1] +1 ) );
  strncpy( filename, tok[1], strlen( tok[1] ) );
  filename[strlen(tok[1])]='\0';

  printf("Opening file %s\n", filename );

  file = fopen( filename , "r");

  if(! file)
  {
     perror("Error: File system image not found.");
     return;
  }

  fileOpen=1;
  getInfo();
  root_Dir= (BPB_NumFATs * BPB_FATz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt*BPB_BytesPerSec);
  openDir(root_Dir);

}



/*
compares the user input filename IN[] to the existing names from the dir structure array
for example if the user inputs filename as "foo.txt", and it is compared with "FOO     TXT"
it is a match
returns 1 if the names match
returns 0 if the names do not match
*/
int compare_fileName(char IMG[], char IN[])
{

  char IMG_Name[strlen(IMG)];
  char input[strlen(IN)];

  strcpy(IMG_Name, IMG);
  strcpy(input, IN);
  
  //printf("comparing %s of size %li with %s of size %li.\n",IMG_Name, strlen(IMG_Name), input, strlen(input));
  
  char expanded_name[12];
  memset( expanded_name, ' ', 12 );

  char *token = strtok( input, "." );

  strncpy( expanded_name, token, strlen( token ) );

  token = strtok( NULL, "." );

  if( token )
  {
    strncpy( (char*)(expanded_name+8), token, strlen(token ) );
  }

  expanded_name[11] = '\0';

  int i;
  for( i = 0; i < 11; i++ )
  {
    expanded_name[i] = toupper( expanded_name[i] );
  }
 return !( strncmp( expanded_name, IMG_Name, 11 )) ;
}


/*
compares the user input directory name IN[] to the existing names from the dir structure array
for example if the user inputs directory name as "foldera", 
and it is compared with "FOLDERA    ", it is a match
returns 1 if the names match
returns 0 if the names do not match
*/
int compare_DirName(char IMG[], char IN[])
{
  char IMG_Name[strlen(IMG)];
  char input[strlen(IN)];

  strcpy(IMG_Name, IMG);
  strcpy(input, IN);

  strtok(IMG_Name, " ");

  int i;
  for(i=0; i< strlen(IMG_Name); i++)
  {  
    IMG_Name[i]=tolower(IMG_Name[i]);
    input[i]=tolower(input[i]);
  }

  return !strcmp(IMG_Name, input);
}


/*
Prints the stat for the file or directory name given by tok
if the given name is that of a directory, 
the size is 0.
if the given name is that of a file,
the attribute byte, starting cluster number and file size are printed.
prints an error message if the file is not found.
*/
void printStat(char* tok)
{
  int i;
  for(i=0; i<16; i++)
  {
    if (dir[i].DIR_FileSize == 0 && compare_DirName(dir[i].DIR_Name, tok) )
    {
      printf("attribute \t size \t starting cluster number \n");
      printf("%d        \t %i\t %i\n", dir[i].Dir_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
      return;
    }

    else if(dir[i].DIR_FileSize != 0 && compare_fileName(dir[i].DIR_Name, tok))
    {
      printf("attribute \t size \t starting cluster number \n");
      printf("%d        \t %i\t %i\n", dir[i].Dir_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
      return;
    }

  }

  printf("Error: File not found.\n");
}


/*
function compares the user input dirName with the subdirectories in the current working directory
returns the address of the required directory if found
and -1 if directory is not found
special case: if the directory name passed by user is '..', it returns -2
*/
uint32_t getDirAddress(char* dirName)
{
  //printf("getting: %s\n", dirName);
  int i;
  uint32_t address=0;
  for(i=0; i<16; i++)
  {
    if(dir[i].Dir_Attr== 0x10)
    { 
     int comp= compare_DirName(dir[i].DIR_Name, dirName);
     if(comp)
     {
       if(strcmp(dirName, "..")==0 && dir[i].DIR_FirstClusterLow==0)
         return ROOT_DIR_PARENT;

       address= LBAToOffset(dir[i].DIR_FirstClusterLow);
       return address;
     }
    }

  }

  if(address==0)
    return DIR_NOT_FOUND;
}


/*
function compares the user input filename with the files in the current working directory
returns the address of the required file if found
and -1 if file is not found
*/
uint32_t getFileAddress(char* filename)
{
  int i;
  uint32_t address=0;
  for(i=0; i<16; i++)
  {
    if(dir[i].Dir_Attr == 0x01 || dir[i].Dir_Attr == 0x20)
    { 
     int comp= compare_fileName(dir[i].DIR_Name, filename);
     if(comp)
     {
       address= LBAToOffset(dir[i].DIR_FirstClusterLow);
       return address;
     }
    }

  }

  if(address==0)
    return -1;
}


/*
returns the file size if the file with name 'filename' is found in the current directory
*/
uint32_t getFileSize(char* filename)
{
  int i;
  uint32_t size=0;
  for(i=0; i<16; i++)
  {
     if(dir[i].Dir_Attr == 0x01 || dir[i].Dir_Attr == 0x20)
     { 
      int comp= compare_fileName(dir[i].DIR_Name, filename);
      if(comp)
      {
       size= dir[i].DIR_FileSize;
       return size;
      }
    }
  }
}


/*
function to read the file at address given by 'address' starting at 'pos'
and read 'bytes' number of bytes from the starting position 'pos'
after reading, it writes the read data to the char array readData
*/
void readFile(uint32_t address, int pos, int bytes, char* readData)
{

  fseek(file, address + pos, SEEK_SET);
  fread(readData, bytes, 1, file);

  readData[bytes]='\0';

}


/*
function to read the file located at 'address' with name 'filename' and size: 'size'
and write a copy of it in the current working directory
*/
void getFile(uint32_t address, char* filename, uint32_t size)
{
 FILE *f;
 f = fopen(filename, "wb");

 char writeData[size];
 readFile(address, 0, size, writeData);

 fwrite(writeData,1, sizeof(writeData), f);

 fclose(f);
}


/*
function to list out the directories and files in the current working directory
*/
void ls()
{
  int i;
  for(i=0; i<16; i++)
  {
    if((dir[i].Dir_Attr==0x01 || dir[i].Dir_Attr== 0x20 || dir[i].Dir_Attr == 0x10 || dir[i].DIR_Name[0] == 0x2e) && dir[i].DIR_Name[0] !=-0x1B)
      printf("%.11s \n", dir[i].DIR_Name);  
  }
  printf("\n");
}


int main()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  
  uint32_t working_dir;  //variable to keep track of the current working directory

  while( 1 )
  {

    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    if(token[0]==NULL)
      continue;				//go to next iteration if no command is entered


  	//if a file is not open currently and user enters any command other than "open"
    else if(strcmp(token[0], "open")!=0 && !fileOpen)
    {
      printf("Error: File system image must be opened first.\n");
      continue;
    }
    

    //if the user enters the "close" command
    else if(strcmp(token[0], "close")==0 && token_count<3)
    {
      if(fileOpen)	//if the file is open
      {
        fclose(file); //close the file
        fileOpen=0;
      }

      else		//if the file is not open, print error message
        printf("Error: File system not open.\n");

      continue;
    }


    //if the user enters the "open" command
    else if(strcmp(token[0], "open")==0)
    {
      if(fileOpen) //if there is a file already open, print a error message and move to next iteration
      {
        printf("Error: File system image already open.\n");
        continue;
      }

      //if a file is not currently open, open the file and set the working directory to root
      openFile(token, token_count);
      working_dir = root_Dir;
    }


    //if the user enters "cd" command
    else if(strcmp(token[0], "cd")==0 && token_count == 3)
    {
      uint32_t starting_dir;	//keep track of the directory from which the cd command was called
      							//if the user inputs an invalid path,
      							//use this to jump back to the original directory

      if(dir[0].DIR_Name[0] == '.' && dir[0].DIR_Name[1] !='.') //if the current directory is not root
      {
       starting_dir= LBAToOffset(dir[0].DIR_FirstClusterLow);
      }

      else  //if the current directory is root
        starting_dir = root_Dir;


      int check = 0;

      char* path = token[1];
      char* tok;

      while((tok = strtok_r(path, "/", &path))) //parse the string if the user entered a path
      {										   //for eg: cd foldera/folderc or cd ../foldera/
        uint32_t address= getDirAddress(tok);

        if(address== DIR_NOT_FOUND)  //if no such directory exists
        {
         printf("Directory does not exist. \n");
         check=1;
         break;
        }

        else if (address== ROOT_DIR_PARENT) //if directory pointed by the path goes through rootdir
        {  
         uint32_t root;
         openDir(root_Dir);
         check=0;
         continue;
        }  

        else		//if the directory exists and is not root directory
        {
          openDir(address);
          working_dir=address;
          check=0;
          continue;
        }
        
      }

      if(check)  //if the loop broke because the path is not valid
      { 
        openDir(starting_dir); //go to the starting directory 
        					  //- directory from where the cd command was called
      }

    }

    

    //if the user enters the "info" command, call the printInfo() function
    else if (strcmp(token[0],"info")==0 && token_count == 2)
      printInfo();

  	//if the user enters "ls" command
    else if(strcmp(token[0], "ls")==0 && (token_count >=2 || token_count<=3))
    {
      if(token_count==2)  //if the user entered only ls
        ls();

      else if(token_count == 3)
      {
        if(strcmp(token[1], ".") == 0) //if the user entered "ls ."
          ls();

        else if(strcmp(token[1], "..") == 0) //if the user entered "ls .."
        {
          uint32_t getback_address = working_dir;  //holds the current working directory

          if (getDirAddress("..") == ROOT_DIR_PARENT)  //if the parent directory is root
          {
            openDir(root_Dir);
          }

          else							//if the parent directory is not root
          {
            //and if the current directory is not root
            if(getDirAddress("..") != DIR_NOT_FOUND && compare_DirName(dir[1].DIR_Name, ".."))   
              openDir(getDirAddress(".."));
          } 
          
          ls();  //list the files and directories present in the parent directory
          openDir(getback_address); //get back to the directory from where the "ls .." was called
          continue;
        }

        else
          printf("invalid command!\n");

      }
    }

    //if the user enters "stat" command followed by a file name or directory name
    //calls the printStat() function and the name given by token[1] is passed into the function
    else if(strcmp(token[0], "stat")==0 && token_count == 3)
      printStat(token[1]);


  	//if the user enters "read" command followed by the filename, 
    //starting position and number of bytes
    else if(strcmp(token[0], "read") == 0 && token_count ==5)
    {
      uint32_t address= getFileAddress(token[1]);

      if(address == -1)
        printf("File not found in the directory.\n");

      char readData[atoi(token[3]) +1];

      readFile(address, atoi(token[2]), atoi(token[3]), readData);
      printf("%s\n",readData);

    }

    //if the user enters the "get" command followed by the filename
    else if(strcmp(token[0], "get") == 0 && token_count ==3)
    {
      uint32_t size, address;

      address= getFileAddress(token[1]);
      if(address== -1)
      {
        printf("Error: File not found.\n");
        continue;
      }

      size= getFileSize(token[1]);
      getFile(address, token[1], size);
      printf("\n");
    }


    //for every other command
    else
      printf("invalid command!\n");

    free( working_root );

  }
  return 0;
}
