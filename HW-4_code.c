#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>

#define NUM_THREADS 3
#define NUM_RANDOMS 500
#define MAX_NUM 1000
#define CHILD_THREADS 10
#define NUM_READS 150

// Global variables
int pipe_fd[2]; // here are the  file descriptors for the pipe
pthread_mutex_t pipe_mutex; // here is the mutex for synchronization
volatile sig_atomic_t start_processing = 0; // This is the signal flag for child process

// Function for the signal handler
void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        start_processing = 1;
    }
}

// Function to setup signal handling
void setup_signal_handling() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
}

// function for the thread function for generating random numbers in the parent process
void* generate_numbers(void* arg) {
    int thread_id = *(int*)arg;
    free(arg); // This will free allocated memory for thread ID
    printf("Thread %d generating numbers...\n", thread_id);

    for (int i = 0; i < NUM_RANDOMS; i++) {
        int random_num = rand() % (MAX_NUM + 1);

        pthread_mutex_lock(&pipe_mutex); // This will lock the mutex for thread-safe pipe write
        if (write(pipe_fd[1], &random_num, sizeof(random_num)) == -1) {
            perror("Error writing to pipe");
        }
        pthread_mutex_unlock(&pipe_mutex); // This will unlock the mutex
    }

    pthread_exit(NULL);
}

// This is the parent process: creates threads and writes data to the pipe
void parent_process() {
    pthread_t threads[NUM_THREADS];
    pthread_mutex_init(&pipe_mutex, NULL);

    // Let's create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        int* thread_id = malloc(sizeof(int));
        *thread_id = i + 1;
        pthread_create(&threads[i], NULL, generate_numbers, thread_id);
    }

    // Let's wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(pipe_fd[1]); // This will close the write end of the pipe
    pthread_mutex_destroy(&pipe_mutex);
    printf("Parent process finished generating numbers.\n");
}

// This is the thread function for processing numbers in the child process
void* process_numbers(void* arg) {
    int thread_id = *(int*)arg;
    free(arg); // This will free allocated memory for thread ID
    int sum = 0, num;

    printf("Child Thread %d reading numbers...\n", thread_id);
    for (int i = 0; i < NUM_READS; i++) {
        if (read(pipe_fd[0], &num, sizeof(num)) > 0) {
            sum += num;
        } else {
            break; // This will exit loop if no more data is available
        }
    }

    int* result = malloc(sizeof(int));
    *result = sum;
    pthread_exit(result);
}

// Child process: creates threads and reads data from the pipe
void child_process() {
    pthread_t threads[CHILD_THREADS];
    int* thread_results[CHILD_THREADS];
    int total_sum = 0;

    // Create threads
    for (int i = 0; i < CHILD_THREADS; i++) {
        int* thread_id = malloc(sizeof(int));
        *thread_id = i + 1;
        pthread_create(&threads[i], NULL, process_numbers, thread_id);
    }

    // Collect thread results
    for (int i = 0; i < CHILD_THREADS; i++) {
        void* result;
        pthread_join(threads[i], &result);
        thread_results[i] = (int*)result;
        total_sum += *thread_results[i];
        free(thread_results[i]); // Free result memory
    }

    double average = total_sum / (double)(CHILD_THREADS * NUM_READS);

    // Write the average to a file
    FILE* output_file = fopen("output.txt", "w");
    if (output_file) {
        fprintf(output_file, "Average of sums: %.2f\n", average);
        fclose(output_file);
    } else {
        perror("Error opening output file");
    }

    close(pipe_fd[0]); // Close read end of the pipe
    printf("Child process finished processing numbers.\n");
}

// Main function to combine everything
int main() {
    // Create a pipe
    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Child process
        close(pipe_fd[1]); // Close write end
        setup_signal_handling();
        printf("Child waiting for signal...\n");

        while (!start_processing); // Wait for signal
        child_process();
        exit(EXIT_SUCCESS);
    } else { // Parent process
        close(pipe_fd[0]); // Close read end
        parent_process();

        printf("Sending signal to child process...\n");
        kill(pid, SIGUSR1); // Send signal
        wait(NULL); // Wait for child to finish
    }

    return 0;
}
