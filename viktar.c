#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <zlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "viktar.h"

#define SIZE 80
#define BUFFER_SIZE 80

char * find_perm(int user_set, int group_set, int user_sticky_bit, int group_sticky_bit, int perm_num);

int main(int argc, char * argv[]) {
    int opt;
    int fd = STDIN_FILENO;
    char * file_name = NULL;
    char buff[100];
    int extract = 0;
    int create = 0;
    int verbose = 0;
    int long_table = 0;
    int short_table = 0;
    int valid = 0;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
            case 'V':
                valid = 1;
                break;
            case 'x':
                extract = 1;
                break;
            case 'c':
                fd = STDOUT_FILENO;
                create = 1;
                break;
            case 't':
                short_table = 1;
                break;
            case 'T':
                long_table = 1;
                break;
            case 'f':
                file_name = optarg;
                break;
            case 'h':
                printf("help text\n");
                printf("\t./viktar\n");
                printf("\tOptions: xctTf:Vhv\n"
                       "\t\t-x\t\textract file/files from archive\n"
                       "\t\t-c\t\tcreate an archive file\n"
                       "\t\t-t\t\tdisplay a short table of contents of the archive file\n"
                       "\t\t-T\t\tdisplay a long table of contents of the archive file\n"
                       "\t\tOnly one of xctTV can be specified\n"
                       "\t\t-f filename\tuse filename as the archive file\n"
                       "\t\t-V\t\tvalidate the crc values in the viktar file\n"
                       "\t\t-v\t\tgive verbose diagnostic messages\n"
                       "\t\t-h\t\tdisplay this AMAZING help message\n");
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                verbose = 1;
                break;
            default:
                fprintf(stderr, "Sign not recognized\n");
                exit(EXIT_FAILURE);
                break;
        }
    }

    if (verbose == 1) {
        fprintf(stderr, "Verbose level: 1\n");
    }

    // If the create option is selected
    if (create == 1) { 
        int nfd = STDOUT_FILENO; // First initialize the new file descriptor to standard output
        struct stat stat_data; // then the structure to hold file metadata
        uint32_t crc32_data; // variable to hold the CRC32 checksum
        unsigned char buffer[100]; // Buffer to hold file data for reading and writing
        ssize_t bytes = 0; // Variable to hold the number of bytes read

        viktar_header_t header; // Declare a header variable to store file metadata
        viktar_footer_t footer; // Declare a footer variable to store CRC data

        if (file_name != NULL) { // Check if a file name is provided
            nfd = open(file_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // Open the file with write-only, truncate if it exists, create if it doesn't, and set permissions
            if (nfd < 0) { // Check if opening the file failed
                perror("Failed to open output file"); // Print error message
                exit(EXIT_FAILURE); // Exit the program with failure status
            }
        }

        if (write(nfd, VIKTAR_TAG, strlen(VIKTAR_TAG)) == -1) { // Write the VIKTAR_TAG to the output file and check for errors
            perror("Write error"); // Print error message
            exit(EXIT_FAILURE); // Exit the program with failure status
        }

        if (optind < argc) { // check if there are non-option arguments (files) to process
            for (int i = optind; i < argc; i++) { // loop through each non-option argument (file)
                if ((fd = open(argv[i], O_RDONLY)) > 0) { // open the file in read-only mode and check if it was successful
                    if (stat(argv[i], &stat_data) == -1) { // retrieve file metadata and check for errors
                        perror("open error"); // Print error message
                        return 1; // Return with error status
                    }

                    strncpy(header.viktar_name, argv[i], VIKTAR_MAX_FILE_NAME_LEN); // Copy the file name to the header
                    header.viktar_name[VIKTAR_MAX_FILE_NAME_LEN] = '\0'; // Ensure null-termination of the file name
                    header.st_size = stat_data.st_size; // Set the file size in the header
                    header.st_mode = stat_data.st_mode; // Set the file mode in the header
                    header.st_uid = stat_data.st_uid; // Set the user ID in the header
                    header.st_gid = stat_data.st_gid; // Set the group ID in the header
                    header.st_mtim = stat_data.st_mtim; // Set the modification time in the header
                    header.st_atim = stat_data.st_atim; // Set the access time in the header
                    write(nfd, &header, sizeof(viktar_header_t)); // Write the header to the output file
                    memset(buffer, 0, 100); // clear the buffer
                    crc32_data = crc32(0x0, Z_NULL, 0); // Initialize the CRC32 checksum
                    while ((bytes = read(fd, buffer, 90)) > 0) { // Loop to read the file data
                        write(nfd, buffer, bytes); // wite the read data to the output file
                        crc32_data = crc32(crc32_data, buffer, bytes); // update the CRC32 checksum
                    }

                    footer.crc32_data = crc32_data; // Set the CRC32 checksum in the footer
                    write(nfd, &footer, sizeof(viktar_footer_t)); // Write the footer to the output file
                    close(fd); // Close the input file descriptor
                }
            }
        }
        return EXIT_SUCCESS; 
    }

    if (file_name == NULL) { // if no file name provided
        fprintf(stderr, "reading archive from stdin\n");
        file_name = STDIN_FILENO;
    }
    if (verbose == 1) {
        fprintf(stderr, "reading archive file: \"%s\"\n", file_name);
    }

    if (file_name != NULL) {
        fd = open(file_name, O_RDONLY);
    }
    if (fd < 0) { // if opening a file failed
        fprintf(stderr, "not a viktar file: \"%s\"\n", file_name == NULL ? "stdin" : file_name);
        fprintf(stderr, "          this is vik-terrible\n");
        fprintf(stderr, "          exiting...\n");
        exit(EXIT_FAILURE);
    }

    memset(buff, 0, 100);
    read(fd, buff, strlen(VIKTAR_TAG));
    if (strncmp(buff, VIKTAR_TAG, strlen(VIKTAR_TAG)) != 0) {
        if (file_name != NULL) {
            fprintf(stderr, "reading archive file: \"%s\"\n", file_name);
        }
        fprintf(stderr, "not a viktar file: \"%s\"\n", file_name == NULL ? "stdin" : file_name);
        fprintf(stderr, "\tthis is vik-terrible\n");
        fprintf(stderr, "\texiting...\n");
        exit(1);
    }

    // If the extract option is selected
    if (extract == 1) { 
        viktar_header_t header; // Declare a header variable to store file metadata
        viktar_footer_t footer; // Declare a footer variable to store CRC data
        struct stat stat_data; // Structure to hold file metadata
        struct timespec restore_time[2]; // Array to hold the access and modification times for restoring

        char buffer[100]; // Buffer to hold the file name
        unsigned char file_data[100]; // Buffer to hold file data for reading and writing
        int nfd = STDOUT_FILENO; // Initialize the new file descriptor to standard output
        unsigned char *arr; // Pointer for dynamically allocated memory for file data
        uInt crc32_data; // Variable to hold the CRC32 checksum
        ssize_t bytes_read = 0; // Variable to hold the number of bytes read

        umask(0); // Set the file mode creation mask to zero
        while ((bytes_read = read(fd, &header, sizeof(viktar_header_t))) > 0) { // Loop to read and process each file's header
            memset(buffer, 0, 100); // Clear the buffer
            strncpy(buffer, header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN); // Copy the file name from the header to the buffer
            nfd = open(buffer, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); // Open the file with write-only, truncate if it exists, create if it doesn't, and set permissions
            stat(header.viktar_name, &stat_data); // Retrieve the status of the file specified in the header

            memset(file_data, 0, 100); // Clear the file data buffer
            crc32_data = crc32(0x0, Z_NULL, 0); // Initialize the CRC32 checksum
            arr = (unsigned char*) malloc(header.st_size * sizeof(char)); // Allocate memory for the file data
            bytes_read = read(fd, arr, header.st_size); // Read the file data into the allocated memory
            restore_time[0] = header.st_atim; // Store the access time from the header
            restore_time[1] = header.st_mtim; // Store the modification time from the header
            write(nfd, arr, header.st_size); // Write the file data to the newly created file

            fchmod(nfd, header.st_mode); // Set the file permissions for the new file according to the mode stored in the header
            crc32_data = crc32(crc32_data, arr, bytes_read); // Update the CRC32 checksum with the data that was just written
            read(fd, &footer, sizeof(viktar_footer_t)); // Read the footer from the archive
            futimens(nfd, restore_time); // Restore the access and modification times for the new file
            free(arr); // Free the allocated memory
            if (footer.crc32_data != crc32_data) { // Check if the CRC32 checksums match
                fprintf(stderr, "*** CRC32 failure data: %s  in file: 0x%08x  extract: 0x%08x ***\n", header.viktar_name, footer.crc32_data, crc32_data); // Print error message if checksums do not match
            }
        }
        if (file_name != NULL) { // Check if a file name was provided
            close(fd); // Close the input file descriptor
            close(nfd); // Close the output file descriptor
        }
        return EXIT_SUCCESS; 
    }

     if (long_table == 1) { 
    viktar_header_t header; // Declare a header variable to store file metadata
    viktar_footer_t footer; // Declare a footer variable to store CRC data

    struct group *grp; // Declare a group structure to store group info
    struct passwd *pws; // Declare a password structure to store user info

    char buffer[100]; // Buffer to hold the file name
    unsigned int user_perm = 0; // Variable to store user permissions
    unsigned int group_perm = 0; // Variable to store group permissions
    unsigned int other_perm = 0; // Variable to store other permissions

    char time_mod[SIZE]; // Buffer to hold the modification time as a string
    char time_acc[SIZE]; // Buffer to hold the access time as a string
    char perm_header = '-'; // Character to hold the permission header
    char * user_mode = NULL; // String to store user permission representation
    char * group_mode = NULL; // String to store group permission representation
    char * other_mode = NULL; // String to store other permission representation

    uint32_t crc32_header; // Variable to hold the CRC32 checksum for the header

    fprintf(stderr, "reading archive file: \"%s\"\n", file_name); // Print the archive file being read to stderr
    printf("Contents of viktar file: \"%s\"\n", file_name); // Print the archive file name
    printf("\n"); // Print a newline for formatting

    while ((read(fd, &header, sizeof(viktar_header_t))) > 0) { // Loop to read and process each file's header
        time_t sec = header.st_atim.tv_sec; // Get the access time in seconds
        time_t sec1 = header.st_mtim.tv_sec; // Get the modification time in seconds

        struct tm timeinfo; // Structure to hold formatted access time
        struct tm timeinfo1; // Structure to hold formatted modification time

        int user_set = 0; // Variable to indicate if user set UID bit is set
        int group_set = 0; // Variable to indicate if group set GID bit is set
        int user_sticky_bit = 0; // Variable to indicate if user sticky bit is set
        int group_sticky_bit = 0; // Variable to indicate if group sticky bit is set

        localtime_r(&sec, &timeinfo); // Convert access time to local time format
        strftime(time_acc, sizeof(time_acc), "%Y-%m-%d %H:%M:%S %Z ", &timeinfo); // Format access time as a string

        localtime_r(&sec1, &timeinfo1); // Convert modification time to local time format
        strftime(time_mod, sizeof(time_mod), "%Y-%m-%d %H:%M:%S %Z ", &timeinfo1); // Format modification time as a string

        memset(buffer, 0, 100); // Clear the buffer
        strncpy(buffer, header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN); // Copy the file name from the header to the buffer
        printf("\tfile name: %s\n", buffer); // Print the file name

        user_perm = (header.st_mode % 512) / 64; // Calculate user permissions
        group_perm = (header.st_mode % 64) / 8; // Calculate group permissions
        other_perm = (header.st_mode % 8); // Calculate other permissions

        if (header.st_mode & S_ISUID) { // Check if the user set UID bit is set
            user_set = 1; // Set the user_set flag
            switch (header.st_mode & S_IFMT) { // Check the file type
                case S_IFREG: // If it's a regular file
                    if (user_mode != NULL && (user_mode[2] == 'x' || user_perm == 1 || user_perm == 3 || user_perm == 5 || user_perm == 7)) { // Check for executable permissions
                        user_sticky_bit = 0; // Clear the user sticky bit
                    }
                    break;
            }
        }

        if (header.st_mode & S_ISGID) { // Check if the group set GID bit is set
            group_set = 1; // Set the group_set flag
            switch (header.st_mode & S_IFMT) { // Check the file type
                case S_IFREG: // If it's a regular file
                    if (group_mode != NULL && (group_mode[2] == 'x' || group_perm == 1 || group_perm == 3 || group_perm == 5 || group_perm == 7)) { // Check for executable permissions
                        group_sticky_bit = 0; // Clear the group sticky bit
                    }
                    break;
            }
        }

        fprintf(stderr, "%d: %s %d %d %d\n", __LINE__, buffer, user_sticky_bit, user_set, group_set); // Print debugging information

        user_mode = find_perm(user_set, group_set, user_sticky_bit, group_sticky_bit, user_perm); // Get user permissions string
        if (user_set == 1) { // Check if the user_set flag is set
            user_set = 0; // Clear the user_set flag
            user_sticky_bit = 0; // Clear the user sticky bit
        }
        group_mode = find_perm(user_set, group_set, user_sticky_bit, group_sticky_bit, group_perm); // Get group permissions string
        if (group_set == 1) { // Check if the group_set flag is set
            group_set = 0; // Clear the group_set flag
            group_sticky_bit = 0; // Clear the group sticky bit
        }
        other_mode = find_perm(user_set, group_set, user_sticky_bit, group_sticky_bit, other_perm); // Get other permissions string

        printf("\t\tmode:         %c%s%s%s\n", perm_header, user_mode, group_mode, other_mode); // Print the permissions
        if ((pws = getpwuid(header.st_uid)) != NULL) { // Get user info based on user ID and check if it is found
            printf("\t\tuser:         %-8.8s\n", pws->pw_name); // Print the user name
        }
        if ((grp = getgrgid(header.st_gid)) != NULL) { // Get group info based on group ID and check if it is found
            printf("\t\tgroup:        %-8.8s\n", grp->gr_name); // Print the group name
        }
        printf("\t\tsize:         %ld\n", header.st_size); // Print the file size

        printf("\t\tmtime:        %s\n", time_mod); // Print the modification time
        printf("\t\tatime:        %s\n", time_acc); // Print the access time

        crc32_header = crc32(0L, Z_NULL, 0); // Initialize CRC32 calculation
        crc32_header = crc32(crc32_header, (const unsigned char *)&header, sizeof(viktar_header_t)); // Calculate CRC32 for the header
        printf("\t\tcrc32 header: 0x%08x\n", crc32_header); // Print the CRC32 checksum for the header

        lseek(fd, header.st_size, SEEK_CUR); // Move the file pointer past the file data
        read(fd, &footer, sizeof(viktar_footer_t)); // Read the footer data
        printf("\t\tcrc32 data:   0x%08x\n", footer.crc32_data); // Print the CRC32 checksum for the file data
    }
    if (file_name != NULL) { // Check if a file name was provided
        close(fd); // Close the input file descriptor
    }
    return EXIT_SUCCESS;	
	return 0;
    }

    if (short_table == 1) {
        viktar_header_t header;
        char buffer[100];
        fprintf(stderr, "Short table of contents\n");
        printf("Contents of viktar file: %s\n", file_name);
        while ((read(fd, &header, sizeof(viktar_header_t))) > 0) {
            memset(buffer, 0, 100);
            strncpy(buffer, header.viktar_name, VIKTAR_MAX_FILE_NAME_LEN);
            printf("\tfile name: %s\n", buffer);
            lseek(fd, header.st_size + sizeof(viktar_footer_t), SEEK_CUR);
        }
        if (file_name != NULL) {
            close(fd);
        }
        return EXIT_SUCCESS;
    }
    if (file_name != NULL) {
        close(fd);
    }

    if (valid == 1) {
        // Still havent implemented
    }

    return 0;
}

