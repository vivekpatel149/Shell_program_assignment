/*
Vivek Patel
UTA ID: 1001398338
*/


// The MIT License (MIT)
// 
// Copyright (c) 2016, 2017 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                               // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10   // Mav shell only supports ten arguments

#define HIST_total 15 //Total number of history msh shell can show at max

#define Pids_total 15 //Total number of pids msh shell can show to user at max

int recent_child = -999 ;    //Store the recent child pid so that if it will get suspended then we can store in suspended pid
int suspended = -999;        //Store the suspended child process and run it in the background when needed
int pids[15] ;              //Child_pid for every new child process
//taking count of the process
int process_count = 0  ;
//taking count of the commands entered by the user
int history_count = 0 ;  

//pid_t child_pid = -99;
          
static void handle_signal (int sig)
{
  /*
    Handling signals Ctrl-C, Ctrl-Z, and Background the suspended processes
  */
   if(sig == 20)
   {
       suspended = recent_child; //The child who is suspended should be stored in suspended i.e. recent child should be stored in suspended
   //     printf("The process you are suspending is %d", suspended);
   }
   
   if( sig !=2 && sig !=20)
   {
      printf("Unable to determine the signal\n");
   }
   printf("\nmsh> ");
}


//Function to print the pids
static void print_pids()
{
  int i;
  for( i = 0; i < process_count; i++)
  {
     printf("%d: %d\n", i, pids[i]);
  }
}

//Function to print history
static void print_history(char ***history)
{
  int i;
  for( i = 0; i < history_count; i++)
  {
     printf("%d: %s\n", i, (*history)[i]);
  }
}

//Manage history
//Passing command to store in history of commands
//Make sure that the history array should have only last 15 commands into it
static void manage_history(char command[], char ***history)
{
     int lm;
     if(history_count < HIST_total)
     {
         strcpy((*history)[history_count], command);       
         history_count +=1;
     }
     else
     {
          for(lm = 0; lm < (HIST_total-1); lm++)
          {
              strcpy((*history)[lm], (*history)[lm+1]);
          }
          strcpy((*history)[14], command);
          history_count = 15;
     }
}

