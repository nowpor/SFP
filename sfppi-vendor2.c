/*
*       sfppi-vendor2.c
*
* 	Author - eoinpk.ek@gmail.com
*	modify - nowpor@poczta.fm
*
*	To compile gcc -o sfppi-vendor sfppi-vendor.c -lwiringPi -lm
*	
*
*       sfppi-vendor is free software: you can redistribute it and/or modify
*       it under the terms of the GNU Lesser General Public License as
*       published by the Free Software Foundation, either version 3 of the
*       License, or (at your option) any later version.
*
*       sfppi-vendor is distributed in the hope that it will be useful,
*       but WITHOUT ANY WARRANTY; without even the implied warranty of
*       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*       GNU Lesser General Public License for more details.
*
*       You should have received a copy of the GNU Lesser General Public
*       License along with sfppi-generic.
*       If not, see <http://www.gnu.org/licenses/>.
***********************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef BEAGLEBONE
#include <wiringPi.h>
#include <wiringPiI2C.h>
#else
#include <linux/i2c-dev.h>
#include <fcntl.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <math.h>

#define VERSION 0.3

int mychecksum(unsigned char start_byte, unsigned char end_byte);
int dump(char *filename);
int readfromfile(char *filename);
int read_eeprom(unsigned char);
int dom(void);
int burnSFP(void);
int writeSFP(char *str);
int read_sfp(void);
int xio,write_checksum;
unsigned char A50[256]; //only interested in the first 128 bytes
unsigned char A51[256];
#ifdef BEAGLEBONE
char filenm[20]="/dev/i2c-1";
#endif

int main (int argc, char *argv[])
{
	int opt;
	write_checksum = 0;
	while ((opt = getopt(argc, argv, "rc:mf:d:")) !=-1) {
		switch (opt) {
		case 'r':
			if(!read_eeprom(0x50)); else {break; exit(EXIT_FAILURE);}
			read_sfp();
			break;
		case 'c':
			writeSFP(optarg);
			break;
		case 'm':
			dom();
			break;
		case 'd':
			dump(optarg);
			break; 
		case 'f':
			readfromfile(optarg);
			read_sfp();
			break;
		default: /* '?' */
			fprintf(stderr,
			"Usage: %s\n"
			"	-r read the sfp or sfp+\n"
			"	-c calculate and write Vendor put \'C\' if copy or filename\n"
			"	-m Print DOM values if SFP supports DOM\n"
			"	-d filename - dump the eprom data to a file (without extenssion)\n"
			"	-f filename - read data from file\n"
			,argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (argc <=1) {
			fprintf(stderr,
			"Usage: %s\n"
			"	-r read the sfp or sfp+\n"
			"	-c calculate and write Vendor, put \'C\' if copy or filename\n"
			"	-m Print DOM values if SFP supports DOM\n"
			"	-d filename - dump the eprom data to a file (without extenssion)\n"
			"       -f filename - read data from file\n"
			,argv[0]);
			exit(EXIT_FAILURE);
	}
}

