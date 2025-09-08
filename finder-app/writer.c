#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]){

    //Opens syslog for user facility
    openlog(NULL, LOG_PID, LOG_USER); //Null = logtag will be program name "writer"
    
    //Check to see if correct number of args passed in
    if (argc != 3){
        syslog(LOG_ERR, "Error: Expected 2 args and got %d", argc-1);
        fprintf(stderr, "Args should be: %s <writefile> <writestr>\n", argv[0]);
        closelog();
        return 1;
    }

    const char* writefile = argv[1];
    const char* writestr = argv[2];

    //Try to open file
    FILE *file = fopen(writefile, "w");
    if (file == NULL){
        syslog(LOG_ERR, "Error: Could not open file %s", writefile);
        perror("fopen");
        closelog();
        return 1;
    }

    //Write string to file
    if (fputs(writestr,file) == EOF){
        syslog(LOG_ERR,"Error: Failed to write to file %s", writefile);
        perror("fputs");
        fclose(file);
        closelog();
        return 1;
    }

    fclose(file);
    //Log a success with log_debug
    syslog(LOG_DEBUG, "Writing \"%s\" to %s", writestr, writefile);
    closelog();
    return 0;
}