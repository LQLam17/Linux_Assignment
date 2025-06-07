#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

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

typedef enum{
    MAIN_MENU,
    CHAT,
    LOG_OUT,
}user_option_t;

typedef struct {
    char user[10];
    char password[10];
    int id;
    int fd;
}account_t;


pthread_t receive_msg_thread;
pthread_mutex_t terminal_print_mutex;

msg_t send_buffer;
msg_t receive_buffer;

int socket_fd = -1;
int id = -1;
int current_option = 0;
int current_friend_id = 0;
volatile int chat_done = 0; 

int main_menu_option_number = 0;
int register_done = 0;
int login_done = 0;
int logout_done = 0;

void *send_msg_function(void *arg);
void *receive_msg_function(void *arg);

void app_user();
void app_feature_chat();
int app_feature_exit();

void app_feature_login_account();
void app_feature_create_new_account();
void app_feature_logout_account();

void send_msg_to_server(int sender_id, int receiver_id, msg_cmd_t cmd, char *payload);

void chat_file_read(int sender_id, int receiver_id);

int main(int argc, char *argv[]){
    pthread_mutex_init(&terminal_print_mutex, NULL);

    struct sockaddr_in socket_addr;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    socket_addr.sin_family = AF_INET;
    socket_addr.sin_addr.s_addr = inet_addr("192.168.67.128");
    socket_addr.sin_port = htons(5000);

    //bind(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr));

    if(connect(socket_fd, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) == 0){
        printf("connect sucessfully!\n");
    }
    else{
        printf("connect failed!\n");
        sleep(10);
        return 0;
    }

    pthread_create(&receive_msg_thread, NULL, receive_msg_function, (void *)socket_fd);
    pthread_detach(receive_msg_thread);

    while(1){
        printf("---MAIN MENU---\n");
        printf("1. Login\n");
        printf("2. Create new account\n");
        do{
            printf("Enter your option number: ");
            scanf("%d", &main_menu_option_number);
        }
        while(main_menu_option_number < 1 || main_menu_option_number > 2);
        
        switch(main_menu_option_number){
            case 1:
                app_feature_login_account();
                break;
            case 2:
                app_feature_create_new_account();
                break;
            default:
                break;
        }
    }


    return 0;
}

void app_feature_create_new_account(){
    account_t client_account_buffer = {0};
    while(1){
        memset(client_account_buffer.user, 0, sizeof(client_account_buffer.user));
        printf("Enter your user: ");
        scanf("%s", client_account_buffer.user);
        if(strncmp(client_account_buffer.user, ":q", 2) == 0){
            break;
        }

        memset(client_account_buffer.password, 0, sizeof(client_account_buffer.password));
        printf("Enter your password: ");
        scanf("%s", client_account_buffer.password);
        if(strncmp(client_account_buffer.password, ":q", 2) == 0){
            break;
        }

        char sub_buffer[30];
        sprintf(sub_buffer, "%s\n%s", client_account_buffer.user, client_account_buffer.password);
        send_msg_to_server(0, 0, REGISTER, sub_buffer);

        while(!register_done);

        register_done = 0;

        break;
    }
}

void app_feature_login_account(){
    account_t client_account_buffer = {0};
    int exit_while_loop = 0;
    while(1){
        memset(client_account_buffer.user, 0, sizeof(client_account_buffer.user));
        do{
            printf("Enter your user: ");
            scanf("%s", client_account_buffer.user);
            if(strncmp(client_account_buffer.user, ":q", 2) == 0){
                exit_while_loop = 1;
                break;
            }
        }
        while(strlen(client_account_buffer.user) <= 0);
        if(exit_while_loop){
            break;
        }
        
        memset(client_account_buffer.password, 0, sizeof(client_account_buffer.password));
        do{
            printf("Enter your password: ");
            scanf("%s", client_account_buffer.password);
            if(strncmp(client_account_buffer.password, ":q", 2) == 0){
                exit_while_loop = 1;
                break;
            }
        }
        while(strlen(client_account_buffer.password) <= 0);
        if(exit_while_loop){
            break;
        }

        char sub_buffer[30];
        sprintf(sub_buffer, "%s\n%s", client_account_buffer.user, client_account_buffer.password);
        send_msg_to_server(0, 0, LOGIN, sub_buffer);


        while(!login_done);
        //break;

        if(login_done == 2){
            break;
        }

        else{
            app_user();
        }

        break;
    }
}

