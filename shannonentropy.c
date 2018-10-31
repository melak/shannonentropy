/*
 * Copyright (c) 2018 Tamas Tevesz <ice@extreme.hu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * cc -Wall -Wextra -W -o shannonentropy shannonentropy.c -lm
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HISTOGRAM_ITEM_COUNT	256

#define MAPSIZE(mapsize, flen, fpos)	(				\
						mapsize > flen - fpos ?	\
							flen - fpos :	\
							mapsize		\
					)

extern char *__progname;
static __attribute__((noreturn)) void usage(void);

static __attribute__((noreturn)) void usage(void)
{
	fprintf(stderr, "Usage: %s file\n", __progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int fd, ret, pagesize;
	char *p;
	off_t flen, fpos, idx, mapsize, maplength;
	struct stat st;
	double histogram[HISTOGRAM_ITEM_COUNT];
	double entropy;

	fd = -1;
	flen = 0;
	fpos = 0;
	p = NULL;
	ret = 1;

	pagesize = (int)sysconf(_SC_PAGESIZE);
	if(pagesize == -1)
	{
		warn("sysconf(_SC_PAGESIZE)");
		mapsize = 4096 * 1024;
	}
	else
	{
		mapsize = pagesize * 1024;
	}

	if(argc != 2)
	{
		usage();
	}

	memset(histogram, 0, sizeof(histogram));

	fd = open(argv[1], O_RDONLY|O_NONBLOCK);
	if(fd == -1)
	{
		warn("open: %s", argv[1]);
		goto out;
	}

	if(fstat(fd, &st) == -1)
	{
		warn("stat: %s", argv[1]);
		goto out;
	}

	flen = st.st_size;
	if(flen == 0)
	{
		goto out;
	}

	maplength = MAPSIZE(mapsize, flen, fpos);
	p = mmap(NULL, maplength, PROT_READ, MAP_PRIVATE, fd, fpos);
	if(p == MAP_FAILED)
	{
		warn("mmap: %s", argv[1]);
		goto out;
	}

	ret = 0;
	idx = 0;

	while (fpos < flen)
	{
		uint8_t byte = (uint8_t)(p[idx++] & 0xff);
		histogram[ byte ] += 1.0;
		fpos++;

		if(idx >= maplength)
		{
			munmap(p, maplength);
			p = NULL;

			if(fpos == flen)
			{
				break;
			}

			maplength = MAPSIZE(mapsize, flen, fpos);
			p = mmap(NULL, maplength, PROT_READ, MAP_PRIVATE, fd, fpos);

			if(p == MAP_FAILED)
			{
				warn("mmap: %s", argv[1]);
				goto out;
			}

			idx = 0;
		}
	}

	entropy = 0.0;
	for(idx = 0; idx < HISTOGRAM_ITEM_COUNT; idx++)
	{
		if(histogram[idx] > 0)
		{
			entropy -= (histogram[idx] / flen) *
				    log2(histogram[idx] / flen);
		}
	}

	if(isatty(STDOUT_FILENO))
	{
		printf("File: %s\n", argv[1]);
		printf("Shannon entropy: %.16f\n", entropy);
	}
	else
	{
		printf("%s|%.16f\n", argv[1], entropy);
	}

out:
	if(p)
	{
		munmap(p, flen);
	}

	if(fd > -1)
	{
		close(fd);
	}

	return ret;
}
