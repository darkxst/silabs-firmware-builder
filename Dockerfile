FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update \
    && apt-get install -y --no-install-recommends \
       bzip2 \
       curl \
       git \
       git-lfs \
       jq \
       yq \
       libgl1 \
       libglib2.0-0 \
       locales \
       make \
       openjdk-21-jre-headless \
       openssh-client \
       patch \
       python3 \
       python3-ruamel.yaml \
       unzip \
       xz-utils

RUN \
    locale-gen en_US.UTF-8 \
    && update-locale LANG=en_US.UTF-8

ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# Install Simplicity Commander (unfortunately no stable URL available, this
# is known to be working with Commander_linux_x86_64_1v15p0b1306.tar.bz).
RUN \
    curl -O https://www.silabs.com/documents/login/software/SimplicityCommander-Linux.zip \
    && unzip -q SimplicityCommander-Linux.zip \
    && tar -C /opt -xjf SimplicityCommander-Linux/Commander_linux_x86_64_*.tar.bz \
    && rm -r SimplicityCommander-Linux \
    && rm SimplicityCommander-Linux.zip

ENV PATH="$PATH:/opt/commander"

# Install Silicon Labs Configurator (slc)
RUN \
    curl -O https://www.silabs.com/documents/login/software/slc_cli_linux.zip \
    && unzip -q -d /opt slc_cli_linux.zip \
    && rm slc_cli_linux.zip

ENV PATH="$PATH:/opt/slc_cli"

# GCC Embedded Toolchain 12.2.rel1 (for Gecko SDK 4.4.0+)
RUN \
    curl -O https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && tar -C /opt -xf arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && rm arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz

# Simplicity SDK 2025.6.0
RUN \
    curl -o simplicity_sdk_2025.6.0.zip -L https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2025.6.0/simplicity-sdk.zip \
    && unzip -q -d simplicity_sdk_2025.6.0 simplicity_sdk_2025.6.0.zip \
    && rm simplicity_sdk_2025.6.0.zip \
    && chown ubuntu:ubuntu -R /simplicity_sdk_2025.6.0

# ZCL Advanced Platform (ZAP) v2025.06.09
RUN \
    curl -o zap_2025.06.09.zip -L https://github.com/project-chip/zap/releases/download/v2025.06.09/zap-linux-x64.zip \
    && unzip -q -d /opt/zap zap_2025.06.09.zip \
    && rm zap_2025.06.09.zip

ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

ARG USERNAME=ubuntu
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# # Create the user
# RUN groupadd --gid $USER_GID $USERNAME \
#     && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME
RUN mkdir -p /build && chown $USERNAME:$USERNAME /build

USER $USERNAME
WORKDIR /build
