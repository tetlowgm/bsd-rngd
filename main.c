/*-
 *  Copyright (c) 2017, W. Dean Freeman
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

#define DEF_BYTES	16
#define DEF_SECS	2

static volatile sig_atomic_t wantdie = 0;

static void
usage(void)
{
	(void)fprintf(stderr,"%s\n",
		      "usage: bsdrngd [-dh] [-b bytes] [-s sleeptime] /dev/devicename");
	exit(EX_USAGE);
}

static void
dodie(int signo)
{

        wantdie = signo;
}

/* read entropy from the trng device */
static void
read_entropy(char *dev, char *buf, uint32_t n)
{
	ssize_t rv = 0;
	int fd = open(dev, O_RDONLY);

	if ( fd < 0 )
	{
		syslog(LOG_ERR, "Unable to open device %s for writing: %s",
		    dev, strerror(errno));
	       	exit(-1);
	}
	flock(fd, LOCK_EX);
	rv = read(fd,buf,n);
	if ( rv < 0 ) 
	{
		syslog(LOG_ERR, "Error reading bytes from entropy source: %s",
		    strerror(errno));
		exit(-1);
	}
	flock(fd, LOCK_UN);
	close(fd);
}

static void
write_entropy(char *buf, int n)
{	
	ssize_t rv = 0;
	int fd = open("/dev/random", O_WRONLY);

	if ( fd < 0 )
	{
		syslog(LOG_ERR, "Unable to open /dev/random for writing: %s",
		    strerror(errno));
		exit(-1);
	}

	if ( rv < 0 ) 
	{
		syslog(LOG_ERR, "Unable to write to /dev/random: %s",
		    strerror(errno));
		exit(-1);
	}
	close(fd);
}

/* main daemon child loop */
static void
entropy_feed(char *dev, uint32_t n, uint32_t s)
{
	char buf[n];

	syslog(LOG_NOTICE,
	    "bsd-rngd: entropy gathering daemon started for device %s", dev);
	explicit_bzero(buf,n);
	/* main loop to do the thing */
	for(;;)
	{
		if (wantdie)
			return;
		read_entropy(dev,buf,n);
		write_entropy(buf,n);
		explicit_bzero(buf,n);
		sleep(s);
	}
}

int
main(int argc, char *argv[])
{
	int ch, bflag, dflag, iflag;
	struct pidfh *pfh;
	pid_t spid;
	char *device;

	bflag = DEF_BYTES;
	dflag = 0;
	iflag = DEF_SECS;

	while((ch = getopt(argc, argv, "b:dhi:")) != -1)
		switch(ch) {
		case 'b':
			bflag = atoi(optarg);
			break;
		case 'd':
			dflag = 1;
			break;
		case 'i':
			iflag = atoi(optarg);
			break;
		case 'h':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	device = argv[0];
	printf("device: %s\n", device);

	pfh = pidfile_open(NULL, 0600, &spid);
	if (pfh == NULL) {
		if (errno == EEXIST)
			errx(EX_OSFILE, "Daemon already running, pid: %d",
			    spid);
		warn("Cannot open or create pidfile");
	}

	if ((dflag == 1) && (daemon(0, 0) == -1))
	{
		pidfile_remove(pfh);
		err(EX_OSERR, "Cannot daemonize");
	}

	(void)signal(SIGTERM, dodie);

	pidfile_write(pfh);

	/* get to doing work */
	entropy_feed(device, bflag, iflag);

	pidfile_remove(pfh);
	return EXIT_SUCCESS;
}
