
/**puts frame around a bmp file
 * bmp pixels ordered as (abgr) in file. 
*/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef union {
    uint32_t pixel;
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a; /* a or x in argb, xrgb   */
    } __attribute__((packed));
} Pixel;
/*or 
char pixel[]={0,0,0,0}; //{b,g,r,a}
*/

/* NOT USED: struct type for image data*/
typedef struct image {
    int offset;     /* starting address in image file.              */
    int headersize; /* header size                                  */
    int width;      /* image width in pixels                        */
    int height;     /* image height in pixels                       */
    int nplanes;    /* number of color planes-1                     */
    int nbits;      /* 32-24-16 etc                                 */
    Pixel pixels[]; /* pointer to image, remaining part of bmp data */
} __attribute__((packed)) Image;


uint32_t FRAMECOLOR = 0 << 24 | 255 << 16 | 0 << 8 | 0; /*a-r-g-b*/
#define FRAMESIZE 50

int printpixel(Pixel p) {
    printf("r:%x-g:%x-b:%x-a:%x ", p.r, p.g, p.b, p.a);
    return 0;
}

void copy_bmp_withframe(FILE *infile, FILE *outfile, uint32_t offset,
                        uint32_t width, uint32_t height) {
    /*storage for the data upto offset of image data*/
    uint8_t *fheader = (uint8_t *)malloc(sizeof(uint8_t) * offset);

    /*rewind to the beginning*/
    rewind(infile);
    /*read and write the header upto offset*/
    fread(fheader, 1, offset, infile);
    fwrite(fheader, 1, offset, outfile);
    Pixel p; /*storage for a pixel data*/
    p.pixel = 0;
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            fread(&p.pixel, 4, 1, infile);

            /*write the pixel to outfile */
            if (j < FRAMESIZE || i < FRAMESIZE || i > height - FRAMESIZE ||
                j > width - FRAMESIZE) {
                fwrite(&FRAMECOLOR, 4, 1, outfile);
            } else {
                fwrite(&p.b, 1, 1, outfile);
                fwrite(&p.g, 1, 1, outfile);
                fwrite(&p.r, 1, 1, outfile);
                fwrite(&p.a, 1, 1, outfile);
            }
        }
    }
}
int main() {
    Pixel p; /*storage for a pixel data*/
    uint32_t offset, hsize, width, height;
    uint16_t nplanes, nbits;
    char *fname = "cute-animals32bit.bmp";
    char *fname2 = "framed.bmp";
    FILE *infile = fopen(fname, "rb");
    FILE *outfile = fopen(fname2, "wb");

    if (infile == NULL) {
        perror("cute animals not found!");
        return 0;
    }

    if (outfile == NULL) {
        perror("outfile not found!");
        return 0;
    }

    p.pixel = 0x12345678;
    printf("\n\nsee integer little endian!!!\n");
    /* integer little endian: p.b = 78, p.g = 56, p.r = 34, p.a = 12 */
    printpixel(p);

    fseek(infile, 10, SEEK_CUR);
    fread(&offset, 4, 1, infile);
    fread(&hsize, 4, 1, infile);
    fread(&width, 4, 1, infile);
    fread(&height, 4, 1, infile);
    fread(&nplanes, 2, 1, infile);
    fread(&nbits, 2, 1, infile);
    printf(
        "offset: %d, hsize: %d, \n"
        "width: %d, height: %d, nplanes: %d, bits: %d",
        offset, hsize, width, height, nplanes, nbits);

    copy_bmp_withframe(infile, outfile, offset, width, height);

    fclose(infile);
    fclose(outfile);
}