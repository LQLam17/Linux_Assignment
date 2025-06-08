#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#define MAX_CLIENT 10
#define MAX_CHAT_FILE 20
#define MAX_CHAT_FILE_NAME_LENGTH 10
#define CURRENT_DIR "/home/lqlam/Documents/lesson6_socket"

int count = 0;
int main_menu_option_number = 0;

//=======================================Message structure=================================================================
typedef enum{
    SEND_MSG,
    INITIALISE_ID,
    EXIT,
    EXIT_ACK,
    INITIALISE_ACK,
    ACK,
    REGISTER,
    REGISTER_ACK,
    REGISTER_NAK,
    LOGIN,
    LOGIN_ACK,
    LOGIN_NAK,
}msg_cmd_t;

typedef struct {
    int sender_id;
    int receiver_id;
    msg_cmd_t cmd;
    char payload[100];
}msg_t;
//=========================================================================================================================


//=======================================Account structure=================================================================
typedef struct {
    char user[10];
    char password[10];
    int id;
    int fd;
}account_t;
//=========================================================================================================================

account_t client_account[MAX_CLIENT];          //registered account
account_t login_client_account[MAX_CLIENT];    //logged in account

//An array contains file name which stores messages of 2 users
char *chat_file[MAX_CHAT_FILE] = {0};

pthread_t client_accept_thread;
pthread_mutex_t count_mutex;

void *client_handle(void *arg);
void *client_accept(void *arg);


//========================================Acount handle functions==========================================================
/**
 * Return 1 if account has been registered
 */
int account_user_available(char *user){
    if(strlen(user) <= 0){
        return -1;
    }
    for(int i = 0; i < MAX_CLIENT; i++){
        if(strcmp(user, client_account[i].user) == 0){
            return 1;
        }
    }
    return 0;
}

/**
 * Add the account to the registered account list
 */
void account_add(account_t *account){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(strlen(client_account[i].user) <= 0){
            memcpy(&client_account[i], account, sizeof(account_t));
            break;
        }
    }
}

/**
 * Show all the id of logged in account
 */
void login_account_show(){
    int count = 0;
    for(int i = 0; i < MAX_CLIENT; i++){
        if(strlen(login_client_account[i].user) <= 0){
            break;
        }
        else{
            count++;
            printf("%d\t", login_client_account[i].id);
        }
    }
    printf("\n");
    if(count == 0){
        printf("Don't have any account logged in!\n");
    }
}

/**
 * Return logged in account quantity
 */
int login_account_count(){
    int count = 0;
    for(int i = 0; i < MAX_CLIENT; i++){
        if(strlen(client_account[i].user) <= 0){
            break;
        }
        else{
            count++;
        }
    }

    return count;
}

/**
 * Return 1 if account has been logged in
 */
int login_account_user_available(char *user){
    if(strlen(user) <= 0){
        return -1;
    }
    for(int i = 0; i < MAX_CLIENT; i++){
        if(strcmp(user, login_client_account[i].user) == 0){
            return 1;
        }
    }
    return 0;
}

/**
 * Add account to the logged in account list
 */
int login_account_add(char *user, char *password){
    int client_account_index = -1;
    for(int i = 0; i < MAX_CLIENT; i++){
        if(strcmp(user, client_account[i].user) == 0){

            for(int j = 0; j < MAX_CLIENT; j++){
                if(strlen(login_client_account[j].user) <= 0){
                    memcpy(&login_client_account[j], &client_account[i], sizeof(account_t));

                    printf("Account ids list: ");
                    login_account_show();
                    
                    return j;
                }
            }

            break;
        }
    }

    return -1;
}

/**
 * Remove an account from the logged in account list
 */
void login_account_logout(int id){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(login_client_account[i].id == id){
            
            for(int j = i; j < MAX_CLIENT - 1; j++){
                memcpy(&login_client_account[j], &login_client_account[j + 1], sizeof(account_t));
            }
            memset(&login_client_account[MAX_CLIENT - 1], 0, sizeof(account_t));
            break;
        }
    }

    login_account_show();
}

/**
 * Send message to client
 */
