#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <net/if.h>
#include <ifaddrs.h>

#define BUFF_SIZE 256
#define MAX_CONNECTIONS 10

int numb_read, numb_write;
int is_server = 0;

char IP[100];
char command[120];
char command_option[10];

pthread_t Accep_Thread_id;
pthread_t recv_thread;

/*Struct for device*/
typedef struct {
    int id;
    int fd;
    int port_num;
    char my_ip[50];
    struct sockaddr_in addr;
} peer;

pthread_mutex_t connection_mutex = PTHREAD_MUTEX_INITIALIZER;

peer peers[MAX_CONNECTIONS];

int peer_count = 0;
int server_fd;

// This function used to assittence
void func_display_help()
{
    printf("--------------Command menu----------------\n");
    printf("1.myip                           : Display IP of this device\n");
    printf("2.myport                         : Display port of this device\n");
    printf("3.connect <destination> <port_no>: Connect to device with IP at <destination> and port at <port_no>\n");
    printf("4.list                           : Display all device connect\n");
    printf("5.terminate <connect id>         : Disconnect with device at id\n");
    printf("6.send <connect id> <message>    : Send message to a device with id\n");
    printf("7.exit                           : Close application\n");
    printf("------------------------------------------\n");
}

// This function used to display all cmd got in program
void func_display_all_cmd()
{
    printf("-------------Chat application-------------\n");
    printf("-------------Command list-----------------\n");
    printf("1.myip                           : Display IP of this device\n");
    printf("2.myport                         : Display port of this device\n");
    printf("3.connect <destination> <port_no>: Connect to device with IP at <destination> and port at <port_no>\n");
    printf("4.list                           : Display all device connect\n");
    printf("5.terminate <connect id>         : Disconnect with device at id\n");
    printf("6.send <connect id> <message>    : Send message to device with id\n");
    printf("7.exit                           : Close application\n");
    printf("8.help                           : Display all command\n");
    printf("-------------------------------------------\n");
}

// Hiển thị thông tin các peers hiện có
void Display_list_peers() {
    pthread_mutex_lock(&connection_mutex);
    printf("----------Connected devices----------\n");
    printf("ID |         IP Address        | Port No.\n");

    for (int i = 0; i < peer_count; i++) {
            printf("%d  |      %s      | %d\n",
                   i + 1,
                   inet_ntoa(peers[i].addr.sin_addr), 
                   ntohs(peers[i].addr.sin_port));
    }
    printf("--------------------------------------\n");
    pthread_mutex_unlock(&connection_mutex);
}



/*print this device 's IP*/
void Display_myIP()
{ 
    struct ifaddrs *ifaddr, *ifa;
    int family;
    char host[NI_MAXHOST];

    // Lấy danh sách các giao diện mạng
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Duyệt qua các giao diện mạng
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        // Chỉ lấy địa chỉ IPv4 (AF_INET)
        if (family == AF_INET) {
            // Chuyển đổi địa chỉ sang dạng chuỗi
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
                // Bỏ qua giao diện loopback (127.0.0.1)
                if (strcmp(ifa->ifa_name, "lo") != 0) {
                    printf("My IP: %s\n", host);
                }
            }
        }
    }

    freeifaddrs(ifaddr);
    
}

/*print this device 's port*/
void Display_port(int port)
{
    printf("Listening port of this app: %d\n", port);
}

/*Function to disconnect to a device with ID*/
void terminate_id(int id)
{
    pthread_mutex_lock(&connection_mutex);
    if (id > 0 && id <= peer_count) {
        close(peers[id - 1].fd);
        printf("Terminate peer with ID %d successfully.\n", id);
        for (int i = id - 1; i < peer_count - 1; i++) {
            peers[i] = peers[i + 1];
        }
        peer_count--;
    } else {
        printf("Invalid peer ID\n");
    }
    pthread_mutex_unlock(&connection_mutex);
}

// This function used to send message 
void send_msg(int id, char *sendbuff)
{
    pthread_mutex_lock(&connection_mutex);
    if (id > 0 && id <= peer_count) {
        send(peers[id - 1].fd, sendbuff, strlen(sendbuff), 0);
    } else {
        printf("ERROR: Can not send message\n");
    }
    pthread_mutex_unlock(&connection_mutex);
}

// This function used to read data from another device
void* handle_peer(void *arg)
{
    char recvbuff[BUFF_SIZE];
    memset(recvbuff, '0', BUFF_SIZE);

    int peer_index = *(int*)arg;
    int peer_socket = peers[peer_index].fd;
    int valread;
    while((valread = read(peer_socket, recvbuff, BUFF_SIZE)) > 0)
    {
        /* Đọc dữ liệu từ socket */
        /* Hàm read sẽ block cho đến khi đọc được dữ liệu */
        recvbuff[valread] = '\0';
        if(valread == -1)
        {
            printf("ERROR: Can not read data\n");
        }
        // Nếu kết nối bị ngắt
        if ( (strncmp("exit", recvbuff, 4) == 0) || (strncmp("terminate", recvbuff, 9) == 0) ) {
            printf("\nThe peer at port %d has disconnected\n", peers[peer_index].port_num);
            pthread_mutex_lock(&connection_mutex);
            for (int i = peer_index; i < peer_count - 1; i++) {
                peers[i] = peers[i + 1];
            }
            peer_count--;
            pthread_mutex_unlock(&connection_mutex);
            break;
        } 
        else{
            printf("\n--------------------------------------------------------------\n");
            printf("* Message receive from: %s\n", peers[peer_index].my_ip);
            printf("* Sender's Port:        %d\n", peers[peer_index].port_num);
            printf("* Message:              %s\n", recvbuff);
            printf("--------------------------------------------------------------\n\n");
        }
    }

    free(arg);
    pthread_exit(NULL);
}