int writeSFP(char *str)
{
	if((str[0]=='C' && str[1]==0)  || (str[0]=='c' && str[1]==0)) {
		printf("\nRead exist SFP\n");
		if(!read_eeprom(0x50)); else return 1;
	}else {
	 if(!readfromfile(str)); else return 1;
	}

 read_sfp();
 write_checksum = 1;
 burnSFP(); 
 return 0;
}
int read_sfp(void)
{
	unsigned char transceiver[8];//8 bytes - address 3 to 10
	unsigned char vendor[16+1]; //16 bytes - address 20 to 35
	unsigned char oui[3]; //3 bytes - address 37 to 39
	unsigned char partnumber[16+1]; //16 bytes - address 40 to 55
	unsigned char revision[4]; //4 bytes - address 56 - 59
	unsigned char serial[16+1]; //16 bytes - address 68 to 83
	unsigned char date[8+1];//8 bytes - address 84 to 91 
	unsigned char vendor_spec[16]; //16 bytes - address 99 to 114
	int cwdm_wave;
	static char *connector[16] = {
                "Unknown",
                "SC",
                "Fibre Channel Style 1 copper connector",
                "Fibre Channel Style 2 copper connector",
                "BNC/TNC",
                "Fibre Channel coaxial headers",
                "FiberJack",
                "LC",
                "MT-RJ",
                "MU",
                "SG",
                "Optical pigtail"};

	printf("SFPpi Version:%0.1f\n\n",VERSION);

	//print the connector type
	printf("Connector Type = %s",connector[A50[2]]);

	//print the transceiver type
	if(A50[6] & 16) {
		printf("\nTransceiver is 100Base FX");
	}
	else if(A50[6] & 8) {
		printf("\nTransceiver is 1000Base TX");
	}
	else if(A50[6] & 4) {
		printf("\nTransceiver is 1000Base CX");
	}
	else if(A50[6] & 2) {
		printf("\nTransceiver is 1000Base LX");
	}
	else if(A50[6] & 1) {
		printf("\nTransceiver is 1000Base SX");
	}
	else if(A50[3] & 64) {
		printf("\nTransceiver is 10GBase-ER");
	}
	else if(A50[3] & 32) {
		printf("\nTransceiver is 10GBase-LRM");
	}
	else if(A50[3] & 16) {
		printf("\nTransceiver is 10GBase-LR");
	}
	else if(A50[4] & 12) {
		printf("\nTransceiver is 10GBase-ZR");
	}
	else if(A50[3] & 8) {
		printf("\nTransceiver is 10GBase-SR");
	}
	else {
		printf("\nTransceiver is unknown");
	}

        //3 bytes.60 high order, 61 low order, byte 62 is the mantissa
        //print sfp wavelength 
        cwdm_wave = ((int) A50[60]<<8) | ((int) A50[61]);
        printf("\nWavelength = %d.%d",cwdm_wave,A50[62]);

	//print vendor id bytes 20 to 35
	memcpy(&vendor, &A50[20],16);
	vendor[16] = '\0';
	printf("\nVendor = %s",vendor);

	//Print partnumber values address 40 to 55  
	memcpy(&partnumber, &A50[40], 16);
	partnumber[16] = '\0';
	printf("\nPartnumber = %s", partnumber);

	//Print serial values address 68 to 83  
	memcpy(&serial, &A50[68], 16);
	serial[16] = '\0';
	printf("\nSerial = %s", serial);

	//Print date values address 84 to 91  
	memcpy(&date, &A50[84], 8);
	date[8] = '\0';
	printf("\ndate = %s", date);

	if(!write_checksum){
	//Calculate the checksum: Add up the first 31 bytes and store
	//the last 8 bits in the 32nd byte
	mychecksum(0x0, 0x3f);

	//Calculate the extended checksum: Add up 31 bytes and store
	//the last 8 bits in the 32nd byte
	mychecksum(0x40, 0x5f);
	}
	//If Digital Diagnostics is enabled and is Internally calibrated print
	//the DOM values.
	if (A50[92] & 0x60){
		dom();
	}
return 0;
}

int readfromfile(char *filename)
{
        FILE *fp;
	int i;
	unsigned char c;

	fp=fopen(filename,"r");
        if (fp == NULL) {
                fprintf (stderr, "File not exist, errno = %s\n",
                                 strerror (errno));
                return 1;
	}
	for(i=0;i<256;i++) A50[i]=0;	//initialize array
	
	i=0;
	while(i<0x80 && ((c=fgetc(fp))!=EOF)) A50[i++]=c; //read first 127 bytesfrom file

	fclose(fp);
	if (i<0x80) return 1;	//too small file
  return 0;
}

int dump(char *filename)
{
        int counter = 0x0;
        FILE *fp;
	char fileext[50];	


	//Copy eeprom SFP details into A50
	if(!read_eeprom(0x50)); else exit(EXIT_FAILURE);

        //save as hex
	strcpy(fileext,filename);
        strcat(fileext,".hex");         //filename with extenssions

	fp=fopen(fileext,"w");
        if (fp == NULL) {
                fprintf (stderr, "File not created okay, errno = %s\n",
				 strerror (errno));
                return 1;
        }
        printf ("Dump eeprom contents to %s\n",fileext) ;
        
	while (counter < 0x100){ //address 0 to 255
		if ((counter % 0x10) == 0){
                        fprintf(fp,"%02x:",counter);
                }
                fprintf(fp,"%02x",A50[counter]);
                counter++;
		if ((counter % 0x10) == 0) fprintf(fp,"\n");
        }
        fclose(fp);

	//save as bin
        strcpy(fileext,filename);
        strcat(fileext,".bin");         //filename with extenssions
        fp=fopen(fileext,"w");

	if (fp == NULL) {
                fprintf (stderr, "File not created okay, errno = %s\n",
                                 strerror (errno));
                return 1;
        }
        printf ("Dump eeprom contents to %s\n",fileext) ;

		for(counter=0;counter<0x100;counter ++) fputc(A50[counter], fp);
	fclose(fp);

	return 0;
}

int read_eeprom(unsigned char address)
{
	int xio,i,fd1;
#ifndef BEAGLEBONE
        xio = wiringPiI2CSetup (address);
	if (xio < 0){
                fprintf (stderr, "xio: Unable to initialise I2C: %s\n",
                                strerror (errno));
                return 1;
        }
#else
    char filenm[20]="/dev/i2c-1"; //i2c bus number must be modified accordingly

    xio = open(filenm, O_RDWR);
	if (xio < 0) {
	  fprintf (stderr, "Unable to open device: %s\n", filenm);
	  return(1);
	}

	if (ioctl(xio, I2C_SLAVE, address) < 0) {
  	    fprintf (stderr, "xio: Unable to initialise I2C: %s\n", strerror (errno));
        return(1);
  	}
#endif
        /*Read in the first 128 bytes 0 to 127*/
        for(i=0; i <128; i++){
#ifndef BEAGLEBONE
                fd1 = wiringPiI2CReadReg8 (xio,i);
#else
                fd1 = i2c_smbus_read_byte_data(xio,i);
#endif
                if  (address == 0x50){
			A50[i] = fd1;
		}
		else{
		       A51[i] = fd1;
		}
		if (fd1 <0){
                fprintf (stderr, "xio: Unable to read i2c address 0x%x: %s\n",
				address, strerror (errno));
			      return 1;
			}
	}
	return 0;
}

