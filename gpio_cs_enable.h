#ifndef GPIO_CS_ENABLE_H
#define GPIO_CS_ENABLE_H

#define SYSFS_GPIO_EXPORT           "/sys/class/gpio/export"  

//#define SYSFS_GPIO_RST_PIN_VAL      "117"   

//#define SYSFS_GPIO_RST_DIR          "/sys/class/gpio/gpio117/direction"

#define SYSFS_GPIO_RST_DIR_VAL      "OUT"  

//#define SYSFS_GPIO_RST_VAL          "/sys/class/gpio/gpio117/value"

#define SYSFS_GPIO_RST_VAL_H        "1"

#define SYSFS_GPIO_RST_VAL_L        "0"

int gpio_cs_enable(int pin, int value);

#endif