void send_msg_to_client(int fd, int sender_id, int receiver_id, msg_cmd_t cmd, char *payload){
    msg_t buffer;
    buffer.cmd = cmd;
    buffer.sender_id = sender_id;
    buffer.receiver_id = receiver_id;

    if(payload != NULL){
        memcpy(buffer.payload, payload, sizeof(payload));
    }
    write(fd, (void *)&buffer, sizeof(msg_t));
}


/**
 * This function is to determine whether the account has been registered, if not it will register the account.
 * parameters: char *buffer: point to an array containing user \n password.
 * return: 0 if account's been registered, an id assigned for account if register successfully.
 */
int receive_register_account(char *buffer){
    char *pos = strchr(buffer, '\n');  
    int return_value = 0;
    if (pos != NULL) {

        *pos = '\0';               
        char *user = buffer;        
        char *password = pos + 1;   

        account_t account;

        if(account_user_available(user) == 1){
            return 0;
        }

        else{
            sprintf(account.user, "%s", user);
            sprintf(account.password, "%s", password);
            pthread_mutex_lock(&count_mutex);
            count++;
            account.id = count;
            
            return_value = count;
            account_add(&account);
            pthread_mutex_unlock(&count_mutex);

            return return_value;
        }
    }  

    return return_value;
}

/**
 * return: 0 if the account hasn't been registered or logged in, return user's id if account logged in successfully.
 */
int receive_login_account(char *buffer){
    char *pos = strchr(buffer, '\n');  
    if (pos != NULL) {
        *pos = '\0';               
        char *user = buffer;        
        char *password = pos + 1;   

        if(account_user_available(user) == 0 || login_account_user_available(user) == 1){
            return 0;
        }

        else{
            int index = login_account_add(user, password);
            if(index > -1){
                return login_client_account[index].id;
            }

            return 0;
        }
    }  

    return 0;
}

/**
 * Set user fd
 */
void set_user_fd(int id, int fd){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(login_client_account[i].id == id){
            login_client_account[i].fd = fd;
            break; 
        }
    }
}

/**
 * Get user fd
 */
int get_user_fd(int id){
    for(int i = 0; i < MAX_CLIENT; i++){
        if(login_client_account[i].id == id){
            return login_client_account[i].fd;
        }
    }

    for(int i = 0; i < MAX_CLIENT; i++){
        if(client_account[i].id == id){
            return 0;
        }
    }
    return -1;
}
//=========================================================================================================================


//===========================================File interaction functions====================================================
void chat_file_init(){
    for(int i = 0; i < MAX_CHAT_FILE; i++){
        chat_file[i] = (char *)malloc(MAX_CHAT_FILE_NAME_LENGTH * sizeof(char));
    }
}


int chat_file_available(int client_id1, int client_id2){
    char chat_file_name[10];
    if(client_id1 <= client_id2){
        sprintf(chat_file_name, "file%d_%d", client_id1, client_id2);
    }
    else{
        sprintf(chat_file_name, "file%d_%d", client_id2, client_id1);
    }

    int i = 0;
    while(strlen(chat_file[i]) > 1){
        if(strcmp(chat_file_name, chat_file[i]) == 0){
            return i;
        }
        i++;
    }
    return -1;
}

void chat_file_create(int client_id1, int client_id2){
    char chat_file_name[10];
    if(client_id1 <= client_id2){
        sprintf(chat_file_name, "file%d_%d", client_id1, client_id2);
    }
    else{
        sprintf(chat_file_name, "file%d_%d", client_id2, client_id1);
    }
    int fd = open(chat_file_name, O_RDWR | O_CREAT, 0644);
    if(fd > 0){
        close(fd);
    }

    for(int i = 0; i < MAX_CHAT_FILE; i++){
        if(strlen(chat_file[i]) <= 1){
            strcpy(chat_file[i], chat_file_name);
            break;
        }
    }
}

void chat_file_remove(int client_id1, int client_id2){
    char chat_file_name[10];
    if(client_id1 <= client_id2){
        sprintf(chat_file_name, "file%d_%d", client_id1, client_id2);
    }
    else{
        sprintf(chat_file_name, "file%d_%d", client_id2, client_id1);
    }

    int chat_file_index = chat_file_available(client_id1, client_id2);
    if(chat_file_index >= 0){
        remove(chat_file_name);
        for(int i = chat_file_index; i < MAX_CHAT_FILE - 1; i++){
            memcpy(chat_file[i], chat_file[i+1], MAX_CHAT_FILE_NAME_LENGTH);
        }
        memset(chat_file[MAX_CHAT_FILE - 1], 0, MAX_CHAT_FILE_NAME_LENGTH);
    }
}

