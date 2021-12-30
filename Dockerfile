ARG BUILD_FROM=homeassistant/amd64-base
FROM ${BUILD_FROM} AS build

# Install build tools and remove apt-cache afterwards
RUN apk add --no-cache --virtual .buildDeps \
	build-base \
    libusb-dev \
    librtlsdr-dev \
	mosquitto-dev

# Switch into our apps working directory
WORKDIR /usr/src/app/345SecurityMQTT/src

COPY . /usr/src/app/345SecurityMQTT
WORKDIR /usr/src/app/345SecurityMQTT/src
RUN ./build.sh

FROM $BUILD_FROM

RUN apk add --no-cache libusb \
    librtlsdr \
	mosquitto-libs++

COPY --from=build /usr/src/app/345SecurityMQTT/src/345toMqtt /345toMqtt

#switch on systemd init system in container
ENV INITSYSTEM on


# Run script
COPY run.sh /
RUN chmod a+x /run.sh

CMD [ "/run.sh" ]
