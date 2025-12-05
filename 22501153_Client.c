#include <stdio.h>              // User inputs
#include <pthread.h>            // Posix Threads
#include <sys/socket.h>         // Socket Library
#include <arpa/inet.h>          // inet_pton Library  
#include <stdbool.h>            // Boolean checks
#include <stdlib.h>             // Dynamic Memory Allocation
#include <string.h>             // Sprintf etc.
#include <unistd.h>             // Read, close etc.
#include <stdint.h>             // uint32_t, uint8_t etc.

#define MAX_LINES               100
#define MAX_LINE_SIZE           256
#define PORT                    8080
#define MESSAGELIST             "messages"  //Messages Location Prefix Definition

int client_fd;

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

char *userMenu[5] = 
{
    "1. List Contacts", 
    "2. Add Contact", 
    "3. Delete Contact", 
    "4. Send Message", 
    "5. Check Messages"
};

/*FUNCTION DECLARATION*/
static bool connect_ToServer(void);
static messageStruct checkUserID(char *argv[], messageStruct message);
static messageStruct requestDataFromServer(messageStruct message);
static messageStruct saveUserID( messageStruct message);
static void getUserInfo(messageStruct message);
static messageStruct displayContactList(messageStruct message);
static bool checkLineInFile(char *filename, messageStruct message, char *line, size_t line_size);
static messageStruct displayMessages(messageStruct message);
static void printSelectedMessages(messageStruct message, int selectedID);

/************************************************************************
* \brief         			CONNECT TO SERVER	
* \param[in]                none
* \return         			on success TRUE is returned
* 							on fail EXIT 
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static bool connect_ToServer(void)
{
    struct sockaddr_in serv_addr;
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("Socket failed\n");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<= 0) 
    {
        perror("Invalid address/ Address not supported\n");
        exit(EXIT_FAILURE);
    }
    if (connect(client_fd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) 
    {
        perror("Connection Failed \n");
        exit(EXIT_FAILURE);
    }
    return 1;
}

/************************************************************************
* \brief         			REQUEST SERVER TO CHECK ID IN USERLIST
* \param[in]     			argv user ID
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct checkUserID(char *argv[], messageStruct message)
{
    message.userID = atoi(argv[1]);
    message = requestDataFromServer(message);
    return message;
}

/************************************************************************
* \brief         			REQUEST DATA SEQUENCE FROM THE SERVER
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct requestDataFromServer(messageStruct message)
{
    if(send(client_fd, (void*)&message, sizeof(message), 0) == -1)
    {
        perror("Couldn't send message.\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("\nCommand sent successfully to the server.\n");  
        if(read(client_fd, (void*)&message, sizeof(message)) == -1)
        {
            perror("\nCouldn't receive message.\n");
        }  
        else
        {
            printf("Message received successfully from the server.\n");

        }    
    }
    return message;
}

/************************************************************************
* \brief         			REQUEST SERVER TO SAVE ID IN USERLIST
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct saveUserID( messageStruct message)
{
    printf("You are new user. To proceed, Please enter your contact information\n\nEnter Phone Number: ");
    scanf("%14s", message.phoneNumber);
    printf("Enter Name: ");
    scanf(" %14[^\n]>", message.name);
    printf("Enter Surname: ");
    scanf(" %14[^\n]", message.surname);

    message = requestDataFromServer(message);  
    return message; 
}

/************************************************************************
* \brief         			REQUEST SERVER TO SAVE ID IN USERLIST
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static void getUserInfo(messageStruct message)
{
    printf("\n");
    for(uint8_t i=0; i<5; i++)
    {
        printf("%s\n", userMenu[i]);
    }
    
    printf("\nPlease type your choice: ");
    scanf("%hhu",&message.userSelection);

    if(message.userSelection == 2)
    {
        printf("Please type target id to add: ");
        scanf("%d",&message.targetID);
    }

    else if(message.userSelection == 3)
    {
        printf("Please type target id to delete: ");
        scanf("%d",&message.targetID);
    }
    
    else if(message.userSelection == 4)
    {
        printf("Please type target id: ");
        scanf("%d",&message.targetID);
        printf("Please type your message: ");
        scanf(" %29[^\n]",message.sent_or_live_message);
    }

    message = requestDataFromServer(message);

    if(message.userSelection == 1)
    {
       message = displayContactList(message);
    }
    else if(message.userSelection == 4)
    { 
        char line[256];
        char filename[256]; // Adjust the buffer size as needed
        sprintf(filename, "%s_%d.txt", MESSAGELIST,message.targetID);
        if(checkLineInFile(filename, message, line, sizeof(line)) == true)
        {
            printf("MESSAGE SENT TO USER %d\n", message.targetID);
        }
    }
    else if(message.userSelection == 5)
    {
        message = displayMessages(message);
    }
}

/************************************************************************
* \brief         			PRINT CONTACTLIST
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct displayContactList(messageStruct message)
{
    for (size_t i = 0; i < message.linecount; i++) 
    {
        printf("%s", message.buffer[i]);
    }
}

/************************************************************************
* \brief         			CHECK USERID IN USERLIST
* \param[in]     			filename
* \param[in]                message struct
* \param[in]                line to hold each line of a file
* \param[in]                line_size size of line
* \return         			TRUE if ID found
* 							FALSE if IF not found
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static bool checkLineInFile(char *filename, messageStruct message, char *line, size_t line_size)
{
    FILE *fp = fopen(filename, "r");
    char buf[50];
    sprintf(buf,"%d-%s",message.userID,message.sent_or_live_message);
    while (fgets(line, line_size, fp) != NULL) 
    {
        if (strncmp(line, buf, strlen(buf)) == 0) 
        {
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}

/************************************************************************
* \brief         			PRINT MESSAGES FOR SELECTED USERID
* \param[in]     			message struct
* \param[in]                selectedID selected id to filter messages
* \return         			none
*
* \authors       			Ferhat Avşar
************************************************************************/
static void printSelectedMessages(messageStruct message, int selectedID)
{
    int length = snprintf( NULL, 0, "%d-", selectedID );
    char* specific_string = malloc( length + 1 );
    snprintf(specific_string, length + 1, "%d-", selectedID);

    for (size_t i = 0; i < message.linecount; i++) 
    {
        if(strncmp(message.buffer[i], specific_string, strlen(specific_string)) == 0)
        {
            printf("%s",message.buffer[i]);
        }
    }
    free(specific_string);
}

