#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sched.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <sys/time.h>

#define PORT 8080
#define MAX_EVENTS 2000
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 10000
#define MAX_NON_PERSISTENT 10
#define PACKET_SIZE 1024
#define NO_EVENT_TIMEOUT 180
#define INACTIVE_TIMEOUT 10.0
#define SUMMARY_INTERVAL 50

volatile sig_atomic_t running = 1;
unsigned long all_persistent_count = 0;
unsigned long all_non_persistent_count = 0;
unsigned long all_only_persistent_count = 0;
unsigned long all_only_non_persistent_count = 0;
unsigned long all_both = 0;
uint64_t all_per_bytes = 0;
uint64_t all_nonper_bytes = 0;
uint64_t all_perpackets = 0;
uint64_t all_nonpacktes = 0;
double conn2_duration = 0;
struct timeval conn1_start = {0}, conn1_end = {0};
struct timeval conn2_start = {0}, conn2_end = {0};
struct timeval last_event_time = {0};
int rejected = 0;
int queue_count = 0;

typedef struct {
    int fd;
    struct timeval connect_time;
} Connection;

Connection non_persistent_queue[MAX_NON_PERSISTENT];

typedef struct {
    unsigned long non_persistent_count;
    unsigned long persistent_count;
    unsigned long total_when_both_present;
    unsigned long only_persistent_count;
    unsigned long only_non_persistent_count;
    uint64_t per_total_bytes_read;
    uint64_t nonper_total_bytes_read;
    double persistent_ratio;
    double non_persistent_ratio;
    double oneconn2_duration;
    double throughput_mbpsper;
    double throughput_mbpsnonper;
    uint64_t per_packets;
    uint64_t nonper_packets;
} ConnectionStats;

ConnectionStats *stats_array = NULL;
size_t stats_count = 0;
size_t connection_count = 0;

void handle_signal(int sig) {
    running = 0;
}

double time_diff(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
}

void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void increase_fd_limit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        rl.rlim_cur = rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
    }
}

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void optimize_socket(int sock) {
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    int rcvbuf = 1024 * 1024 * 8;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    set_nonblocking(sock);
}

void write_summary_to_file(ConnectionStats *stats_array, size_t index, size_t start_conn, size_t end_conn, FILE *file) {
    ConnectionStats *stats = &stats_array[index];
    double count = end_conn - start_conn;

    fprintf(file, "Summary for Connections %zu to %zu:\n", start_conn + 1, end_conn);
    fprintf(file, "  Avg Non-persistent count: %.2f\n", (double)stats->non_persistent_count / count);
    fprintf(file, "  Avg Persistent count: %.2f\n", (double)stats->persistent_count / count);
    fprintf(file, "  Avg Total when both present: %.2f\n", (double)stats->total_when_both_present / count);
    fprintf(file, "  Avg Only persistent count: %.2f\n", (double)stats->only_persistent_count / count);
    fprintf(file, "  Avg Only non-persistent count: %.2f\n", (double)stats->only_non_persistent_count / count);
    fprintf(file, "  Avg Persistent bytes read: %.2f\n", (double)stats->per_total_bytes_read / count);
    fprintf(file, "  Avg Non-persistent bytes read: %.2f\n", (double)stats->nonper_total_bytes_read / count);
    fprintf(file, "  Avg Persistent ratio: %.2f\n", stats->persistent_ratio);
    fprintf(file, "  Avg Non-persistent ratio: %.2f\n", stats->non_persistent_ratio);
    fprintf(file, "  Avg Connection 2 duration: %.6f sec\n", stats->oneconn2_duration / count);
    fprintf(file, "  Avg Throughput Persistent: %.2f Mbps\n", stats->throughput_mbpsper);
    fprintf(file, "  Avg Throughput Non-Persistent: %.2f Mbps\n", stats->throughput_mbpsnonper);
    fprintf(file, "  Avg Persistent packets: %.2f\n", (double)stats->per_packets / count);
    fprintf(file, "  Avg Non-Persistent packets: %.2f\n", (double)stats->nonper_packets / count);
    fprintf(file, "\n");
}

