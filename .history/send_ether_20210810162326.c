#include <argp.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#define MAX_ETHERNET_FRAME_SIZE 1514
#define MAX_ETHERNET_DATA_SIZE 1500

#define ETHERNET_HEADER_SIZE 14
#define ETHERNET_DST_ADDR_OFFSET 0
#define ETHERNET_SRC_ADDR_OFFSET 6
#define ETHERNET_TYPE_OFFSET 12
#define ETHERNET_DATA_OFFSET 14

#define MAC_BYTES 6
#define DATA_LENGTH 2

#define PI 3.1415926
#define DATA_BUFFER 720
#define MAX_LEVEL 1024

int frame_size = 0;
int data_size = 0;
int file_read = 0;
int file_data_size = 0;
int flags_sig = 0;
int data_index = 0;
int count = 50;



void printf2(uint16_t n) {
    uint16_t i = 0;
    for(i = 0; i < 16; i++) {
        if(n & (0x8000) >> i) {
            printf("1");
        }else {
            printf("0");
        }
    }
    printf("\n");
}


/**
 *  Convert readable MAC address to binary format.
 *
 *  Arguments
 *      a: buffer for readable format, like "08:00:27:c8:04:83".
 *
 *      n: buffer for binary format, 6 bytes at least.
 *
 *  Returns
 *      0 if success, -1 if error.
 **/
int mac_aton(const char *a, unsigned char *n) {
    int matches = sscanf(a, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", n, n+1, n+2,
                         n+3, n+4, n+5);

    return (6 == matches ? 0 : -1);
}


/**
 *  Fetch MAC address of given iface.
 *
 *  Arguments
 *      iface: name of given iface.
 *
 *      mac: buffer for binary MAC address, 6 bytes at least.
 *
 *      s: socket for ioctl, optional.
 *
 *  Returns
 *      0 if success, -1 if error.
 **/
int fetch_iface_mac(char const *iface, unsigned char *mac, int s) {
    // value to return, 0 for success, -1 for error
    int value_to_return = -1;

    // create socket if needed(s is not given)
    bool create_socket = (s < 0);
    if (create_socket) {
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (-1 == s) {
            return value_to_return;
        }
    }

    // fill iface name to struct ifreq
    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface, 15);

    // call ioctl to get hardware address
    int ret = ioctl(s, SIOCGIFHWADDR, &ifr);
    if (-1 == ret) {
        goto cleanup;
    }

    // copy MAC address to given buffer
    memcpy(mac, ifr.ifr_hwaddr.sa_data, MAC_BYTES);

    // success, set return value to 0
    value_to_return = 0;

cleanup:
    // close socket if created here
    if (create_socket) {
        close(s);
    }

    return value_to_return;
}


/**
 *  Fetch index of given iface.
 *
 *  Arguments
 *      iface: name of given iface.
 *
 *      s: socket for ioctl, optional.
 *
 *  Returns
 *      Iface index(which is greater than 0) if success, -1 if error.
 **/
int fetch_iface_index(char const *iface, int s) {
    // iface index to return, -1 means error
    int if_index = -1;

    // create socket if needed(s is not given)
    bool create_socket = (s < 0);
    if (create_socket) {
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (-1 == s) {
            return if_index;
        }
    }

    // fill iface name to struct ifreq
    struct ifreq ifr;
    strncpy(ifr.ifr_name, iface, 15);

    // call ioctl system call to fetch iface index
    int ret = ioctl(s, SIOCGIFINDEX, &ifr);
    if (-1 == ret) {
        goto cleanup;
    }

    if_index = ifr.ifr_ifindex;

cleanup:
    // close socket if created here
    if (create_socket) {
        close(s);
    }

    return if_index;
}


/**
 * Bind socket with given iface.
 *
 *  Arguments
 *      s: given socket.
 *
 *      iface: name of given iface.
 *
 *  Returns
 *      0 if success, -1 if error.
 **/
