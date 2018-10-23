#include "app.h"
#include "apa102.h"
#include "get_config.h"
#include "state_handler.h"

#include <pthread.h>
#include <signal.h>
#include <posix_sockets.h>

volatile sig_atomic_t   flag_terminate = 0;
short                   flag_update = 1;
short                   flag_sleepmode = 0;

APA102      leds = {0, -1, NULL, 127};
STATE       curr_state = ON_IDLE;

int         fd_sock = -1;
pthread_t   curr_thread;
uint8_t     sleep_hour;
uint8_t     sleep_minute;
uint8_t     weak_hour;
uint8_t     weak_minute;

char        rcv_site_id[255]= "";

const char	*addr;
const char	*port;

const char* topics[]={
    HOT_OFF,
    STA_LIS,
    END_LIS,
    STA_SAY,
    END_SAY,
    HOT_ON,
    SUD_ON,
    SUD_OFF,
    LED_ON,
    LED_OFF
};

snipsSkillConfig configList[CONFIG_NUM]={
    {"model", 0},           //0
    {"spi_dev", 0},         //1
    {"led_num", 0},         //2
    {"led_bri", 0},         //3
    {"mqtt_host", 0},       //4
    {"mqtt_port", 0},       //5
    {"on_idle", 0},         //6
    {"on_listen", 0},       //7
    {"on_speak", 0},        //8
    {"to_mute", 0},         //9
    {"to_unmute", 0},       //10
    {"on_success", 0},      //11
    {"on_error", 0},        //12
    {"nightmode", 0},       //13
    {"go_sleep", 0},        //14
    {"go_weak", 0},         //15
    {"on_disabled", "1"},   //16
    {"site_id", 0}          //17
};

int main(int argc, char const *argv[])
{	
    int i;
    char *client_id;
    // generate a random id as client id
    client_id = generate_client_id();
    signal(SIGINT, int_handler);
    // get config.ini
    config(configList, CONFIG_NUM);

    switch_on_power();
    // get input parameters
    leds.numLEDs = (argc > 1)? atoi(argv[1]) : atoi(configList[2].value);
    addr = (argc > 2)? argv[2] : configList[4].value; // mqtt_host
    port = (argc > 3)? argv[3] : configList[5].value; // mqtt_port

    // get brightness
    leds.brightness = (strlen(configList[3].value) != 0) ? atoi(configList[3].value) : 127;

    // if sleep mode is enabled
    if (if_config_true("nightmode", configList, NULL) == 1){
        flag_sleepmode = 1;
        parse_hour_minute(configList[14].value, &sleep_hour, &sleep_minute);
        parse_hour_minute(configList[15].value, &weak_hour, &weak_minute);
    }
    
    /* open the non-blocking TCP socket (connecting to the broker) */
    int sockfd = open_nb_socket(addr, port);
    if (sockfd == -1) {
        perror("Failed to open socket: ");
        close_all(EXIT_FAILURE, NULL);
    }
    /* setup a client */
    struct mqtt_client client;
    uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
    uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback);
    mqtt_connect(&client, client_id, NULL, NULL, 0, NULL, NULL, 0, 400);
    /* check that we don't have any errors */
    if (client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        close_all(EXIT_FAILURE, NULL);
    }
    /* start a thread to refresh the client (handle egress and ingree client traffic) */
    pthread_t client_daemon;
    if(pthread_create(&client_daemon, NULL, client_refresher, &client)) {
        fprintf(stderr, "Failed to start client daemon.\n");
        close_all(EXIT_FAILURE, NULL);
    }
    /* subscribe */
    for(i=0;i<NUM_TOPIC;i++){
        mqtt_subscribe(&client, topics[i], 0);
        printf("[Info] Subscribed to '%s'.\n", topics[i]);
    }
    for(i=0;i<CONFIG_NUM;i++){
        printf("[Conf] %s - '%s'\n", configList[i].key, configList[i].value);
    }
    apa102_spi_setup();
    /* start publishing the time */
    printf("[Info] Initilisation looks good.....\n");
    printf("[Info] Client id : %s\n", client_id);
    printf("[Info] Program : %s\n", argv[0]);
    printf("[Info] LED number : %d with max brightness: %d\n", leds.numLEDs, leds.brightness);
    printf("[Info] Device : %s\n", configList[0].value);
    printf("[Info] Listening to MQTT bus: %s:%s \n",addr, port);
    printf("[Info] Press CTRL-C to exit.\n\n");

    /* block */
    while(1){
        if(flag_sleepmode)
            check_nightmode();
        
        if (flag_update) 
            state_machine_update();

        if (flag_terminate) break;

        usleep(10000);
    }
    // disconnect
    printf("[Info] %s disconnecting from %s\n", argv[0], addr);
    sleep(1);

    // clean
    close_all(EXIT_SUCCESS, &client_daemon);
    return 0;
}

