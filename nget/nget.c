/**
 * @brief   Retrieve file from network endpoint
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license GPL v. 3, see LICENSE for details.
 */

#include "fujicom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>

/* buffers for commands like OPEN are expected to be 256 bytes */
unsigned char url[256];

/* large byte buffer used for copy. */
unsigned char buf[8192];

/* Status structure */
struct _status
{
	unsigned short bw;	 /* # of bytes waiting 0-65535 	*/
	unsigned char connected; /* Are we connected? 		*/
	unsigned char error;	 /* error code	1-255		*/
} status;

/**
 * @brief main nget function
 * @param src string ptr to the N: source URL
 * @param dst string ptr to the destination filename
 * @return The exit error code 0 = success
 */
int nget(char *src, char *dst)
{
	FILE *fp = fopen(dst,"wb");
	int err  = 0;
	unsigned long total=0;
	char *username=NULL, *password=NULL;

	if ((src[0] != 'N') && (src[1] != ':'))
	{
		printf("\nInvalid N: URL. Terminating.\n");
		return 1;
	}

	if (!fp)
	{
		printf("\nCould not open destination file. Terminating.\n");
		return 2;
	}

	username=getenv("FUJI_USER");
	password=getenv("FUJI_PASS");

	if (username)
	{
		memset(url,0,sizeof(url));
		strcpy(url,username);

		/* Perform username command */
		fujiF5_write(DEVICEID_FN_NETWORK, CMD_USERNAME, 0, 0, &url, sizeof(url));
	}

	if (password)
	{
		memset(url,0,sizeof(url));
		strcpy(url,password);

		/* Perform password command */
		fujiF5_write(DEVICEID_FN_NETWORK, CMD_PASSWORD, 0, 0, &url, sizeof(url));
	}

	memset(url,0,sizeof(url));
	strcpy(url,src);

	/* Perform OPEN command */
	// FIXME - define constants:
	//r.h.dl   = 0x04; /* READ ONLY */
	//r.h.dh   = 0x00; /* NO TRANSLATION */
	fujiF5_write(DEVICEID_FN_NETWORK, CMD_OPEN, 0x0004, 0, &url, sizeof(url));

 	delay(10);

	/* Perform initial status command */
	fujiF5_read(DEVICEID_FN_NETWORK, CMD_STATUS, 0, 0, &status, sizeof(status));

	if (status.error > 1 && !status.bw)
	{
		printf("\nOPEN ERROR: %u\n",status.error);
		return status.error;
	}

	/* if (!status.connected)
	{
		printf("\nOPEN ERROR: Host immediately disconnected.\n");
		return 0xFF;
	} */

	while (1)
	{
		int bw = (status.bw > sizeof(buf) ? sizeof(buf) : status.bw);
		char reply = 0;

		if (!bw && status.error == 136)
			break;
		else if (!bw)
			continue;

		delay(1);

		/* Do read */
		reply = fujiF5_read(DEVICEID_FN_NETWORK, CMD_READ, bw, 0, &buf, bw);

		if (reply != 'C')
		{
		 	printf("\nREAD ERROR AT %lu bytes. Reply was %c\n",total,reply);
		 	return 144;
		}

		/* Do write */
		fwrite(buf,bw,1,fp);

		total += bw;

		printf("%10lu bytes transferred.\r",total);
		fflush(stdout);

		delay(1);

		/* Do next status */
		fujiF5_read(DEVICEID_FN_NETWORK, CMD_STATUS, 0, 0, &status, sizeof(status));
	}

	/* Perform CLOSE command */
	fujiF5_none(DEVICEID_FN_NETWORK, CMD_CLOSE, 0, 0, NULL, 0);

	fclose(fp);
	
	return err;
}

/**
 * @brief show usage if incorrect # of parameters.
 */
void usage()
{
	printf("\nNGET <N:url> <fn>\n");
}

/**
 * @brief Main program entry
 */
int main(int argc, char *argv[])
{
	if (argc<3)
	{
		char src[256];
		char dst[24];

		memset(src,0,sizeof(src));
		memset(dst,0,sizeof(dst));

		if (!argv[1])
		{
			printf("SOURCE URL:\n");
			gets(src);

			if (!src[0])
				return 1;

			printf("DEST FILE:\n");
			gets(dst);

			if (!src[1])
				return 1;

			return nget(src,dst);
		}
	}

	return nget(argv[1],argv[2]);
}