int dom(void)
{
	//102 TX MSB, 103 TX LSB, 104 RX MSB, 105 RX LSB.
	float temperature, vcc, tx_bias, optical_rx, optical_tx;

	if(!read_eeprom(0x51)); else exit(EXIT_FAILURE);

	temperature = (A51[96] + (float) A51[97]/256);
	vcc = (float)(A51[98]<<8 | A51[99]) * 0.0001;
	tx_bias = (float)(A51[100]<<8 | A51[101]) * 0.002;
	optical_tx = 10 * log10((float)(A51[102]<<8 | A51[103]) * 0.0001);
	optical_rx = 10 * log10((float)(A51[104]<<8 | A51[105]) * 0.0001);
	//Print the results
	printf ("Internal SFP Temperature = %4.2fC\n", temperature);
	printf ("Internal supply voltage = %4.2fV\n", vcc);
	printf ("TX bias current = %4.2fmA\n", tx_bias);
	printf ("Optical power Tx = %4.2f dBm\n", optical_tx);
	printf ("Optical power Rx = %4.2f dBm\n", optical_rx);

	return 0;
}

int burnSFP(void)
{
	int i;



	printf( "\n\n\nWrite   Yes/No?"
		"\nIf Yes, Insert new device to slot: ");
		int ch = 0;
		ch = getchar();
		getchar();
		if ((ch == 'Y') || (ch == 'y'))
		{
			printf("Writing Digest wait....\n");
#ifdef BEAGLEBONE
            xio = open(filenm, O_RDWR);
            if (xio < 0) {
              fprintf (stderr, "Unable to open device: %s\n", filenm);
              return(1);
            }

            if (ioctl(xio, I2C_SLAVE, 0x50) < 0) {
                fprintf (stderr, "xio: Unable to initialise I2C: %s\n", strerror (errno));
                return(1);
            }
#else
			xio = wiringPiI2CSetup (0x50);
				if (xio < 0){
				fprintf (stderr, "xio: Unable to initialise I2C: %s\n",
                                strerror (errno));
				return 1;
				}
#endif
			for(i = 0; i < 0x100; i++) {
#ifndef BEAGLEBONE
			wiringPiI2CWriteReg8(xio, i, A50[i]);
#else
            i2c_smbus_write_byte_data(xio, i, A50[i]);
#endif
			usleep(50000);//sleep for 0.5ms per byte 
			}
		} else printf("nothing written");

	
	if (write_checksum)
	{
	//Calculate the checksum: Add up the first 31 bytes and store
	//the last 8 bits in the 32nd byte
	mychecksum(0x0, 0x3f);

	//Calculate the extended checksum: Add up 31 bytes and store
	//the last 8 bits in the 32nd byte
	mychecksum(0x40, 0x5f);
	}
	return 0;
}

int mychecksum(unsigned char start_byte, unsigned char end_byte)
{
	int sum = 0;
	int counter;
	int cc_base;
	for (counter = start_byte; counter <end_byte; counter++)
	sum = (A50[counter] + sum);
	sum = sum & 0x0ff;
	cc_base = A50[end_byte]; //sum stored in address 63 or 95.
	if (start_byte == 0x0) printf("\ncc_base = %x, sum = %x",cc_base,sum);
	else printf("\nextended cc_base = %x and sum = %x\n",cc_base,sum);
		if (cc_base != sum && write_checksum){
			printf("\nCheck Sum failed, Do you want to write the"
			       "	checksum value %x to address byte \"%x\" ?"
			       "	(Y/N)", sum, end_byte);
			int ch = 0;
			ch = getchar();
			getchar();
			if ((ch == 'Y') || (ch == 'y')) {
#ifndef BEAGLEBONE
				xio = wiringPiI2CSetup (0x50);
				if (xio < 0){
				fprintf (stderr, "xio: Unable to initialise I2C: %s\n",
                                strerror (errno));
				return 1;
				}
#else
                xio = open(filenm, O_RDWR);
                if (xio < 0) {
                  fprintf (stderr, "Unable to open device: %s\n", filenm);
                  return(1);
                }

                if (ioctl(xio, I2C_SLAVE, 0x50) < 0) {
                    fprintf (stderr, "xio: Unable to initialise I2C: %s\n", strerror (errno));
                    return(1);
                }
#endif
				printf("end_byte = %x and sum = %x - yes\n",end_byte, sum);
#ifndef BEAGLEBONE
				wiringPiI2CWriteReg8(xio, end_byte, sum);
#else
                i2c_smbus_write_byte_data(xio, end_byte, sum);
#endif
			} else printf("not written checksum\n");
		}
		return 0;
}
