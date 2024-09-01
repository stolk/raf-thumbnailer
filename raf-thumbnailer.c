#include <sys/mman.h>
#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <strings.h>


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"


static int opt_verbose = 0;

static int opt_png = 0;

//
// Helper functions to read unsigned ints in lsb and msb formats.
//

uint32_t read_u32_motorola(const uint8_t* v)
{
	return
		v[0] * 0x1000000 +
		v[1] * 0x0010000 +
		v[2] * 0x0000100 +
		v[3] * 0x0000001;
}


uint32_t read_u32_intel(const uint8_t* v)
{
	return
		v[3] * 0x1000000 +
		v[2] * 0x0010000 +
		v[1] * 0x0000100 +
		v[0] * 0x0000001;
}


uint32_t read_u32(const uint8_t* v, int intel)
{
	return intel ? read_u32_intel(v) : read_u32_motorola(v);
}


uint16_t read_u16_motorola(const uint8_t* v)
{
	return
		v[0] * 0x0100 +
		v[1] * 0x0001;
}


uint16_t read_u16_intel(const uint8_t* v)
{
	return
		v[1] * 0x00100 +
		v[0] * 0x00001;
}


uint32_t read_u16(const uint8_t* v, int intel)
{
	return intel ? read_u16_intel(v) : read_u16_motorola(v);
}

//
// Process the jpeg thumbnail data.
//

int process_jpeg_thumbnail(const char* iname, const char* oname, const uint8_t* dat, size_t len)
{
	if (!opt_png)
	{
		// No conversion to png requested... just write out the jpeg to file.
		FILE* f = fopen(oname, "wb");
		if (!f)
		{
			fprintf(stderr, "Failed to open %s for writing: %s\n", oname, strerror(errno));
			return errno;
		}
		const size_t numwritten = fwrite(dat, len, 1, f);
		if (numwritten != 1)
		{
			fprintf(stderr, "Failed to write: %s\n", strerror(errno));
			return errno;
		}
		fclose(f);
		return 0;
	}

	// We need to convert this to png.
	int w=0, h=0;
	int numch=0;

	stbi_uc* im = stbi_load_from_memory(dat, len, &w, &h, &numch, 0);
	if (!im)
	{
		fprintf(stderr, "jpeg loading by STBI has failed.\n");
		return EINVAL;
	}

	const int wr = stbi_write_png(oname, w, h, numch, im, 0);
	if (!wr)
	{
		fprintf(stderr, "png writing to %s by STBI has failed.\n", oname);
		return EIO;
	}

	stbi_image_free(im);

	return 0;
}


//
// Processes fullsize jpeg data.
//

int process_jpeg(const char* iname, const char* oname, const uint8_t* dat, size_t len)
{
	if (dat[0] != 0xff || dat[1] != 0xd8)
	{
		fprintf(stderr, "jpeg in %s is not valid.\n", iname);
		return EINVAL;
	}

	const uint8_t* app_dat = dat + 2;
	if (app_dat[0] != 0xff || app_dat[1] != 0xe1)
	{
		fprintf(stderr, "jpeg in %s is missing APP1 marker.\n", iname);
		return EINVAL;
	}
	app_dat += 2;
	const uint16_t app1_sz = read_u16_motorola(app_dat);
	(void) app1_sz;

	if (app_dat[2] != 'E' || app_dat[3] != 'x' || app_dat[4] != 'i' || app_dat[5] != 'f')
	{
		fprintf(stderr, "File %s is missing EXIF tag, found %c%c%c%c instead.\n", iname, app_dat[2], app_dat[3], app_dat[4], app_dat[5]);
		return EINVAL;
	}

	const uint8_t* exif_body = app_dat + 8;

	const int intel = exif_body[0] == 'I' && exif_body[1] == 'I';
	const int motor = exif_body[0] == 'M' && exif_body[1] == 'M';

	if (!intel && !motor)
	{
		fprintf(stderr, "File %s is neither intel nor motorola byte-ordering?\n", iname);
		return EINVAL;
	}

	const uint32_t ifd0_off = read_u32(exif_body + 4, intel);

	const uint8_t* ifd0 = exif_body + ifd0_off;

	const uint16_t ifd0_num_entries = read_u16(ifd0, intel);

	ifd0 += 2;
	ifd0 += 12 * ifd0_num_entries;

	const uint32_t ifd1_off = read_u32(ifd0, intel);

	const uint8_t* ifd1 = exif_body + ifd1_off;
	const uint16_t ifd1_num_entries = read_u16(ifd1, intel);

	ifd1 += 2;

	const uint16_t TAG_JPEG_OFF = 0x201;
	const uint16_t TAG_JPEG_LEN = 0x202;

	uint32_t jpeg_off = 0;
	uint32_t jpeg_len = 0;
	for (int i=0; i<ifd1_num_entries; ++i)
	{
		const uint16_t tag = read_u16(ifd1, intel);
		if (tag == TAG_JPEG_OFF)
		{
			jpeg_off = read_u32(ifd1 + 8, intel);
		}
		if (tag == TAG_JPEG_LEN)
		{
			jpeg_len = read_u32(ifd1 + 8, intel);
		}
		ifd1 += 12;
	}
	if (!jpeg_off || !jpeg_len)
	{
		fprintf(stderr, "Could not find jpeg offset/size in IFD1 or APP1 of %s\n", iname);
		return EINVAL;
	}

	if (opt_verbose)
		fprintf(stderr, "Thumbnail of size %d is at offset %d\n", jpeg_len, jpeg_off);

	const uint8_t* thumb = exif_body + jpeg_off;

	return process_jpeg_thumbnail(iname, oname, thumb, jpeg_len);
}


