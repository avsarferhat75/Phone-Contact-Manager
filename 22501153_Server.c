#include <stdio.h>              // User inputs
#include <pthread.h>            // Posix Threads
#include <sys/socket.h>         // Socket Library
#include <stdbool.h>            // Boolean checks
#include <stdlib.h>             // Dynamic Memory Allocation
#include <string.h>             // Sprintf etc.
#include <unistd.h>             // Read, close etc.
#include <stdint.h>             // uint32_t, uint8_t etc.
#include <netinet/in.h>         // Socket Library Struct Definitions
#include <sys/stat.h>           // isFileExist function     
#include <asm-generic/socket.h> // setsockopt Definitions   

#define MAX_LINES               100
#define MAX_LINE_SIZE           256
#define PORT                    8080
#define USERLIST                "userlist.txt"
#define CONTACTLIST             "contaclist"
#define MESSAGELIST             "messages"

typedef enum
{
    list_Contact = 1,
    add_Contact,
    delete_Contact,
    send_Message,
    check_Message
}userMenu;

typedef struct 
{
    bool        isAlreadySaved;     // is userID already in userlist.txt
    uint32_t    userID;             // each client ID
    uint32_t    targetID;           // target ID to send message or add/delete as contact
    uint8_t     userSelection;      // selection between list/add/delete contact, send/check message
    char        phoneNumber[15];    // phoneNumber of each user
    char        name[15];           // name of each user
    char        surname[15];        // surname of each user
    char        sent_or_live_message[30];   // message for current client
    char        buffer[MAX_LINES][MAX_LINE_SIZE];   //buffer holding messages and contacts   
    uint32_t    linecount;          // linecount for messages and contacts
} messageStruct;

struct sockaddr_in server_addr;
struct sockaddr_in client_addr;
pthread_t accept_new_conn_thread;
int server_fd, new_socket, *new_sock;


/*FUNCTION DECLARATION*/
static bool IsFileExist(const char* filename);
static void initFile(char *filename);
static void initServer(void);
static void startAcceptorThread(void);
static void *connectionHandler(void *socket_desc);
static messageStruct checkUserID(int sock, messageStruct message, size_t messagesize);
static bool checkIdInFile(char *filename, uint32_t targetID, char *line, size_t line_size);
static messageStruct saveUserID(int sock, messageStruct message, size_t messagesize);
static void createUser(uint32_t userID, char *phoneNumber, char *name, char *surname);
static void getUserInfo(int sock, messageStruct message, size_t messagesize);
static messageStruct displayContactList(messageStruct message);
static void createContact(uint32_t userID, uint32_t target_userID);
static void deleteContact(int userID, int target_userID);
static void deleteContactinFile(char *filename, uint32_t targetID, char *line, size_t line_size);
static void sendMessage(messageStruct message);
static messageStruct displayMessages(messageStruct message);
static messageStruct getAllLines(char *filename, char *line, size_t line_size, messageStruct message);