/************************************************************************
* \brief         			PRINT AND FILTER MESSAGES
* \param[in]                message struct
* \return         			message
*
*
* \authors       			Ferhat Avşar
************************************************************************/
static messageStruct displayMessages(messageStruct message)
{
    uint32_t userID[message.linecount];
    size_t count = 0;
    int selectedID = 0;

    for (size_t i = 0; i < message.linecount; i++) 
    {
        int id;
        if (sscanf(message.buffer[i], "%d-", &id) == 1) 
        {
            int duplicate = 0;
            for (size_t j = 0; j < count; j++) 
            {
                if (userID[j] == id) 
                {
                    duplicate = 1;
                    break;
                }
            }

            if (!duplicate) 
            {
                userID[count++] = id;
            }
        }
    }
    for(int i=0; i<count; i++)
    {
        printf("You have messages from User %d\n",userID[i]);
    }
    if(count < 1)
    {
        printf("You don't have any messages!\n");
    }
    else
    {
        printf("Whose messages do you want to read? ");
        scanf("%d",&selectedID);
        printSelectedMessages(message, selectedID);
    }
}

/************************************************************************
* \brief         			MAIN FUNCTION
* \param[in]                argc executable
* \param[in]                argv userID
* \return         			return from main
*
*
* \authors       			Ferhat Avşar
************************************************************************/
int main(int argc, char *argv[])
{  
    messageStruct message; 

    if(connect_ToServer())
    {
        message = checkUserID(argv, message);
        if(message.isAlreadySaved == false)
        {
            message = saveUserID(message);
        }
        while(true)
        {
            getUserInfo(message);
        }

        close(client_fd);
    }

    return 0;
}