void app_user(){
    while(1){
        int back_to_previous_menu = 0;
        printf("---MAIN MENU---\n");
        printf("1. Chat with your friend\n");
        printf("2. Log out\n");
        
        do{
            printf("Your option: ");
            scanf("%d", &current_option);
        }
        while(current_option < 1 || current_option > 2);

        switch (current_option)
        {
        case CHAT:
            app_feature_chat();
            break;
        
        case LOG_OUT:
            back_to_previous_menu = app_feature_exit();
            break;
        }

        if(back_to_previous_menu){
            break;
        }
    }
    
}

void app_feature_chat(){
    do{
        printf("Input your friend's id: ");
        scanf("%d", &current_friend_id);
    }
    while(current_friend_id <= 0);

    getchar();

    pthread_mutex_lock(&terminal_print_mutex);
    chat_file_read(id, current_friend_id);
    pthread_mutex_unlock(&terminal_print_mutex);

    while(1){
        

        memset(send_buffer.payload, 0, 100);
        fgets(send_buffer.payload, 100, stdin);

        pthread_mutex_lock(&terminal_print_mutex);
        printf("\033[A");   // Move the cursor up 1 line
        printf("\r");       // Move the cursor to the beginning of the line
        printf("\033[K"); 
        pthread_mutex_unlock(&terminal_print_mutex);

        size_t len = strlen(send_buffer.payload);
        if (len > 0 && send_buffer.payload[len - 1] == '\n') {
            send_buffer.payload[len - 1] = '\0';
        }

        if(strcmp(send_buffer.payload, ":q") == 0){
            break;
        }

        pthread_mutex_lock(&terminal_print_mutex);
        printf("                                        %s\n", send_buffer.payload);
        pthread_mutex_unlock(&terminal_print_mutex);

        send_buffer.cmd = SEND_MSG;
        send_buffer.sender_id = id;
        send_buffer.receiver_id = current_friend_id;
        write(socket_fd, (void *)&send_buffer, sizeof(msg_t));
    }

    current_option = 0;
    current_friend_id = 0;
}

int app_feature_exit(){
    send_buffer.cmd = EXIT;
    send_buffer.sender_id = id;
    send_buffer.receiver_id = 0;

    write(socket_fd, (void *)&send_buffer, sizeof(msg_t));
    while(!logout_done);
    logout_done = 0;

    return 1;
}

void *receive_msg_function(void *arg){
    int socket_fd = (int)arg;
    while(1){
        read(socket_fd, (void *)&receive_buffer, sizeof(msg_t));

        if(receive_buffer.cmd == SEND_MSG && current_option == CHAT && receive_buffer.sender_id == current_friend_id){
            pthread_mutex_lock(&terminal_print_mutex);
            printf("%d: %s\n", receive_buffer.sender_id, receive_buffer.payload);
            pthread_mutex_unlock(&terminal_print_mutex);
        }

        else if(receive_buffer.cmd == EXIT_ACK){
            logout_done = 1;
        }

        else if(receive_buffer.cmd == REGISTER_ACK){
            //id = receive_buffer.receiver_id;
            printf("Register successfully\n");
            register_done = 1;
        }
        else if(receive_buffer.cmd == REGISTER_NAK){
            printf("Register failed\n");
            register_done = 2;
        }
        else if(receive_buffer.cmd == LOGIN_ACK){
            id = receive_buffer.receiver_id;
            printf("Login successfully\n");
            printf("Your id: %d\n", id);
            login_done = 1;
        }

        else if(receive_buffer.cmd == LOGIN_NAK){
            printf("Login failed\n");
            login_done = 2;
        }
    }
}



void send_msg_to_server(int sender_id, int receiver_id, msg_cmd_t cmd, char *payload){
    send_buffer.cmd = cmd;
    send_buffer.sender_id = sender_id;
    send_buffer.receiver_id = receiver_id;
    
    if(payload != NULL){
        memcpy(send_buffer.payload, payload, strlen(payload));
    }
    
    write(socket_fd, (void *)&send_buffer, sizeof(msg_t));
}

void chat_file_read(int sender_id, int receiver_id){
    char chat_file_name[10];
    if(sender_id <= receiver_id){
        sprintf(chat_file_name, "file%d_%d", sender_id, receiver_id);
    }
    else{
        sprintf(chat_file_name, "file%d_%d", receiver_id, sender_id);
    }

    FILE *fp = fopen(chat_file_name, "r");
    if (!fp) {
        perror("File open failed");
        return;
    }

    char line[100];
    char id_string[3] = {0};
  
    while (fgets(line, sizeof(line), fp)) {
        int i = 0;
        while(*(line + i) != ':'){
            *(id_string + i) = *(line + i);
            i++;
        }
        
        if(atoi(id_string) == id){
            printf("                                        %s", (line + i + 2));
        }

        else if(atoi(id_string) == current_friend_id){
            printf("%s", line);
        }
        
    }

    fclose(fp);
}


