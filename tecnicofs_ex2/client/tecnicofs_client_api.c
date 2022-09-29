#include "tecnicofs_client_api.h"

int session_id;
int server_pipe;
int client_pipe;


int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    char mount_request[BUFFER_SIZE]; 
    int op_code = TFS_OP_CODE_MOUNT;
    if (unlink(client_pipe_path) != 0 && errno != ENOENT) {
       return -1;
    }


    if (mkfifo(client_pipe_path, 0640) != 0) {
        return -1;
    }

    server_pipe = open(server_pipe_path, O_WRONLY);

    if(server_pipe < 0) {
        return -1;
    }

    memcpy(mount_request, &op_code, sizeof(int));
    memcpy(mount_request + 4, client_pipe_path, MAX_PIPE_PATH);
    ssize_t w = write(server_pipe, mount_request, sizeof(mount_request));
    if (w < 0) {
        return -1;
    }

    client_pipe = open(client_pipe_path, O_RDONLY);

    ssize_t r = read(client_pipe, &session_id, sizeof(int));
    if (r < 0) {
        return -1;
    }
    return 0;
}

int tfs_unmount() {
    void *unmount_request = malloc(BUFFER_SIZE);
    int op_code = TFS_OP_CODE_UNMOUNT;
    int answer = 0;
    int offset = 0;

    if(server_pipe < 0) {
        return -1;
    }
    memcpy(unmount_request, &op_code, sizeof(int));
    offset += sizeof(int);
    memcpy(unmount_request + offset, &session_id, sizeof(int));
  
    write(server_pipe, unmount_request, sizeof(unmount_request));
    ssize_t r = read(client_pipe, &answer, sizeof(int));
    if (r < 0) {
        return -1;
    }
    return answer;

}

int tfs_open(char const *name, int flags) {
    
    void *buffer = malloc(BUFFER_SIZE);
    int op_code = TFS_OP_CODE_OPEN;
    size_t offset = 0;
    int fhandle = 0;
    if(server_pipe < 0) {
        return -1;
    }

    memcpy(buffer + offset, &op_code, sizeof(int));
    offset += 4;
    memcpy(buffer + offset, &session_id, sizeof(int)); 
    offset += 4;
    memcpy(buffer + offset, name, 40);
    offset += 40;
    memcpy(buffer + offset, &flags, sizeof(int));
    offset += 4;
  
    if(write(server_pipe, buffer, offset) < 0){
        return -1;
    }
    
    if(read(client_pipe, &fhandle, sizeof(int)) < 0){
        return -1;
    }
    read(client_pipe, &fhandle, sizeof(int));
    free(buffer);
    
    return fhandle;
}

int tfs_close(int fhandle) {
    void *buffer = malloc(BUFFER_SIZE);
    int op_code = TFS_OP_CODE_CLOSE;
    size_t offset = 0;
    int answer;
    if(server_pipe < 0) {
        return -1;
    }

    memcpy(buffer + offset, &op_code, sizeof(int));
    offset += 4;
    memcpy(buffer + offset, &session_id, sizeof(int)); 
    offset += 4;
    memcpy(buffer + offset, &fhandle, sizeof(int));
    offset += 4;
  
    if(write(server_pipe, buffer, offset) < 0){
        return -1;
    }
    
    free(buffer);
    ssize_t r = read(client_pipe, &answer, sizeof(int));
    if (r < 0) {
        return -1;
    }
    return answer;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    
    char wbuffer[BUFFER_SIZE];
    int op_code = TFS_OP_CODE_WRITE;
    size_t offset = 0;
    ssize_t wr_bytes = 0;

    if(server_pipe < 0) {
        return -1;
    }
    memcpy(wbuffer, &op_code, sizeof(int));
    offset += 4;
    memcpy(wbuffer + offset, &fhandle, sizeof(int));
    offset += 4;
    memcpy(wbuffer + offset, &len, sizeof(size_t));
    offset += sizeof(size_t);
    memcpy(wbuffer + offset, buffer, len); 
    offset += len;
    
    
    if(write(server_pipe, wbuffer, sizeof(wbuffer)) < 0){
        return -1;
    }
    if (read(client_pipe, &wr_bytes, sizeof(ssize_t))<0) {
        return -1;
    }

    return wr_bytes;
    
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    void *rbuffer = malloc(BUFFER_SIZE);
    int op_code = TFS_OP_CODE_READ;
    int offset = 0;
    ssize_t answer;
    if(server_pipe < 0) {
        return -1;
    }

    memcpy(rbuffer + offset, &op_code, sizeof(int));
    offset += 4;
    memcpy(rbuffer + offset, &session_id, sizeof(int)); 
    offset += 4;
    memcpy(rbuffer, &fhandle, sizeof(int));
    offset += 4;
    memcpy(rbuffer + offset, &len, sizeof(size_t));
    offset += sizeof(size_t);

    if(write(server_pipe, rbuffer, offset) < 0){
        return -1;
    }

    if(read(server_pipe, &answer, sizeof(ssize_t)) < 0){
        return -1;
    }
    if(answer != -1){ 
        if(read(server_pipe, buffer, answer) < 0){       
        }
    }

    free(rbuffer);
    return answer;      
}    

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
