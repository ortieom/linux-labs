#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    FILE *file;
    unsigned char buffer[2];
    long offset = 0x159e;
    
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    
    file = fopen(argv[1], "r+b");
    if (file == NULL) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1;
    }
    
    // seek to the address
    if (fseek(file, offset, SEEK_SET) != 0) {
        printf("Error: Could not seek to address 0x%lx\n", offset);
        fclose(file);
        return 1;
    }
    
    // read current bytes to verify
    if (fread(buffer, 1, 2, file) != 2) {
        printf("Error: Could not read bytes at address 0x%lx\n", offset);
        fclose(file);
        return 1;
    }
    
    // verify current instruction is JNZ (0x75 0x07)
    if (buffer[0] != 0x75 || buffer[1] != 0x07) {
        printf("Error: Expected JNZ (75 07) at address 0x%lx, but found %02x %02x\n", 
               offset, buffer[0], buffer[1]);
        fclose(file);
        return 1;
        }
    
    // go back
    if (fseek(file, offset, SEEK_SET) != 0) {
        printf("Error: Could not seek back to address 0x%lx\n", offset);
        fclose(file);
        return 1;
    }
    
    // write new instruction JZ
    buffer[0] = 0x74;
    buffer[1] = 0x07;
    if (fwrite(buffer, 1, 2, file) != 2) {
        printf("Error: Could not write bytes at address 0x%lx\n", offset);
        fclose(file);
        return 1;
    }
    
    fclose(file);
    
    printf("Successfully patched %s\n", argv[1]);
    
    return 0;
}
