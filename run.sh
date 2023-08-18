#!/usr/bin/env bash
set -e
CONFIG_PATH=/data/options.json

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
./345toMqtt -d $DEVICE_ID -f $FREQUENCY -a $AUTODISCOVERY -g $GAIN -s $SAMPLE_RATE
echo "Exiting run.sh"
exit $?