void write_stats_to_file(ConnectionStats *stats_array, size_t count, int rejected, int msgs_per_conn, int run) {
    char filename[128];
    snprintf(filename, sizeof(filename), "stats_%d_%d.log", msgs_per_conn, run);

    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open stats log");
        return;
    }

    // Write summaries
    for (size_t i = 0; i < count; i++) {
        size_t start_conn = i * SUMMARY_INTERVAL;
        size_t end_conn = (i + 1) * SUMMARY_INTERVAL;
        if (end_conn > connection_count) end_conn = connection_count;
        write_summary_to_file(stats_array, i, start_conn, end_conn, file);
    }

    // Compute overall statistics
    double avg_persistent_ratio = all_both > 0 ? (double)all_persistent_count / all_both : 0.0;
    double avg_non_persistent_ratio = all_both > 0 ? (double)all_non_persistent_count / all_both : 0.0;
    double avg_non_persistent_count = connection_count > 0 ? (double)all_non_persistent_count / connection_count : 0.0;
    double avg_persistent_count = connection_count > 0 ? (double)all_persistent_count / connection_count : 0.0;
    double avg_total_when_both = connection_count > 0 ? (double)all_both / connection_count : 0.0;
    double avg_only_persistent = connection_count > 0 ? (double)(all_only_persistent_count - all_both) / connection_count : 0.0;
    double avg_only_non_persistent = connection_count > 0 ? (double)(all_only_non_persistent_count - all_both) / connection_count : 0.0;
    double avg_per_bytes = connection_count > 0 ? (double)all_per_bytes / connection_count : 0.0;
    double avg_nonper_bytes = connection_count > 0 ? (double)all_nonper_bytes / connection_count : 0.0;
    double avg_per_packets = connection_count > 0 ? (double)all_perpackets / connection_count : 0.0;
    double avg_nonper_packets = connection_count > 0 ? (double)all_nonpacktes / connection_count : 0.0;
    double oneconn1_duration = time_diff(conn1_start, conn1_end);
    double avg_conn2_duration = conn2_duration;
    double avg_throughput_mbpsper = oneconn1_duration > 0 ? (all_per_bytes * 8.0) / (oneconn1_duration * 1e6) : 0.0;
    double avg_throughput_mbpsnonper = conn2_duration > 0 ? (all_nonper_bytes * 8.0) / (conn2_duration * 1e6) : 0.0;

    // Write Overall Statistics
    fprintf(file, "\nOverall Statistics (Averaged):\n");
    fprintf(file, "Average events when both were present: %.2f\n", avg_total_when_both);
    fprintf(file, "Average Persistent count: %.2f\n", avg_persistent_count);
    fprintf(file, "Average Non-Persistent count: %.2f\n", avg_non_persistent_count);
    fprintf(file, "Average Persistent Ratio: %.2f\n", avg_persistent_ratio);
    fprintf(file, "Average Non-Persistent Ratio: %.2f\n", avg_non_persistent_ratio);
    fprintf(file, "Rejected connections: %d\n", rejected);
    fprintf(file, "Average Only Persistent count: %.2f\n", avg_only_persistent);
    fprintf(file, "Average Only Non Persistent count: %.2f\n", avg_only_non_persistent);
    fprintf(file, "Average persistent bytes per connection: %.2f\n", avg_per_bytes);
    fprintf(file, "Average non-persistent bytes per connection: %.2f\n", avg_nonper_bytes);
    fprintf(file, "Average persistent packets per connection: %.2f\n", avg_per_packets);
    fprintf(file, "Average non-persistent packets per connection: %.2f\n", avg_nonper_packets);
    fprintf(file, "Total persistent duration: %.6f sec\n", oneconn1_duration);
    fprintf(file, "Total non-persistent duration: %.6f sec\n", avg_conn2_duration);
    fprintf(file, "Average Throughput Persistent: %.2f Mbps\n", avg_throughput_mbpsper);
    fprintf(file, "Average Throughput Non-Persistent: %.2f Mbps\n", avg_throughput_mbpsnonper);

    fflush(file);
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <MSGS_PER_CONN> <run_number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int msgs_per_conn = atoi(argv[1]);
    int run = atoi(argv[2]);

    // Register signal handlers for SIGINT and SIGTERM
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    increase_fd_limit();
    set_thread_affinity(0);

    stats_array = malloc(sizeof(ConnectionStats) * MAX_CONNECTIONS / SUMMARY_INTERVAL);
    if (!stats_array) {
        perror("Failed to allocate memory for stats_array");
        exit(EXIT_FAILURE);
    }

    int server_fd, epoll_fd;
    struct sockaddr_in server_addr;
    struct epoll_event ev, events[MAX_EVENTS];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        free(stats_array);
        exit(EXIT_FAILURE);
    }
    optimize_socket(server_fd);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        free(stats_array);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3000) < 0) {
        perror("Listen failed");
        close(server_fd);
        free(stats_array);
        exit(EXIT_FAILURE);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(server_fd);
        free(stats_array);
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl ADD server_fd");
        close(server_fd);
        close(epoll_fd);
        free(stats_array);
        exit(EXIT_FAILURE);
    }

    int persistent_fd = -1, non_persistent_fd = -1;
    struct timeval non_persistent_connect_time = {0};
    int conn1_started = 0, conn2_started = 0;
    int both = 0, pick = -1, active_clients = 0;
    unsigned long onepertotal_count = 0, onenonpertotal_count = 0;
    uint64_t per_bytes_read = 0, nonper_bytes_read = 0;
    uint64_t per_packets = 0, nonper_packets = 0;
    unsigned long non_persistent_count = 0, persistent_count = 0;
    ConnectionStats temp_stats = {0};

    gettimeofday(&last_event_time, NULL);

    while (running) {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        double no_event_duration = time_diff(last_event_time, current_time);

        if (event_count == 0) {
            if (no_event_duration >= NO_EVENT_TIMEOUT) {
                running = 0;
                continue;
            }
        } else {
            gettimeofday(&last_event_time, NULL);
        }

        if (non_persistent_fd != -1) {
            double inactive_duration = time_diff(non_persistent_connect_time, current_time);
            if (inactive_duration >= INACTIVE_TIMEOUT && nonper_bytes_read == 0) {
                active_clients--;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, non_persistent_fd, NULL);
                shutdown(non_persistent_fd, SHUT_RDWR);
                close(non_persistent_fd);

                double oneconn2_duration = time_diff(conn2_start, conn2_end);
                conn2_duration += oneconn2_duration;
                connection_count++;

                // Update temp_stats
                temp_stats.non_persistent_count += non_persistent_count;
                temp_stats.persistent_count += persistent_count;
                temp_stats.total_when_both_present += both;
                temp_stats.only_persistent_count += onepertotal_count - both;
                temp_stats.only_non_persistent_count += onenonpertotal_count - both;
                temp_stats.per_total_bytes_read += per_bytes_read;
                temp_stats.nonper_total_bytes_read += nonper_bytes_read;
                temp_stats.oneconn2_duration += oneconn2_duration;
                temp_stats.per_packets += per_packets;
                temp_stats.nonper_packets += nonper_packets;
                double total = both;
                temp_stats.persistent_ratio = total > 0 ? (double)persistent_count / total : 0.0;
                temp_stats.non_persistent_ratio = total > 0 ? (double)non_persistent_count / total : 0.0;
                temp_stats.throughput_mbpsper = oneconn2_duration > 0 ? (per_bytes_read * 8.0) / (oneconn2_duration * 1e6) : 0.0;
                temp_stats.throughput_mbpsnonper = oneconn2_duration > 0 ? (nonper_bytes_read * 8.0) / (oneconn2_duration * 1e6) : 0.0;

                // Check if we've reached SUMMARY_INTERVAL
                if (connection_count % SUMMARY_INTERVAL == 0 && stats_count < MAX_CONNECTIONS / SUMMARY_INTERVAL) {
                    stats_array[stats_count++] = temp_stats;
                    memset(&temp_stats, 0, sizeof(ConnectionStats));
                }

                non_persistent_fd = -1;
                non_persistent_count = 0;
                persistent_count = 0;
                both = 0;
                onepertotal_count = 0;
                onenonpertotal_count = 0;
                per_bytes_read = 0;
                nonper_bytes_read = 0;
                per_packets = 0;
                nonper_packets = 0;
                conn2_started = 0;

                if (queue_count > 0) {
                    non_persistent_fd = non_persistent_queue[0].fd;
                    non_persistent_connect_time = non_persistent_queue[0].connect_time;
                    for (int j = 0; j < queue_count - 1; j++) {
                        non_persistent_queue[j] = non_persistent_queue[j + 1];
                    }
                    queue_count--;
                    nonper_bytes_read = 0;
                    nonper_packets = 0;
                    ev.events = EPOLLIN;
                    ev.data.fd = non_persistent_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, non_persistent_fd, &ev) == -1) {
                        perror("epoll_ctl ADD queued non_persistent_fd");
                        close(non_persistent_fd);
                        active_clients--;
                        non_persistent_fd = -1;
                        continue;
                    }
                }
            }
        }

        for (int i = 0; i < event_count; i++) {
            int event_fd = events[i].data.fd;

            if (event_fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("Accept failed");
                    continue;
                }
                active_clients++;
                optimize_socket(client_fd);

                struct timeval connect_time;
                gettimeofday(&connect_time, NULL);

                if (persistent_fd == -1) {
                    persistent_fd = client_fd;
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        perror("epoll_ctl ADD client_fd");
                        close(client_fd);
                        active_clients--;
                        continue;
                    }
                } else if (non_persistent_fd == -1) {
                    non_persistent_fd = client_fd;
                    non_persistent_connect_time = connect_time;
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        perror("epoll_ctl ADD client_fd");
                        close(client_fd);
                        active_clients--;
                        continue;
                    }
                } else if (queue_count < MAX_NON_PERSISTENT) {
                    non_persistent_queue[queue_count].fd = client_fd;
                    non_persistent_queue[queue_count].connect_time = connect_time;
                    queue_count++;
                } else {
                    printf("Rejecting connection: queue full and persistent/non-persistent active\n");
                    close(client_fd);
                    active_clients--;
                    rejected++;
                }
            } else {
                char buffer[BUFFER_SIZE];
                int bytes_read = recv(event_fd, buffer, BUFFER_SIZE, 0);

                if (bytes_read > 0) {
                    if (event_fd == persistent_fd && buffer[0] != 'P') {
                        if (non_persistent_fd == -1) {
                            non_persistent_fd = persistent_fd;
                            non_persistent_connect_time = conn1_start;
                            persistent_fd = -1;
                            conn1_started = 0;
                            nonper_bytes_read = per_bytes_read;
                            nonper_packets = per_packets;
                            all_nonper_bytes += per_bytes_read;
                            all_nonpacktes += per_packets;
                            per_bytes_read = 0;
                            per_packets = 0;
                        } else if (queue_count < MAX_NON_PERSISTENT) {
                            non_persistent_queue[queue_count].fd = persistent_fd;
                            non_persistent_queue[queue_count].connect_time = conn1_start;
                            queue_count++;
                            persistent_fd = -1;
                            conn1_started = 0;
                            all_nonper_bytes += per_bytes_read;
                            all_nonpacktes += per_packets;
                            per_bytes_read = 0;
                            per_packets = 0;
                        } else {
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_fd, NULL);
                            shutdown(event_fd, SHUT_RDWR);
                            close(event_fd);
                            active_clients--;
                            persistent_fd = -1;
                            conn1_started = 0;
                            rejected++;
                            continue;
                        }
                    } else if (event_fd == non_persistent_fd && buffer[0] == 'P') {
                        // Handle misidentified non-persistent connection
                    }

                    if (event_fd == persistent_fd) {
                        if (!conn1_started) {
                            gettimeofday(&conn1_start, NULL);
                            conn1_started = 1;
                        }
                        gettimeofday(&conn1_end, NULL);
                        per_bytes_read += bytes_read;
                        all_per_bytes += bytes_read;
                        per_packets += bytes_read / PACKET_SIZE;
                        all_perpackets += bytes_read / PACKET_SIZE;
                        onepertotal_count++;
                        all_only_persistent_count++;

                        if (persistent_fd != -1 && non_persistent_fd != -1) {
                            if (pick == -1) {
                                pick = persistent_fd;
                            } else if (pick == non_persistent_fd) {
                                non_persistent_count++;
                                all_non_persistent_count++;
                                pick = -1;
                                both++;
                                all_both++;
                            }
                        }
                    } else if (event_fd == non_persistent_fd) {
                        if (!conn2_started) {
                            gettimeofday(&conn2_start, NULL);
                            conn2_started = 1;
                        }
                        gettimeofday(&conn2_end, NULL);
                        nonper_bytes_read += bytes_read;
                        all_nonper_bytes += bytes_read;
                        nonper_packets += bytes_read / PACKET_SIZE;
                        all_nonpacktes += bytes_read / PACKET_SIZE;
                        onenonpertotal_count++;
                        all_only_non_persistent_count++;

                        if (persistent_fd != -1 && non_persistent_fd != -1) {
                            if (pick == -1) {
                                pick = non_persistent_fd;
                            } else if (pick == persistent_fd) {
                                persistent_count++;
                                all_persistent_count++;
                                pick = -1;
                                both++;
                                all_both++;
                            }
                        }
                    }
                } else if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    active_clients--;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_fd, NULL);
                    shutdown(event_fd, SHUT_RDWR);
                    close(event_fd);

                    if (event_fd == persistent_fd) {
                        gettimeofday(&conn1_end, NULL);
                        persistent_fd = -1;
                        conn1_started = 0;
                    } else if (event_fd == non_persistent_fd) {
                        double oneconn2_duration = time_diff(conn2_start, conn2_end);
                        conn2_duration += oneconn2_duration;
                        connection_count++;

                        // Update temp_stats
                        temp_stats.non_persistent_count += non_persistent_count;
                        temp_stats.persistent_count += persistent_count;
                        temp_stats.total_when_both_present += both;
                        temp_stats.only_persistent_count += onepertotal_count - both;
                        temp_stats.only_non_persistent_count += onenonpertotal_count - both;
                        temp_stats.per_total_bytes_read += per_bytes_read;
                        temp_stats.nonper_total_bytes_read += nonper_bytes_read;
                        temp_stats.oneconn2_duration += oneconn2_duration;
                        temp_stats.per_packets += per_packets;
                        temp_stats.nonper_packets += nonper_packets;
                        double total = both;
                        temp_stats.persistent_ratio = total > 0 ? (double)persistent_count / total : 0.0;
                        temp_stats.non_persistent_ratio = total > 0 ? (double)non_persistent_count / total : 0.0;
                        temp_stats.throughput_mbpsper = oneconn2_duration > 0 ? (per_bytes_read * 8.0) / (oneconn2_duration * 1e6) : 0.0;
                        temp_stats.throughput_mbpsnonper = oneconn2_duration > 0 ? (nonper_bytes_read * 8.0) / (oneconn2_duration * 1e6) : 0.0;

                        // Check if we've reached SUMMARY_INTERVAL
                        if (connection_count % SUMMARY_INTERVAL == 0 && stats_count < MAX_CONNECTIONS / SUMMARY_INTERVAL) {
                            stats_array[stats_count++] = temp_stats;
                            memset(&temp_stats, 0, sizeof(ConnectionStats));
                        }

                        non_persistent_fd = -1;
                        non_persistent_count = 0;
                        persistent_count = 0;
                        both = 0;
                        onepertotal_count = 0;
                        onenonpertotal_count = 0;
                        per_bytes_read = 0;
                        nonper_bytes_read = 0;
                        per_packets = 0;
                        nonper_packets = 0;
                        conn2_started = 0;

                        if (queue_count > 0) {
                            non_persistent_fd = non_persistent_queue[0].fd;
                            non_persistent_connect_time = non_persistent_queue[0].connect_time;
                            for (int j = 0; j < queue_count - 1; j++) {
                                non_persistent_queue[j] = non_persistent_queue[j + 1];
                            }
                            queue_count--;
                            nonper_bytes_read = 0;
                            nonper_packets = 0;
                            ev.events = EPOLLIN;
                            ev.data.fd = non_persistent_fd;
                            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, non_persistent_fd, &ev) == -1) {
                                perror("epoll_ctl ADD queued non_persistent_fd");
                                close(non_persistent_fd);
                                active_clients--;
                                non_persistent_fd = -1;
                                continue;
                            }
                        }
                    }

                    if (active_clients == 0 && queue_count == 0) {
                        running = 0;
                    }
                }
            }
        }
    }

    // Save any remaining stats
    if (connection_count % SUMMARY_INTERVAL != 0 && stats_count < MAX_CONNECTIONS / SUMMARY_INTERVAL && temp_stats.total_when_both_present > 0) {
        stats_array[stats_count++] = temp_stats;
    }

    // Close any remaining queued connections
    for (int i = 0; i < queue_count; i++) {
        close(non_persistent_queue[i].fd);
        active_clients--;
    }
    queue_count = 0;

    printf("Final stats_count: %zu\n", stats_count);
    write_stats_to_file(stats_array, stats_count, rejected, msgs_per_conn, run);
    free(stats_array);
    close(server_fd);
    close(epoll_fd);
    return 0;
}