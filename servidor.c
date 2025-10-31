#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>
#include <openssl/sha.h>

#define MAX_CLIENTS 256
#define BUF 4096

// Envío de todos los bytes
ssize_t send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    const char *p = buf;
    while (sent < len) {
        ssize_t s = send(fd, p + sent, len - sent, 0);
        if (s <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += s;
    }
    return sent;
}

// lectura hasta '\n'
ssize_t recv_line(int fd, char *buf, size_t maxlen) {
    size_t idx = 0;
    while (idx + 1 < maxlen) {
        ssize_t r = recv(fd, buf + idx, 1, 0);
        if (r == 0) return 0;
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (buf[idx] == '\n') {
            idx++;
            buf[idx] = '\0';
            return idx;
        }
        idx++;
    }
    buf[idx] = '\0';
    return idx;
}

typedef struct {
    uint64_t next_start;
    int range_size;
    char *base;
    int num_ceros;
    int clients[MAX_CLIENTS];
    int n_clients;
    pthread_mutex_t mtx;
    int found_flag;
    int listen_fd;
} SharedState;

SharedState state;

void broadcast_stop() {
    pthread_mutex_lock(&state.mtx);
    for (int i = 0; i < state.n_clients; ++i) {
        int fd = state.clients[i];
        if (fd <= 0) continue;
        send_all(fd, "STOP\n", 5);
        close(fd);
    }
    state.n_clients = 0;
    pthread_mutex_unlock(&state.mtx);

    if (state.listen_fd > 0) {
        close(state.listen_fd);
        state.listen_fd = -1;
    }
}

void remove_client_fd(int fd) {
    pthread_mutex_lock(&state.mtx);
    for (int i = 0; i < state.n_clients; ++i) {
        if (state.clients[i] == fd) {
            for (int j = i; j + 1 < state.n_clients; ++j)
                state.clients[j] = state.clients[j+1];
            state.clients[state.n_clients-1] = 0;
            state.n_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&state.mtx);
}

void *client_thread(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);

    pthread_mutex_lock(&state.mtx);
    if (state.n_clients < MAX_CLIENTS) state.clients[state.n_clients++] = client_fd;
    pthread_mutex_unlock(&state.mtx);

    char line[BUF];
    while (1) {
        pthread_mutex_lock(&state.mtx);
        if (state.found_flag) {
            pthread_mutex_unlock(&state.mtx);
            send_all(client_fd, "STOP\n", 5);
            break;
        }

        uint64_t start = state.next_start;
        uint64_t end = start + (uint64_t)state.range_size - 1;
        state.next_start = end + 1;
        int num_ceros = state.num_ceros;
        char *base = state.base;
        pthread_mutex_unlock(&state.mtx);

        char task[BUF];
        int tlen = snprintf(task, sizeof(task), "TASK;%s;%d;%llu;%llu\n",
                            base, num_ceros,
                            (unsigned long long)start, (unsigned long long)end);
        if (send_all(client_fd, task, tlen) <= 0) break;

        ssize_t r = recv_line(client_fd, line, sizeof(line));
        if (r <= 0) break;

        if (line[r-1] == '\n') line[r-1] = '\0';

        if (strcmp(line, "N") == 0) {
            continue;
        } else if (strncmp(line, "F;", 2) == 0) {
            char *saveptr = NULL;
            char *nonce_s = strtok_r(line+2, ";", &saveptr);
            char *hash_s = strtok_r(NULL, ";", &saveptr);

            // Reconstruir la cadena completa con el mensaje base
            char mensaje_completo[BUF];
            snprintf(mensaje_completo, sizeof(mensaje_completo), "%s%s", state.base, nonce_s);

            pthread_mutex_lock(&state.mtx);
            state.found_flag = 1;
            pthread_mutex_unlock(&state.mtx);

            printf("\n=== Resultado recibido ===\n");
            printf("nonce = %s\n", nonce_s);
            printf("hash  = %s\n", hash_s);
            printf("cadena= %s\n", mensaje_completo);
            printf("==========================\n");

            broadcast_stop();
            break;
        } else {
            break;
        }
    }

    close(client_fd);
    remove_client_fd(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <puerto> <mensaje_base> <num_ceros> <rango_por_cliente>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    char *base = argv[2];
    int num_ceros = atoi(argv[3]);
    int range_size = atoi(argv[4]);

    memset(&state, 0, sizeof(state));
    state.next_start = 0;
    state.range_size = range_size;
    state.base = strdup(base);
    state.num_ceros = num_ceros;
    state.found_flag = 0;
    pthread_mutex_init(&state.mtx, NULL);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }
    state.listen_fd = listen_fd;

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(listen_fd, 16) < 0) { perror("listen"); return 1; }

    printf("Servidor escuchando en puerto %d\nBase='%s' ceros=%d rango=%d\n",
           port, base, num_ceros, range_size);

           while (1) {
            if (state.found_flag) break;  // salir si ya se encontró la cadena
        
            struct sockaddr_in cli;
            socklen_t clilen = sizeof(cli);
            int *client_fd = malloc(sizeof(int));
            if (!client_fd) continue;
        
            *client_fd = accept(listen_fd, (struct sockaddr*)&cli, &clilen);
            if (*client_fd < 0) {
                free(client_fd);
                if (state.found_flag) break; // romper si fue por cierre del socket
                continue;
            }
        
            printf("Cliente conectado: %s:%d (fd=%d)\n",
                   inet_ntoa(cli.sin_addr), ntohs(cli.sin_port), *client_fd);
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread, client_fd);
            pthread_detach(tid);
        }

    printf("Servidor finalizado.\n");
    close(listen_fd);
    free(state.base);
    pthread_mutex_destroy(&state.mtx);
    return 0;
}