void check_nightmode(void){
    time_t curr_time;
    struct tm *read_time = NULL;

    curr_time = time(NULL);
    read_time = localtime(&curr_time);

    if(read_time->tm_hour == sleep_hour && 
        read_time->tm_min == sleep_minute &&
        curr_state != ON_DISABLED){
        curr_state = ON_DISABLED;
        flag_update = 1;
        printf("[Info] ------>  Nightmode started\n");
    }
    if(read_time->tm_hour == weak_hour && 
        read_time->tm_min == weak_minute &&
        curr_state == ON_DISABLED){
        curr_state = ON_IDLE;
        flag_update = 1;
        printf("[Info] ------>  Nightmode terminated\n");
    }
}

void switch_on_power(void){
    int fd_gpio;
    char gpio_66[] = {'6','6'};
    char direction[] = {'o','u','t'};
    char value[] = {'0'};

    if(if_config_true("model", configList, "rsp_corev2")){
        // export gpio
        if(fd_gpio = open("/sys/class/gpio/export", O_RDWR)<0){
            printf("[Error] Can't open gpio export, please manually echo '66' to this file\n");
            return;
        }

        if(write(fd_gpio, gpio_66, sizeof(gpio_66))>0){
            printf("[Info] Exported GPIO66..\n");
            close(fd_gpio);
        }
        else{
            close(fd_gpio);
            printf("[Info] Failed to GPIO66..\n");
            return;
        }
        // set direction
        if(fd_gpio = open("/sys/class/gpio/gpio66/direction", O_RDWR)<0){
            printf("[Error] Can't open gpio export/gpio66/direction, please manually echo 'out' to this file\n");
            return;
        }
        
        if(write(fd_gpio, direction, sizeof(direction))>0){
            printf("[Info] Set GPIO66 direction out..\n");
            close(fd_gpio);
        }
        else{
            close(fd_gpio);
            printf("[Info] Failed to set GPIO66 direction， please manually echo 'out' to this file\n");
            return;
        }
        // enable by set to 0
        if(fd_gpio = open("/sys/class/gpio/gpio66/value", O_RDWR)<0){
            printf("[Error] Can't open gpio export/gpio66/value, please manually echo '0' to this file\n");
            return;
        }
        
        if(write(fd_gpio, value, sizeof(value))>0){
            printf("[Info] Set GPIO66 value 0..\n");
            close(fd_gpio);
        }
        else{
            close(fd_gpio);
            printf("[Info] Failed to set GPIO66 value 0, please manually echo '0' to this file\n");
            return;
        }
    }
}

void apa102_spi_setup(void){
    int temp,i;
    leds.pixels = (uint8_t *)malloc(leds.numLEDs * 4);
    if (begin()){
        for (i = 0; i < 3; i++){
            printf("[Error] Failed to start SPI! Retrying..%d\n",i+1); 
            sleep(30);
            if (begin() == 0) return;
        }
        printf("[Error] Failed to start SPI!\n"); 
        close_all(EXIT_FAILURE, NULL);
    }
}

void publish_callback(void** unused, struct mqtt_response_publish *published) {
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char *topic_name = (char*) malloc(published->topic_name_size + 1);

    memcpy(topic_name, published->topic_name, published->topic_name_size);

    topic_name[published->topic_name_size] = '\0';
    get_site_id(published->application_message);
    if (strcmp(configList[17].value, rcv_site_id) != 0)
        return;

    printf("[Received] %s on Site: %s\n", topic_name, rcv_site_id);

    state_handler_main(topic_name);

    free(topic_name);
}

void* client_refresher(void* client){
    while(1) 
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}

char *generate_client_id(void){
    int i ,seed;
    static char id[CLIENT_ID_LEN + 1] = {0};
    srand(time(NULL));
    for (i = 0; i < CLIENT_ID_LEN; i++){
        seed = rand()%3;
        switch(seed){
            case 0:
                id[i] = rand()%26 + 'a';
                break;
            case 1:
                id[i] = rand()%26 + 'A';
                break;
            case 2:
                id[i] = rand()%10 + '0';
                break;
        }
    }
    return id;
}

void close_all(int status, pthread_t *client_daemon){
    int fd_gpio;
    char gpio_66[]={'6','6'};

    clear();
    if(if_config_true("model", configList, "rsp_corev2")){
        fd_gpio = open("/sys/class/gpio/unexport", O_RDWR);
        if(write(fd_gpio, gpio_66, sizeof(gpio_66))){
            printf("[Info] Closed GPIO66..\n");
            close(fd_gpio);
        }  
    }
    if (fd_sock != -1) close(fd_sock);
    if (leds.fd_spi != -1) close(leds.fd_spi);
    if (leds.pixels) free(leds.pixels);
    if (client_daemon != NULL) pthread_cancel(*client_daemon);
    pthread_cancel(curr_thread);
    exit(status);
}

static void get_site_id(char *msg){
    char *start;
    int count = 0;
    start = strstr(msg, "\"siteId\":\""); // len = 10
    if (start == NULL )
        return;
    start += 10;
    while(*start != '\"'){
        rcv_site_id[count] = *start;
        start += 1;
        count += 1;
    }
    rcv_site_id[count] = '\0';
}

static void int_handler(int sig){
    flag_terminate = 1;
}