void chat_file_remove_all(){
    for(int i = 0; i < MAX_CHAT_FILE; i++){
        if(strlen(chat_file[i]) > 1){
            remove(chat_file[i]);
        }
        else{
            break;
        }
    }
}

void chat_file_write(int sender_id, int receiver_id, char *message){
    char chat_file_name[10];
    if(sender_id <= receiver_id){
        sprintf(chat_file_name, "file%d_%d", sender_id, receiver_id);
    }
    else{
        sprintf(chat_file_name, "file%d_%d", receiver_id, sender_id);
    }

    int fd = open(chat_file_name, O_RDWR | O_CREAT | O_APPEND, 0644);
    if(fd < 0){
        return;
    }

    char message_buffer[200];
    sprintf(message_buffer, "%d: %s\n", sender_id, message);
    write(fd, message_buffer, strlen(message_buffer));

    close(fd);
}

//========================================================================================================================

int sigint_flag = 0;
void signal_handler(int signal){
    if(signal == SIGINT){
        sigint_flag = 1;      
    }
}

int main(){
    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in socket_addr;
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = inet_addr("192.168.67.128"); //inet_addr("192.168.67.1") 127.0.0.1
    socket_addr.sin_port = htons(5000);

    if (bind(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0) {
        exit(1);
    }

    if (listen(socket_fd, MAX_CLIENT) < 0) {
        exit(1);
    }

    pthread_mutex_init(&count_mutex, NULL);
    pthread_create(&client_accept_thread, NULL, client_accept, (void *) socket_fd);

    pthread_detach(client_accept_thread);

    chat_file_init();
    signal(SIGINT, signal_handler);

    while(1){
        if(sigint_flag == 1){
            chat_file_remove_all();
            break; 
        }
        sleep(1);
    }

    close(socket_fd);

    return 0;
}

void *client_accept(void *arg){
    int socket_fd = (int)arg;
    while(1){
        int socket_client_fd = accept(socket_fd, NULL, NULL);

        pthread_t thread;
        pthread_create(&thread, NULL, client_handle, (void *)socket_client_fd);
        pthread_detach(thread);
    }
    return NULL;
}

/**
 * Handle messages received from client.
 */
void *client_handle(void *arg){
    int fd = (int)arg;
    msg_t buffer;
    int id;

    while(1){
        ssize_t recv_len = recv(fd, &buffer, sizeof(msg_t), 0);
        if(recv_len <= 0){
            login_account_logout(id);
            close(fd);
            break;
        }
        
        if(buffer.cmd == SEND_MSG){
            int destination_fd = get_user_fd(buffer.receiver_id);
            if(destination_fd >= 0){
                if(destination_fd > 0){
                    if(write(destination_fd, (void *)&buffer, sizeof(buffer)) == -1){
                        if(errno == EPIPE){
                            close(fd);
                            break;
                        }
                    }
                }
                int chat_file_index = chat_file_available(buffer.sender_id, buffer.receiver_id);
                if(chat_file_index < 0){
                    chat_file_create(buffer.sender_id, buffer.receiver_id);
                }
                chat_file_write(buffer.sender_id, buffer.receiver_id, buffer.payload);
            }
            
        }

        else if(buffer.cmd == REGISTER){
            id = receive_register_account(buffer.payload);
            if(id > 0){
                send_msg_to_client(fd, 0, id, REGISTER_ACK, NULL);
            }
            else{
                send_msg_to_client(fd, 0, id, REGISTER_NAK, NULL);
            }
        }

        else if(buffer.cmd == LOGIN){
            id = receive_login_account(buffer.payload);
            if(id > 0){
                set_user_fd(id, fd);
                send_msg_to_client(fd, 0, id, LOGIN_ACK, NULL);
            }
            else{
                send_msg_to_client(fd, 0, id, LOGIN_NAK, NULL);
            }
        }

        else if(buffer.cmd == EXIT){
            //printf("client id %d exit\n", client_object->client_id);
            send_msg_to_client(fd, 0, id, EXIT_ACK, NULL);
            login_account_logout(id);
            break;
        }
    }

    return NULL;
}