char * find_perm(int user_set, int group_set, int user_sticky_bit, int group_sticky_bit, int perm_num) {
    switch (perm_num) {
        case 0:
            if (user_set == 1 || group_set == 1)
                return "--S";
            return "---";
        case 1:
            if (user_sticky_bit == 1 || group_sticky_bit == 1)
                return "--s";
            if (user_set == 1 || group_set == 1)
                return "--S";
            return "--x";
        case 2:
            if (user_set == 1 || group_set == 1)
                return "-wS";
            return "-w-";
        case 3:
            if (user_sticky_bit == 1 || group_sticky_bit == 1)
                return "-ws";
            if (user_set == 1 || group_set == 1)
                return "-wS";
            return "-wx";
        case 4:
            if (user_set == 1 || group_set == 1)
                return "r-S";
            return "r--";
        case 5:
            if (user_sticky_bit == 1 || group_sticky_bit == 1)
                return "r-s";
            if (user_set == 1 || group_set == 1)
                return "r-S";
            return "r-x";
        case 6:
            if (user_set == 1 || group_set == 1)
                return "rwS";
            return "rw-";
        case 7:
            if (user_sticky_bit == 1 || group_sticky_bit == 1)
                return "rws";
            if (user_set == 1 || group_set == 1)
                return "rwS";
            return "rwx";
    }
    return NULL;
}