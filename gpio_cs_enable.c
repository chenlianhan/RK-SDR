#include <stdlib.h>
#include <stdio.h>  
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "gpio_cs_enable.h"

#define SYSFS_GPIO_EXPORT           "/sys/class/gpio/export" 

#define SYSFS_GPIO_UNEXPORT           "/sys/class/gpio/unexport"  

//#define SYSFS_GPIO_RST_PIN_VAL      "117"   

//#define SYSFS_GPIO_RST_DIR          "/sys/class/gpio/gpio117/direction"
#define SYSFS_GPIO_RST_DIR_VAL_OUT       "out"
#define SYSFS_GPIO_RST_DIR_VAL_HIGH      "high"  
#define SYSFS_GPIO_RST_DIR_VAL_LOW      "low" 

//#define SYSFS_GPIO_RST_VAL          "/sys/class/gpio/gpio117/value"

#define SYSFS_GPIO_RST_VAL_H        "1"

#define SYSFS_GPIO_RST_VAL_L        "0"

int gpio_cs_enable(int pin, int value)
{
    int fd; 

    //打开端口/sys/class/gpio# echo xxx > export

    fd = open(SYSFS_GPIO_EXPORT, O_WRONLY);

    if(fd == -1)
    {
        printf("ERR: Radio hard reset pin open error.\n");
        return EXIT_FAILURE;
    }

    char SYSFS_GPIO_RST_PIN_VAL[20] = {0};
    sprintf(SYSFS_GPIO_RST_PIN_VAL,"%d",pin);

    write(fd, SYSFS_GPIO_RST_PIN_VAL ,sizeof(SYSFS_GPIO_RST_PIN_VAL)); 

    close(fd); 

    //设置端口方向/sys/class/gpio/gpio117# echo out > direction
    char SYSFS_GPIO_RST_DIR[50]="";
    char SYSFS_GPIO_RST_VAL[50]="";
    strcat(SYSFS_GPIO_RST_DIR,"/sys/class/gpio/gpio");
    strcat(SYSFS_GPIO_RST_DIR,SYSFS_GPIO_RST_PIN_VAL);
    strcpy(SYSFS_GPIO_RST_VAL,SYSFS_GPIO_RST_DIR);
    strcat(SYSFS_GPIO_RST_DIR,"/direction");
    strcat(SYSFS_GPIO_RST_VAL,"/value");

    // printf(SYSFS_GPIO_RST_DIR);
    // printf("\n");
    // printf(SYSFS_GPIO_RST_VAL);
    // printf("\n");

    fd = open(SYSFS_GPIO_RST_DIR, O_WRONLY);

    if(fd == -1)
    {
        printf("ERR: Radio hard reset pin direction open error.\n");
        return EXIT_FAILURE;
    } 
    write(fd, SYSFS_GPIO_RST_DIR_VAL_OUT, sizeof(SYSFS_GPIO_RST_DIR_VAL_OUT)); 

    // close(fd); 

    // fd = open(SYSFS_GPIO_RST_VAL, O_RDWR);

    // if(fd == -1)
    // {
    //     printf("ERR: Radio hard reset pin value open error.\n");

    //     return EXIT_FAILURE;
    // }     
    if(value == 1)
    {
        //输出高电平
        // write(fd, SYSFS_GPIO_RST_VAL_H, sizeof(SYSFS_GPIO_RST_VAL_H));
        write(fd, SYSFS_GPIO_RST_DIR_VAL_HIGH, sizeof(SYSFS_GPIO_RST_DIR_VAL_HIGH));
        printf("ChipSelect(GPIO Pin: %d) sets to high.\n",pin);

    }
    else if(value == 0)
    {
        //输出低电平
        write(fd, SYSFS_GPIO_RST_DIR_VAL_LOW, sizeof(SYSFS_GPIO_RST_DIR_VAL_LOW)); 
        printf("ChipSelect(GPIO Pin: %d) sets to low.\n",pin);
    }
    else
    {
        printf("ERR: FRadio hard reset pin value open error.\n");

        return EXIT_FAILURE;
    }

    close(fd);

    fd = open(SYSFS_GPIO_UNEXPORT, O_WRONLY);
    if(fd == -1)
    {
        printf("ERR: Radio hard reset pin open error.\n");
        return EXIT_FAILURE;
    }
    write(fd, SYSFS_GPIO_RST_PIN_VAL ,sizeof(SYSFS_GPIO_RST_PIN_VAL)); 

    close(fd);
    

    printf("INFO: GPIO control finish.\n");
    
    return 0;
}