/************************************************************************
* \brief         			Check if file exist
* \param[in]     			File name
* \param[out]				none
* \return        			on success TRUE is returned
* 							on fail FALSE is returned
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static bool IsFileExist(const char* filename)
{
	bool returnCode;
	struct stat filestat = {0};

	if(stat(filename,&filestat) == 0)
	{
		returnCode = true;
	}
	else
	{
		returnCode = false;
	}

	return returnCode;
}

/************************************************************************
* \brief         			INIT FILE 
* \param[in]     			File name
* \param[out]				none
* \return        			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void initFile(char *filename)
{
    FILE *fp = fopen(filename,"wb");
    fclose(fp);
}

/************************************************************************
* \brief         			INIT SOCKET COMMUNICATION
* \param[in]     			File name
* \param[out]				none
* \return        			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void initServer(void) 
{
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket failed\n");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) 
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) 
    {
        perror("Acceptor socket bind failed\n");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 5) < 0 ) 
    {
        perror("listen failed\n");
        exit(EXIT_FAILURE);
    }
}

/************************************************************************
* \brief         			START accept_new_conn_thread
* \param[in]     			none
* \param[out]				none
* \return        			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void startAcceptorThread(void)
{
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    new_sock = malloc(1);
	*new_sock = new_socket;
    if (pthread_create(&accept_new_conn_thread, &attr, connectionHandler, (void*) new_sock) < 0) 
    {
        perror("Thread Creation failed!\n");
        exit(EXIT_FAILURE);
    }
    printf ("Service started : NewClientConnectionThread\n");
}

/************************************************************************
* \brief         			CALLBACK FUNCTION
* \param[in]     			fd file descriptor
* \param[out]				none
* \return        			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void *connectionHandler(void *socket_desc)
{
    messageStruct message; 
	int sock = *(int*)socket_desc;

    message = checkUserID(sock, message, sizeof(message));
    if(message.isAlreadySaved == false)
    {
        message = saveUserID(sock, message, sizeof(message));
    }
    while(true)
    {
        getUserInfo(sock, message, sizeof(message));
    }
}

/************************************************************************
* \brief         			CALL CHECKIDINFILE TO CHECK USERID IN USERLIST
* \param[in]     			sock file descriptor
* \param[in]                message struct
* \param[in]                message struct size 
* \return         			message struct
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct checkUserID(int sock, messageStruct message, size_t messagesize)
{
    if(read(sock, (void*) &message, messagesize) != -1)
    {
        char line[256];

        if(checkIdInFile(USERLIST, message.userID, line, sizeof(line)) == true)
        {
            message.isAlreadySaved = true;
        }
        else
        {
            message.isAlreadySaved = false;
        }
        send(sock,(void*) &message, messagesize,0);
    }
    return message;
}

/************************************************************************
* \brief         			CHECK USERID IN USERLIST
* \param[in]     			filename
* \param[in]                targetID ID to check in a file
* \param[in]                line to hold each line of a file
* \param[in]                line_size size of line
* \return         			TRUE if ID found
* 							FALSE if IF not found
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static bool checkIdInFile(char *filename, uint32_t targetID, char *line, size_t line_size)
{
    FILE *fp = fopen(filename, "r");

    int length = snprintf( NULL, 0, "%d-", targetID );
    char* specific_string = malloc( length + 1 );
    snprintf(specific_string, length + 1, "%d-", targetID);
    
    while (fgets(line, line_size, fp) != NULL) 
    {
        if (strncmp(line, specific_string, strlen(specific_string)) == 0) 
        {
            fclose(fp);
            free(specific_string);
            return true;
        }
    }
    free(specific_string);
    fclose(fp);
    return false;
}

/************************************************************************
* \brief         			SAVE USERID TO USERLIST
* \param[in]     			sock file descriptor
* \param[in]                message struct
* \param[in]                messagesize size of struct
* \return         			message struct
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct saveUserID(int sock, messageStruct message, size_t messagesize)
{
    if(read(sock, (void*) &message, messagesize) != -1)
    {
        createUser(message.userID, message.phoneNumber, message.name, message.surname);
        send(sock, (void*) &message, messagesize, 0);
    }
    return message;
}

/************************************************************************
* \brief         			CREATE USER
* \param[in]     			userID
* \param[in]                phoneNumber
* \param[in]                name
* \param[in]                surname
* \return         			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void createUser(uint32_t userID, char *phoneNumber, char *name, char *surname) 
{
    FILE *fp = fopen(USERLIST,"a");
    fprintf(fp, "%d-%s-%s-%s\n", userID, phoneNumber, name, surname);
    fclose(fp);
}

/************************************************************************
* \brief         			GET USER INFO FROM CLIENT	
* \param[in]                sock file descriptor
* \param[in]                message struct
* \param[in]                messagesize size of struct
* \return         			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void getUserInfo(int sock, messageStruct message, size_t messagesize)
{
    userMenu menu;
    if(read(sock, (void*) &message, messagesize) != -1)
    {
        menu = message.userSelection;
        switch (menu)
        {
        case list_Contact:
            message = displayContactList(message);
            break;

        case add_Contact:
            createContact(message.userID, message.targetID);
            break;

        case delete_Contact:
            deleteContact(message.userID, message.targetID);
            break;

        case send_Message:
            sendMessage(message);
            break;

        case check_Message:
            message = displayMessages(message);
            break;
        
        default:
            break;
        }
        send(sock, (void*) &message, messagesize, 0);
    }
}

/************************************************************************
* \brief         			Get contact list and update message struct
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct displayContactList(messageStruct message)
{
    char line[256];
    char filename[256]; // Adjust the buffer size as needed
    sprintf(filename, "%s_%d.txt", CONTACTLIST,message.userID);
    message = getAllLines(filename, line, sizeof(line), message);
    return message;
}

/************************************************************************
* \brief         			ADD CONTACT	
* \param[in]                userID current client ID
* \param[in]                target_userID ID of the contact to be added
* \return         			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void createContact(uint32_t userID, uint32_t target_userID) 
{
    if(userID != target_userID)
    {
        char line[256];
        char previousline[256];
        if(checkIdInFile(USERLIST, target_userID, line, sizeof(line)) == false)
        {
            perror("THIS IS NOT A VALID PROCESS. ");
        }
        else
        {
            strcpy(previousline,line);
            char filename[256]; // Adjust the buffer size as needed
            memset(line, 0, sizeof(line));
            sprintf(filename, "%s_%d.txt", CONTACTLIST,userID);
            if(IsFileExist(filename) == false)
            {
                initFile(filename);
            }
            if(checkIdInFile(filename, target_userID, line, sizeof(line)) == true)
            {
                printf("Target user id is already in contact list.");
            }
            else
            {
                FILE *fp = fopen(filename, "a"); 
                fprintf(fp, "%s", previousline);
                fclose(fp);
            }
        }
    }
}

/************************************************************************
* \brief         			DELETE CONTACT	
* \param[in]                userID current client ID
* \param[in]                target_userID ID of the contact to be deleted
* \return         			none
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void deleteContact(int userID, int target_userID) 
{
    char line[256];
    char previousline[256];
    if(checkIdInFile(USERLIST, target_userID, line, sizeof(line)) == false)
    {
        perror("THIS IS NOT A VALID PROCESS. ");
    }
    else
    {
        strcpy(previousline,line);
        char filename[256]; // Adjust the buffer size as needed
        memset(line, 0, sizeof(line));
        sprintf(filename, "%s_%d.txt", CONTACTLIST,userID);
        if(IsFileExist(filename) == true)
        {
            deleteContactinFile(filename,target_userID, line, sizeof(line));
        }

    }
}

/************************************************************************
* \brief         			ROOT FUNCTION TO DELETE CONTACT IN A FILE	
* \param[in]     			filename
* \param[in]                targetID ID to check in a file
* \param[in]                line to hold each line of a file
* \param[in]                line_size size of line
* \return         			none
*
* \authors       			Ferhat Avşar
************************************************************************/
static void deleteContactinFile(char *filename, uint32_t targetID, char *line, size_t line_size)
{
    
    FILE *originalFile = fopen(filename, "r");
    FILE *tempFile = fopen("temp.txt", "w"); // temporary file

    int length = snprintf(NULL, 0, "%d-", targetID);
    char* specific_string = malloc( length + 1 );
    snprintf( specific_string, length + 1, "%d-", targetID );
    
    while (fgets(line, line_size, originalFile) != NULL) 
    {
        if (strncmp(line, specific_string, strlen(specific_string)) == 0) 
        {
            continue;
        }
        // Copy non-matching lines to the new file
        fputs(line, tempFile);
    }
    free(specific_string);
    fclose(originalFile);
    fclose(tempFile);

    // Replace the original file with the new file
    remove(filename);
    rename("temp.txt", filename);


}

