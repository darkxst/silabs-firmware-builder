FROM debian:bookworm

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
       make \
       default-jre-headless \
       patch \
       python3 \
       python3-ruamel.yaml \
       unzip \
       xz-utils

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

# Simplicity SDK 2024.6.0
RUN \
    curl -o simplicity_sdk_2024.6.0.zip -L https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2024.6.0/sisdk-sdk.zip \
    && unzip -q -d simplicity_sdk_2024.6.0 simplicity_sdk_2024.6.0.zip \
    && rm simplicity_sdk_2024.6.0.zip

# Gecko SDK 4.4.2
RUN \
    curl -o gecko_sdk_4.4.2.zip -L https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.4.2/gecko-sdk.zip \
    && unzip -q -d gecko_sdk_4.4.2 gecko_sdk_4.4.2.zip \
    && rm gecko_sdk_4.4.2.zip

ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

USER $USERNAME
WORKDIR /build
