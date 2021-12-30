#!/usr/bin/env bashio

if bashio::services.available "mqtt"; then
    MQTT_HOST=$(bashio::services "mqtt" "host")
    MQTT_PASSWORD=$(bashio::services "mqtt" "password")
    MQTT_PORT=$(bashio::services "mqtt" "port")
    MQTT_USERNAME=$(bashio::services "mqtt" "username")
    DISCOVERY=$(bashio::config "discovery")
    DEVICE_ID=$(bashio::config "device_id")
    FREQUENCY=$(bashio::config "frequency")
else
    bashio::log.info "The mqtt addon is not available."
    bashio::log.info "Manually update the output line in the configuration file with mqtt connection settings, and restart the addon."
    exit $?
fi

echo "Starting 345toMqtt -d $DEVICE_ID -f $FREQUENCY"
./345toMqtt -d $DEVICE_ID -f $FREQUENCY
echo "Exiting run.sh"
exit $?
