#include "nfs4_internal.h"
// #include "nfs4.h"
// #include "nfs4xdr.h"
#include <string.h>

void fh_str(char* dst, struct file* f)
{
    for (int i = 0; i < NFS4_FHSIZE; i++) {
        sprintf(&(dst[2*i]), "%02x", f->filehandle[i]);
    }
    dst[2*NFS4_FHSIZE] = 0;
}

void test_opens()
{
    heap h = 0;
    client c;
    file f, jf;
    status s;
    char buff[1000];
    char data[4096];
    char* hostname_str = "192.168.1.36";
    char* filename_str = "tpcc-nfs";
    char* journal_filename_str = "tpcc-nfs-journal";

    s = create_client(hostname_str, &c);
    if (s != STATUS_OK) {
        exit(1);
    }

    vector filename = allocate_buffer(h, strlen(filename_str) + 1);
    push_bytes(filename, filename_str, strlen(filename_str));
    printf("have length of filename %d\n", strlen(filename_str));
    vector path = allocate_buffer(h, 10);
    vector_push(path, filename);

    s = file_create(c, path, &f);
    if (s != STATUS_OK) {
        exit(1);
    }

    fh_str(buff, f);
    printf("created file with handle %s on file %p fh %p\n", buff, f, f->filehandle);

    // s = readfile(f, data, 0, 40);
    s = readfile(f, data, 0, 4096);
    if (s != STATUS_OK) {
        exit(1);
    }

    // journal file with open_read
    vector journal_filename = allocate_buffer(h, strlen(journal_filename_str) + 1);
    push_bytes(journal_filename, journal_filename_str, strlen(journal_filename_str));
    printf("have length of filename %d\n", strlen(journal_filename_str));
    vector journal_path = allocate_buffer(h, 10);
    vector_push(journal_path, journal_filename);

    s = file_open_read(c, journal_path, &jf);
    if (s != STATUS_OK) {
        exit(1);
    }

    s = readfile(jf, data, 0, 1);
    if (s != STATUS_OK) {
        exit(1);
    }

    file_close(f);
    file_close(jf);
}

void test_locking()
{
    heap h = 0;
    client c;
    file f, jf;
    status s;
    char buff[1000];
    char data[4096];
    char* hostname_str = "192.168.1.36";
    char* filename_str = "tpcc-nfs";
    char* journal_filename_str = "tpcc-nfs-journal";

    s = create_client(hostname_str, &c);
    if (s != STATUS_OK) {
        exit(1);
    }

    vector filename = allocate_buffer(h, strlen(filename_str) + 1);
    push_bytes(filename, filename_str, strlen(filename_str));
    printf("have length of filename %d\n", strlen(filename_str));
    vector path = allocate_buffer(h, 10);
    vector_push(path, filename);

    printf("file open write\n");
    s = file_open_write(c, path, &f);
    if (s != STATUS_OK) {
        exit(1);
    }

    // printf("confirm open\n");
    // s = confirm_open(f);
    // if (s != STATUS_OK) {
    //     exit(1);
    // }

    // s = readfile(f, data, 0, 1);
    // if (s != STATUS_OK) {
    //     exit(1);
    // }

    printf("lock\n");
    s = lock_range(f, READ_LT, 0, 1000);

    printf("unlock\n");
    s = unlock_range(f, READ_LT, 0, 1000);


    file_close(f);
}

void test_double_locking()
{
    heap h = 0;
    client c;
    file f, jf;
    status s;
    char buff[1000];
    char data[4096];
    char* hostname_str = "192.168.1.36";
    char* filename_str = "tpcc-nfs";
    char* journal_filename_str = "tpcc-nfs-journal";

    s = create_client(hostname_str, &c);
    if (s != STATUS_OK) {
        exit(1);
    }

    vector filename = allocate_buffer(h, strlen(filename_str) + 1);
    push_bytes(filename, filename_str, strlen(filename_str));
    printf("have length of filename %d\n", strlen(filename_str));
    vector path = allocate_buffer(h, 10);
    vector_push(path, filename);

    printf("file open write\n");
    s = file_open_write(c, path, &f);
    if (s != STATUS_OK) {
        exit(1);
    }

    // printf("confirm open\n");
    // s = confirm_open(f);
    // if (s != STATUS_OK) {
    //     exit(1);
    // }

    // s = readfile(f, data, 0, 1);
    // if (s != STATUS_OK) {
    //     exit(1);
    // }

    printf("lock\n");
    s = lock_range(f, READ_LT, 0, 1000);

    printf("lock again\n");
    s = lock_range(f, READ_LT, 0, 1000);

    printf("unlock\n");
    s = unlock_range(f, READ_LT, 0, 1000);


    printf("unlock again\n");
    s = unlock_range(f, READ_LT, 0, 1000);

    file_close(f);
}
int main(int argc, char* argv[])
{
    // test_opens();
    // test_locking();
    test_double_locking();
}
