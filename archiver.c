#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <windows.h>
#include <io.h>

// https://www.fileformat.info/format/arc/corion.htm
// http://www.textfiles.com/programming/FORMATS/arc_fmts.txt

char* ARCHIVE;
char COMMAND;
char** FILES;
int COUNT_FILES;
int HEADER_SIZE = 29;

void parse_args(int count, char** args) { 
    int i = 1;

    while (i < count) {
        if (strcmp(args[i], "--file") == 0) {
            ARCHIVE = args[++i];
        } else if (strcmp(args[i], "--create") == 0) {
            COMMAND = 'c';
        } else if (strcmp(args[i], "--extract") == 0) {
            COMMAND = 'e';
        } else if (strcmp(args[i], "--list") == 0) {
            COMMAND = 'l';
        } else {
            COUNT_FILES = count - i;
            FILES = args + i;
            i = count;
        }

        ++i;
    }
}

int get_file_creation_date(char* file) {
    WIN32_FILE_ATTRIBUTE_DATA info;
    SYSTEMTIME T;
    GetFileAttributesExA(file, GetFileExInfoStandard, &info);
	FileTimeToSystemTime(&info.ftCreationTime, &T);
    int year = T.wYear - 1980;
    int month = T.wMonth;
    int day = T.wDay;
    int hour = T.wHour;
    int minute = T.wMinute;
    int second_2 = T.wSecond / 2;
    int date = 0;

    date |= year << 25;
    date |= month << 21;
    date |= day << 16;
    date |= hour << 11;
    date |= minute << 5;
    date |= second_2;

    return date;
}

void write_number(int n, FILE* file, int number) {
    for (int i = n - 1; i >= 0; --i) {
        int j = (number & (0b11111111 << (8 * i))) >> (8 * i);
        fprintf(file, "%c", j);
    }
}

int read_number(int n, FILE* file) {
    int number = 0;

    for (int i = n - 1; i >= 0; --i) {
        number |= fgetc(file) << (8 * i);
    }

    return number;
}

void read_file_name(char* file_name, FILE* archive) {
    fseek(archive, 2, SEEK_CUR);
    fread(file_name, 1, 13, archive);
    file_name[13] = '\0';
}

void advance_pointer_to_next_file(FILE* archive) {
    int file_size = read_number(4, archive);
    fseek(archive, 10 + file_size, SEEK_CUR);
}

void write_file_name(char* file, FILE* archive) {
    fprintf(archive, "%s", file);  // Filename

    for (int i = 0; i < 13 - strlen(file); ++i) {
        fprintf(archive, "%c", 0);  // Filename padding
    }
}

void write_file(char* file, FILE* archive) {
    FILE* f = fopen(file, "rb");
    fseek(f, 0L, SEEK_END);  // get file size
    long sz = ftell(f);  // for the binary files ftell returns # of bytes from the beginning of the file

    fprintf(archive, "%c", 0x1a);  // Header Flag
    fprintf(archive, "%c", 0);  // Compression method

    write_file_name(file, archive);  // Filename

    write_number(4, archive, sz);  // Compressed file size
    write_number(4, archive, get_file_creation_date(file));  // File date in MS-DOS format
    write_number(4, archive, sz);  // Original file size
    write_number(2, archive, 0);  // Skip CRC-16
    
    fseek(f, 0L, SEEK_SET);
    printf("%d\n", sz);

    for (int i = 0; i < sz; ++i) {
        char c = fgetc(f);
        fprintf(archive, "%c", c);  // File Contents 
    }

    fclose(f);
}

void create_archive() {
    FILE* archive = fopen(ARCHIVE, "wb");

    for (int i = 0; i < COUNT_FILES; ++i) {
        write_file(FILES[i], archive);
    }

    fclose(archive);
}

void list_archive() {
    printf("===Archived files===\n");
    FILE* archive = fopen(ARCHIVE, "rb");

    while (fgetc(archive) != EOF) {        
        char file_name[14];
        fseek(archive, -1, SEEK_CUR);
        read_file_name(file_name, archive);
        printf("%s\n", file_name);
        advance_pointer_to_next_file(archive);
    }

    fclose(archive);
}

void extract_archive() {
    FILE* archive = fopen(ARCHIVE, "rb");

    while (fgetc(archive) != EOF) {
        char file_name[14];
        fseek(archive, -1, SEEK_CUR);
        read_file_name(file_name, archive);
        int file_size = read_number(4, archive);
        int date = read_number(4, archive);
        fseek(archive, 6, SEEK_CUR);

        FILE* file = fopen(file_name, "wb");
        // set file date
        HANDLE handle = (HANDLE)_get_osfhandle(_fileno(file));
        SYSTEMTIME system_time;
        FILETIME file_time;
        system_time.wYear = (date >> 25) + 1980;
        system_time.wMonth = (date >> 21) & 0b1111;
        system_time.wDay = (date >> 16) & 0b11111;
        system_time.wHour = (date >> 11) & 0b11111;
        system_time.wMinute = (date >> 5) & 0b111111;
        system_time.wSecond = (date & 0b11111) * 2;
        system_time.wMilliseconds = 0;

        SystemTimeToFileTime(&system_time, &file_time);
        SetFileTime(handle, &file_time, &file_time, &file_time);
    
        CloseHandle(handle);
        fclose(file);
        
        file = fopen(file_name, "wb");
        for (int i = 0; i < file_size; ++i) {
            fprintf(file, "%c", fgetc(archive));
        }

        fclose(file);
    }

    fclose(archive);
}

int main(int argc, char* argv[]) {
    parse_args(argc, argv);

    if (COMMAND == 'c') {
        create_archive();
    } else if (COMMAND == 'e') {
        extract_archive();
    } else if (COMMAND == 'l') {
        list_archive();
    }

    return 0;
}