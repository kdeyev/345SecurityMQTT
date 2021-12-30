#!/usr/bin/env bashio

if bashio::services.available "mqtt"; then
    MQTT_HOST=$(bashio::services "mqtt" "host")
    MQTT_PASSWORD=$(bashio::services "mqtt" "password")
    MQTT_PORT=$(bashio::services "mqtt" "port")
    MQTT_USERNAME=$(bashio::services "mqtt" "username")
    SEND_DISCOVERY=$(bashio::config "discovery")
else
    bashio::log.info "The mqtt addon is not available."
    bashio::log.info "Manually update the output line in the configuration file with mqtt connection settings, and restart the addon."
fi

echo "Starting 345SecurityMQTT"
./345toMqtt
echo "Exiting run.sh"
exit $?