//Storing the pids everytime when there is new process start
static void manage_pid(int pid)
{
  // recent_child = pid;
  int lm;
  if(process_count < Pids_total)           
  {
     pids[process_count] = pid;
     process_count+=1;
  }
  else
  {
      for(lm = 0; lm < (Pids_total-1); lm++)
      {
          pids[lm] = pids[lm+1];
      }
      pids[14] = pid;
      process_count = 15;
  }
}


     
int main()
{  
     
     //Signal Handling
     struct sigaction act;
 
     /*
       Zero out the sigaction struct
    */ 
     memset (&act, '\0', sizeof(act));
 
     /*
       Set the handler to use the function handle_signal()
     */ 
     act.sa_handler = &handle_signal;
 
     /* 
       Install the handler for SIGINT and SIGTSTP and check the 
       return value.
     */ 
     if (sigaction(SIGINT , &act, NULL) < 0) 
     {
        perror ("sigaction: ");
        return 1;
     }
     if (sigaction(SIGTSTP , &act, NULL) < 0) 
     {
        perror ("sigaction: ");
        return 1;
     }
    
     
     //variable going to be used in for loop most of the time
     int lm = 0;
    
     
     //array for storing the history of the valid commands enetered by the user
     char** history = (char**) malloc(HIST_total*sizeof(char*));

     for( lm = 0; lm<HIST_total;lm++)
     {
         history[lm] = (char*) malloc(MAX_COMMAND_SIZE * sizeof(char) );
     } 

     //Cmd_str getting the whole input from the user
     char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
     //Perfect command in command
     char * command = (char*) malloc( MAX_COMMAND_SIZE );

     //storing the first process which is a parent process
     pids[process_count] = getpid();
     process_count++;
     
     while( 1 )
     {
  
        // Print out the msh prompt
        printf ("msh> ");
       
        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while(!fgets (command, MAX_COMMAND_SIZE, stdin));
    
        //Checking whether the user is just inputing alot of space and then press enter
        int spaces = 0;
        int count_space, counting  = 0;
        int cmd_len = (int) strlen(command);
        for( count_space = 0; count_space < cmd_len; count_space++)
        {
            int k = (int)command[count_space];
            if( k == 32) {spaces++;}
            if( k != 32 && k != 10)
            {
                counting++; break;
            }
         }
    
         // If user input does not have any characters then do not go in to the while loop to execute any commmand
        if( counting )   
        {
             //Parsing the real command after eliminating all the spaces in front
            strncpy(cmd_str, command + spaces, cmd_len);
            
            //If user want to access the history command by using '!n' command then spliting string into a integer
            
            int repeat_command = 99;
            if(cmd_str[0] == '!')
              {
                  for(lm =1; lm < strlen(cmd_str); lm++)
                  {
                      if(cmd_str[lm] == ' ' || cmd_str[lm]== '\0')
                      { break;}
                      else 
                      { 
                           if((int)cmd_str[lm] > 47 && (int)cmd_str[lm] < 58)
                           {
                               if(lm == 1){ repeat_command = 0;repeat_command += (int) cmd_str[lm] - 48;}
                               else
                               {
                                   repeat_command = repeat_command*10;
                                   repeat_command += (int) cmd_str[lm] - 48;
                               }
                           }else {break;}
                      }
                 }
                 //If user want to acces history higher than history_count then 'command not found' will be shown to user
                  if(repeat_command < history_count)
                  {     
                       strcpy(cmd_str, history[repeat_command]);
                  }
               } 
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

              //Exit the program with status 0 if user put "exit" or "quit"
              if( !strcmp(token[0], "quit") || !strcmp(token[0], "exit"))
              {
                     return 0;
              }
              //Changing directory command
              if( !strcmp(token[0], "cd"))
              {
                    
                     if(chdir(token[1]) == -1)
                     {
                        printf("Unable to access directory: %s.\n", token[1]);
                     }
                     else
                     {
                         manage_history(cmd_str, &history);
                     }
               }
               else if( !strcmp(token[0], "bg")) //running the suspended program into background
               {
                     if(suspended != -999)
                     {
                         printf("BG %d\n", suspended );
                         kill(suspended, SIGCONT);
                         suspended = -999;
                     }
                     else {printf("There is no process to run in background\n");} //If there is no process which is suspended then user will get this message
                     manage_history("bg",&history);
               }
               else if( !strcmp(token[0], "listpids"))
               {
                      print_pids(); //Print the list pids
                      manage_history("listpids",&history); 
               }
               else if( !strcmp(token[0], "history"))
               {    
                      manage_history("history",&history);
                      print_history(&history); //Print the history
               }
               else
               {  
                    manage_history(cmd_str, &history);
                    //executing command using execv
                    //Creating a child process first 
                    pid_t child_pid = fork();
                    if(child_pid !=0)
		    { recent_child = child_pid; }
		    else
                    { recent_child = getpid(); }
                    int status;
      
          
                   //If it is an child process then execute the command 
                  if( child_pid == 0 )
                  {
                        //If failure to find command then print command not find
                        if( execv(token[0], token ) == -1)
                        {  
                             char addon[256] = "/usr/local/bin/";
                             strcat(addon,token[0]);
                             if( execv(addon, token ) == -1)
                             {
                                 char addonn[256] = "/usr/bin/";
                                 strcat(addonn,token[0]);
                                 if( execv(addonn, token ) == -1)
                                 {
                                     char addonnn[256] = "/bin/";
                                     strcat(addonnn,token[0]);
                                     if(execv(addonnn, token ) == -1)
                                     {
                                         printf("%s : Command not found\n", token[0]);
                                     }
                                  }
                              }
                           } 
                           exit( EXIT_SUCCESS );
                    }
                  
                    free( working_root );
                   
                  waitpid( child_pid, &status, 0 );     

                   

 manage_pid(child_pid); //Storing the child pid
               }    
                   
             } 
             for( lm = 0; lm <256; lm++)
             { 
                 cmd_str[lm] = '\0';command[lm] = '\0';
             }           //nullifying the cmd str 
        }
  free(cmd_str);
  free(command);
  return 0;
}
