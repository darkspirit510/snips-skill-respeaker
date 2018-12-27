#include "mqtt_client.h"
#include "state_handler.h"
#include "cJSON.h"
#include "mqtt.h"
#include "posix_sockets.h"
#include "log.h"

#include <pthread.h>

static void mqtt_callback_handler(void** unused, struct mqtt_response_publish *published);
static int match_site_id(const char *message);
static void* mqtt_client_refresher(void* mqtt_client);

extern const char *site_id;

struct      mqtt_client mqtt_client;
pthread_t   mqtt_client_daemon;
int         fd_mqtt_sock = -1;
char        rcv_site_id[255]= "";
uint8_t     mqtt_sendbuf[2048];
uint8_t     mqtt_recvbuf[1024];

const char *topics[]={
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

int start_mqtt_client(const char *mqtt_client_id,
                      const char *mqtt_addr,
                      const char *mqtt_port,
                      const char *username,
                      const char *password){

    fd_mqtt_sock = open_nb_socket(mqtt_addr, mqtt_port);
    if (fd_mqtt_sock == -1) {
        perror("[Error] Failed to open socket: ");
        return 0;
    }

    mqtt_init(&mqtt_client,
              fd_mqtt_sock,
              mqtt_sendbuf,
              sizeof(mqtt_sendbuf),
              mqtt_recvbuf,
              sizeof(mqtt_recvbuf),
              mqtt_callback_handler);

    mqtt_connect(&mqtt_client,
                 mqtt_client_id,
                 NULL,
                 NULL,
                 0,
                 username,
                 password,
                 0,
                 400);

    if (mqtt_client.error != MQTT_OK) {
        fprintf(stderr, "error: %s\n", mqtt_error_str(mqtt_client.error));
        return 0;
    }

    if(pthread_create(&mqtt_client_daemon, NULL, mqtt_client_refresher, &mqtt_client)) {
        fprintf(stderr, "[Error] Failed to start client daemon.\n");
        return 0;
    }

    for(int i=0;i<NUM_TOPIC;i++)
        if( MQTT_OK != mqtt_subscribe(&mqtt_client, topics[i], 0)){
            printf("[Error] Subscribe error\n");
            return 0;
        }
    return 1;
}

void terminate_mqtt_client(void){
    fprintf(stdout, "[Info] disconnecting mqtt \n");
    sleep(1);
    if (fd_mqtt_sock != -1)
        close(fd_mqtt_sock);
    if (mqtt_client_daemon)
        pthread_cancel(mqtt_client_daemon);
}

static void mqtt_callback_handler(void** unused, struct mqtt_response_publish *published){
    char *topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    printf("[Receive] topic "PURPLE" %s "NONE"\n", topic_name);
    if (!match_site_id(published->application_message))
        return;
    state_handler_main(topic_name);
    free(topic_name);
}

/**
 * @brief: check if the coming message corresponds to specified siteId
 *
 * @param[in] mqtt_received_message
 * @param[in] target_site_id
 *
 * @returns \0 not match or \1 match \-1 error
 */
static int match_site_id(const char *message){
    const cJSON *rev_site_id = NULL;

    cJSON *payload_json = cJSON_Parse(message);
    if (payload_json == NULL){
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
            fprintf(stderr, "[Error] from parsing message : %s\n", error_ptr);
        return -1;
    }

    rev_site_id = cJSON_GetObjectItemCaseSensitive(payload_json, "siteId");

    if(!strcmp(site_id, rev_site_id->valuestring)){
        fprintf(stdout, "[Info] Current site" GREEN " %s" NONE " / Received from site" GREEN " %s " NONE " \n", site_id, rev_site_id->valuestring);
        cJSON_Delete(payload_json);
        return 1;
    }
    else{
        fprintf(stdout, "[Info] Current site" GREEN " %s" NONE " / Received from site" RED " %s " NONE " \n", site_id, rev_site_id->valuestring);
        cJSON_Delete(payload_json);
        return 0;
    }
}

static void* mqtt_client_refresher(void* mqtt_client){
    while(1){
        mqtt_sync((struct mqtt_client*) mqtt_client);
        usleep(100000U);
    }
    return NULL;
}