void accept_connections() {
    while (1) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        int new_sock = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addr_len);

        if (new_sock < 0) {
            perror("Error accepting connection");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(&connection_mutex);
        if (peer_count < MAX_CONNECTIONS) {
            peers[peer_count].fd = new_sock;
            peers[peer_count].addr = client_addr;
            peers[peer_count].port_num = ntohs(client_addr.sin_port);
            inet_ntop(AF_INET, &client_addr.sin_addr, peers[peer_count].my_ip, sizeof(peers[peer_count].my_ip));
            peers[peer_count].id = peer_count + 1;
            printf("\nNew connection accepted from %s:%d\n",peers[peer_count].my_ip, 
                                                          peers[peer_count].port_num);

            // Tạo thread cho kết nối mới
            int *peer_index = malloc(sizeof(int));
            *peer_index = peer_count;
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, handle_peer, (void*)peer_index);
            pthread_detach(thread_id); // Tự động thu hồi thread sau khi kết thúc

            peer_count++;
        } else {
            printf("Maximum connections reached. Rejecting new connection.\n");
            close(new_sock);
        }
        pthread_mutex_unlock(&connection_mutex);
    }
}

// Hàm kết nối đến peer khác
void connect_to_peer(char *ip, int port) {
    struct sockaddr_in remote_addr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &remote_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return;
    }

    pthread_mutex_lock(&connection_mutex);
    if (peer_count < MAX_CONNECTIONS) {
        peers[peer_count].fd = sock;
        peers[peer_count].addr = remote_addr;
        peers[peer_count].port_num = port;
        strcpy(peers[peer_count].my_ip, ip);
        peers[peer_count].id = peer_count + 1;

        // Tạo thread để xử lý peer mới
        int *peer_index = malloc(sizeof(int));
        *peer_index = peer_count;
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_peer, (void*)peer_index);
        pthread_detach(thread_id); // Tự động thu hồi thread sau khi kết thúc

        printf("Connected to peer %s:%d\n", ip, port);
        peer_count++;
    } else {
        printf("Maximum connections reached. Cannot connect to more peers.\n");
        close(sock);
    }
    pthread_mutex_unlock(&connection_mutex);
}

int main(int argc, char *argv[])
{
    // Xử lý lệnh từ người dùng
    char command[256];
    char command_option[10];
    int opt = 1;

    system("clear"); // Call to clear screen

    if (argc != 2) {
        printf("No port provided\nTrue command: ./run <port number>\n");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    
    /* Initialize address for server */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    /* Ngăn lỗi : “address already in use” */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        perror("setsockopt()");  

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("ERROR: Can not bind for this socket\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONNECTIONS) == -1)
    {
        close(server_fd);
        printf("ERROR: Can not listen for another device\n");
        exit(EXIT_FAILURE);
    }

    func_display_all_cmd();

    printf("Application is listening on port : %d\n", port);

    pthread_t accept_thread;
    pthread_create(&accept_thread, NULL, (void*)accept_connections, NULL);
    pthread_detach(accept_thread);

    while(1)
    {
        printf("\nType your command:  ");
        fgets(command, 120, stdin);
        command[strcspn(command, "\n")] = 0; // Xóa ký tự xuống dòng

        /*process input and take command to command_option*/
        sscanf(command, "%s", command_option);

        if (!strcmp(command_option,"send"))
        {
            char *temp;
            char *mes;
            int id;

            /*process input and take infor to ID and mes*/
            temp = strtok(command, " ");
            id = atoi(strtok(NULL, " "));
            mes = strtok(NULL, "");
            (void)temp;
            /*send mes located by ID*/
            send_msg(id, mes);

            if( (strncmp(mes, "exit", 4) != 0) || (strncmp(mes, "terminate", 9) != 0) ){
                printf("Sent message successfully.\n");
            }
        }

        else if (!strcmp(command_option, "myip"))
        {
            Display_myIP();
        }

        else if (!strcmp(command_option, "myport")){
            Display_port(port);
        }

        else if (!strcmp(command_option,"list"))
        {
            Display_list_peers();
        }

        else if (!strcmp(command_option, "connect"))
        {
            int port_numb;
            char IP_dev[20];
            char temp[10];

            is_server = 0;

            /*get impotant daa to IP and port*/
            sscanf(command, "%s %s %d",temp, IP_dev, &port_numb);

            connect_to_peer(IP_dev, port_numb);
            printf("Connected successfully. Ready for data transmission\n");
            // break;
        }

        /*terminate a device located by ID*/
        else if (!strcmp(command_option,"terminate"))
        {
            int ID_temp;
            char temp_cmd[20];

            sscanf(command,"%s %d", temp_cmd, &ID_temp);
            send_msg(ID_temp, "terminate");
            terminate_id(ID_temp);
        }

        else if (!strcmp(command_option, "exit"))
        {
            for (int i =0; i< peer_count; i++)
            {
                send_msg(peers[i].id, "exit");
                close(peers[i].fd);
            }     
            printf("\n-----------------------ENDING PROGRAMMING--------------------------\n");
            close(server_fd);
            exit(0);
        }

        else if (!strcmp(command_option, "help"))
        {
            func_display_help();
        }

        else
        {
            printf("INVALID COMMAND\n");
        }         
    }
    return 0;
}