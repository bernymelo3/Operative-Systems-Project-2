#include "operations.h"
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


char sessions[NUMBER_OF_SESSIONS][MAX_PIPE_PATH];
int number_of_sessions = 0;
int client_pipe_g;
pthread_t contador;
int contador_int;


void contador_counter() {
    while(1) {
        printf("%d",contador_int);
        sleep(2);
    }

}


int existing_session(const char *client_pipe_path) {
    int current_session = 0; 
    //printf("cpipe->%d\n", sizeof(client_pipe_path));
    for(int i = 0; i < 50; i++) {
        //printf("spipe->%d\n", sizeof(sessions[i][0]));
        if (strcmp(sessions[i], client_pipe_path) == 0) { 
            current_session = i;
            return current_session;
            break;
        }
        current_session = -1;
    }
    return -1;
}

int create_session(const char *client_pipe_path) {
    int current_session;
    for (int i = 0; i < 50; i++) {
        if (sessions[i][0] == '\0') {
            memcpy(sessions[i], client_pipe_path, sizeof(client_pipe_path));
            current_session = i;
            number_of_sessions += 1;
            return current_session;
            break;
        }
    }
    return -1;
}


int tfs_mount(const char *client_pipe_path) {
    int current_session = 0;
    int client_pipe;
    if (number_of_sessions >= 0) {
        if (existing_session(client_pipe_path) != -1) {
            current_session = existing_session(client_pipe_path);
            printf("yes\n");
        }
        if (number_of_sessions <= NUMBER_OF_SESSIONS) {
            printf("nono\n");
            current_session = create_session(client_pipe_path);
            printf("current---->%d", current_session);
        }
    }

    
    client_pipe = open(client_pipe_path, O_WRONLY);
    if (client_pipe < 0) {
        return -1;
    }
    client_pipe_g = client_pipe;
    
    ssize_t w = write(client_pipe, &current_session, sizeof(int));
    if (w == -1) {
        exit(-1);
    }
    return current_session;
}

int tfs_unmount(int session_id) {
    int answer = 0;
    printf("session->%d\n", session_id);
    //printf("%s", sessions[session_id]);
    if (existing_session(sessions[session_id]) == -1) {
        answer = -1;
        if(write(client_pipe_g, &answer, sizeof(int)) < 0){
            return -1;
        }
    }
    else{
        memset(sessions[session_id], '\0', sizeof(sessions[session_id]));
        if(write(client_pipe_g, &answer, sizeof(int)) < 0){
            return -1;
        }
    }

    return 0;
}


int tfs_open_server(int session_id, int flags, char const *name){
    int answer = 0;

    

    if (tfs_open(name, flags) == -1){
        answer = -1;
        contador_int +=1;
        if(write(client_pipe_g, &answer, sizeof(int)) < 0){
            return -1;
        }

    }
    else{
        if(write(client_pipe_g, &answer, sizeof(int)) < 0){
            return -1;
        }
    }

    return answer;
}

int tfs_close_server(int session_id, int fhandle){
    int answer = 0;
    
    if (tfs_close(fhandle) == -1){
        answer = -1;
        contador_int -= 1;
        if(write(client_pipe_g, &answer, sizeof(int)) < 0){
            return -1;
        }
    }
    else{
        if(write(client_pipe_g, &answer, sizeof(int)) < 0){
            return -1;
        }
    }

    return answer;
}

ssize_t tfs_write_server(int fhandle, char const *buffer, size_t len){
    ssize_t answer = 0;
    if(tfs_write(fhandle, buffer, len) != -1) {
        ssize_t answer = tfs_write(fhandle, buffer, len);
    }
    else {
        answer = -1;
    }
  
    if(write(client_pipe_g, &answer, sizeof(ssize_t)) < 0){
        return -1;
    }

    return answer;
}