/************************************************************************
* \brief         			Save into messageXXX.txt
* \param[in]                message struct
* \return         			none
*
* \authors       			Ferhat Avşar
************************************************************************/
static void sendMessage(messageStruct message)
{
    if(message.userID != message.targetID)
    {
        char line[256];
        char previousline[256];
        if(checkIdInFile(USERLIST, message.targetID, line, sizeof(line)) == false)
        {
            perror("THIS IS NOT A VALID PROCESS.");
        }
        else
        {
            char filename[256]; // Adjust the buffer size as needed
            sprintf(filename, "%s_%d.txt", MESSAGELIST,message.targetID);
            FILE *fp = fopen(filename, "a"); 
            fprintf(fp, "%d-%s\n", message.userID,message.sent_or_live_message);
            fclose(fp);
        }
    }
}

/************************************************************************
* \brief         			Get messages and update message struct
* \param[in]                message struct
* \return         			message
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct displayMessages(messageStruct message)
{
    char line[256];
    char filename[256]; // Adjust the buffer size as needed
    sprintf(filename, "%s_%d.txt", MESSAGELIST,message.userID);
    message = getAllLines(filename, line, sizeof(line), message);
    return message;

}

/************************************************************************
* \brief         			ROOT FUNCTION TO DELETE CONTACT IN A FILE	
* \param[in]     			filename
* \param[in]                targetID ID to check in a file
* \param[in]                line to hold each line of a file
* \param[in]                line_size size of line
* \param[in]                message struct
* \return         			message
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct getAllLines(char *filename, char *line, size_t line_size, messageStruct message)
{
    message.linecount = 0;
    FILE *fp = fopen(filename, "r");
    
    while (fgets(line, line_size, fp) != NULL) 
    {
        strncpy(message.buffer[message.linecount],line,line_size);
        message.linecount++;
    }

    fclose(fp);
    return message;
}


int main(void)
{
    if(IsFileExist(USERLIST) == false)
    {
        initFile(USERLIST);
    }
    initServer();

    int c = sizeof(struct sockaddr_in);
    while (new_socket =  accept (server_fd,(struct sockaddr *)&client_addr, (socklen_t*)&c)) 
    {
        startAcceptorThread();
    }
    if(new_socket < 0) 
    {
        printf ("Error in Accepting New Connections\n");
    }

    return 0;
}























