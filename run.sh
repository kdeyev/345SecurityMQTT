#!/usr/bin/env bashio

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

AUTODISCOVERY=$(bashio::config "autodiscovery")
DEVICE_ID=$(bashio::config "device_id")
FREQUENCY=$(bashio::config "frequency")
SAMPLE_RATE=$(bashio::config "sample_rate")
GAIN=$(bashio::config "gain")

# export MQTT_HOST=$(bashio::config "mqtt_host")
# export MQTT_PORT=$(bashio::config "mqtt_port")
# export MQTT_USERNAME=$(bashio::config "mqtt_username")
# export MQTT_PASSWORD=$(bashio::config "mqtt_password")

# AUTODISCOVERY=false
# DEVICE_ID=1
# FREQUENCY=345000000
# SAMPLE_RATE=1000000
# GAIN=700

export MQTT_HOST=localhost
export MQTT_PORT=1883
export MQTT_USERNAME=mqtt
export MQTT_PASSWORD=mqtt

echo "Starting 345toMqtt -d $DEVICE_ID -f $FREQUENCY -a $AUTODISCOVERY -g $GAIN -s $SAMPLE_RATE"
echo "$MQTT_HOST $MQTT_PORT $MQTT_USERNAME $MQTT_PASSWORD"
./345toMqtt -d $DEVICE_ID -f $FREQUENCY -a $AUTODISCOVERY -g $GAIN -s $SAMPLE_RATE
echo "Exiting run.sh"
exit $?
