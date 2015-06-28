/*
  This file will create a file of a given size.  It is incredibly
  stupid but is provided just in case you don't have "dd" on your
  system.

  Usage:  mkfile -m 64 big_file

          This will create a 64 megabyte file called "big_file".
  
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include <stdio.h>
#include <stdlib.h>


main(int argc, char **argv)
{
    int i,j,k;
    int block_size = 128 * 1024;
    int file_size  = 8 * 1024 * 1024;
    char *buff, *fname = "big_file";
    FILE *fp;

    for(i=1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0) {
            block_size = strtol(argv[++i], NULL, 0) * 1024;
        } else if (strcmp(argv[i], "-m") == 0) {
            file_size = strtol(argv[++i], NULL, 0) * 1024 * 1024;
        } else {
            fname = argv[i];
        }
    }

    buff = (char *)calloc(block_size, 1);
    if (buff == NULL) {
        fprintf(stderr, "can't allocate a %d k block!\n", block_size/1024);
        exit(5);
    }

    fp = fopen(fname, "wb");
    if (fp == NULL) {
        fprintf(stderr, "can't create %s!\n", fname);
        exit(5);
    }
    
    printf("creating %s @ %d megabytes in size %d k per chunk\n",
           fname, file_size / (1024*1024), block_size / 1024);


    for(i=0; i < file_size; i += block_size) {
        j = fwrite(buff, 1, block_size, fp);
        if (j != block_size) {
            fprintf(stderr, "error writing @ pos %d\n", i);
            free(buff);
            fclose(fp);
        }
    }

    free(buff);
    fclose(fp);
}
