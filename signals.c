#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#define PORT 5000
#define BUFFER_SIZE 1024

volatile sig_atomic_t wasSigHup = 0; //переменная с атомарным доступом

void sigHupHandler(int r)
{
    wasSigHup = 1;
}

int main(){
    
    int server_fd;
    struct sockaddr_in server_address;
    
    // Создание сокета
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket failed.\n");
        exit(-1);
    }

    // Настройка адреса и порта
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    // Привязка сокета к адресу и порту
    if (bind(server_fd, &server_address, sizeof(server_address)) < 0) {
        printf("Bind failed %s\n", strerror(errno));
        close(server_fd);
        exit(-1);
    }

    // Прослушивание входящих соединений
    if (listen(server_fd, 1) < 0) {
        printf("Listen failed.\n");
        close(server_fd);
        exit(-1);
    }
    printf("Listening...\n");


    sigset_t origMask;
    //Регистрация обработчика сигнала
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);
    
    //Блокировка сигнала
    sigset_t blockedMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    
    //client;
    int client_fd, active_client = 0;
    struct sockaddr_in client_address;
    char buffer[BUFFER_SIZE] = {0};
    
    while(1){    //Работа основного цикла
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        int maxFd = server_fd;
    
        if (active_client == 1) {
            FD_SET(client_fd, &fds);
            if(client_fd > maxFd) {
                maxFd = client_fd;
            }
        }

        if (pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask) == -1) {  //атомарно ожидаем сигнала или события
            if (errno == EINTR) {
                printf("Some actions on receiving the signal.\n");
                // обработка сигнала sighup
                if (wasSigHup == 1) {
                    wasSigHup = 0;
                    printf("wasSigHup.\n");
                }
                continue;
            }        
        }
        //for the main socket and for every established connection

        if (FD_ISSET(server_fd, &fds)){ 
            if(active_client == 0) { // при отсутствии подключений - подключаем
                int len = sizeof(client_address);
                client_fd = accept(server_fd, &client_address, &len);
                if (client_fd >= 0) {
                    printf("Connected!\n");
                    active_client++;
                }
                else {
                    printf("Accept error.\n");
                }
            }
            else { // иначе - закрываем новое соединение
                struct sockaddr_in close_address;
                int len = sizeof(close_address);
                int close_fd = accept(server_fd, &close_address, &len);
                if (close_fd >= 0) {
                    printf("New connection was denied.\n");
                    close(close_fd);
                }
            }
        }

        //some actions on the descriptor activity
        //получение данных и закрытие соединения
        if (FD_ISSET(client_fd, &fds)) {
            int read_len = read(client_fd, &buffer, BUFFER_SIZE - 1);
            if (read_len > 0) {
                buffer[read_len - 1] = 0;
                printf("Recived: %s\n", buffer);
            }
            else {
                close(client_fd);
                FD_CLR(client_fd, &fds);
                printf("Connection closed.\n");
                active_client--;
            }
        }
    }
    return 0;
}