//
// Processes RAF data.
// It locates an embedded (full size) JPEG and goes from there.
//

int process_raf(const char* iname, const char* oname, const uint8_t* dat)
{
	if (strncmp((const char*) dat, "FUJIFILMCCD-RAW", 15))
	{
		fprintf(stderr, "Image %s is not a RAF file.\n", iname);
		return EINVAL;
	}

	const uint8_t* camid = dat + 28;
	if (opt_verbose)
		fprintf(stderr, "%s image: %s\n", (const char*)camid, iname);

	const uint8_t* directory = camid + 32;

	if (opt_verbose)
		fprintf(stderr, "version %c%c%c%c\n", directory[0], directory[1], directory[2], directory[3]);

	const uint8_t* jpeg_offset = directory + 24;
	const uint8_t* jpeg_length = jpeg_offset + 4;

	const uint32_t joff = read_u32_motorola(jpeg_offset);
	const uint32_t jlen = read_u32_motorola(jpeg_length);

	if (opt_verbose)
		fprintf(stderr, "jpeg at offset %d, length %d\n", joff, jlen);

	const uint8_t* jpeg = dat + joff;

	process_jpeg(iname, oname, jpeg, jlen);

#if 0
	FILE* f = fopen("out.jpeg", "wb");
	fwrite(jpeg, jlen, 1, f);
	fclose(f);
#endif

	return 0;
}


//
// Processes a file.
// After verifying existance, its size is checked, and then memory-mapped.
//

int process(const char* iname, const char* oname)
{
	const int fd = open(iname, O_RDONLY);
	if (fd < 0)
	{
		fprintf(stderr, "failed to open for reading: %s (%s)\n", iname, strerror(errno));
		return errno;
	}

	struct stat sbuf;
	const int stat_res = fstat(fd, &sbuf);
	if (stat_res)
	{
		fprintf(stderr, "stat() failed: %s : %s\n", strerror(errno), iname);
		return errno;
	}

	void* adr = (uint8_t*)mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (adr == MAP_FAILED)
	{
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		return errno;
	}

	const int process_err = process_raf(iname, oname, (uint8_t*) adr);

	const int unmap_res = munmap(adr, sbuf.st_size);
	if (unmap_res < 0)
	{
		fprintf(stderr, "munmap() failed: %s\n", strerror(errno));
	}

	const int close_res = close(fd);
	if (close_res < 0)
	{
		fprintf(stderr, "close() failed %s\n", strerror(errno));
	}

	return process_err;
}


int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s IMAGE.RAF THUMB.JPEG\n", argv[0]);
		return 1;
	}

	opt_verbose = getenv("RAF_VERBOSE") ? 1 : 0;

	const char* iname = argv[1];
	const char* oname = argv[2];

	const char* dot = strrchr(oname, '.');
	const char* ext = (!dot || dot == iname) ? "" : dot+1;

	if (!strncasecmp(ext, "raf", 3))
	{
		fprintf(stderr, "Refusing to write jpg contents to a filename with raf extension: %s\n", oname);
		return 2;
	}

	opt_png = !strncasecmp(ext, "png", 3);

	process(iname, oname);
}

