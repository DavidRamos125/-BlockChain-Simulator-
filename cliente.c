#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <time.h>
#include <errno.h>

#define BUF 4096

// Convertir hash a hexadecimal
void to_hex(unsigned char *hash, char *output) {
    for (int i = 0; i < 32; ++i)
        sprintf(output + i*2, "%02x", hash[i]);
    output[64] = '\0';
}

// Verifica si los últimos num_ceros dígitos hex son 0
int hash_has_trailing_hex_zeros(unsigned char *hash, int num_ceros) {
    char hex[65];
    to_hex(hash, hex);
    int len = 64;
    for (int i = 0; i < num_ceros; ++i)
        if (hex[len-1-i] != '0') return 0;
    return 1;
}

// nanosleep en milisegundos
void msleep(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
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

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <IP> <Puerto> <ms_sleep>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int ms_sleep = atoi(argv[3]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server.sin_addr);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect"); return 1;
    }

    printf("Conectado al servidor %s:%d\n", ip, port);

    char line[BUF];
    while (1) {
        ssize_t r = recv_line(sock, line, sizeof(line));
        if (r <= 0) break;

        if (strncmp(line, "TASK;", 5) == 0) {
            char *saveptr = NULL;
            strtok_r(line, ";", &saveptr); // "TASK"
            char *task_base = strtok_r(NULL, ";", &saveptr);
            char *task_ceros = strtok_r(NULL, ";", &saveptr);
            char *start_s = strtok_r(NULL, ";", &saveptr);
            char *end_s   = strtok_r(NULL, ";", &saveptr);

            unsigned long long start = strtoull(start_s, NULL, 10);
            unsigned long long end   = strtoull(end_s, NULL, 10);
            int ceros = atoi(task_ceros);

            printf("Procesando rango: %llu -> %llu (ceros=%d)\n", start, end, ceros);

            unsigned char hash[32];
            char hash_hex[65];
            int found = 0;
            char found_combined[BUF];

            for (unsigned long long nonce = start; nonce <= end; ++nonce) {
                int len = snprintf(line, sizeof(line), "%s%llu", task_base, nonce);
                SHA256((unsigned char*)line, len, hash);

                if (hash_has_trailing_hex_zeros(hash, ceros)) {
                    to_hex(hash, hash_hex);
                    found = 1;
                    snprintf(found_combined, sizeof(found_combined), "%s%llu", task_base, nonce);
                    break;
                }

                msleep(ms_sleep); // pausa configurable
            }

            if (found) {
                int slen = snprintf(line, sizeof(line), "F;%llu;%s;%s\n",
                                    start, hash_hex, found_combined);
                send(sock, line, slen, 0);
                printf("Encontrado: %s -> %s\n", found_combined, hash_hex);
                break;
            } else {
                send(sock, "N\n", 2, 0);
            }

        } else if (strncmp(line, "STOP", 4) == 0) {
            printf("Servidor detuvo la ejecución\n");
            break;
        }
    }

    close(sock);
    printf("Cliente finalizado.\n");
    return 0;
}
