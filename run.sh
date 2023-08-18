#!/usr/bin/env bash
set -e

CONFIG_PATH=/data/options.json


# if bashio::services.available "mqtt"; then
#     export MQTT_HOST=$(bashio::services "mqtt" "host")
#     export MQTT_PASSWORD=$(bashio::services "mqtt" "password")
#     export MQTT_PORT=$(bashio::services "mqtt" "port")
#     export MQTT_USERNAME=$(bashio::services "mqtt" "username")
# else
#     bashio::log.info "The mqtt addon is not available."
#     bashio::log.info "Manually update the output line in the configuration file with mqtt connection settings, and restart the addon."
#     exit $?
# fi

AUTODISCOVERY=$(jq --raw-output '.autodiscovery // empty' $CONFIG_PATH)
DEVICE_ID=$(jq --raw-output '.device_id // empty' $CONFIG_PATH)
FREQUENCY=$(jq --raw-output '.frequency // empty' $CONFIG_PATH)
SAMPLE_RATE=$(jq --raw-output '.sample_rate // empty' $CONFIG_PATH)
GAIN=$(jq --raw-output '.gain // empty' $CONFIG_PATH)

export MQTT_HOST=$(jq --raw-output '.mqtt_host // empty' $CONFIG_PATH)
export MQTT_PORT=$(jq --raw-output '.mqtt_port // empty' $CONFIG_PATH)
export MQTT_USERNAME=$(jq --raw-output '.mqtt_username // empty' $CONFIG_PATH)
export MQTT_PASSWORD=$(jq --raw-output '.mqtt_password // empty' $CONFIG_PATH)

echo "Starting 345toMqtt -d $DEVICE_ID -f $FREQUENCY -a $AUTODISCOVERY -g $GAIN -s $SAMPLE_RATE"
echo "$MQTT_HOST $MQTT_PORT $MQTT_USERNAME $MQTT_PASSWORD"


# STUPID BASHIO DOES NOT WORK 
AUTODISCOVERY=false
DEVICE_ID=1
FREQUENCY=345000000
SAMPLE_RATE=1000000
GAIN=700

export MQTT_HOST=core-mosquitto
export MQTT_PORT=1883
export MQTT_USERNAME=mqtt
export MQTT_PASSWORD=mqtt

echo "Starting 345toMqtt -d $DEVICE_ID -f $FREQUENCY -a $AUTODISCOVERY -g $GAIN -s $SAMPLE_RATE"
echo "$MQTT_HOST $MQTT_PORT $MQTT_USERNAME $MQTT_PASSWORD"
./345toMqtt -d $DEVICE_ID -f $FREQUENCY -a $AUTODISCOVERY -g $GAIN -s $SAMPLE_RATE
echo "Exiting run.sh"
exit $?
