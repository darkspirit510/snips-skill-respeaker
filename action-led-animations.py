#!/usr/bin/env python3

from cls.is31fl3236 import IS31FL3236
import paho.mqtt.client as mqtt
import time

hot_on = "hermes/hotword/toggleOn"
hot_off = "hermes/hotword/toggleOff"
sta_lis = "hermes/asr/startListening"
end_lis = "hermes/asr/stopListening"
sta_say = "hermes/tts/say"
end_say = "hermes/tts/sayFinished"
sud_on = "hermes/feedback/sound/toggleOn"
sud_off = "hermes/feedback/sound/toggleOff"
led_on = "hermes/feedback/led/toggleOn"
led_off = "hermes/feedback/led/toggleOff"

mqtt_topics = [hot_on, hot_off, sta_lis, end_lis, sta_say, end_say, sud_on, sud_off, led_on, led_off]

controller = IS31FL3236()

def on_connect(client, userdata, flags, rc):
    for topic in mqtt_topics:
        client.subscribe(topic)

def on_message(client, userdata, msg):    
    # on_idle_handler
    if msg.topic ==  sta_lis:
        controller.run_params.curr_state = "ON_LISTEN"
    elif msg.topic == sud_off:
        controller.run_params.curr_state = "TO_MUTE"
    elif msg.topic == sud_on:
        controller.run_params.curr_state = "TO_UNMUTE"
    elif msg.topic == sta_say:
        controller.run_params.curr_state = "ON_SPEAK"
    elif msg.topic == led_off:
        controller.run_params.curr_state = "ON_DISABLED"
    
    # on_speak_handler
    elif msg.topic == end_say:
        controller.run_params.curr_state = "ON_IDLE"
    elif msg.topic == sta_lis:
        controller.run_params.curr_state = "ON_LISTEN"
    elif msg.topic == hot_on:
        controller.run_params.curr_state = "ON_IDLE"

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("localhost", 1883, 60)

client.loop_start()

controller.run()