int bind_iface(int s, char const *iface) {
    // fetch iface index
    int if_index = fetch_iface_index(iface, s);
    if (-1 == if_index) {
        return -1;
    }

    // fill iface index to struct sockaddr_ll for binding
    struct sockaddr_ll sll;
    bzero(&sll, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_index;
    sll.sll_pkttype = PACKET_HOST;

    // call bind system call to bind socket with iface
    int ret = bind(s, (struct sockaddr *)&sll, sizeof(sll));
    if (-1 == ret) {
        return -1;
    }

    return 0;
}


/**
 * struct for an ethernet frame
 **/
struct ethernet_frame {
    // destination MAC address, 6 bytes
    unsigned char dst_addr[6];

    // source MAC address, 6 bytes
    unsigned char src_addr[6];

    // type, in network byte order
    unsigned short type;

    // unsigned int length;
    unsigned short length;

    // data
    uint16_t data[DATA_BUFFER];
};


/**
 * struct for storing command line arguments.
 **/
struct arguments {
    // name of iface through which data is sent
    char const *iface;

    // destination MAC address
    char const *to;

    // data type
    unsigned short type;

    // data to send
    uint16_t *data;

    // signal type
    char *s_type;

    // signal frequency
    float freq;

    // sample rate
    float sample_rate;
};


/**
 * opt_handler function for GNU argp.
 **/
static error_t opt_handler(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;
    FILE *fd = NULL;
    uint16_t *buff;
    
    switch(key) {
        case 'd':
            data_size = strlen(arg);
            if (sscanf(arg, "%d", &arguments->data) != 1) {
                return ARGP_ERR_UNKNOWN;
            }
            break;

        case 'i':
            arguments->iface = arg;
            break;

        case 'T':
            if (sscanf(arg, "%hx", &arguments->type) != 1) {
                return ARGP_ERR_UNKNOWN;
            }
            break;

        case 't':
            arguments->to = arg;
            break;

        case 'f':
            file_read = 1;
            fd = fopen(arg,"r");
            fseek(fd, 0, SEEK_END);
            file_data_size = ftell(fd);
            fseek(fd, 0, SEEK_SET);
            buff = (uint16_t*)malloc(sizeof(uint16_t)*file_data_size);
            fread(buff, 1,file_data_size, fd);
            arguments->data = buff;
            break;

        case 'S':
            flags_sig = 1;
            arguments->s_type = "SIN";
            if (sscanf(arg, "%f", &arguments->freq) != 1) {
                return ARGP_ERR_UNKNOWN;
            }
            break;
            
        case 's':
            flags_sig = 1;
            if (sscanf(arg, "%f", &arguments->sample_rate) != 1) {
                return ARGP_ERR_UNKNOWN;
            }
            break;

        case 'c':
            count = atoi(arg);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}


/**
 * Parse command line arguments given by argc, argv.
 *
 *  Arguments
 *      argc: the same with main function.
 *
 *      argv: the same with main function.
 *
 *  Returns
 *      Pointer to struct arguments if success, NULL if error.
 **/
static struct arguments *parse_arguments(int argc, char *argv[]) {
    // docs for program and options
    static char const doc[] = "send_ether: send data through ethernet frame";
    static char const args_doc[] = "";

    // command line options
    static struct argp_option const options[] = {
        // Option -i --iface: name of iface through which data is sent
        {"iface", 'i', "IFACE", 0, "name of iface for sending"},

        // Option -t --to: destination MAC address
        {"to", 't', "TO", 0, "destination mac address"},

        // Option -T --type: data type
        {"type", 'T', "TYPE", 0, "data type"},

        // Option -d --data: data to send, optional since default value is set
        {"data", 'd', "DATA", 0, "data to send"},

        // Option -f --file: read file and send
        {"file", 'f', "FILE", 0, "Read file and send"},

        // Option -S --SIN: send the sin waveform
        {"sin", 'S', "SIN", 0, "Send the sin waveform"},

        // Option -s --sample rate: set the sample rate
        {"sample rate", 's', "SAMPLE RATE", 0, "Set the sample rate"},

        // Option -c --count: set the times of sending frames
        {"Count", 'c', "COUNT", 0, "set the times of sending frames"},

        { 0 }
    };

    static struct argp const argp = {
        options,
        opt_handler,
        args_doc,
        doc,
        0,
        0,
        0,
    };

    // for storing results
    static struct arguments arguments = {
        //default iface:eth0
        .iface = "eth0",
        //default to:11:22:33:44:55:66
        .to = "11:22:33:44:55:66",
        //default data type: 0x0900
        .type = 0x0900,
        // default data, 46 bytes string of 'a'
        // since for ethernet frame data is 46 bytes at least
        .data = NULL,
    };

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    return &arguments;
}

/**
 * Print message about data
 *
 *  Arguments
 *      arguments: pointer to struct arguments
 *
 *      ret: will be -1 if send data failed and print error message.
 *
 *  Returns
 *      0 if success, 3 if error.
 **/
int print_mesg(struct arguments *arguments ,int ret) {
    if (-1 == ret) {
        perror("Fail to send ethernet frame: ");
        return 3;
    }else{
        printf("\n");
        printf("Send successfully!!!!!\n");
        printf("\n");
        printf("------------------------------------------------------\n");
        printf("Information of frame:\n");
        printf("------------------------------------------------------\n");
        printf("iface: %s\n",arguments->iface);
        printf("Destination: %s\n",arguments->to);
        printf("Type: 0x%x\n",arguments->type);
        printf("Signal: %8.2f Hz %s\n",arguments->freq, arguments->s_type);
        printf("Sample Rate: %8.2f Hz \n",arguments->sample_rate);
        printf("Size of data: %d bytes\n",data_size-2);
        printf("Size of frame: %d bytes\n",frame_size);
        printf("------------------------------------------------------\n");
        return 0;
    }
}



/**
 * Write data into .bin files for checking
 *
 *  Arguments
 *      data: uint16_t data.
 *
 *  Returns
 *      No returns.
 **/
void write_data_signal_generating(uint16_t data[]) {
    FILE *fw = fopen("data.bin", "wb");
    for (int i = 0; i < DATA_BUFFER-1; i++)
    {   
        fwrite((data+i), sizeof(uint16_t), 1, fw);
    }

    fclose(fw);
}


void write_frame_data(uint16_t *d, uint16_t data[]) {
    FILE *fw = fopen("data2.bin", "wb");
    for (d; d < data + DATA_BUFFER-1; d++)
    {
        fwrite(d, sizeof(uint16_t), 1, fw);
    }

    fclose(fw);
}

/**
 * Generate waveform data and be ready to send
 *
 *  Arguments
 *      arguments: pointer to struct arguments.
 *
 *  Returns
 *      No return.
 **/
void signal_generate(struct arguments *arguments) {
    uint16_t data[DATA_BUFFER - 1];

    unsigned int i = 0;
    unsigned int aver = MAX_LEVEL;
    unsigned long freq = arguments->freq;
    float rad = 2 * PI * (freq/arguments->sample_rate);
    float res = 0, key = 4;     //used to judge whether res can be divided by key (discrete sample)

    float I_temp = 0.0, Q_temp = 0.0;
    uint16_t I_amp = 0, Q_amp = 0;


    for(i = 0; i < DATA_BUFFER / 2 - 1; i++) {
        I_temp = aver * sin(rad * data_index);
        if(I_temp < 0) {
            I_amp = (uint16_t)(8192 - I_temp);
        }else{
            I_amp = (uint16_t)I_temp;
        }
        
        Q_temp = aver * cos(rad * data_index);
        if(Q_temp < 0) {
            Q_amp = (uint16_t)(8192 - Q_temp);
        }else {
            Q_amp = (uint16_t)Q_temp;
        }
        data[i * 2] = I_amp;
        *(arguments->data + i*2)=I_amp;
        printf2(I_amp);
        data[i * 2 + 1] = Q_amp;
        *(arguments->data + i*2 + 1)=Q_amp;
        printf2(Q_amp);
        res = data_index * (freq/arguments->sample_rate);

        if(fmod(res, key) == 0 && data_index > 0) {
            data_index = 0;
        }else {
            data_index++;
        }
    }
    data_size = sizeof(data);
    // arguments->data = data;
    write_data_signal_generating(data);
    write_frame_data(arguments->data, data);
}

/**
 *  Send data through given iface by ethernet protocol, using raw socket.
 *
 *  Arguments
 *      iface: name of iface for sending.
 *
 *      to: destination MAC address, in binary format.
 *
 *      type: protocol type.
 *
 *      data: data to send.
 *
 *      s: socket for ioctl, optional.
 *
 *  Returns
 *      0 if success, -1 if error.
 **/
int send_ether(char const *iface, unsigned char const *to, short type,
        uint16_t *data, struct arguments *arguments, int s) {
    // value to return, 0 for success, -1 for error
    int value_to_return = -1;

    // create socket if needed(s is not given)
    bool create_socket = (s < 0);
    if (create_socket) {
        s = socket(PF_PACKET, SOCK_RAW | SOCK_CLOEXEC, 0);
        if (-1 == s) {
            return value_to_return;
        }
    }

    // bind socket with iface
    int ret = bind_iface(s, iface);
    if (-1 == ret) {
        goto cleanup;
    }

    // fetch MAC address of given iface, which is the source address
    unsigned char fr[6];
    ret = fetch_iface_mac(iface, fr, s);
    if (-1 == ret) {
        goto cleanup;
    }

    // construct ethernet frame, which can be 1514 bytes at most
    struct ethernet_frame frame;

    // fill destination MAC address
    memcpy(frame.dst_addr, to, MAC_BYTES);

    // fill source MAC address
    memcpy(frame.src_addr, fr, MAC_BYTES);
    
    // fill type
    frame.type = htons(type);

    signal_generate(arguments);
    
    // printf("type:%u \n",frame.type);
    // truncate if data is too long
    if (data_size > MAX_ETHERNET_DATA_SIZE) {
        data_size = MAX_ETHERNET_DATA_SIZE;
    }
    
    frame.length = htons(data_size-2);

    // printf("length:%u \n",frame.length);
    if(file_read == 0) {
        // fill data
        memcpy(frame.data, data, data_size);

        frame_size = ETHERNET_HEADER_SIZE + data_size;

    }else {
        memcpy(frame.data, data, file_data_size);

        frame_size = ETHERNET_HEADER_SIZE + file_data_size;
    }
    

    ret = sendto(s, &frame, frame_size, 0, NULL, 0);
    if (-1 == ret) {
        goto cleanup;
    }

    // set return value to 0 if success
    value_to_return = 0;

cleanup:
    // close socket if created here
    if (create_socket) {
        close(s);
    }

    return value_to_return;
}



int main(int argc, char *argv[]) {

    // parse command line options to struct arguments
    struct arguments *arguments = parse_arguments(argc, argv);
    if (NULL == arguments) {
        fprintf(stderr, "Bad command line options given\n");
        return 1;
    }

    // convert destinaction MAC address to binary format
    unsigned char to[6];
    int ret = mac_aton(arguments->to, to);
    if (0 != ret) {
        fprintf(stderr, "Bad MAC address given: %s\n", arguments->to);
        return 2;
    }
    // arguments->data = (uint16_t*)malloc(sizeof(uint16_t)*DATA_BUFFER*2);
    for(int i = 0; i < count; i++) {
        if(flags_sig == 0) {
            //send data just once
            ret = send_ether(arguments->iface, to, arguments->type,
                     arguments->data, arguments, -1);
            print_mesg(arguments, ret);
            return 0;
        }else if(flags_sig == 1) {
            //send data for particular times
            signal_generate(arguments);
            ret = send_ether(arguments->iface, to, arguments->type,
                     arguments->data, arguments, -1);
            print_mesg(arguments, ret);
        }else {
            perror("Fail to send ethernet frame: ");
            return 3;
        }
    }
}