int tfs_read_server(int fhandle, size_t len){
    ssize_t answer = 0;
    int size = 0;
    void *buffer = malloc(len);         
    void *buffer_to_pipe = malloc(BUFFER_SIZE);    
    answer = tfs_read(fhandle, buffer, len);

    memcpy(buffer_to_pipe, answer, sizeof(ssize_t));
    size += sizeof(ssize_t);                              
    if(answer != -1){
        memcpy(buffer_to_pipe + size, buffer, answer);              
        size += answer;
    }

    free(buffer);

    if(write(client_pipe_g, buffer_to_pipe, size) < 0){       
        free(buffer_to_pipe);
        return -1;
    }
    free(buffer_to_pipe);
    return 0;
}



int main(int argc, char **argv) {
    int server_pipe;
    int op_code;
    ssize_t readed;
    char name[MAX_PIPE_PATH];
    int session_id = 0;
    char *client_pipe_path = malloc(MAX_PIPE_PATH);
    int flags;
    int fhandle;
    size_t wlen;
    size_t len;
    int sesssion_id;;
    char wbuffer[BUFFER_SIZE];
    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *server_pipe_path = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", server_pipe_path);
    
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 40; j++) {
            sessions[i][j] = '\0';
        }
    }
    

    
    if (unlink(server_pipe_path) != 0 && errno != ENOENT) {
       return -1;
    }

    if (mkfifo(server_pipe_path, 0640) != 0) {
        return -1;
    }

    server_pipe = open(server_pipe_path, O_RDONLY);
    pthread_create(&contador, NULL, &contador_counter, NULL);
    while(1) {

        if (server_pipe < 0) {
            return -1;
        }
        readed = read(server_pipe, &op_code, sizeof(int));

        if (readed == -1) {
            exit(-1);
        }
        
        switch(op_code) {
            case(TFS_OP_CODE_MOUNT):
                
                readed = read(server_pipe, client_pipe_path, 40);
                if (readed < 0) {
                    return -1;
                }
                
                session_id = tfs_mount(client_pipe_path);
                if(session_id < 0) {
                    return -1;
                }
            case(TFS_OP_CODE_UNMOUNT):
                
                readed = read(server_pipe, &sesssion_id, sizeof(int));
                if (readed < 0) {
                    return -1;
                }
                printf("session-id main --> %d\n",session_id);
                tfs_unmount(session_id);
                return 0;
            case(TFS_OP_CODE_OPEN):
                
                if(read(server_pipe, &session_id, 4) < 0){
                    return -1;
                }
                if(read(server_pipe, name, 40) < 0){
                    return -1;
                }
                if(read(server_pipe, &flags, 4) < 0){
                    return -1;
                }

                if(tfs_open_server(session_id, flags, name) <0){
                    return -1;
                }
                return 0;
                
            case(TFS_OP_CODE_CLOSE):
            
                
                if(read(server_pipe, &session_id, 4) < 0){
                    return -1;
                }
                if(read(server_pipe, &fhandle, 4) < 0){
                    return -1;
                }

                if(tfs_close_server(session_id, fhandle) < 0){
                    return -1;
                }

                return 0;
            
            case(TFS_OP_CODE_WRITE):
                printf("coco\n");
                if(read(server_pipe, &fhandle, sizeof(int)) < 0){
                    return -1;
                }
                printf("write ->%d\n", fhandle);


                if(read(server_pipe, &wlen, sizeof(size_t)) < 0){
                    return -1;
                }
                printf("write ->%ld\n", wlen);
                if(read(server_pipe, wbuffer, wlen*sizeof(char)) < 0){
                    return -1;
                }
                printf("write ->%d\n", tfs_write_server);
                if(tfs_write_server(fhandle, wbuffer, wlen) < 0){
                    return -1;
                }

                return 0;
                
            case(TFS_OP_CODE_READ):

                if(read(server_pipe, &session_id, 4) < 0){
                    return -1;
                }
                if(read(server_pipe, &fhandle, sizeof(int)) < 0){
                    return -1;
                }
                if(read(server_pipe, &len, sizeof(size_t)) < 0){
                    return -1;
                }

                if(tfs_read_server(fhandle, len) < 0){
                    return -1;
                }


            case(TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED):
                return 0;
            default:
                return -1;
        }
          
    }
    pthread_join(contador_counter, NULL);
    close(server_pipe);  
    return 0;
}