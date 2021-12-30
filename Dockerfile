ARG BUILD_FROM=homeassistant/amd64-base-debian
FROM ${BUILD_FROM} AS build

# Install build tools and remove apt-cache afterwards
RUN apt-get -q update && apt-get install -yq --no-install-recommends \
	build-essential librtlsdr-dev rtl-sdr libmosquittopp-dev git \
	&& apt-get clean && rm -rf /var/lib/apt/lists/*

# Switch into our apps working directory
WORKDIR /usr/src/app/345SecurityMQTT/src

COPY . /usr/src/app/345SecurityMQTT
WORKDIR /usr/src/app/345SecurityMQTT/src
RUN ./build.sh

FROM $BUILD_FROM

RUN apt-get -q update && apt-get install -yq --no-install-recommends \
	librtlsdr-dev rtl-sdr libmosquittopp-dev \
	&& apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/src/app/345SecurityMQTT/src/345toMqtt /345toMqtt

#switch on systemd init system in container
ENV INITSYSTEM on


# Run script
COPY run.sh /
RUN chmod a+x /run.sh

CMD [ "/run.sh" ]
